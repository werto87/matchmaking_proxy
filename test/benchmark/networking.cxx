#include "catch2/catch.hpp"
#include "matchmaking_proxy/database/constant.hxx"
#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/server/server.hxx"
#include "matchmaking_proxy/util.hxx"
#include "test/networkingUtil.hxx"
#include "util.hxx"
#include <algorithm>
#include <boost/asio/signal_set.hpp>
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
#include <soci/soci.h>
#include <sodium/core.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
TEST_CASE ("1000 messages from one player", "[!benchmark]")
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
  auto messagesFromGamePlayer1 = std::vector<std::string>{};
  size_t messagesSend = 0;
  auto handleMsgFromGame = [&messagesSend] (boost::asio::io_context &_ioContext, std::string const &msg, std::shared_ptr<my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>> myWebsocket) {
    if (boost::starts_with (msg, "LoginAsGuestSuccess"))
      {
        for (uint64_t i = 0; i < 1000; ++i)
          {
            myWebsocket->queueMessage (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{}));
          }
      }
    if (boost::starts_with (msg, "JoinMatchMakingQueue"))
      {
        ++messagesSend;
        if (messagesSend == 1000)
          {
            _ioContext.stop ();
          }
      }
  };
  co_spawn (ioContext, connectWebsocketSSL (handleMsgFromGame, ioContext, { ip::tcp::v4 (), userMatchmakingPort }, messagesFromGamePlayer1), my_web_socket::printException);
  BENCHMARK ("benchmark123") { return ioContext.run_for (std::chrono::seconds{ 30 }); };
  ioContext.stop ();
  ioContext.reset ();
}
