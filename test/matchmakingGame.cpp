//
// Created by walde on 10/24/22.
//

#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/database/database.hxx" // for cre...
#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/util.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/sml.hpp>
#include <catch2/catch.hpp> // for Ass...
#include <login_matchmaking_game_shared/matchmakingGameSerialization.hxx>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <my_web_socket/mockServer.hxx>
using namespace matchmaking_proxy;
using namespace user_matchmaking;
using namespace matchmaking_game;

TEST_CASE ("game sends message to matchmaking", "[matchmaking game]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::weak_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages1 = std::vector<std::string>{};
  auto messages2 = std::vector<std::string>{};

  auto matchmaking1 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages1] (std::string message) { messages1.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
  auto matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages2] (std::string message) { messages2.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
  matchmakings.emplace_back (matchmaking1);
  matchmakings.emplace_back (matchmaking2);
  REQUIRE (matchmaking1->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "a", "" })));
  REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "b", "" })));
  ioContext.run ();
  auto matchmakingGameTmp = MatchmakingGame{ { "matchmaking_proxy.db", matchmakings, [] (auto) {} } };
  matchmakingGameTmp.process_event (objectToStringWithObjectName (GameOver{ {}, true, { "a" }, { "b" }, {} }));
  REQUIRE (messages1.at (1) == "RatingChanged|{\"oldRating\":1500,\"newRating\":1510}");
  REQUIRE (messages2.at (1) == "RatingChanged|{\"oldRating\":1500,\"newRating\":1490}");
}

TEST_CASE ("SubscribeGetTopRatedPlayers game over", "[matchmaking game]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context{};
  auto matchmakingGame = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "MOCK_matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "MOCK_userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<std::weak_ptr<Matchmaking>> matchmakings{};
  auto matchmaking = std::shared_ptr<Matchmaking>{};
  auto matchmaking2 = std::shared_ptr<Matchmaking>{};
  auto messages2 = std::vector<std::string>{};
  auto player1Logic = [&] (auto const &) {};
  matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 1000 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
  matchmakings.push_back (matchmaking);
  auto player2Logic = [&] (auto const &msg) {
    messages2.push_back (msg);
    if (boost::starts_with (msg, "LoginAccountSuccess"))
      {
        REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::SubscribeGetTopRatedPlayers{ 5 })));
        REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
      }
  };
  matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
  matchmakings.push_back (matchmaking2);
  REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
  ioContext.run ();
  auto matchmakingGameTmp = MatchmakingGame{ { "matchmaking_proxy.db", matchmakings, [] (auto) {} } };
  matchmakingGameTmp.process_event (objectToStringWithObjectName (GameOver{ {}, true, { "player1" }, { "player2" }, {} }));
  CHECK (messages2.size () == 5);
  CHECK (messages2.at (1) == "TopRatedPlayers|{\"players\":[{\"RatedPlayer\":{\"name\":\"player2\",\"rating\":1500}}]}");
  CHECK (messages2.at (2) == "TopRatedPlayers|{\"players\":[{\"RatedPlayer\":{\"name\":\"player2\",\"rating\":1500}},{\"RatedPlayer\":{\"name\":\"player1\",\"rating\":1500}}]}");
  CHECK (messages2.at (3) == "RatingChanged|{\"oldRating\":1500,\"newRating\":1490}");
  CHECK (messages2.at (4) == "TopRatedPlayers|{\"players\":[{\"RatedPlayer\":{\"name\":\"player1\",\"rating\":1510}},{\"RatedPlayer\":{\"name\":\"player2\",\"rating\":1490}}]}");
  matchmakingGame.shutDownUsingMockServerIoContext ();
  userGameViaMatchmaking.shutDownUsingMockServerIoContext ();
}

TEST_CASE ("matchmaking game custom message", "[matchmaking game]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::weak_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto called = false;
  auto matchmakingGame = MatchmakingGame{ { "matchmaking_proxy.db", matchmakings, [] (auto) {}, [&called] (std::string const &, std::string const &, MatchmakingGameData &) { called = true; } } };
  matchmakingGame.process_event (objectToStringWithObjectName (matchmaking_game::CustomMessage{}));
  REQUIRE (called);
}