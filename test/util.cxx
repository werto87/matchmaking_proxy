#include "matchmaking_proxy/util.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include <boost/asio/ip/tcp.hpp>
#include <catch2/catch.hpp>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
using namespace matchmaking_proxy;
std::shared_ptr<Matchmaking>
createAccountAndJoinMatchmakingQueue (const std::string &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<std::shared_ptr<Matchmaking>> &matchmakings, boost::asio::thread_pool &pool, const user_matchmaking::JoinMatchMakingQueue &joinMatchMakingQueue)
{
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [] (auto) {}, gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string msg) { messages.push_back (msg); }, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 2222 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } });
  ioContext.stop ();
  ioContext.reset ();
  ioContext.restart ();
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ playerName, "abc" })));
  ioContext.run_for (std::chrono::milliseconds{ 100 });
  ioContext.stop ();
  ioContext.reset ();
  ioContext.restart ();
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (joinMatchMakingQueue)));
  ioContext.run_for (std::chrono::milliseconds{ 10 });
  return matchmaking;
}
