#include "../../src/logic/client.hxx"
#include "src/serialization.hxx"
#include <boost/asio/co_spawn.hpp>
#include <catch2/catch.hpp>
#include <memory>

boost::asio::awaitable<void>
callAnApiFunction (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, std::list<Game> &games)
{
  REQUIRE_NOTHROW (co_await handleMessage (msg, io_context, pool, users, user, gameLobbies, games));
}

TEST_CASE ("handleMessage not logged in", "[every api function]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  io_context io_context{};
  thread_pool pool{ 2 };
  auto users = std::list<std::shared_ptr<User>>{};
  auto user = std::make_shared<User> ();
  auto gameLobbies = std::list<GameLobby>{};
  auto games = std::list<Game>{};
  boost::hana::for_each (shared_class::sharedClasses, [&] (const auto &x) {
    SECTION (confu_json::type_name<typename std::decay<decltype (x)>::type> (), "[every api function]")
    {
      co_spawn (
          io_context, [&, msg = objectToStringWithObjectName (x)] { return callAnApiFunction (msg, io_context, pool, users, user, gameLobbies, games); }, detached);
      io_context.run ();
    }
  });
  database::createEmptyDatabase ();
  database::createTables ();
}

boost::asio::awaitable<void>
logInCallAnApiFunction (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, std::list<Game> &games)
{
  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::CreateAccount{ "name", "password" }), io_context, pool, users, user, gameLobbies, games));
  REQUIRE_NOTHROW (co_await handleMessage (msg, io_context, pool, users, user, gameLobbies, games));
}

TEST_CASE ("handleMessage logged in", "[every api function]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  io_context io_context{};
  thread_pool pool{ 2 };
  auto users = std::list<std::shared_ptr<User>>{};
  auto user = std::make_shared<User> ();
  auto gameLobbies = std::list<GameLobby>{};
  auto games = std::list<Game>{};
  boost::hana::for_each (shared_class::sharedClasses, [&] (const auto &x) {
    SECTION (confu_json::type_name<typename std::decay<decltype (x)>::type> (), "[api]")
    {
      auto msg = objectToStringWithObjectName (x);
      co_spawn (
          io_context, [&, msg] { return logInCallAnApiFunction (msg, io_context, pool, users, user, gameLobbies, games); }, detached);
      io_context.run ();
    }
  });
  database::createEmptyDatabase ();
  database::createTables ();
}

boost::asio::awaitable<void>
logInQueueForQuickGameAndCallAnApiFunction (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, std::list<Game> &games)
{
  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::CreateAccount{ "name", "password" }), io_context, pool, users, user, gameLobbies, games));
  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::JoinMatchMakingQueue{ false }), io_context, pool, users, user, gameLobbies, games));
  REQUIRE_NOTHROW (co_await handleMessage (msg, io_context, pool, users, user, gameLobbies, games));
}

TEST_CASE ("handleMessage logged in and in queue for quick game", "[every api function]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  io_context io_context{};
  thread_pool pool{ 2 };
  auto users = std::list<std::shared_ptr<User>>{};
  auto user = std::make_shared<User> ();
  auto gameLobbies = std::list<GameLobby>{};
  auto games = std::list<Game>{};
  boost::hana::for_each (shared_class::sharedClasses, [&] (const auto &x) {
    SECTION (confu_json::type_name<typename std::decay<decltype (x)>::type> (), "[api]")
    {
      co_spawn (
          io_context, [&, msg = objectToStringWithObjectName (x)] { return logInQueueForQuickGameAndCallAnApiFunction (msg, io_context, pool, users, user, gameLobbies, games); }, detached);
      io_context.run ();
    }
  });
  database::createEmptyDatabase ();
  database::createTables ();
}

boost::asio::awaitable<void>
logInAndCreateGameLobbyAndCallAnApiFunction (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, std::list<Game> &games)
{

  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::CreateAccount{ "name", "password" }), io_context, pool, users, user, gameLobbies, games));
  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::CreateGameLobby{ "name", "" }), io_context, pool, users, user, gameLobbies, games));
  REQUIRE_NOTHROW (co_await handleMessage (msg, io_context, pool, users, user, gameLobbies, games));
}

TEST_CASE ("handleMessage logged in and create game lobby", "[every api function]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  io_context io_context{};
  thread_pool pool{ 2 };
  auto users = std::list<std::shared_ptr<User>>{};
  auto user = std::make_shared<User> ();
  auto gameLobbies = std::list<GameLobby>{};
  auto games = std::list<Game>{};
  boost::hana::for_each (shared_class::sharedClasses, [&] (const auto &x) {
    SECTION (confu_json::type_name<typename std::decay<decltype (x)>::type> (), "[api]")
    {
      co_spawn (
          io_context, [&, msg = objectToStringWithObjectName (shared_class::CreateGameLobby{})] { return logInAndCreateGameLobbyAndCallAnApiFunction (msg, io_context, pool, users, user, gameLobbies, games); }, detached);
      io_context.run ();
    }
  });
  database::createEmptyDatabase ();
  database::createTables ();
}
