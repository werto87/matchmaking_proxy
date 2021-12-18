#include "src/logic/logic.hxx"
#include "src/serialization.hxx"
#include <boost/asio/co_spawn.hpp>
#include <catch2/catch.hpp>
#include <memory>

boost::asio::awaitable<void>
startA2PlayerQuickGame (boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User>> &users, std::list<GameLobby> &gameLobbies, std::list<Game> &games)
{
  auto user1 = users.front ();
  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::LoginAsGuest{}), io_context, pool, users, user1, gameLobbies, games));
  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::JoinMatchMakingQueue{}), io_context, pool, users, user1, gameLobbies, games));
  auto user2 = *std::next (users.begin (), 1);
  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::LoginAsGuest{}), io_context, pool, users, user2, gameLobbies, games));
  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::JoinMatchMakingQueue{}), io_context, pool, users, user2, gameLobbies, games));
  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::WantsToJoinGame{ true }), io_context, pool, users, user2, gameLobbies, games));
  REQUIRE_NOTHROW (co_await handleMessage (objectToStringWithObjectName (shared_class::WantsToJoinGame{ true }), io_context, pool, users, user1, gameLobbies, games));
}

TEST_CASE ("handleMessage start a 2 player quick game", "[use cases]")
{
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  io_context io_context{};
  thread_pool pool{ 2 };
  auto users = std::list<std::shared_ptr<User>>{ std::make_shared<User> (), std::make_shared<User> () };
  auto gameLobbies = std::list<GameLobby>{};
  auto games = std::list<Game>{};
  SECTION ("start quick game with 2 players and accept invite", "[use case]")
  {
    co_spawn (
        io_context, [&, msg = objectToStringWithObjectName (shared_class::CreateGameLobby{})] { return startA2PlayerQuickGame (io_context, pool, users, gameLobbies, games); }, detached);
    io_context.run ();
  }
  database::createEmptyDatabase ();
  database::createTables ();
}