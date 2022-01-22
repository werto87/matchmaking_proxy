#include "../matchmaking_proxy/logic/matchmaking.hxx"
#include "../matchmaking_proxy/userMatchmakingSerialization.hxx" // for Cre...
#include "matchmaking_proxy/database/database.hxx"               // for cre...
#include "matchmaking_proxy/matchmakingGameSerialization.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/util.hxx"
#include "test/mockserver.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>
#include <catch2/catch.hpp> // for Ass...
#include <chrono>

using namespace user_matchmaking;

Matchmaking &
createAccountAndJoinMatchmakingGame (std::string const &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<Matchmaking> &matchmakings, boost::asio::thread_pool &pool, JoinMatchMakingQueue const &joinMatchMakingQueue)
{
  auto &matchmaking = matchmakings.emplace_back (
      ioContext, matchmakings, [] (auto) {}, gameLobbies, pool);
  auto matchmakingItr = std::prev (matchmakings.end ());
  matchmaking = { ioContext, matchmakings,
                  [&messages, matchmakingItr] (std::string msg) {
                    messages.push_back (msg);
                    if (msg == "ProxyStarted|{}")
                      {
                        matchmakingItr->process_event (objectToStringWithObjectName (LeaveGame{}));
                      }
                  },
                  gameLobbies, pool };
  matchmaking.process_event (objectToStringWithObjectName (CreateAccount{ playerName, "abc" }));
  ioContext.run_for (std::chrono::seconds{ 1 });
  ioContext.stop ();
  ioContext.reset ();
  matchmaking.process_event (objectToStringWithObjectName (joinMatchMakingQueue));
  return matchmaking;
}

TEST_CASE ("2 player join quick game queue not ranked", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  auto mockserver = Mockserver{ { ip::tcp::v4 (), 44444 }, { .requestResponse = { { R"foo(StartGame|{"players":["player1","player2"],"gameOption":{"someBool":false,"someString":""},"ratedGame":false})foo", "StartGameSuccess|{}" }, { "LeaveGame|{}", "LeaveGameSuccess|{}" } } } };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<Matchmaking> matchmakings{};
  auto messagesPlayer1 = std::vector<std::string>{};
  auto &matchmakingPlayer1 = createAccountAndJoinMatchmakingGame ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ false });
  CHECK (messagesPlayer1.size () == 2);
  CHECK (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player1" + std::string{ "\"}" } == messagesPlayer1.at (0));
  CHECK (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer1.at (1));
  auto messagesPlayer2 = std::vector<std::string>{};
  auto &matchmakingPlayer2 = createAccountAndJoinMatchmakingGame ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ false });
  CHECK (messagesPlayer1.size () == 3);
  CHECK (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player2" + std::string{ "\"}" } == messagesPlayer2.at (0));
  CHECK (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer2.at (1));
  CHECK (R"foo(AskIfUserWantsToJoinGame|{})foo" == messagesPlayer2.at (2));
  CHECK (gameLobbies.size () == 1);
  messagesPlayer1.clear ();
  messagesPlayer2.clear ();
  SECTION ("both player accept invite", "[matchmaking]")
  {
    matchmakingPlayer1.process_event (objectToStringWithObjectName (WantsToJoinGame{ true }));
    matchmakingPlayer2.process_event (objectToStringWithObjectName (WantsToJoinGame{ true }));
    ioContext.run_for (std::chrono::seconds{ 1 });
    CHECK (messagesPlayer1.size () == 3);
    CHECK ("ProxyStarted|{}" == messagesPlayer1.at (0));
    CHECK ("LeaveGameSuccess|{}" == messagesPlayer1.at (1));
    CHECK ("ProxyStopped|{}" == messagesPlayer1.at (2));
    CHECK (messagesPlayer2.size () == 3);
    CHECK ("ProxyStarted|{}" == messagesPlayer2.at (0));
    CHECK ("LeaveGameSuccess|{}" == messagesPlayer2.at (1));
    CHECK ("ProxyStopped|{}" == messagesPlayer2.at (2));
  }
  SECTION ("one player accept one player declined", "[matchmaking]")
  {
    matchmakingPlayer1.process_event (objectToStringWithObjectName (WantsToJoinGame{ true }));
    matchmakingPlayer2.process_event (objectToStringWithObjectName (WantsToJoinGame{ false }));
    ioContext.run_for (std::chrono::seconds{ 1 });
    CHECK (messagesPlayer1.size () == 1);
    CHECK ("GameStartCanceled|{}" == messagesPlayer1.at (0));
    CHECK (messagesPlayer2.size () == 1);
    CHECK ("GameStartCanceledRemovedFromQueue|{}" == messagesPlayer2.at (0));
  }
  SECTION ("one player declined one player accept", "[matchmaking]")
  {
    matchmakingPlayer1.process_event (objectToStringWithObjectName (WantsToJoinGame{ false }));
    matchmakingPlayer2.process_event (objectToStringWithObjectName (WantsToJoinGame{ true }));
    ioContext.run_for (std::chrono::seconds{ 1 });
    CHECK (messagesPlayer1.size () == 1);
    CHECK ("GameStartCanceledRemovedFromQueue|{}" == messagesPlayer1.at (0));
    CHECK (messagesPlayer2.size () == 2);
    CHECK ("GameStartCanceled|{}" == messagesPlayer2.at (0));
    CHECK (R"foo(WantsToJoinGameError|{"error":"No game to join"})foo" == messagesPlayer2.at (1));
  }
  ioContext.stop ();
  ioContext.reset ();
}

