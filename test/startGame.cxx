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
  std::list<std::weak_ptr<Matchmaking>> matchmakings{};
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
  std::list<std::weak_ptr<Matchmaking>> matchmakings{};
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
  matchmakingGame.shutDownUsingMockServerIoContext ();
  userGameViaMatchmaking.shutDownUsingMockServerIoContext ();
}

TEST_CASE ("2 player join quick game queue ranked", "[matchmaking]")
{
  auto const pathToMatchmakingDatabase = std::filesystem::path{ PATH_TO_BINARY + std::string{ "/matchmaking_proxy.db" } };
  database::createEmptyDatabase (pathToMatchmakingDatabase.string ());
  database::createTables (pathToMatchmakingDatabase.string ());
  using namespace boost::asio;
  auto ioContext = io_context ();
  auto matchmakingGame = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "MOCK_matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "MOCK_userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<std::weak_ptr<Matchmaking>> matchmakings{};
  auto matchmaking = std::shared_ptr<Matchmaking>{};
  auto messageReceived = false;
  auto messageReceived2 = false;
  auto matchmaking2 = std::shared_ptr<Matchmaking>{};
  SECTION ("both player accept invite", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{ true })));
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
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 1000 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{ true })));
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
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("UserStatistics", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{ true })));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (GetUserStatistics{})));
        }
      else if (boost::starts_with (msg, "UserStatistics"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 1000 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking);
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
    auto player2Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{ true })));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|{}"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
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

