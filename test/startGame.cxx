#include "matchmaking_proxy/database/database.hxx" // for cre...
#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "mockserver.hxx"
#include "util.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>
#include <catch2/catch.hpp> // for Ass...
#include <chrono>
#include <login_matchmaking_game_shared/matchmakingGameSerialization.hxx>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
using namespace user_matchmaking;

TEST_CASE ("playerOne joins queue and leaves", "[matchmaking]")
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
  auto matchmakingPlayer1 = createAccountAndJoinMatchmakingQueue ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ false });
  REQUIRE (messagesPlayer1.size () == 2);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player1" + std::string{ "\"}" } == messagesPlayer1.at (0));
  REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer1.at (1));
  messagesPlayer1.clear ();
  SECTION ("join queue and leave queue")
  {
    REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (LeaveQuickGameQueue{})));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);                              // cppcheck-suppress knownConditionTrueFalse //false positive
    REQUIRE ("LeaveQuickGameQueueSuccess|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.empty ());
  }
  SECTION ("join queue and disconnect")
  {
    matchmakingPlayer1->cleanUp ();
    matchmakingPlayer1.reset ();
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 0); // cppcheck-suppress knownConditionTrueFalse //false positive
    REQUIRE (gameLobbies.empty ());
  }
}

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
  auto matchmakingPlayer1 = createAccountAndJoinMatchmakingQueue ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ false });
  REQUIRE (messagesPlayer1.size () == 2);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player1" + std::string{ "\"}" } == messagesPlayer1.at (0));
  REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer1.at (1));
  auto messagesPlayer2 = std::vector<std::string>{};
  auto matchmakingPlayer2 = createAccountAndJoinMatchmakingQueue ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ false });
  REQUIRE (messagesPlayer2.size () == 3);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player2" + std::string{ "\"}" } == messagesPlayer2.at (0));
  REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer2.at (1));
  REQUIRE (R"foo(AskIfUserWantsToJoinGame|{})foo" == messagesPlayer2.at (2));
  REQUIRE (gameLobbies.size () == 1);
  messagesPlayer1.clear ();
  messagesPlayer2.clear ();
  SECTION ("both player accept invite", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);                // cppcheck-suppress knownConditionTrueFalse //false positive
    REQUIRE ("ProxyStarted|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.size () == 1);                // cppcheck-suppress knownConditionTrueFalse //false positive
    REQUIRE ("ProxyStarted|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.empty ());
  }
  SECTION ("playerOne accept playerTwo declined", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("GameStartCanceledRemovedFromQueue|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.size () == 1);
    REQUIRE (gameLobbies.front ().accountNames.front () == "player1");
  }
  SECTION ("playerTwo accept playerOne declined", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("GameStartCanceledRemovedFromQueue|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.size () == 1);
    REQUIRE (gameLobbies.front ().accountNames.front () == "player2");
  }
  SECTION ("playerOne accept playerTwo does not answer", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    ioContext.run_for (std::chrono::seconds{ 15 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("AskIfUserWantsToJoinGameTimeOut|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.size () == 1);
    REQUIRE (gameLobbies.front ().accountNames.front () == "player1");
  }
  SECTION ("playerTwo accept playerOne does not answer", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    ioContext.run_for (std::chrono::seconds{ 15 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("AskIfUserWantsToJoinGameTimeOut|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.size () == 1);
    REQUIRE (gameLobbies.front ().accountNames.front () == "player2");
  }
  SECTION ("playerTwo and playerOne does not answer", "[matchmaking]")
  {
    ioContext.run_for (std::chrono::seconds{ 15 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("AskIfUserWantsToJoinGameTimeOut|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("AskIfUserWantsToJoinGameTimeOut|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.empty ());
  }
  SECTION ("playerOne declines playerTwo still tries to join", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
    REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    ioContext.run_for (std::chrono::seconds{ 15 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("GameStartCanceledRemovedFromQueue|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.size () == 2);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer2.at (0));                                      // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (R"foo(WantsToJoinGameError|{"error":"No game to join"})foo" == messagesPlayer2.at (1)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.size () == 1);
    REQUIRE (gameLobbies.front ().accountNames.front () == "player2");
  }
  SECTION ("playerTwo declines playerOne still tries to join", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
    REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    ioContext.run_for (std::chrono::seconds{ 15 });
    REQUIRE (messagesPlayer1.size () == 2);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer1.at (0));                                      // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (R"foo(WantsToJoinGameError|{"error":"No game to join"})foo" == messagesPlayer1.at (1)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("GameStartCanceledRemovedFromQueue|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.size () == 1);
    REQUIRE (gameLobbies.front ().accountNames.front () == "player1");
  }
  SECTION ("playerOne accepts playerTwo disconnects", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    ioContext.run_for (std::chrono::milliseconds{ 10 });
    matchmakingPlayer2->cleanUp ();
    matchmakingPlayer2.reset ();
    ioContext.run_for (std::chrono::seconds{ 15 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.size () == 1);
    REQUIRE (gameLobbies.front ().accountNames.front () == "player1");
  }
  SECTION ("playerTwo accepts playerOne disconnects", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    ioContext.run_for (std::chrono::milliseconds{ 10 });
    matchmakingPlayer1->cleanUp ();
    matchmakingPlayer1.reset ();
    ioContext.run_for (std::chrono::seconds{ 15 });
    REQUIRE (messagesPlayer1.empty ());
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.size () == 1);
    REQUIRE (gameLobbies.front ().accountNames.front () == "player2");
  }
  SECTION ("playerTwo disconnects before playerOne can answer", "[matchmaking]")
  {
    matchmakingPlayer2->cleanUp ();
    matchmakingPlayer2.reset ();
    ioContext.run_for (std::chrono::seconds{ 15 });
    REQUIRE (messagesPlayer1.size () == 1);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.empty ());
    REQUIRE (gameLobbies.size () == 1);
    REQUIRE (gameLobbies.front ().accountNames.front () == "player1");
  }
  SECTION ("playerOne disconnects before playerTwo can answer", "[matchmaking]")
  {
    matchmakingPlayer1->cleanUp ();
    matchmakingPlayer1.reset ();
    ioContext.run_for (std::chrono::seconds{ 15 });
    REQUIRE (messagesPlayer1.empty ());
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.size () == 1);
    REQUIRE (gameLobbies.front ().accountNames.front () == "player2");
  }
  SECTION ("playerOne disconnects playerTow disconnects", "[matchmaking]")
  {
    matchmakingPlayer1->cleanUp ();
    matchmakingPlayer1.reset ();
    matchmakingPlayer2->cleanUp ();
    matchmakingPlayer2.reset ();
    ioContext.run_for (std::chrono::seconds{ 15 });
    REQUIRE (messagesPlayer1.empty ());
    REQUIRE (messagesPlayer2.size () == 1);
    REQUIRE ("GameStartCanceled|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (gameLobbies.empty ());
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
  auto matchmakingPlayer1 = createAccountAndJoinMatchmakingQueue ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ true });
  REQUIRE (messagesPlayer1.size () == 2);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player1" + std::string{ "\"}" } == messagesPlayer1.at (0));
  REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer1.at (1));
  auto messagesPlayer2 = std::vector<std::string>{};
  auto matchmakingPlayer2 = createAccountAndJoinMatchmakingQueue ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ true });
  REQUIRE (messagesPlayer1.size () == 3);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player2" + std::string{ "\"}" } == messagesPlayer2.at (0));
  REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer2.at (1));
  REQUIRE (R"foo(AskIfUserWantsToJoinGame|{})foo" == messagesPlayer2.at (2));
  REQUIRE (gameLobbies.size () == 1);
  messagesPlayer1.clear ();
  messagesPlayer2.clear ();
  SECTION ("both player accept invite", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);                // cppcheck-suppress knownConditionTrueFalse //false positive
    REQUIRE ("ProxyStarted|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.size () == 1);                // cppcheck-suppress knownConditionTrueFalse //false positive
    REQUIRE ("ProxyStarted|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
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
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ playerName, "abc" })));
  ioContext.run_for (std::chrono::seconds{ 5 });
  ioContext.stop ();
  ioContext.reset ();
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (createGameLobby)));
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
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ playerName, "abc" })));
  ioContext.run_for (std::chrono::seconds{ 5 });
  ioContext.stop ();
  ioContext.reset ();
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (joinGameLobby)));
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
  REQUIRE (R"foo(UsersInGameLobby|{"name":"name","users":[{"UserInGameLobby":{"accountName":"player1"}}],"maxUserSize":2,"gameOptionAsString":{"gameOptionAsString":""}})foo" == messagesPlayer1.at (2));
  auto messagesPlayer2 = std::vector<std::string>{};
  auto matchmakingPlayer2 = createAccountJoinGameLobby ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinGameLobby{ "name", "" }, proxyStartedCalled);
  REQUIRE (messagesPlayer2.size () == 3);
  REQUIRE (R"foo(LoginAccountSuccess|{"accountName":)foo" + std::string{ "\"" } + "player2" + std::string{ "\"}" } == messagesPlayer2.at (0));
  REQUIRE (R"foo(JoinGameLobbySuccess|{})foo" == messagesPlayer2.at (1));
  REQUIRE (R"foo(UsersInGameLobby|{"name":"name","users":[{"UserInGameLobby":{"accountName":"player1"}},{"UserInGameLobby":{"accountName":"player2"}}],"maxUserSize":2,"gameOptionAsString":{"gameOptionAsString":""}})foo" == messagesPlayer2.at (2));
  REQUIRE (gameLobbies.size () == 1);
  messagesPlayer1.clear ();
  messagesPlayer2.clear ();
  SECTION ("both player accept invite", "[matchmaking]")
  {
    REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (CreateGame{})));
    REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
    ioContext.run_for (std::chrono::seconds{ 5 });
    REQUIRE (messagesPlayer1.size () == 1);                            // cppcheck-suppress knownConditionTrueFalse //false positive
    REQUIRE ("ProxyStarted|{}" == messagesPlayer1.at (0));             // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE (messagesPlayer2.size () == 2);                            // cppcheck-suppress knownConditionTrueFalse //false positive
    REQUIRE ("AskIfUserWantsToJoinGame|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
    REQUIRE ("ProxyStarted|{}" == messagesPlayer2.at (1));             // cppcheck-suppress containerOutOfBounds //false positive
  }
  ioContext.stop ();
  ioContext.reset ();
}