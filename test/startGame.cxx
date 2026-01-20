#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/util.hxx"
#include "networkingUtil.hxx"
#include "test/util.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>
#include <catch2/catch.hpp>
#include <chrono>
#include <login_matchmaking_game_shared/matchmakingGameSerialization.hxx>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <my_web_socket/mockServer.hxx>

using namespace matchmaking_proxy;
using namespace user_matchmaking;
TEST_CASE ("playerOne joins queue and leaves", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  auto matchmakingGame = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "MOCK_matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "MOCK_userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  auto matchmaking = std::shared_ptr<Matchmaking>{};
  auto messageReceived = false;
  SECTION ("join queue and leave queue")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "JoinMatchMakingQueueSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LeaveQuickGameQueue{})));
        }
      else if (boost::starts_with (msg, "LeaveQuickGameQueueSuccess"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    ioContext.run ();
    REQUIRE (messageReceived);
  }
  SECTION ("join queue and disconnect")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "JoinMatchMakingQueueSuccess"))
        {
          messageReceived = true;
          matchmaking->cleanUp ();
          matchmaking.reset ();
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    ioContext.run ();
    REQUIRE (messageReceived);
  }
  matchmakingGame.shutDownUsingMockServerIoContext ();
  userGameViaMatchmaking.shutDownUsingMockServerIoContext ();
}

