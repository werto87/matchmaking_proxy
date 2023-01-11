//
// Created by walde on 10/24/22.
//

#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "../matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/database/database.hxx" // for cre...
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/util.hxx"
#include "mockserver.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/sml.hpp>
#include <catch2/catch.hpp> // for Ass...
#include <login_matchmaking_game_shared_type/matchmakingGameSerialization.hxx>
#include <login_matchmaking_game_shared_type/userMatchmakingSerialization.hxx>

using namespace user_matchmaking;
using namespace matchmaking_game;

TEST_CASE ("game sends message to matchmaking", "[matchmaking game]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking1 = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  auto &matchmaking2 = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  matchmaking1->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "a", "" }));
  matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "b", "" }));
  ioContext.run ();
  ioContext.stop ();
  ioContext.reset ();
  auto matchmakingGame = MatchmakingGame{ matchmakings, [] (auto) {} };
  matchmakingGame.process_event (objectToStringWithObjectName (GameOver{ {}, true, { "a" }, { "b" }, {} }));
  REQUIRE (messages.at (2) == "RatingChanged|{\"oldRating\":1500,\"newRating\":1490}");
  REQUIRE (messages.at (3) == "RatingChanged|{\"oldRating\":1500,\"newRating\":1510}");
}