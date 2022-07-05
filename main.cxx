#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/matchmakingGameSerialization.hxx"
#include "matchmaking_proxy/server/matchmakingOption.hxx"
#include "matchmaking_proxy/server/myWebsocket.hxx"
#include "matchmaking_proxy/server/server.hxx"
#include "matchmaking_proxy/userMatchmakingSerialization.hxx"
#include "matchmaking_proxy/util.hxx"
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/json/src.hpp>
#include <sodium/core.h>

auto const DEFAULT_PORT_USER = u_int16_t{ 55555 };
auto const DEFAULT_PORT_MATCHMAKING_TO_GAME = u_int16_t{ 4242 };
auto const DEFAULT_PORT_USER_TO_GAME_VIA_MATCHMAKING = u_int16_t{ 3232 };
auto const DEFAULT_PORT_GAME_TO_MATCHMAKING = u_int16_t{ 12312 };
auto const PATH_TO_SECRETS = std::getenv ("HOME") / std::filesystem::path{ "certificate/fastCert" };

int
main ()
{
  if (sodium_init () < 0)
    {
      std::cout << "sodium_init <= 0" << std::endl;
      std::terminate ();
      /* panic! the library couldn't be initialized, it is not safe to use */
    }
  database::createEmptyDatabase ();
  database::createTables ();
  using namespace boost::asio;
  try
    {
      using namespace boost::asio;
      io_context ioContext{};
      signal_set signals (ioContext, SIGINT, SIGTERM);
      signals.async_wait ([&] (auto, auto) { ioContext.stop (); });
      thread_pool pool{ 2 };
      auto server = Server{ ioContext, pool };
      using namespace boost::asio::experimental::awaitable_operators;
      auto userEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), DEFAULT_PORT_USER };
      auto matchmakingGameEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), DEFAULT_PORT_MATCHMAKING_TO_GAME };
      auto userGameViaMatchmakingEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), DEFAULT_PORT_USER_TO_GAME_VIA_MATCHMAKING };
      auto gameMatchmakingEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), DEFAULT_PORT_GAME_TO_MATCHMAKING };
      co_spawn (ioContext, server.userMatchmaking (userEndpoint, PATH_TO_SECRETS, MatchmakingOption{}, matchmakingGameEndpoint, userGameViaMatchmakingEndpoint) && server.gameMatchmaking (gameMatchmakingEndpoint), printException);
      ioContext.run ();
    }
  catch (std::exception &e)
    {
      std::printf ("Exception: %s\n", e.what ());
    }
}