TEST_CASE ("2 player join quick game queue ranked", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  auto mockserver = Mockserver{ { ip::tcp::v4 (), 44444 }, { .requestResponse = { { R"foo(StartGame|{"players":["player1","player2"],"gameOption":{"someBool":false,"someString":""},"ratedGame":true})foo", "StartGameSuccess|{}" }, { "LeaveGame|{}", "LeaveGameSuccess|{}" } } } };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<Matchmaking> matchmakings{};
  auto messagesPlayer1 = std::vector<std::string>{};
  auto &matchmakingPlayer1 = createAccountAndJoinMatchmakingGame ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ true });
  CHECK (messagesPlayer1.size () == 2);
  CHECK (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player1" + std::string{ "\"}" } == messagesPlayer1.at (0));
  CHECK (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer1.at (1));
  auto messagesPlayer2 = std::vector<std::string>{};
  auto &matchmakingPlayer2 = createAccountAndJoinMatchmakingGame ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ true });
  CHECK (messagesPlayer1.size () == 3);
  CHECK (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player2" + std::string{ "\"}" } == messagesPlayer2.at (0));
  CHECK (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer2.at (1));
  CHECK (R"foo(AskIfUserWantsToJoinGame|{})foo" == messagesPlayer2.at (2));
  CHECK (gameLobbies.size () == 1);
  messagesPlayer1.clear ();
  messagesPlayer2.clear ();
  SECTION ("both player accept invite", "[matchmaking]")
  {
    matchmakingPlayer1.process_event (objectToStringWithObjectName (WantsToJoinGame{ true }));
    matchmakingPlayer2.process_event (objectToStringWithObjectName (WantsToJoinGame{ true }));
    ioContext.run_for (std::chrono::seconds{ 1 });
    CHECK (messagesPlayer1.size () == 3);
    CHECK ("ProxyStarted|{}" == messagesPlayer1.at (0));
    CHECK ("LeaveGameSuccess|{}" == messagesPlayer1.at (1));
    CHECK ("ProxyStopped|{}" == messagesPlayer1.at (2));
    CHECK (messagesPlayer2.size () == 3);
    CHECK ("ProxyStarted|{}" == messagesPlayer2.at (0));
    CHECK ("LeaveGameSuccess|{}" == messagesPlayer2.at (1));
    CHECK ("ProxyStopped|{}" == messagesPlayer2.at (2));
  }
  ioContext.stop ();
  ioContext.reset ();
}

Matchmaking &
createAccountCreateGameLobby (std::string const &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<Matchmaking> &matchmakings, boost::asio::thread_pool &pool, CreateGameLobby const &createGameLobby)
{
  auto &matchmaking = matchmakings.emplace_back (
      ioContext, matchmakings, [] (auto) {}, gameLobbies, pool);
  auto matchmakingItr = std::prev (matchmakings.end ());
  matchmaking = { ioContext, matchmakings,
                  [&messages, matchmakingItr] (std::string msg) {
                    messages.push_back (msg);
                    if (msg == "ProxyStarted|{}")
                      {
                        matchmakingItr->process_event (objectToStringWithObjectName (LeaveGame{}));
                      }
                  },
                  gameLobbies, pool };
  matchmaking.process_event (objectToStringWithObjectName (CreateAccount{ playerName, "abc" }));
  ioContext.run_for (std::chrono::seconds{ 1 });
  ioContext.stop ();
  ioContext.reset ();
  matchmaking.process_event (objectToStringWithObjectName (createGameLobby));
  return matchmaking;
}

