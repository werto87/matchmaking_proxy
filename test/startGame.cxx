#include "../matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/database/database.hxx" // for cre...
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/util.hxx"
#include "test/mockserver.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>
#include <catch2/catch.hpp> // for Ass...
#include <chrono>
#include <login_matchmaking_game_shared_type/matchmakingGameSerialization.hxx>
#include <login_matchmaking_game_shared_type/userMatchmakingSerialization.hxx>
using namespace user_matchmaking;

TEST_CASE ("2 player join quick game queue not ranked", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  auto matchmakingGame = Mockserver{ { ip::tcp::v4 (), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "MOCK_matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = Mockserver{ { ip::tcp::v4 (), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "MOCK_userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  auto messagesPlayer1 = std::vector<std::string>{};
  auto proxyStartedCalled = 0;
  auto matchmakingPlayer1 = createAccountAndJoinMatchmakingGame ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ false }, proxyStartedCalled);
  REQUIRE (messagesPlayer1.size () == 2);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player1" + std::string{ "\"}" } == messagesPlayer1.at (0));
  REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer1.at (1));
  auto messagesPlayer2 = std::vector<std::string>{};
  auto matchmakingPlayer2 = createAccountAndJoinMatchmakingGame ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ false }, proxyStartedCalled);
  REQUIRE (messagesPlayer2.size () == 3);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player2" + std::string{ "\"}" } == messagesPlayer2.at (0));
  REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer2.at (1));
  REQUIRE (R"foo(AskIfUserWantsToJoinGame|{})foo" == messagesPlayer2.at (2));
  REQUIRE (gameLobbies.size () == 1);
  messagesPlayer1.clear ();
  messagesPlayer2.clear ();
  SECTION ("both player accept invite", "[matchmaking]")
  {
    matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true }));
    matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true }));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("ProxyStarted|{}" == messagesPlayer1.at (0));
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("ProxyStarted|{}" == messagesPlayer2.at (0));
  }
  SECTION ("one player accept one player declined", "[matchmaking]")
  {
    matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true }));
    matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false }));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer1.at (0));
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("GameStartCanceledRemovedFromQueue|{}" == messagesPlayer2.at (0));
  }
  SECTION ("one player declined one player accept", "[matchmaking]")
  {
    matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false }));
    matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true }));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("GameStartCanceledRemovedFromQueue|{}" == messagesPlayer1.at (0));
    REQUIRE (messagesPlayer2.size () == 2);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer2.at (0));
    REQUIRE (R"foo(WantsToJoinGameError|{"error":"No game to join"})foo" == messagesPlayer2.at (1));
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
  auto matchmakingGame = Mockserver{ { ip::tcp::v4 (), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "TEST_matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = Mockserver{ { ip::tcp::v4 (), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "TEST_userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  auto messagesPlayer1 = std::vector<std::string>{};
  auto proxyStartedCalled = 0;
  auto matchmakingPlayer1 = createAccountAndJoinMatchmakingGame ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ true }, proxyStartedCalled);
  REQUIRE (messagesPlayer1.size () == 2);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player1" + std::string{ "\"}" } == messagesPlayer1.at (0));
  REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer1.at (1));
  auto messagesPlayer2 = std::vector<std::string>{};
  auto matchmakingPlayer2 = createAccountAndJoinMatchmakingGame ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ true }, proxyStartedCalled);
  REQUIRE (messagesPlayer1.size () == 3);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player2" + std::string{ "\"}" } == messagesPlayer2.at (0));
  REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer2.at (1));
  REQUIRE (R"foo(AskIfUserWantsToJoinGame|{})foo" == messagesPlayer2.at (2));
  REQUIRE (gameLobbies.size () == 1);
  messagesPlayer1.clear ();
  messagesPlayer2.clear ();
  SECTION ("both player accept invite", "[matchmaking]")
  {
    matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true }));
    matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true }));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("ProxyStarted|{}" == messagesPlayer1.at (0));
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("ProxyStarted|{}" == messagesPlayer2.at (0));
  }
  ioContext.stop ();
  ioContext.reset ();
}

std::shared_ptr<Matchmaking>
createAccountCreateGameLobby (std::string const &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<std::shared_ptr<Matchmaking>> &matchmakings, boost::asio::thread_pool &pool, CreateGameLobby const &createGameLobby, int &proxyStartedCalled)
{
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [] (auto) {}, gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings,
                                                                [&messages, &ioContext, &proxyStartedCalled] (std::string msg) {
                                                                  messages.push_back (msg);
                                                                  if (msg == "ProxyStarted|{}")
                                                                    {
                                                                      proxyStartedCalled++;
                                                                      if (proxyStartedCalled == 2)
                                                                        {
                                                                          ioContext.stop ();
                                                                        }
                                                                    }
                                                                },
                                                                gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } });
  matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ playerName, "abc" }));
  ioContext.run_for (std::chrono::seconds{ 5 });
  ioContext.stop ();
  ioContext.reset ();
  matchmaking->processEvent (objectToStringWithObjectName (createGameLobby));
  return matchmaking;
}

