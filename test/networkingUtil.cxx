#include "catch2/catch.hpp"
#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/server/matchmakingOption.hxx"
#include "matchmaking_proxy/util.hxx"
#include "util.hxx"
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <iostream> // for operator<<, ostream
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <my_web_socket/myWebSocket.hxx>
#include <vector> // for allocator
using namespace matchmaking_proxy;

std::shared_ptr<Matchmaking>
createAccountAndJoinMatchmakingQueue (const std::string &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<std::shared_ptr<Matchmaking>> &matchmakings, boost::asio::thread_pool &pool, const user_matchmaking::JoinMatchMakingQueue &joinMatchMakingQueue)
{
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [] (auto) {}, gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address("127.0.0.1"), 33333 } ,"matchmaking_proxy.db"}));
  matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string msg) { messages.push_back (msg); }, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 2222 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address("127.0.0.1"), 33333 },"matchmaking_proxy.db" });
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