#include "../matchmaking_proxy/logic/matchmaking.hxx"
#include "../matchmaking_proxy/userMatchmakingSerialization.hxx" // for Cre...
#include "matchmaking_proxy/database/database.hxx"               // for cre...
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/server/user.hxx" // for User
#include "matchmaking_proxy/util.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/sml.hpp>
#include <catch2/catch.hpp> // for Ass...
#include <memory>           // for sha...
using namespace user_matchmaking;

TEST_CASE ("matchmaking NotLoggedin -> Loggedin", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::sml;
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<GameLobby> gameLobbies_{};
  std::list<Matchmaking> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (
      ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_);
  SECTION ("CreateAccount", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" }));
    ioContext.run ();
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (0));
  }
  SECTION ("LoginAccount", "[matchmaking]")
  {
    database::createAccount ("oldAcc", "$argon2id$v=19$m=8,t=1,p=1$+Z8rjMS3CYbgMdG+JRgc6A$IAmEYrfE66+wsRmzeyPkyZ+xUJn+ybnx0HzKykO9NeY");
    matchmaking.process_event (objectToStringWithObjectName (LoginAccount{ "oldAcc", "abc" }));
    ioContext.run ();
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"oldAcc"})foo" == messages.at (0));
  }
  SECTION ("LoginAsGuest", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (LoginAsGuest{}));
    ioContext.run ();
    CHECK (boost::starts_with (messages.at (0), "LoginAsGuestSuccess"));
  }
  ioContext.stop ();
  ioContext.reset ();
}

TEST_CASE ("matchmaking NotLoggedin -> NotLoggedin", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::sml;
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<GameLobby> gameLobbies_{};
  std::list<Matchmaking> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (
      ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_);
  SECTION ("CreateAccountCancel", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" }));
    matchmaking.process_event (objectToStringWithObjectName (CreateAccountCancel{}));
    ioContext.run ();
    CHECK (messages.at (0) == "CreateAccountCancel|{}");
  }
  SECTION ("LoginAccountCancel", "[matchmaking]")
  {
    database::createAccount ("oldAcc", "$argon2id$v=19$m=8,t=1,p=1$+Z8rjMS3CYbgMdG+JRgc6A$IAmEYrfE66+wsRmzeyPkyZ+xUJn+ybnx0HzKykO9NeY");
    matchmaking.process_event (objectToStringWithObjectName (LoginAccount{ "oldAcc", "abc" }));
    matchmaking.process_event (objectToStringWithObjectName (LoginAccountCancel{}));
    ioContext.run ();
    CHECK (messages.at (0) == "LoginAccountCancel|{}");
  }
  ioContext.stop ();
  ioContext.reset ();
}

TEST_CASE ("matchmaking Loggedin -> Loggedin", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::sml;
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<GameLobby> gameLobbies_{};
  std::list<Matchmaking> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (
      ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_);
  matchmaking.process_event (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" }));
  ioContext.run ();
  ioContext.stop ();
  ioContext.reset ();
  CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (0));
  messages.clear ();
  SECTION ("CreateAccount", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" }));
    ioContext.run ();
    CHECK (messages.size () == 2);
    CHECK (R"foo(LogoutAccountSuccess|{})foo" == messages.at (0));
    CHECK (R"foo(CreateAccountError|{"accountName":"newAcc","error":"Account already Created"})foo" == messages.at (1));
  }
  SECTION ("LoginAccount", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (LoginAccount{ "newAcc", "abc" }));
    ioContext.run ();
    CHECK (messages.size () == 2);
    CHECK (R"foo(LogoutAccountSuccess|{})foo" == messages.at (0));
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (1));
  }
  SECTION ("JoinChannel", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (JoinChannel{ "my channel" }));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(JoinChannelSuccess|{"channel":"my channel"})foo" == messages.at (0));
  }
  SECTION ("BroadCastMessage", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (JoinChannel{ "my channel" }));
    matchmaking.process_event (objectToStringWithObjectName (BroadCastMessage{ "my channel", "Hello World!" }));
    ioContext.run ();
    CHECK (messages.size () == 2);
    CHECK (R"foo(JoinChannelSuccess|{"channel":"my channel"})foo" == messages.at (0));
    CHECK (R"foo(Message|{"fromAccount":"newAcc","channel":"my channel","message":"Hello World!"})foo" == messages.at (1));
  }
  SECTION ("LeaveChannel", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (LeaveChannel{ "my channel" }));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(LeaveChannelError|{"channel":"my channel","error":"channel not found"})foo" == messages.at (0));
  }
  SECTION ("CreateGameLobby", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (CreateGameLobby{ "my channel", "" }));
    ioContext.run ();
    CHECK (messages.size () == 2);
    CHECK (R"foo(JoinGameLobbySuccess|{})foo" == messages.at (0));
    CHECK (R"foo(JoinGameLobbySuccess|{})foo" == messages.at (0));
  }
  SECTION ("JoinGameLobby", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (JoinGameLobby{ "my channel", "" }));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(JoinGameLobbyError|{"name":"my channel","error":"wrong password name combination or lobby does not exists"})foo" == messages.at (0));
  }
  SECTION ("SetMaxUserSizeInCreateGameLobby", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (SetMaxUserSizeInCreateGameLobby{ 42 }));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(SetMaxUserSizeInCreateGameLobbyError|{"error":"could not find a game lobby for account"})foo" == messages.at (0));
  }
  SECTION ("GameOption", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (user_matchmaking::GameOption{ true, "some string" }));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(GameOptionError|{"error":"could not find a game lobby for account"})foo" == messages.at (0));
  }
  SECTION ("LeaveGameLobby", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (LeaveGameLobby{}));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(LeaveGameLobbyError|{"error":"not allowed to leave a game lobby which is controlled by the matchmaking system with leave game lobby"})foo" == messages.at (0));
  }
  SECTION ("RelogTo", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (RelogTo{}));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(UnhandledEventError|{"error":"event not handled: 'RelogTo'"})foo" == messages.at (0));
  }
  SECTION ("CreateGame", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (CreateGame{}));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(CreateGameError|{"error":"Could not find a game lobby for the user"})foo" == messages.at (0));
  }
  SECTION ("WantsToJoinGame", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (WantsToJoinGame{}));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(WantsToJoinGameError|{"error":"No game to join"})foo" == messages.at (0));
  }
  SECTION ("LeaveQuickGameQueue", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (LeaveQuickGameQueue{}));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(LeaveQuickGameQueueError|{"error":"User is not in queue"})foo" == messages.at (0));
  }
  SECTION ("JoinMatchMakingQueue", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (JoinMatchMakingQueue{}));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messages.at (0));
  }
  ioContext.stop ();
  ioContext.reset ();
}

TEST_CASE ("matchmaking Loggedin -> NotLoggedin", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::sml;
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<GameLobby> gameLobbies_{};
  std::list<Matchmaking> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (
      ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_);
  matchmaking.process_event (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" }));
  ioContext.run ();
  ioContext.stop ();
  ioContext.reset ();
  CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (0));
  messages.clear ();
  SECTION ("LogoutAccount", "[matchmaking]")
  {
    matchmaking.process_event (objectToStringWithObjectName (LogoutAccount{}));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(LogoutAccountSuccess|{})foo" == messages.at (0));
  }
  ioContext.stop ();
  ioContext.reset ();
}