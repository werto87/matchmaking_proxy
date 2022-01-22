#ifndef FB5474CE_322D_4D7A_B298_185229E7B05A
#define FB5474CE_322D_4D7A_B298_185229E7B05A

#include "matchmaking_proxy/server/myWebsocket.hxx"
#include "util.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>
#include <cstddef>
#include <exception>
#include <map>
#include <variant>

typedef boost::beast::websocket::stream<boost::asio::use_awaitable_t<>::as_default_on_t<boost::beast::tcp_stream>> Websocket;
typedef boost::asio::basic_stream_socket<boost::asio::ip::tcp, boost::asio::use_awaitable_t<>::executor_with_default<boost::asio::any_io_executor>> Socket;

struct MockserverOption
{
  std::map<std::string, std::string> requestResponse{};
  std::map<std::string, std::string> requestStartsWithResponse{};
};

struct Mockserver
{
  Mockserver (boost::asio::ip::tcp::endpoint endpoint, MockserverOption const &mockserverOption_) : mockserverOption{ mockserverOption_ }
  {
    co_spawn (ioContext, listener (endpoint), printException);
    thread = std::thread{ [this] () { ioContext.run (); } };
  }

  ~Mockserver ()
  {
    ioContext.stop ();
    thread.join ();
  }

  boost::asio::awaitable<void>
  listener (boost::asio::ip::tcp::endpoint endpoint)
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
            auto socket = co_await(acceptor.async_accept ());
            auto connection = std::make_shared<Websocket> (Websocket{ std::move (socket) });
            connection->set_option (websocket::stream_base::timeout::suggested (role_type::server));
            connection->set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
            co_await connection->async_accept ();
            websockets.emplace_back (MyWebsocket<Websocket>{ std::move (connection) });
            std::list<MyWebsocket<Websocket>>::iterator websocket = std::prev (websockets.end ());
            boost::asio::co_spawn (executor, websocket->readLoop ([websocket, &mockserverOption = mockserverOption] (const std::string &msg) mutable {
              if (mockserverOption.requestResponse.count (msg)) websocket->sendMessage (mockserverOption.requestResponse.at (msg));
              auto msgFound = false;
              for (auto const &[startsWith, response] : mockserverOption.requestStartsWithResponse)
                {
                  if (boost::starts_with (msg, startsWith))
                    {
                      msgFound = true;
                      websocket->sendMessage (response);
                      break;
                    }
                }
              if (not msgFound)
                {
                  std::cout << "unhandled message: " << msg << std::endl;
                }
            }) || websocket->writeLoop (),
                                   [&websockets = websockets, websocket] (auto, auto) { websockets.erase (websocket); });
          }
        catch (std::exception &e)
          {
            std::cout << "Mockserver::listener ()  Exception : " << e.what () << std::endl;
            throw;
          }
      }
    for (auto &websocket : websockets)
      websocket.close ();
  }
  MockserverOption mockserverOption{};
  bool shouldRun = true;
  boost::asio::io_context ioContext;
  std::thread thread{};
  std::list<MyWebsocket<Websocket>> websockets{ {} };
};

#endif /* FB5474CE_322D_4D7A_B298_185229E7B05A */
