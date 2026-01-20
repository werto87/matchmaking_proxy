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
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages1 = std::vector<std::string>{};
  auto messages2 = std::vector<std::string>{};
  auto &matchmaking1 = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages1] (std::string message) { messages1.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  auto &matchmaking2 = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages2] (std::string message) { messages2.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  REQUIRE (matchmaking1->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "a", "" })));
  REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "b", "" })));
  ioContext.run ();
  ioContext.stop ();
  // ioContext.reset ();
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
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages1 = std::vector<std::string>{};
  auto messages2 = std::vector<std::string>{};
  auto &matchmaking1 = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages1] (std::string message) { messages1.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  auto &matchmaking2 = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages2] (std::string message) { messages2.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  REQUIRE (matchmaking1->processEvent (objectToStringWithObjectName (user_matchmaking::SubscribeGetTopRatedPlayers{ 5 })));
  REQUIRE (matchmaking1->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "a", "" })));
  ioContext.run ();
  ioContext.stop ();
  // ioContext.reset ();
  REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "b", "" })));
  ioContext.run ();
  ioContext.stop ();
  // ioContext.reset ();
  auto matchmakingGameTmp = MatchmakingGame{ { "matchmaking_proxy.db", matchmakings, [] (auto) {} } };
  matchmakingGameTmp.process_event (objectToStringWithObjectName (GameOver{ {}, true, { "a" }, { "b" }, {} }));
  REQUIRE (messages1.at (1) == "TopRatedPlayers|{\"players\":[{\"RatedPlayer\":{\"name\":\"a\",\"rating\":1500}}]}");
  REQUIRE (messages1.at (2) == "TopRatedPlayers|{\"players\":[{\"RatedPlayer\":{\"name\":\"a\",\"rating\":1500}},{\"RatedPlayer\":{\"name\":\"b\",\"rating\":1500}}]}");
  REQUIRE (messages1.at (4) == "TopRatedPlayers|{\"players\":[{\"RatedPlayer\":{\"name\":\"a\",\"rating\":1510}},{\"RatedPlayer\":{\"name\":\"b\",\"rating\":1490}}]}");
}

TEST_CASE ("matchmaking game custom message", "[matchmaking game]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto called = false;
  auto matchmakingGame = MatchmakingGame{ { "matchmaking_proxy.db", matchmakings, [] (auto) {}, [&called] (std::string const &, std::string const &, MatchmakingGameData &) { called = true; } } };
  matchmakingGame.process_event (objectToStringWithObjectName (matchmaking_game::CustomMessage{}));
  REQUIRE (called);
}