std::shared_ptr<Matchmaking>
createAccountJoinGameLobby (std::string const &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<std::shared_ptr<Matchmaking>> &matchmakings, boost::asio::thread_pool &pool, JoinGameLobby const &joinGameLobby, int &proxyStartedCalled)
{
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [] (auto) {}, gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings,
                                                                [&messages, &ioContext, &proxyStartedCalled] (const std::string &msg) {
                                                                  messages.push_back (msg);
                                                                  if (msg == "ProxyStarted|{}")
                                                                    {
                                                                      proxyStartedCalled++;
                                                                      if (proxyStartedCalled == 2)
                                                                        {
                                                                          ioContext.stop ();
                                                                        }
                                                                    }
                                                                },
                                                                gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } });
  matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ playerName, "abc" }));
  ioContext.run_for (std::chrono::seconds{ 5 });
  ioContext.stop ();
  ioContext.reset ();
  matchmaking->processEvent (objectToStringWithObjectName (joinGameLobby));
  return matchmaking;
}

TEST_CASE ("2 player join custom game", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  auto matchmakingGame = Mockserver{ { ip::tcp::v4 (), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = Mockserver{ { ip::tcp::v4 (), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  auto messagesPlayer1 = std::vector<std::string>{};
  auto proxyStartedCalled = 0;
  auto matchmakingPlayer1 = createAccountCreateGameLobby ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, CreateAccount{ "name", "" }, proxyStartedCalled);
  REQUIRE (messagesPlayer1.size () == 3);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player1" + std::string{ "\"}" } == messagesPlayer1.at (0));
  REQUIRE (R"foo(JoinGameLobbySuccess|{})foo" == messagesPlayer1.at (1));
  REQUIRE (R"foo(UsersInGameLobby|{"name":"name","users":[{"UserInGameLobby":{"accountName":"player1"}}],"maxUserSize":2,"durakGameOption":{"gameOption":{"maxCardValue":9,"typeCount":4,"numberOfCardsPlayerShouldHave":6,"roundToStart":1,"trump":null,"customCardDeck":null,"cardsInHands":null},"timerOption":{"timerType":"noTimer","timeAtStartInSeconds":0,"timeForEachRoundInSeconds":0},"computerControlledPlayerCount":0,"opponentCards":"showNumberOfOpponentCards"}})foo" == messagesPlayer1.at (2));
  auto messagesPlayer2 = std::vector<std::string>{};
  auto matchmakingPlayer2 = createAccountJoinGameLobby ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinGameLobby{ "name", "" }, proxyStartedCalled);
  REQUIRE (messagesPlayer2.size () == 3);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player2" + std::string{ "\"}" } == messagesPlayer2.at (0));
  REQUIRE (R"foo(JoinGameLobbySuccess|{})foo" == messagesPlayer2.at (1));
  REQUIRE (R"foo(UsersInGameLobby|{"name":"name","users":[{"UserInGameLobby":{"accountName":"player1"}},{"UserInGameLobby":{"accountName":"player2"}}],"maxUserSize":2,"durakGameOption":{"gameOption":{"maxCardValue":9,"typeCount":4,"numberOfCardsPlayerShouldHave":6,"roundToStart":1,"trump":null,"customCardDeck":null,"cardsInHands":null},"timerOption":{"timerType":"noTimer","timeAtStartInSeconds":0,"timeForEachRoundInSeconds":0},"computerControlledPlayerCount":0,"opponentCards":"showNumberOfOpponentCards"}})foo" == messagesPlayer2.at (2));
  REQUIRE (gameLobbies.size () == 1);
  messagesPlayer1.clear ();
  messagesPlayer2.clear ();
  SECTION ("both player accept invite", "[matchmaking]")
  {
    matchmakingPlayer1->processEvent (objectToStringWithObjectName (CreateGame{}));
    matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true }));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("ProxyStarted|{}" == messagesPlayer1.at (0));
    REQUIRE (messagesPlayer2.size () == 2);
    REQUIRE ("AskIfUserWantsToJoinGame|{}" == messagesPlayer2.at (0));
    REQUIRE ("ProxyStarted|{}" == messagesPlayer2.at (1));
  }
  ioContext.stop ();
  ioContext.reset ();
}