TEST_CASE ("2 player join custom game", "[matchmaking]")
{
  auto const pathToMatchmakingDatabase = std::filesystem::path{ PATH_TO_BINARY + std::string{ "/matchmaking_proxy.db" } };
  database::createEmptyDatabase (pathToMatchmakingDatabase.string ());
  database::createTables (pathToMatchmakingDatabase.string ());
  using namespace boost::asio;
  auto ioContext = io_context ();
  auto matchmakingGame = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "MOCK_matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "MOCK_userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<std::weak_ptr<Matchmaking>> matchmakings{};
  auto matchmaking = std::shared_ptr<Matchmaking>{};
  auto messageReceived = false;
  auto messageReceived2 = false;
  auto matchmaking2 = std::shared_ptr<Matchmaking>{};
  SECTION ("both player accept invite", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "ProxyStarted|{}"))
        {
          messageReceived = true;
          matchmaking->disconnectFromProxy ();
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking);
    auto player2Logic = [&] (std::string const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "UsersInGameLobby|"))
        {
          if (std::ranges::contains_subrange (msg, std::string{ "player1" }))
            {
              REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateGame{})));
            }
          else
            {
              REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
            }
        }
      else if (boost::starts_with (msg, "ProxyStarted|{}"))
        {
          messageReceived2 = true;
          matchmaking2->disconnectFromProxy ();
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking2);
    REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
    ioContext.run ();
    CHECK (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
  }
  SECTION ("UserStatistics", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "ProxyStarted|{}"))
        {
          messageReceived = true;
          matchmaking->disconnectFromProxy ();
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking);
    auto player2Logic = [&] (std::string const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "UsersInGameLobby|"))
        {
          if (std::ranges::contains_subrange (msg, std::string{ "player1" }))
            {
              REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateGame{})));
            }
          else
            {
              REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
            }
        }
      else if (boost::starts_with (msg, "ProxyStarted|{}"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (GetUserStatistics{})));
        }
      else if (boost::starts_with (msg, "UserStatistics|"))
        {
          messageReceived2 = true;
          matchmaking2->disconnectFromProxy ();
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
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

TEST_CASE ("3 player join quick game queue not ranked", "[matchmaking]")
{
  auto const pathToMatchmakingDatabase = std::filesystem::path{ PATH_TO_BINARY + std::string{ "/matchmaking_proxy.db" } };
  database::createEmptyDatabase (pathToMatchmakingDatabase.string ());
  database::createTables (pathToMatchmakingDatabase.string ());
  using namespace boost::asio;
  auto ioContext = io_context ();
  auto matchmakingGame = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 44444 }, { .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "MOCK_matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), 33333 }, { .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "MOCK_userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  boost::asio::thread_pool pool{};
  std::list<GameLobby> gameLobbies{};
  std::list<std::weak_ptr<Matchmaking>> matchmakings{};
  auto matchmaking = std::shared_ptr<Matchmaking>{};
  auto messageReceived = false;
  auto messageReceived2 = false;
  auto matchmaking2 = std::shared_ptr<Matchmaking>{};
  auto messageReceived3 = false;
  auto matchmaking3 = std::shared_ptr<Matchmaking>{};
  SECTION ("player 3 creates game player 1 and player 2 accept invite", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "ProxyStarted|{}"))
        {
          messageReceived = true;
          matchmaking->disconnectFromProxy ();
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking);
    auto player2Logic = [&] (std::string const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "ProxyStarted|{}"))
        {
          messageReceived2 = true;
          matchmaking2->disconnectFromProxy ();
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking2);
    auto player3Logic = [&, createGameAllreadyCalled = false] (std::string const &msg) mutable {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::CreateGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "JoinGameLobbySuccess|{}"))
        {
          REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobby{ 3 })));
        }
      else if (boost::starts_with (msg, "MaxUserSizeInCreateGameLobby|"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
        }
      else if (boost::starts_with (msg, "UsersInGameLobby") and not createGameAllreadyCalled and std::ranges::contains_subrange (msg, std::string{ "player1" }) and std::ranges::contains_subrange (msg, std::string{ "player3" }))
        {
          createGameAllreadyCalled = true;
          REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::CreateGame{})));
        }
      else if (boost::starts_with (msg, "ProxyStarted|{}"))
        {
          messageReceived3 = true;
          matchmaking3->disconnectFromProxy ();
        }
    };
    matchmaking3 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player3Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking3);
    REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player3", "abc" })));
    ioContext.run ();
    CHECK (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
    CHECK (messageReceived3);
  }
  SECTION ("UserStatistics", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "ProxyStarted|{}"))
        {
          messageReceived = true;
          matchmaking->disconnectFromProxy ();
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking);
    auto player2Logic = [&] (std::string const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "ProxyStarted|{}"))
        {
          messageReceived2 = true;
          matchmaking2->disconnectFromProxy ();
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking2);
    auto player3Logic = [&, createGameAllreadyCalled = false] (std::string const &msg) mutable {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::CreateGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "JoinGameLobbySuccess|{}"))
        {
          REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobby{ 3 })));
        }
      else if (boost::starts_with (msg, "MaxUserSizeInCreateGameLobby|"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
        }
      else if (boost::starts_with (msg, "UsersInGameLobby") and not createGameAllreadyCalled and std::ranges::contains_subrange (msg, std::string{ "player1" }) and std::ranges::contains_subrange (msg, std::string{ "player2" }))
        {
          createGameAllreadyCalled = true;
          REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::CreateGame{})));
        }
      else if (boost::starts_with (msg, "ProxyStarted|{}"))
        {
          REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::GetUserStatistics{})));
        }
      else if (boost::starts_with (msg, "UserStatistics|"))
        {
          messageReceived3 = true;
          matchmaking3->disconnectFromProxy ();
        }
    };
    matchmaking3 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player3Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking3);
    REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player3", "abc" })));
    ioContext.run ();
    CHECK (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
    CHECK (messageReceived3);
  }
  SECTION ("player2 declines. player1 and player3 gets asked to join and accept", "[matchmaking]")
  {
    auto player1Logic = [&] (auto const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::JoinGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true })));
        }
      else if (boost::starts_with (msg, "GameStartCanceled|"))
        {
          messageReceived = true;
        }
    };
    matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player1Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking);
    auto player2Logic = [&] (std::string const &msg) {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::JoinGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame|"))
        {
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ false })));
        }
      else if (boost::starts_with (msg, "GameStartCanceled|"))
        {
          messageReceived2 = true;
        }
    };
    matchmaking2 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player2Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 33333 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking2);
    auto player3Logic = [&, createGameAllreadyCalled = false] (std::string const &msg) mutable {
      if (boost::starts_with (msg, "LoginAccountSuccess"))
        {
          REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::CreateGameLobby{ "gameLobby", "abc" })));
        }
      else if (boost::starts_with (msg, "JoinGameLobbySuccess|{}"))
        {
          REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobby{ 3 })));
        }
      else if (boost::starts_with (msg, "MaxUserSizeInCreateGameLobby|"))
        {
          REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player1", "abc" })));
          REQUIRE (matchmaking2->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player2", "abc" })));
        }
      else if (boost::starts_with (msg, "UsersInGameLobby") and not createGameAllreadyCalled and std::ranges::contains_subrange (msg, std::string{ "player1" }) and std::ranges::contains_subrange (msg, std::string{ "player2" }))
        {
          createGameAllreadyCalled = true;
          REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::CreateGame{})));
        }
      else if (boost::starts_with (msg, "GameStartCanceled|"))
        {
          messageReceived3 = true;
        }
    };
    matchmaking3 = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, player3Logic, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 3000 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, pathToMatchmakingDatabase.string () });
    matchmakings.push_back (matchmaking3);
    REQUIRE (matchmaking3->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ "player3", "abc" })));
    ioContext.run ();
    CHECK_FALSE (gameLobbies.empty ());
    CHECK (messageReceived);
    CHECK (messageReceived2);
    CHECK (messageReceived3);
  }
  matchmakingGame.shutDownUsingMockServerIoContext ();
  userGameViaMatchmaking.shutDownUsingMockServerIoContext ();
}