Matchmaking &
createAccountJoinGameLobby (std::string const &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<Matchmaking> &matchmakings, boost::asio::thread_pool &pool, JoinGameLobby const &joinGameLobby)
{
  auto &matchmaking = matchmakings.emplace_back (
      ioContext, matchmakings, [] (auto) {}, gameLobbies, pool);
  auto matchmakingItr = std::prev (matchmakings.end ());
  matchmaking = { ioContext, matchmakings,
                  [&messages, matchmakingItr] (std::string msg) {
                    messages.push_back (msg);
                    if (msg == "ProxyStarted|{}")
                      {
                        matchmakingItr->process_event (objectToStringWithObjectName (LeaveGame{}));
                      }
                  },
                  gameLobbies, pool };
  matchmaking.process_event (objectToStringWithObjectName (CreateAccount{ playerName, "abc" }));
  ioContext.run_for (std::chrono::seconds{ 1 });
  ioContext.stop ();
  ioContext.reset ();
  matchmaking.process_event (objectToStringWithObjectName (joinGameLobby));
  return matchmaking;
}

TEST_CASE ("2 player join coustom game", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  auto mockserver = Mockserver{ { ip::tcp::v4 (), 44444 }, { .requestResponse = { { R"foo(StartGame|{"players":["player1","player2"],"gameOption":{"someBool":false,"someString":""},"ratedGame":false})foo", "StartGameSuccess|{}" }, { "LeaveGame|{}", "LeaveGameSuccess|{}" } } } };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<Matchmaking> matchmakings{};
  auto messagesPlayer1 = std::vector<std::string>{};
  auto &matchmakingPlayer1 = createAccountCreateGameLobby ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, CreateAccount{ "name", "" });
  CHECK (messagesPlayer1.size () == 3);
  CHECK (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player1" + std::string{ "\"}" } == messagesPlayer1.at (0));
  CHECK (R"foo(JoinGameLobbySuccess|{})foo" == messagesPlayer1.at (1));
  CHECK (R"foo(UsersInGameLobby|{"name":"name","users":[{"UserInGameLobby":{"accountName":"player1"}}],"maxUserSize":2,"durakGameOption":{"someBool":false,"someString":""}})foo" == messagesPlayer1.at (2));
  auto messagesPlayer2 = std::vector<std::string>{};
  auto &matchmakingPlayer2 = createAccountJoinGameLobby ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinGameLobby{ "name", "" });
  CHECK (messagesPlayer2.size () == 3);
  CHECK (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player2" + std::string{ "\"}" } == messagesPlayer2.at (0));
  CHECK (R"foo(JoinGameLobbySuccess|{})foo" == messagesPlayer2.at (1));
  CHECK (R"foo(UsersInGameLobby|{"name":"name","users":[{"UserInGameLobby":{"accountName":"player1"}},{"UserInGameLobby":{"accountName":"player2"}}],"maxUserSize":2,"durakGameOption":{"someBool":false,"someString":""}})foo" == messagesPlayer2.at (2));
  CHECK (gameLobbies.size () == 1);
  messagesPlayer1.clear ();
  messagesPlayer2.clear ();
  SECTION ("both player accept invite", "[matchmaking]")
  {

    matchmakingPlayer1.process_event (objectToStringWithObjectName (CreateGame{}));
    matchmakingPlayer2.process_event (objectToStringWithObjectName (WantsToJoinGame{ true }));
    ioContext.run_for (std::chrono::seconds{ 1 });
    CHECK (messagesPlayer1.size () == 3);
    CHECK ("ProxyStarted|{}" == messagesPlayer1.at (0));
    CHECK ("LeaveGameSuccess|{}" == messagesPlayer1.at (1));
    CHECK ("ProxyStopped|{}" == messagesPlayer1.at (2));
    CHECK (messagesPlayer2.size () == 4);
    CHECK ("AskIfUserWantsToJoinGame|{}" == messagesPlayer2.at (0));
    CHECK ("ProxyStarted|{}" == messagesPlayer2.at (1));
    CHECK ("LeaveGameSuccess|{}" == messagesPlayer2.at (2));
    CHECK ("ProxyStopped|{}" == messagesPlayer2.at (3));
  }
  ioContext.stop ();
  ioContext.reset ();
}