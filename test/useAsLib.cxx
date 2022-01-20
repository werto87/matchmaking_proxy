#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/server/server.hxx"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <filesystem>
#include <range/v3/algorithm/find_if.hpp>
#include <sodium/core.h>
#ifdef BOOST_ASIO_HAS_CLANG_LIBCXX
#include <experimental/coroutine>
#endif
#include "matchmaking_proxy/server/myWebsocket.hxx"
#include <algorithm> // for max
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
#include <catch2/catch.hpp>
#include <deque>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator> // for next
#include <openssl/ssl3.h>
#include <range/v3/view.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility> // for pair

using namespace boost::asio;

auto const printException = [] (std::exception_ptr eptr, auto) {
  try
    {
      if (eptr)
        {
          std::rethrow_exception (eptr);
        }
    }
  catch (std::exception const &e)
    {
      std::cout << "unhandled exception: '" << e.what () << "'" << std::endl;
    }
  std::cout << "userMatchmaking || gameMatchmaking DONE" << std::endl;
};
auto const printException0 = [] (std::exception_ptr eptr) {
  try
    {
      if (eptr)
        {
          std::rethrow_exception (eptr);
        }
    }
  catch (std::exception const &e)
    {
      std::cout << "unhandled exception: '" << e.what () << "'" << std::endl;
    }
  std::cout << "connectWebsocket DONE" << std::endl;
};

typedef boost::beast::websocket::stream<boost::asio::use_awaitable_t<>::as_default_on_t<boost::beast::tcp_stream>> Websocket;
boost::asio::awaitable<void>
connectWebsocket (io_context &ioContext, boost::asio::ip::tcp::endpoint const &endpoint, std::filesystem::path const &pathToSecrets)
{
  try
    {
      using namespace boost::asio;
      using namespace boost::beast;
      ssl::context ctx{ ssl::context::tlsv12_client };
      ctx.set_verify_mode (boost::asio::ssl::verify_none); // DO NOT USE THIS IN PRODUCTION THIS WILL IGNORE CHECKING FOR TRUSTFUL CERTIFICATE
      try
        {
          typedef boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> SSLWebsocket;

          // auto ws = std::make_shared<SSLWebsocket> (SSLWebsocket{ ioContext, ctx });
          auto connection = std::make_shared<SSLWebsocket> (SSLWebsocket{ ioContext, ctx });
          get_lowest_layer (*connection).expires_never ();

          // Set suggested timeout settings for the websocket
          connection->set_option (websocket::stream_base::timeout::suggested (role_type::client));

          // Set a decorator to change the User-Agent of the handshake
          connection->set_option (websocket::stream_base::decorator ([] (websocket::request_type &req) { req.set (http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl"); }));
          // connection->set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
          // co_await connection->next_layer ().next_layer ().async_connect (endpoint, use_awaitable);
          // if (!SSL_set_tlsext_host_name (connection->next_layer ().native_handle (), "/"))
          //   {
          //     error_code ec{};
          //     ec = error_code (static_cast<int> (::ERR_get_error ()), net::error::get_ssl_category ());
          //     std::cerr << "connect"
          //               << ": " << ec.message () << "\n";
          //   }
          co_await get_lowest_layer (*connection).async_connect (endpoint, use_awaitable);
          co_await connection->next_layer ().async_handshake (ssl::stream_base::client, use_awaitable);
          co_await connection->async_handshake ("localhost:44444", "/", use_awaitable);
          flat_buffer buffer;
          // co_await connection->async_write (buffer, use_awaitable);

          auto myWebsocket = std::make_shared<MyWebsocket<SSLWebsocket>> (MyWebsocket<SSLWebsocket>{ std::move (connection) });
          using namespace boost::asio::experimental::awaitable_operators;
          co_await(myWebsocket->readLoop ([myWebsocket] (const std::string &msg) {
            if (msg == "HUHU")
              {
                myWebsocket->sendMessage ("LOL");
              }
            std::cout << "msgToUser: " << msg << std::endl;
          }) || myWebsocket->writeLoop ());
        }
      catch (std::exception const &e)
        {
          std::cout << "connectWebsocket () connect  Exception : " << e.what () << std::endl;
        }
    }
  catch (std::exception const &e)
    {
      std::cout << "exception: " << e.what () << std::endl;
    }
}
TEST_CASE ("integration test", "[integration]")
{

  SECTION ("start the server to play around", "[matchmaking]")
  {
    if (sodium_init () < 0)
      {
        std::cout << "sodium_init <= 0" << std::endl;
        std::terminate ();
        /* panic! the library couldn't be initialized, it is not safe to use */
      }
    database::createEmptyDatabase ();
    database::createTables ();
    using namespace boost::asio;
    io_context io_context (1);
    signal_set signals (io_context, SIGINT, SIGTERM);
    signals.async_wait ([&] (auto, auto) { io_context.stop (); });
    thread_pool pool{ 2 };
    auto server = Server{ io_context, pool };
    auto const userPort = 55555;
    auto const gamePort = 33333;
    auto const pathToSecrets = std::filesystem::path{ "/home/walde/certificate/otherTestCert" };
    auto userEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), userPort };
    auto gameEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), gamePort };
    using namespace boost::asio::experimental::awaitable_operators;
    co_spawn (io_context, server.userMatchmaking (userEndpoint, pathToSecrets) || server.gameMatchmaking (gameEndpoint), printException);
    co_spawn (io_context, connectWebsocket (io_context, userEndpoint, pathToSecrets), printException0);
    io_context.run ();
  }
}
