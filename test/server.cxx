#include "matchmaking_proxy/server/server.hxx"
#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/server/myWebsocket.hxx"
#include "mockserver.hxx"
#include "util.hxx"
#include <algorithm> // for max
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
#include <catch2/catch.hpp>
#include <cstddef>
#include <deque>
#include <exception>
#include <filesystem>
#include <fmt/color.h>
#include <functional>
#include <iostream>
#include <iterator> // for next
#include <login_matchmaking_game_shared/matchmakingGameSerialization.hxx>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <matchmaking_proxy/server/matchmakingOption.hxx>
#include <openssl/ssl3.h>
#include <sodium/core.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility> // for pair
using namespace boost::asio;
typedef boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> SSLWebsocket;
typedef boost::beast::websocket::stream<boost::asio::use_awaitable_t<>::as_default_on_t<boost::beast::tcp_stream>> Websocket;
boost::asio::awaitable<void>
connectWebsocketSSL (auto handleMsgFromGame, io_context &ioContext, boost::asio::ip::tcp::endpoint endpoint, std::vector<std::string> &messagesFromGame)
{
  try
    {
      using namespace boost::asio;
      using namespace boost::beast;
      ssl::context ctx{ ssl::context::tlsv12_client };
      ctx.set_verify_mode (boost::asio::ssl::verify_none); // DO NOT USE THIS IN PRODUCTION THIS WILL IGNORE CHECKING FOR TRUSTFUL CERTIFICATE
      try
        {
          auto connection = std::make_shared<SSLWebsocket> (SSLWebsocket{ ioContext, ctx });
          get_lowest_layer (*connection).expires_never ();
          connection->set_option (websocket::stream_base::timeout::suggested (role_type::client));
          connection->set_option (websocket::stream_base::decorator ([] (websocket::request_type &req) { req.set (http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl"); }));
          co_await get_lowest_layer (*connection).async_connect (endpoint, use_awaitable);
          co_await connection->next_layer ().async_handshake (ssl::stream_base::client, use_awaitable);
          co_await connection->async_handshake ("localhost:" + std::to_string (endpoint.port ()), "/", use_awaitable);
          co_await connection->async_write (boost::asio::buffer (std::string{ "LoginAsGuest|{}" }), use_awaitable);
          static size_t id = 0;
          auto myWebsocket = std::make_shared<MyWebsocket<SSLWebsocket>> (MyWebsocket<SSLWebsocket>{ std::move (connection), "connectWebsocketSSL", fmt::fg (fmt::color::chocolate), std::to_string (id++) });
          using namespace boost::asio::experimental::awaitable_operators;
          co_await (myWebsocket->readLoop ([myWebsocket, handleMsgFromGame, &ioContext, &messagesFromGame] (const std::string &msg) {
            messagesFromGame.push_back (msg);
            handleMsgFromGame (ioContext, msg, myWebsocket);
          }) && myWebsocket->writeLoop ());
        }
      catch (std::exception const &e)
        {
          std::cout << "connectWebsocketSSL () connect  Exception : " << e.what () << std::endl;
        }
    }
  catch (std::exception const &e)
    {
      std::cout << "exception: " << e.what () << std::endl;
    }
}

boost::asio::awaitable<void>
connectWebsocket (io_context &ioContext, boost::asio::ip::tcp::endpoint const &endpoint, std::vector<std::string> &messageFromMatchmaking, std::vector<std::string> const &sendMessageBeforeStartRead)
{
  try
    {
      using namespace boost::asio;
      using namespace boost::beast;

      try
        {
          auto connection = std::make_shared<Websocket> (Websocket{ ioContext });
          get_lowest_layer (*connection).expires_never ();
          connection->set_option (websocket::stream_base::timeout::suggested (role_type::client));
          connection->set_option (websocket::stream_base::decorator ([] (websocket::request_type &req) { req.set (http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl"); }));
          co_await get_lowest_layer (*connection).async_connect (endpoint, use_awaitable);
          co_await connection->async_handshake ("localhost:" + std::to_string (endpoint.port ()), "/", use_awaitable);
          for (auto message : sendMessageBeforeStartRead)
            {
              co_await connection->async_write (boost::asio::buffer (message), use_awaitable);
            }
          static size_t id = 0;
          auto myWebsocket = std::make_shared<MyWebsocket<Websocket>> (MyWebsocket<Websocket>{ std::move (connection), "connectWebsocket", fmt::fg (fmt::color::beige), std::to_string (id++) });
          using namespace boost::asio::experimental::awaitable_operators;
          co_await (myWebsocket->readLoop ([&ioContext, myWebsocket, &messageFromMatchmaking] (const std::string &msg) {
            if (msg == "GameOverSuccess|{}")
              {
                ioContext.stop ();
              }
            messageFromMatchmaking.push_back (msg);
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

TEST_CASE ("user,matchmaking, game", "[integration]")
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
  io_context ioContext (1);
  signal_set signals (ioContext, SIGINT, SIGTERM);
  signals.async_wait ([&] (auto, auto) { ioContext.stop (); });
  thread_pool pool{ 2 };
  auto server = Server{ ioContext, pool };
  auto const userMatchmakingPort = 55555;
  auto const gameMatchmakingPort = 22222;
  auto const matchmakingGamePort = 44444;
  auto const userGameViaMatchmakingPort = 33333;
  auto matchmakingGame = Mockserver{ { ip::tcp::v4 (), matchmakingGamePort }, { .requestResponse = { { "LeaveGame|{}", "LeaveGameSuccess|{}" } }, .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = Mockserver{ { ip::tcp::v4 (), userGameViaMatchmakingPort }, { .requestResponse = {}, .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  auto const PATH_TO_CHAIN_FILE = std::string{ "/etc/letsencrypt/live/test-name/fullchain.pem" };
  auto const PATH_TO_PRIVATE_File = std::string{ "/etc/letsencrypt/live/test-name/privkey.pem" };
  auto const PATH_TO_DH_File = std::string{ "/etc/letsencrypt/live/test-name/dhparams.pem" };
  auto const POLLING_SLEEP_TIMER = std::chrono::seconds{ 2 };
  using namespace boost::asio::experimental::awaitable_operators;
  co_spawn (ioContext, server.userMatchmaking ({ ip::tcp::v4 (), userMatchmakingPort }, PATH_TO_CHAIN_FILE, PATH_TO_PRIVATE_File, PATH_TO_DH_File, POLLING_SLEEP_TIMER, MatchmakingOption{}, "localhost", std::to_string (matchmakingGamePort), std::to_string (userGameViaMatchmakingPort)) || server.gameMatchmaking ({ ip::tcp::v4 (), gameMatchmakingPort }), printException);
  SECTION ("start, connect, create account, join game, leave", "[matchmaking]")
  {
    auto messagesFromGamePlayer1 = std::vector<std::string>{};
    size_t gameOver = 0;
    auto handleMsgFromGame = [&gameOver] (boost::asio::io_context &_ioContext, std::string const &msg, std::shared_ptr<MyWebsocket<SSLWebsocket>> myWebsocket) {
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
          gameOver++;
          if (gameOver == 2)
            {
              _ioContext.stop ();
            }
        }
    };
    co_spawn (ioContext, connectWebsocketSSL (handleMsgFromGame, ioContext, { ip::tcp::v4 (), userMatchmakingPort }, messagesFromGamePlayer1), printException);
    auto messagesFromGamePlayer2 = std::vector<std::string>{};
    co_spawn (ioContext, connectWebsocketSSL (handleMsgFromGame, ioContext, { ip::tcp::v4 (), userMatchmakingPort }, messagesFromGamePlayer2), printException);
    ioContext.run_for (std::chrono::seconds{ 5 });
    CHECK (messagesFromGamePlayer1.size () == 4);
    CHECK (boost::starts_with (messagesFromGamePlayer1.at (0), "LoginAsGuestSuccess"));
    CHECK (messagesFromGamePlayer1.at (1) == "JoinMatchMakingQueueSuccess|{}");
    CHECK (messagesFromGamePlayer1.at (2) == "AskIfUserWantsToJoinGame|{}");
    CHECK (messagesFromGamePlayer1.at (3) == "ProxyStarted|{}");
    CHECK (messagesFromGamePlayer2.size () == 4);
    CHECK (boost::starts_with (messagesFromGamePlayer2.at (0), "LoginAsGuestSuccess"));
    CHECK (messagesFromGamePlayer2.at (1) == "JoinMatchMakingQueueSuccess|{}");
    CHECK (messagesFromGamePlayer2.at (2) == "AskIfUserWantsToJoinGame|{}");
    CHECK (messagesFromGamePlayer2.at (3) == "ProxyStarted|{}");
  }
  ioContext.stop ();
  ioContext.reset ();
}