TEST_CASE ("2 player join quick game queue not ranked", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  auto matchmakingGame = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "MOCK_matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "MOCK_userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  auto matchmaking = std::shared_ptr<Matchmaking>{};
  auto messageReceived = false;
  auto messageReceived2 = false;
  auto matchmaking2 = std::shared_ptr<Matchmaking>{};
  SECTION ("both player accept invite", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "ProxyStarted"))
        {
          messageReceived = true;
          matchmaking->disconnectFromProxy ();
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 1000 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "ProxyStarted"))
        {
          messageReceived2 = true;
          matchmaking2->disconnectFromProxy ();
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("UserStatistics user in game", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "ProxyStarted"))
        {
          messageReceived = true;
          matchmaking->disconnectFromProxy ();
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "ProxyStarted"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (GetUserStatistics{})));
        }
      else if (boost::starts_with (msg, "UserStatistics"))
        {
          messageReceived2 = true;
          matchmaking2->disconnectFromProxy ();
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerOne accept playerTwo declined", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "GameStartCanceled|{}"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
        }
      else if (boost::starts_with (msg, "GameStartCanceledRemovedFromQueue|{}"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerTwo accept playerOne declined", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
        }
      else if (boost::starts_with (msg, "GameStartCanceledRemovedFromQueue|{}"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "GameStartCanceled|{}"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerOne accept playerTwo does not answer", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "GameStartCanceled|{}"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGameTimeOut"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerTwo accept playerOne does not answer", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGameTimeOut|{}"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "GameStartCanceled"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerTwo and playerOne do not answer", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGameTimeOut"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGameTimeOut"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerOne declines playerTwo still tries to join", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
        }
      else if (boost::starts_with (msg, "GameStartCanceledRemovedFromQueue"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "GameStartCanceled|{}"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
        }
      else if (boost::starts_with (msg, "WantsToJoinGameError"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerTwo declines playerOne still tries to join", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "GameStartCanceled|{}")) // has to be fully named or it get confused with GameStartCanceledRemovedFromQueue
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
        }
      else if (boost::starts_with (msg, "WantsToJoinGameError"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
        }
      else if (boost::starts_with (msg, "GameStartCanceledRemovedFromQueue"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerOne accepts playerTwo disconnects", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "GameStartCanceled"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          messageReceived2 = true;
          matchmaking2.reset ();
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerTwo accepts playerOne disconnects", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          messageReceived = true;
          matchmaking.reset ();
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "GameStartCanceled"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerTwo disconnects before playerOne can answer", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "GameStartCanceled"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
        {
          messageReceived2 = true;
          matchmaking2->cleanUp ();
          matchmaking2.reset ();
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerOne disconnects before playerTwo can answer", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
        {
          messageReceived = true;
          matchmaking->cleanUp ();
          matchmaking.reset ();
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "GameStartCanceled"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerOne disconnects playerTow disconnects", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
        {
          messageReceived = true;
          matchmaking->cleanUp ();
          matchmaking.reset ();
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
        {
          messageReceived2 = true;
          matchmaking2->cleanUp ();
          matchmaking2.reset ();
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("playerOne disconnects playerTow disconnects", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{})));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (GetUserStatistics{})));
        }
      else if (boost::starts_with (msg, "UserStatistics"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  matchmakingGame.shutDownUsingMockServerIoContext ();
  userGameViaMatchmaking.shutDownUsingMockServerIoContext ();
}

// TEST_CASE ("2 player join quick game queue ranked", "[matchmaking]")
// {
//   database::createEmptyDatabase ("matchmaking_proxy.db");
//   database::createTables ("matchmaking_proxy.db");
//   using namespace boost::asio;
//   auto ioContext = io_context ();
//   auto matchmakingGame = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "TEST_matchmaking_game", fmt::fg (fmt::color::violet), "0" };
//   auto userGameViaMatchmaking = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "TEST_userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
//   boost::asio::thread_pool pool{};
//   std::list<GameLobby> gameLobbies{};
//   std::list<std::shared_ptr<Matchmaking>> matchmakings{};
//   auto messagesPlayer1 = std::vector<std::string>{};
//   auto matchmakingPlayer1 = createAccountAndJoinMatchmakingQueue ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ true });
//   REQUIRE (messagesPlayer1.size () == 2);
//   REQUIRE (R"foo(LoginAccountSuccess|{"accountName":"player1"})foo" == messagesPlayer1.at (0));
//   REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer1.at (1));
//   auto messagesPlayer2 = std::vector<std::string>{};
//   auto matchmakingPlayer2 = createAccountAndJoinMatchmakingQueue ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ true });
//   REQUIRE (messagesPlayer1.size () == 3);
//   REQUIRE (R"foo(LoginAccountSuccess|{"accountName":"player2"})foo" == messagesPlayer2.at (0));
//   REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer2.at (1));
//   REQUIRE (R"foo(AskIfUserWantsToJoinGame|{})foo" == messagesPlayer2.at (2));
//   REQUIRE (gameLobbies.size () == 1);
//   messagesPlayer1.clear ();
//   messagesPlayer2.clear ();
//   SECTION ("both player accept invite", "[matchmaking]")
//   {
//     REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
//     REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
//     ioContext.run ();
//     REQUIRE (messagesPlayer1.size () == 1);                // cppcheck-suppress knownConditionTrueFalse //false positive
//     REQUIRE ("ProxyStarted|{}" == messagesPlayer1.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
//     REQUIRE (messagesPlayer2.size () == 1);                // cppcheck-suppress knownConditionTrueFalse //false positive
//     REQUIRE ("ProxyStarted|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
//   }
//   SECTION ("UserStatistics", "[matchmaking]")
//   {
//     REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (GetUserStatistics{})));
//     ioContext.run (); // short wait
//     REQUIRE (messagesPlayer1.size () == 1);
//     REQUIRE (messagesPlayer1.front () == R"foo(UserStatistics|{"userInCreateCustomGameLobby":0,"userInUnRankedQueue":0,"userInRankedQueue":2,"userInGame":0})foo");
//   }
//   ioContext.stop ();
//   // ioContext.reset ();
// }

// std::shared_ptr<Matchmaking>
// createAccountCreateGameLobby (std::string const &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<std::shared_ptr<Matchmaking>> &matchmakings, boost::asio::thread_pool &pool, CreateGameLobby const &createGameLobby, int &proxyStartedCalled)
// {
//   auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [] (auto) {}, gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
//   matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings,
//                                                                 [&messages, &ioContext, &proxyStartedCalled] (std::string msg) {
//                                                                   messages.push_back (msg);
//                                                                   if (msg == "ProxyStarted|{}")
//                                                                     {
//                                                                       proxyStartedCalled++;
//                                                                       if (proxyStartedCalled == 2)
//                                                                         {
//                                                                           ioContext.stop ();
//                                                                         }
//                                                                     }
//                                                                 },
//                                                                 gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
//   REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ playerName, "abc" })));
//   ioContext.run ();
//   ioContext.stop ();
//   // ioContext.reset ();
//   REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (createGameLobby)));
//   return matchmaking;
// }

// std::shared_ptr<Matchmaking>
// createAccountJoinGameLobby (std::string const &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<std::shared_ptr<Matchmaking>> &matchmakings, boost::asio::thread_pool &pool, JoinGameLobby const &joinGameLobby, int &proxyStartedCalled)
// {
//   auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [] (auto) {}, gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
//   matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings,
//                                                                 [&messages, &ioContext, &proxyStartedCalled] (const std::string &msg) {
//                                                                   messages.push_back (msg);
//                                                                   if (msg == "ProxyStarted|{}")
//                                                                     {
//                                                                       proxyStartedCalled++;
//                                                                       if (proxyStartedCalled == 2)
//                                                                         {
//                                                                           ioContext.stop ();
//                                                                         }
//                                                                     }
//                                                                 },
//                                                                 gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" });
//   REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ playerName, "abc" })));
//   ioContext.run ();
//   ioContext.stop ();
//   // ioContext.reset ();
//   REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (joinGameLobby)));
//   return matchmaking;
// }

// TEST_CASE ("2 player join custom game", "[matchmaking]")
// {
//   database::createEmptyDatabase ("matchmaking_proxy.db");
//   database::createTables ("matchmaking_proxy.db");
//   using namespace boost::asio;
//   auto ioContext = io_context ();
//   auto matchmakingGame = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "matchmaking_game", fmt::fg (fmt::color::violet), "0" };
//   auto userGameViaMatchmaking = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
//   boost::asio::thread_pool pool{};
//   std::list<GameLobby> gameLobbies{};
//   std::list<std::shared_ptr<Matchmaking>> matchmakings{};
//   auto messagesPlayer1 = std::vector<std::string>{};
//   auto proxyStartedCalled = 0;
//   auto matchmakingPlayer1 = createAccountCreateGameLobby ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, CreateAccount{ "name", "" }, proxyStartedCalled);
//   REQUIRE (messagesPlayer1.size () == 3);
//   REQUIRE (R"foo(LoginAccountSuccess|{"accountName":"player1"})foo" == messagesPlayer1.at (0));
//   REQUIRE (R"foo(JoinGameLobbySuccess|{})foo" == messagesPlayer1.at (1));
//   // REQUIRE (R"foo(UsersInGameLobby|{"name":"name","users":[{"UserInGameLobby":{"accountName":"player1"}}],"maxUserSize":2,"gameOptionAsString":{"gameOptionAsString":"{\"gameOption\":{\"maxCardValue\":9,\"typeCount\":4,\"numberOfCardsPlayerShouldHave\":6,\"roundToStart\":1,\"trump\":null,\"customCardDeck\":null,\"cardsInHands\":null},\"timerOption\":{\"timerType\":\"noTimer\",\"timeAtStartInSeconds\":0,\"timeForEachRoundInSeconds\":0},\"computerControlledPlayerCount\":0,\"opponentCards\":\"showNumberOfOpponentCards\"}"}})foo" == messagesPlayer1.at (2));
//   REQUIRE ("UsersInGameLobby|{\"name\":\"name\",\"users\":[{\"UserInGameLobby\":{\"accountName\":\"player1\"}}],\"maxUserSize\":2,\"gameOptionAsString\":{\"gameOptionAsString\":\"{\\\"gameOption\\\":{\\\"maxCardValue\\\":8,\\\"typeCount\\\":4,\\\"numberOfCardsPlayerShouldHave\\\":6,\\\"roundToStart\\\":1,\\\"trump\\\":null,\\\"customCardDeck\\\":null,\\\"cardsInHands\\\":null},\\\"timerOption\\\":{\\\"timerType\\\":\\\"noTimer\\\",\\\"timeAtStartInSeconds\\\":0,\\\"timeForEachRoundInSeconds\\\":0},\\\"computerControlledPlayerCount\\\":0,\\\"opponentCards\\\":\\\"showNumberOfOpponentCards\\\",\\\"create3CardsVs3CardsPuzzle\\\":false}\"}}" == messagesPlayer1.at (2));
//   auto messagesPlayer2 = std::vector<std::string>{};
//   auto matchmakingPlayer2 = createAccountJoinGameLobby ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinGameLobby{ "name", "" }, proxyStartedCalled);
//   REQUIRE (messagesPlayer2.size () == 3);
//   REQUIRE (R"foo(LoginAccountSuccess|{"accountName":"player2"})foo" == messagesPlayer2.at (0));
//   REQUIRE (R"foo(JoinGameLobbySuccess|{})foo" == messagesPlayer2.at (1));
//   REQUIRE ("UsersInGameLobby|{\"name\":\"name\",\"users\":[{\"UserInGameLobby\":{\"accountName\":\"player1\"}},{\"UserInGameLobby\":{\"accountName\":\"player2\"}}],\"maxUserSize\":2,\"gameOptionAsString\":{\"gameOptionAsString\":\"{\\\"gameOption\\\":{\\\"maxCardValue\\\":8,\\\"typeCount\\\":4,\\\"numberOfCardsPlayerShouldHave\\\":6,\\\"roundToStart\\\":1,\\\"trump\\\":null,\\\"customCardDeck\\\":null,\\\"cardsInHands\\\":null},\\\"timerOption\\\":{\\\"timerType\\\":\\\"noTimer\\\",\\\"timeAtStartInSeconds\\\":0,\\\"timeForEachRoundInSeconds\\\":0},\\\"computerControlledPlayerCount\\\":0,\\\"opponentCards\\\":\\\"showNumberOfOpponentCards\\\",\\\"create3CardsVs3CardsPuzzle\\\":false}\"}}" == messagesPlayer2.at (2));
//   REQUIRE (gameLobbies.size () == 1);
//   messagesPlayer1.clear ();
//   messagesPlayer2.clear ();
//   SECTION ("both player accept invite", "[matchmaking]")
//   {
//     REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (CreateGame{})));
//     REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
//     ioContext.run ();
//     REQUIRE (messagesPlayer1.size () == 1);                            // cppcheck-suppress knownConditionTrueFalse //false positive
//     REQUIRE ("ProxyStarted|{}" == messagesPlayer1.at (0));             // cppcheck-suppress containerOutOfBounds //false positive
//     REQUIRE (messagesPlayer2.size () == 2);                            // cppcheck-suppress knownConditionTrueFalse //false positive
//     REQUIRE ("AskIfUserWantsToJoinGame|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
//     REQUIRE ("ProxyStarted|{}" == messagesPlayer2.at (1));             // cppcheck-suppress containerOutOfBounds //false positive
//   }
//   SECTION ("UserStatistics", "[matchmaking]")
//   {
//     REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (GetUserStatistics{})));
//     ioContext.run (); // short wait
//     REQUIRE (messagesPlayer1.size () == 1);
//     REQUIRE (messagesPlayer1.front () == R"foo(UserStatistics|{"userInCreateCustomGameLobby":2,"userInUnRankedQueue":0,"userInRankedQueue":0,"userInGame":0})foo");
//   }
//   ioContext.stop ();
//   // ioContext.reset ();
// }

// TEST_CASE ("3 player join quick game queue not ranked", "[matchmaking]")
// {
//   database::createEmptyDatabase ("matchmaking_proxy.db");
//   database::createTables ("matchmaking_proxy.db");
//   using namespace boost::asio;
//   auto ioContext = io_context ();
//   auto matchmakingGame = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "MOCK_matchmaking_game", fmt::fg (fmt::color::violet), "0" };
//   auto userGameViaMatchmaking = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "MOCK_userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
//   boost::asio::thread_pool pool{};
//   std::list<GameLobby> gameLobbies{};
//   std::list<std::shared_ptr<Matchmaking>> matchmakings{};
//   auto messagesPlayer1 = std::vector<std::string>{};
//   auto matchmakingPlayer1 = createAccountAndJoinMatchmakingQueue ("player1", ioContext, messagesPlayer1, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ false });
//   REQUIRE (messagesPlayer1.size () == 2);
//   REQUIRE (R"foo(LoginAccountSuccess|{"accountName":"player1"})foo" == messagesPlayer1.at (0));
//   REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer1.at (1));
//   auto messagesPlayer2 = std::vector<std::string>{};
//   auto matchmakingPlayer2 = createAccountAndJoinMatchmakingQueue ("player2", ioContext, messagesPlayer2, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ false });
//   REQUIRE (messagesPlayer2.size () == 3);
//   REQUIRE (R"foo(LoginAccountSuccess|{"accountName":"player2"})foo" == messagesPlayer2.at (0));
//   REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer2.at (1));
//   REQUIRE (R"foo(AskIfUserWantsToJoinGame|{})foo" == messagesPlayer2.at (2));
//   auto messagesPlayer3 = std::vector<std::string>{};
//   auto matchmakingPlayer3 = createAccountAndJoinMatchmakingQueue ("player3", ioContext, messagesPlayer3, gameLobbies, matchmakings, pool, JoinMatchMakingQueue{ false });
//   REQUIRE (messagesPlayer3.size () == 2);
//   REQUIRE (R"foo(LoginAccountSuccess|{"accountName":"player3"})foo" == messagesPlayer3.at (0));
//   REQUIRE (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messagesPlayer3.at (1));
//   REQUIRE (gameLobbies.size () == 2);
//   messagesPlayer1.clear ();
//   messagesPlayer2.clear ();
//   messagesPlayer3.clear ();
//   SECTION ("UserStatistics", "[matchmaking]")
//   {
//     REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (GetUserStatistics{})));
//     ioContext.run (); // short wait
//     REQUIRE (messagesPlayer1.size () == 1);
//     REQUIRE (messagesPlayer1.front () == R"foo(UserStatistics|{"userInCreateCustomGameLobby":0,"userInUnRankedQueue":3,"userInRankedQueue":0,"userInGame":0})foo");
//   }
//   SECTION ("player2 declines. player1 and player3 gets asked to join and accept", "[matchmaking]")
//   {
//     REQUIRE (matchmakingPlayer2->processEvent (objectToStringWithObjectName (WantsToJoinGame{ false })));
//     REQUIRE (matchmakingPlayer1->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
//     REQUIRE (matchmakingPlayer3->processEvent (objectToStringWithObjectName (WantsToJoinGame{ true })));
//     ioContext.run ();
//     REQUIRE (messagesPlayer2.size () == 1);
//     REQUIRE ("GameStartCanceledRemovedFromQueue|{}" == messagesPlayer2.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
//     REQUIRE (messagesPlayer1.size () == 4);
//     REQUIRE ("GameStartCanceled|{}" == messagesPlayer1.at (0));           // cppcheck-suppress containerOutOfBounds //false positive
//     REQUIRE ("JoinMatchMakingQueueSuccess|{}" == messagesPlayer1.at (1)); // cppcheck-suppress containerOutOfBounds //false positive
//     REQUIRE ("AskIfUserWantsToJoinGame|{}" == messagesPlayer1.at (2));    // cppcheck-suppress containerOutOfBounds //false positive
//     REQUIRE ("ProxyStarted|{}" == messagesPlayer1.at (3));                // cppcheck-suppress containerOutOfBounds //false positive
//     REQUIRE (messagesPlayer3.size () == 2);
//     REQUIRE ("AskIfUserWantsToJoinGame|{}" == messagesPlayer3.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
//     REQUIRE ("ProxyStarted|{}" == messagesPlayer3.at (1));             // cppcheck-suppress containerOutOfBounds //false positive
//     REQUIRE (gameLobbies.empty ());
//   }
// ioContext.stop ();
// ioContext.reset ();
// }
