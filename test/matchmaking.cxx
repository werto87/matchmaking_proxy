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
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};

  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  SECTION ("CreateAccount", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
    ioContext.run ();
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (0));
  }
  SECTION ("LoginAccount", "[matchmaking]")
  {
    database::createAccount ("oldAcc", "$argon2id$v=19$m=8,t=1,p=1$+Z8rjMS3CYbgMdG+JRgc6A$IAmEYrfE66+wsRmzeyPkyZ+xUJn+ybnx0HzKykO9NeY", "matchmaking_proxy.db");
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
  // ioContext.reset ();
}

TEST_CASE ("matchmaking NotLoggedIn -> NotLoggedIn", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  SECTION ("CreateAccountCancel", "[matchmaking]")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccountCancel{})));
    ioContext.run ();
    CHECK (messages.at (0) == "CreateAccountCancel|{}");
  }
  SECTION ("LoginAccountCancel", "[matchmaking]")
  {
    database::createAccount ("oldAcc", "$argon2id$v=19$m=8,t=1,p=1$+Z8rjMS3CYbgMdG+JRgc6A$IAmEYrfE66+wsRmzeyPkyZ+xUJn+ybnx0HzKykO9NeY", "matchmaking_proxy.db");
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LoginAccount{ "oldAcc", "abc" })));
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LoginAccountCancel{})));
    ioContext.run ();
    CHECK (messages.at (0) == "LoginAccountCancel|{}");
  }
  ioContext.stop ();
  // ioContext.reset ();
}

TEST_CASE ("matchmaking LoggedIn -> LoggedIn", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
  ioContext.run ();
  ioContext.stop ();
  // ioContext.reset ();
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
    CHECK (R"foo(LeaveGameLobbyError|{"error":"could not remove user from lobby user not found in lobby"})foo" == messages.at (0)); // cppcheck-suppress containerOutOfBounds //false positive
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
  // ioContext.reset ();
}

TEST_CASE ("matchmaking LoggedIn -> NotLoggedIn", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
  ioContext.run ();
  ioContext.stop ();
  // ioContext.reset ();
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
  // ioContext.reset ();
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
  auto matchmaking = Matchmaking{ MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" } };
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
  auto matchmaking = Matchmaking{ MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" } };
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
  auto matchmaking = Matchmaking{ MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" } };
  auto result = matchmaking.processEvent (objectToStringWithObjectName (user_matchmaking::JoinChannel{})); // cppcheck-suppress danglingTemporaryLifetime //false positive
  REQUIRE_FALSE (result.has_value ());
  REQUIRE (result.error () == "No transition found");
}

TEST_CASE ("matchmaking GetTopRatedPlayers", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  database::createAccount ("aa", "", "matchmaking_proxy.db", 0);
  database::createAccount ("bb", "", "matchmaking_proxy.db", 1);
  database::createAccount ("cc", "", "matchmaking_proxy.db", 3);
  database::createAccount ("dd", "", "matchmaking_proxy.db", 42);
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto matchmaking = Matchmaking{ MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" } };
  auto result = matchmaking.processEvent (objectToStringWithObjectName (user_matchmaking::GetTopRatedPlayers{ 2 })); // cppcheck-suppress danglingTemporaryLifetime //false positive
  REQUIRE (messages.at (0) == R"MyStringLiteral(TopRatedPlayers|{"players":[{"RatedPlayer":{"name":"dd","rating":42}},{"RatedPlayer":{"name":"cc","rating":3}}]})MyStringLiteral");
}

TEST_CASE ("matchmaking GetTopRatedPlayers no accounts", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto matchmaking = Matchmaking{ MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" } };
  auto result = matchmaking.processEvent (objectToStringWithObjectName (user_matchmaking::GetTopRatedPlayers{ 1 })); // cppcheck-suppress danglingTemporaryLifetime //false positive
  // TODO this is a bug in soci (works in msvc!) this will fail. the returned vector should be empty something like ""players":[]"
  REQUIRE (messages.at (0) == R"MyStringLiteral(TopRatedPlayers|{"players":[]})MyStringLiteral");
}

TEST_CASE ("matchmaking subscribe get top rated players", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::SubscribeGetTopRatedPlayers{ 5 })));
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
  ioContext.run ();
  CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (0));
  CHECK (R"foo(TopRatedPlayers|{"players":[{"RatedPlayer":{"name":"newAcc","rating":1500}}]})foo" == messages.at (1));
}

TEST_CASE ("matchmaking GetLoggedInPlayers no player logged in", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::GetLoggedInPlayers{ 5 })));
  CHECK (messages.at (0) == R"foo(LoggedInPlayers|{"players":[]})foo");
}

