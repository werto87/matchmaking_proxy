#include "matchmaking_proxy/server/server.hxx"
#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/util.hxx"
#include "networkingUtil.hxx"
#include "util.hxx"
#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
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
#include <iterator>
#include <login_matchmaking_game_shared/matchmakingGameSerialization.hxx>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <matchmaking_proxy/server/matchmakingOption.hxx>
#include <my_web_socket/mockServer.hxx>
#include <my_web_socket/myWebSocket.hxx>
#include <openssl/ssl3.h>
#include <sodium/core.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
using namespace matchmaking_proxy;
using namespace boost::asio;

TEST_CASE ("INTEGRATION TEST user,matchmaking, game", "[.][integration]")
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
  auto matchmakingGame = my_web_socket::MockServer{ { ip::tcp::v4 (), matchmakingGamePort }, { .requestResponse = { { "LeaveGame|{}", "LeaveGameSuccess|{}" } }, .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = my_web_socket::MockServer{ { ip::tcp::v4 (), userGameViaMatchmakingPort }, { .requestResponse = {}, .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  auto const PATH_TO_CHAIN_FILE = std::string{ "/etc/letsencrypt/live/test-name/fullchain.pem" };
  auto const PATH_TO_PRIVATE_File = std::string{ "/etc/letsencrypt/live/test-name/privkey.pem" };
  auto const PATH_TO_DH_File = std::string{ "/etc/letsencrypt/live/test-name/dhparams.pem" };
  auto const POLLING_SLEEP_TIMER = std::chrono::seconds{ 2 };
  using namespace boost::asio::experimental::awaitable_operators;
  co_spawn (ioContext, server.userMatchmaking ({ ip::tcp::v4 (), userMatchmakingPort }, PATH_TO_CHAIN_FILE, PATH_TO_PRIVATE_File, PATH_TO_DH_File, POLLING_SLEEP_TIMER, MatchmakingOption{}, "localhost", std::to_string (matchmakingGamePort), std::to_string (userGameViaMatchmakingPort)) || server.gameMatchmaking ({ ip::tcp::v4 (), gameMatchmakingPort }), my_web_socket::printException);
  SECTION ("start, connect, create account, join game, leave", "[matchmaking]")
  {
    auto messagesFromGamePlayer1 = std::vector<std::string>{};
    size_t gameOver = 0;
    auto handleMsgFromGame = [&gameOver] (boost::asio::io_context &_ioContext, std::string const &msg, std::shared_ptr<my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>> myWebsocket) {
      if (boost::starts_with (msg, "LoginAsGuestSuccess"))
        {
          myWebsocket->queueMessage (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{}));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
        {
          myWebsocket->queueMessage (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true }));
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
    co_spawn (ioContext, connectWebsocketSSL (handleMsgFromGame, ioContext, { ip::tcp::v4 (), userMatchmakingPort }, messagesFromGamePlayer1), my_web_socket::printException);
    auto messagesFromGamePlayer2 = std::vector<std::string>{};
    co_spawn (ioContext, connectWebsocketSSL (handleMsgFromGame, ioContext, { ip::tcp::v4 (), userMatchmakingPort }, messagesFromGamePlayer2), my_web_socket::printException);
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
