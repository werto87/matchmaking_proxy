// TODO write here some tests which uses the server class
// TODO find out how to mock it so server sends game over to macthmaking
// LOL i could just send the msg to matchmaking by connecting on the game port 33333 of matchmaking xD but i can keep this code as integration test

#include "matchmaking_proxy/server/server.hxx"
#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/userMatchmakingSerialization.hxx"
#include "matchmaking_proxy/util.hxx"
#include "test/mockserver.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
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
          auto connection = std::make_shared<SSLWebsocket> (SSLWebsocket{ ioContext, ctx });
          get_lowest_layer (*connection).expires_never ();
          connection->set_option (websocket::stream_base::timeout::suggested (role_type::client));
          connection->set_option (websocket::stream_base::decorator ([] (websocket::request_type &req) { req.set (http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl"); }));
          co_await get_lowest_layer (*connection).async_connect (endpoint, use_awaitable);
          co_await connection->next_layer ().async_handshake (ssl::stream_base::client, use_awaitable);
          co_await connection->async_handshake ("localhost:44444", "/", use_awaitable);
          co_await connection->async_write (boost::asio::buffer (std::string{ "LoginAsGuest|{}" }), use_awaitable);
          flat_buffer buffer;
          auto myWebsocket = std::make_shared<MyWebsocket<SSLWebsocket>> (MyWebsocket<SSLWebsocket>{ std::move (connection) });

          using namespace boost::asio::experimental::awaitable_operators;
          co_await(myWebsocket->readLoop ([myWebsocket, &ioContext] (const std::string &msg) {
            if (boost::starts_with (msg, "LoginAsGuestSuccess"))
              {
                myWebsocket->sendMessage (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{}));
              }
            else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
              {
                myWebsocket->sendMessage (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true }));
              }
            else if (boost::starts_with (msg, "ProxyStarted"))
              {
                myWebsocket->sendMessage ("LeaveGame|{}");
              }
            else if (boost::starts_with (msg, "LeaveGameSuccess"))
              {
                // TODO create a test where this has to be called CHECK_IS_CALLED OR SOMETHING
                ioContext.stop ();
              }
          }) && myWebsocket->writeLoop ());
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
TEST_CASE ("server", "[integration]")
{

  SECTION ("start connect create account join game leave", "[matchmaking]")
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
    auto mockserver = Mockserver{ { ip::tcp::v4 (), 44444 }, { .requestResponse = { { "LeaveGame|{}", "LeaveGameSuccess|{}" } }, .requestStartsWithResponse = { { R"foo(StartGame)foo", "StartGameSuccess|{}" } } } };
    auto const pathToSecrets = std::filesystem::path{ "/home/walde/certificate/otherTestCert" };
    auto userEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), userPort };
    auto gameEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), gamePort };
    using namespace boost::asio::experimental::awaitable_operators;
    co_spawn (io_context, server.userMatchmaking (userEndpoint, pathToSecrets) || server.gameMatchmaking (gameEndpoint), printException);
    co_spawn (io_context, connectWebsocket (io_context, userEndpoint, pathToSecrets), printException);
    co_spawn (io_context, connectWebsocket (io_context, userEndpoint, pathToSecrets), printException);
    io_context.run ();
  }
}
