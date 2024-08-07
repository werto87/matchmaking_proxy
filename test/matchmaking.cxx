#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/util.hxx"
#include "util.hxx"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/thread_pool.hpp>
#include <catch2/catch.hpp>
#include <confu_json/to_json.hxx>
#include <modern_durak_game_option/userDefinedGameOption.hxx>
#include <sstream>
using namespace user_matchmaking;
using namespace matchmaking_proxy;
TEST_CASE ("matchmaking NotLoggedIn -> LoggedIn", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};

  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  SECTION ("CreateAccount", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
    ioContext.run ();
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (0));
  }
  SECTION ("LoginAccount", "[matchmaking]")
  {
    database::createAccount ("oldAcc", "$argon2id$v=19$m=8,t=1,p=1$+Z8rjMS3CYbgMdG+JRgc6A$IAmEYrfE66+wsRmzeyPkyZ+xUJn+ybnx0HzKykO9NeY");
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LoginAccount{ "oldAcc", "abc" })));
    ioContext.run ();
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"oldAcc"})foo" == messages.at (0));
  }
  SECTION ("LoginAsGuest", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LoginAsGuest{})));
    ioContext.run ();
    CHECK (boost::starts_with (messages.at (0), "LoginAsGuestSuccess"));
  }
  ioContext.stop ();
  ioContext.reset ();
}

TEST_CASE ("matchmaking NotLoggedIn -> NotLoggedIn", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  SECTION ("CreateAccountCancel", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccountCancel{})));
    ioContext.run ();
    CHECK (messages.at (0) == "CreateAccountCancel|{}");
  }
  SECTION ("LoginAccountCancel", "[matchmaking]")
  {
    database::createAccount ("oldAcc", "$argon2id$v=19$m=8,t=1,p=1$+Z8rjMS3CYbgMdG+JRgc6A$IAmEYrfE66+wsRmzeyPkyZ+xUJn+ybnx0HzKykO9NeY");
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LoginAccount{ "oldAcc", "abc" })));
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LoginAccountCancel{})));
    ioContext.run ();
    CHECK (messages.at (0) == "LoginAccountCancel|{}");
  }
  ioContext.stop ();
  ioContext.reset ();
}

TEST_CASE ("matchmaking LoggedIn -> LoggedIn", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
  ioContext.run ();
  ioContext.stop ();
  ioContext.reset ();
  CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (0));
  messages.clear ();
  SECTION ("CreateAccount", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
    ioContext.run ();
    CHECK (messages.size () == 2);                                                                                       // cppcheck-suppress knownConditionTrueFalse //false positive
    CHECK (R"foo(LogoutAccountSuccess|{})foo" == messages.at (0));                                                       // cppcheck-suppress containerOutOfBounds //false positive
    CHECK (R"foo(CreateAccountError|{"accountName":"newAcc","error":"Account already Created"})foo" == messages.at (1)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("LoginAccount", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LoginAccount{ "newAcc", "abc" })));
    ioContext.run ();
    CHECK (messages.size () == 2);
    CHECK (R"foo(LogoutAccountSuccess|{})foo" == messages.at (0));                      // cppcheck-suppress containerOutOfBounds //false positive
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (1)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("JoinChannel", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (JoinChannel{ "my channel" })));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(JoinChannelSuccess|{"channel":"my channel"})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("BroadCastMessage", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (JoinChannel{ "my channel" })));
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (BroadCastMessage{ "my channel", "Hello World!" })));
    ioContext.run ();
    CHECK (messages.size () == 2);
    CHECK (R"foo(JoinChannelSuccess|{"channel":"my channel"})foo" == messages.at (0));                                      // cppcheck-suppress containerOutOfBounds //false positive
    CHECK (R"foo(Message|{"fromAccount":"newAcc","channel":"my channel","message":"Hello World!"})foo" == messages.at (1)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("LeaveChannel", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LeaveChannel{ "my channel" })));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(LeaveChannelError|{"channel":"my channel","error":"channel not found"})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("CreateGameLobby", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateGameLobby{ "my channel", "" })));
    ioContext.run ();
    CHECK (messages.size () == 2);
    CHECK (R"foo(JoinGameLobbySuccess|{})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("JoinGameLobby", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (JoinGameLobby{ "my channel", "" })));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(JoinGameLobbyError|{"name":"my channel","error":"wrong password name combination or lobby does not exists"})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("SetMaxUserSizeInCreateGameLobby", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (SetMaxUserSizeInCreateGameLobby{ 42 })));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(SetMaxUserSizeInCreateGameLobbyError|{"error":"could not find a game lobby for account"})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("GameOption", "[matchmaking]")
  {
    auto ss = std::stringstream{};
    ss << confu_json::to_json (shared_class::GameOption{});
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking_game::GameOptionAsString{ ss.str () })));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(GameOptionError|{"error":"could not find a game lobby for account"})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("LeaveGameLobby", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LeaveGameLobby{})));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(LeaveGameLobbyError|{"error":"not allowed to leave a game lobby which is controlled by the matchmaking system with leave game lobby"})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("CreateGame", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateGame{})));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(CreateGameError|{"error":"Could not find a game lobby for the user"})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("WantsToJoinGame", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (WantsToJoinGame{})));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(WantsToJoinGameError|{"error":"No game to join"})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("LeaveQuickGameQueue", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LeaveQuickGameQueue{})));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(LeaveQuickGameQueueError|{"error":"User is not in queue"})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("JoinMatchMakingQueue", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (JoinMatchMakingQueue{})));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  ioContext.stop ();
  ioContext.reset ();
}