TEST_CASE ("matchmaking GetLoggedInPlayers 1 player logged in", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
  ioContext.run ();
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::GetLoggedInPlayers{ 5 })));
  CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == messages.at (0));
  CHECK (R"foo(LoggedInPlayers|{"players":["newAcc"]})foo" == messages.at (1));
}

TEST_CASE ("matchmaking subscribe logged in players", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto subscriberMessages = std::vector<std::string>{};
  auto &subscriber = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&subscriberMessages] (std::string message) { subscriberMessages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  REQUIRE (subscriber->processEvent (objectToStringWithObjectName (user_matchmaking::SubscribeGetLoggedInPlayers{ 5 })));
  auto playerMessages = std::vector<std::string>{};
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&playerMessages] (std::string message) { playerMessages.push_back (std::move (message)); }, gameLobbies, pool_, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  SECTION ("create acc")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
    ioContext.run ();
    ioContext.stop ();
    // ioContext.reset ();
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == playerMessages.at (0));
    CHECK (R"foo(LoggedInPlayers|{"players":["newAcc"]})foo" == subscriberMessages.at (0));
  }
  SECTION ("create acc log out")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
    ioContext.run ();
    ioContext.stop ();
    // ioContext.reset ();
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LogoutAccount{})));
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == playerMessages.at (0));
    CHECK (R"foo(LoggedInPlayers|{"players":["newAcc"]})foo" == subscriberMessages.at (0));
    CHECK (R"foo(LogoutAccountSuccess|{})foo" == playerMessages.at (1));
    CHECK (R"foo(LoggedInPlayers|{"players":[]})foo" == subscriberMessages.at (1));
  }
  SECTION ("create acc unsubscribe log out")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
    ioContext.run ();
    ioContext.stop ();
    // ioContext.reset ();
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::UnSubscribeGetLoggedInPlayers{})));
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (LogoutAccount{})));
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == playerMessages.at (0));
    CHECK (R"foo(LoggedInPlayers|{"players":["newAcc"]})foo" == subscriberMessages.at (0));
    CHECK (R"foo(LogoutAccountSuccess|{})foo" == playerMessages.at (1));
  }
  SECTION ("create acc terminate connection")
  {
    REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (CreateAccount{ "newAcc", "abc" })));
    ioContext.run ();
    ioContext.stop ();
    // ioContext.reset ();
    // simulate connection lost by running the clean up logic
    auto loggedInPlayerLostConnection = matchmaking->loggedInWithAccountName ().has_value ();
    matchmaking->cleanUp ();
    matchmakings.pop_back ();
    if (loggedInPlayerLostConnection)
      {
        for (auto &_matchmaking : matchmakings)
          {
            _matchmaking->proccessSendLoggedInPlayersToUser ();
          }
      }
    CHECK (R"foo(LoginAccountSuccess|{"accountName":"newAcc"})foo" == playerMessages.at (0));
    CHECK (R"foo(LoggedInPlayers|{"players":["newAcc"]})foo" == subscriberMessages.at (0));
    CHECK (R"foo(LoggedInPlayers|{"players":[]})foo" == subscriberMessages.at (1));
  }
}

TEST_CASE ("matchmaking custom message", "[matchmaking]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context ();
  boost::asio::thread_pool pool_{};
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};
  std::list<GameLobby> gameLobbies{};
  auto messages = std::vector<std::string>{};
  auto matchmakingOption = MatchmakingOption{};
  auto called = false;
  matchmakingOption.handleCustomMessageFromUser = [&called] (std::string const &,std::string const &, MatchmakingData &) {
    called = true;
  };
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string message) { messages.push_back (std::move (message)); }, gameLobbies, pool_, matchmakingOption, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address ("127.0.0.1"), 33333 }, "matchmaking_proxy.db" }));
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CustomMessage{})));
  REQUIRE (called);
}