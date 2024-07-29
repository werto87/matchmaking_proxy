#ifndef FB5474CE_322D_4D7A_B298_185229E7B05A
#define FB5474CE_322D_4D7A_B298_185229E7B05A

#include "matchmaking_proxy/util.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/websocket.hpp>
#include <cstddef>
#include <exception>
#include <map>
#include <my_web_socket/myWebSocket.hxx>
#include <thread>
#include <variant>
using namespace matchmaking_proxy;

typedef boost::asio::basic_stream_socket<boost::asio::ip::tcp, boost::asio::use_awaitable_t<>::executor_with_default<boost::asio::any_io_executor>> Socket;

struct MockserverOption
{
  std::optional<std::string> disconnectOnMessage{};
  std::map<std::string, std::string> requestResponse{};
  std::map<std::string, std::string> requestStartsWithResponse{};
  std::map<std::string, std::function<void ()>> callOnMessageStartsWith{};
};

struct Mockserver
{
  Mockserver (boost::asio::ip::tcp::endpoint endpoint, MockserverOption const &mockserverOption_, std::string loggingName_ = {}, fmt::text_style loggingTextStyleForName_ = {}, std::string id_ = {}) : mockserverOption{ mockserverOption_ }
  {
    co_spawn (ioContext, listener (endpoint, loggingName_, loggingTextStyleForName_, id_), printException);
    thread = std::thread{ [this] () { ioContext.run (); } };
  }

  ~Mockserver ()
  {
    ioContext.stop ();
    thread.join ();
  }

  boost::asio::awaitable<void>
  listener (boost::asio::ip::tcp::endpoint endpoint, std::string loggingName_, fmt::text_style loggingTextStyleForName_, std::string id_)
  {
    using namespace boost::beast;
    using namespace boost::asio;
    using boost::asio::ip::tcp;
    using tcp_acceptor = use_awaitable_t<>::as_default_on_t<tcp::acceptor>;
    auto executor = co_await this_coro::executor;
    tcp_acceptor acceptor (executor, endpoint);
    while (shouldRun)
      {
        try
          {
            using namespace boost::asio::experimental::awaitable_operators;
            auto socket = co_await (acceptor.async_accept ());
            auto connection = my_web_socket::WebSocket{ std::move (socket) };
            connection.set_option (websocket::stream_base::timeout::suggested (role_type::server));
            connection.set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
            co_await connection.async_accept ();
            websockets.emplace_back (my_web_socket::MyWebSocket<my_web_socket::WebSocket>{ std::move (connection), loggingName_, loggingTextStyleForName_, id_ });
            std::list<my_web_socket::MyWebSocket<my_web_socket::WebSocket>>::iterator websocket = std::prev (websockets.end ());
            boost::asio::co_spawn (executor, websocket->readLoop ([websocket, &_mockserverOption = mockserverOption, &_ioContext = ioContext] (const std::string &msg) mutable {
              for (auto const &[startsWith, callback] : _mockserverOption.callOnMessageStartsWith)
                {
                  if (boost::starts_with (msg, startsWith))
                    {
                      callback ();
                      break;
                    }
                }
              if (_mockserverOption.disconnectOnMessage && _mockserverOption.disconnectOnMessage.value () == msg)
                {
                  _ioContext.stop ();
                }
              else if (_mockserverOption.requestResponse.count (msg))
                websocket->queueMessage (_mockserverOption.requestResponse.at (msg));
              else
                {
                  auto msgFound = false;
                  for (auto const &[startsWith, response] : _mockserverOption.requestStartsWithResponse)
                    {
                      if (boost::starts_with (msg, startsWith))
                        {
                          msgFound = true;
                          websocket->queueMessage (response);
                          break;
                        }
                    }
                  if (not msgFound)
                    {
                      std::cout << "unhandled message: " << msg << std::endl;
                    }
                }
            }) && websocket->writeLoop (),
                                   [&_websockets = websockets, websocket] (auto eptr) {
                                     printException (eptr);
                                     _websockets.erase (websocket);
                                   });
          }
        catch (std::exception const &e)
          {
            std::cout << "Mockserver::listener ()  Exception : " << e.what () << std::endl;
            throw e;
          }
      }
    for (auto &websocket : websockets)
      websocket.close ();
  }
  MockserverOption mockserverOption{};
  bool shouldRun = true;
  boost::asio::io_context ioContext;
  std::thread thread{};
  std::list<my_web_socket::MyWebSocket<my_web_socket::WebSocket>> websockets{ {} };
};

#endif /* FB5474CE_322D_4D7A_B298_185229E7B05A */