TEST_CASE ("matchmaking LoggedIn -> NotLoggedIn", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
  ioContext.run ();
  ioContext.stop ();
  ioContext.reset ();
  CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (0));
  messages.clear ();
  SECTION ("LogoutAccount", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LogoutAccount{})));
    ioContext.run ();
    CHECK (messages.size () == 1);                                 // cppcheck-suppress knownConditionTrueFalse //false positive
    CHECK (R"foo(LogoutAccountSuccess|{})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  SECTION ("LogoutAccount user in lobby", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LogoutAccount{})));
    ioContext.run ();
    CHECK (messages.size () == 1);                                 // cppcheck-suppress knownConditionTrueFalse //false positive
    CHECK (R"foo(LogoutAccountSuccess|{})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  ioContext.stop ();
  ioContext.reset ();
}

// TODO put this in one section not logged in global state
TEST_CASE ("matchmaking currentStatesAsString", "[matchmaking]")
{
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto matchmaking = Matchmaking{ MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } } };
  REQUIRE (matchmaking.currentStatesAsString ().size () == 2); // cppcheck-suppress danglingTemporaryLifetime //false positive
}

TEST_CASE ("matchmaking GetMatchmakingLogic", "[matchmaking]")
{
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto matchmaking = Matchmaking{ MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } } };
  REQUIRE (matchmaking.processEvent (objectToStringWithObjectName (user_matchmaking::GetMatchmakingLogic{}))); // cppcheck-suppress danglingTemporaryLifetime //false positive
  REQUIRE (not messages.empty ());
}

TEST_CASE ("matchmaking error handling proccessEvent no transition", "[matchmaking]")
{
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto matchmaking = Matchmaking{ MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } } };
  auto result = matchmaking.processEvent (objectToStringWithObjectName (user_matchmaking::JoinChannel{})); // cppcheck-suppress danglingTemporaryLifetime //false positive
  REQUIRE_FALSE (result.has_value ());
  REQUIRE (result.error () == "No transition found");
}
TEST_CASE ("matchmaking option errors matchmaking LoggedIn -> LoggedIn", "[matchmaking]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto matchmakingOption = MatchmakingOption{};
  matchmakingOption.usersNeededToStartQuickGame = 0;
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, matchmakingOption, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
  ioContext.run ();
  ioContext.stop ();
  ioContext.reset ();
  CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (0));
  messages.clear ();
  SECTION ("JoinMatchMakingQueue", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (JoinMatchMakingQueue{})));
    ioContext.run ();
    CHECK (messages.size () == 1);
    CHECK (R"foo(JoinMatchMakingQueueSuccess|{})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
  }
  ioContext.stop ();
  ioContext.reset ();
}