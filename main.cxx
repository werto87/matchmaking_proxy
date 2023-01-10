#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/server/matchmakingOption.hxx"
#include "matchmaking_proxy/server/myWebsocket.hxx"
#include "matchmaking_proxy/server/server.hxx"
#include "matchmaking_proxy/util.hxx"
#include <Corrade/Utility/Arguments.h>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/json/src.hpp>
#include <sodium/core.h>

auto const DEFAULT_PORT_USER = std::string{ "55555" };
auto const DEFAULT_PORT_MATCHMAKING_TO_GAME = std::string{ "4242" };
auto const DEFAULT_PORT_USER_TO_GAME_VIA_MATCHMAKING = std::string{ "3232" };
auto const DEFAULT_PORT_GAME_TO_MATCHMAKING = std::string{ "12312" };
auto const DEFAULT_PATH_TO_CHAIN_FILE = std::string{ "/etc/letsencrypt/live/test-name/fullchain.pem" };
auto const DEFAULT_PATH_TO_PRIVATE_FILE = std::string{ "/etc/letsencrypt/live/test-name/privkey.pem" };
auto const DEFAULT_PATH_TO_DH_File = std::string{ "/etc/letsencrypt/dhparams/dhparam.pem" };
auto const DEFAULT_SECRETS_POLLING_SLEEP_TIMER_SECONDS = std::string{ "2" };
auto const DEFAULT_ADDRESS_OF_GAME = std::string{ "localhost" };

int
main (int argc, char **argv)
{
  Corrade::Utility::Arguments args{};

  // clang-format off
  args
    .addOption("port-user", DEFAULT_PORT_USER).setHelp("port-user", "port user to game via matchmaking")
    .addOption("port-matchmaking-to-game", DEFAULT_PORT_MATCHMAKING_TO_GAME).setHelp("port-matchmaking-to-game", "port matchmaking to game")
    .addOption("port-user-to-game-via-matchmaking", DEFAULT_PORT_USER_TO_GAME_VIA_MATCHMAKING).setHelp("port-user-to-game-via-matchmaking", "port user to game via matchmaking")
    .addOption("port-game-to-matchmaking", DEFAULT_PORT_GAME_TO_MATCHMAKING).setHelp("port-game-to-matchmaking", "port game to matchmaking")
    .addOption("path-to-chain-file", DEFAULT_PATH_TO_CHAIN_FILE).setHelp("path-to-chain-file", "path to chain file")
    .addOption("path-to-private-file", DEFAULT_PATH_TO_PRIVATE_FILE).setHelp("path-to-private-file", "path to private file")
    .addOption("path-to-dh-file", DEFAULT_PATH_TO_DH_File).setHelp("path-to-dh-file", "path to dh file")
    .addOption("secrets-polling-sleep-time-seconds", DEFAULT_SECRETS_POLLING_SLEEP_TIMER_SECONDS).setHelp("secrets-polling-sleep-time-seconds", "secrets polling sleep time seconds")
    .addOption("address-of-game", DEFAULT_ADDRESS_OF_GAME).setHelp("address-of-game", "address of game")
    .addBooleanOption("ssl-context-verify-none").setHelp("ssl-context-verify-none", "disable ssl verification for user to matchmaking usefull for debuging with google chrome")
    .setGlobalHelp("durak matchmaking")
    .parse(argc, argv);
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
      auto const PORT_USER =  boost::numeric_cast<u_int16_t>(std::stoul(args.value ("port-user")));
      auto const PORT_MATCHMAKING_TO_GAME =  args.value ("port-matchmaking-to-game");
      auto const PORT_USER_TO_GAME_VIA_MATCHMAKING =  args.value ("port-user-to-game-via-matchmaking");
      auto const PORT_GAME_TO_MATCHMAKING =  boost::numeric_cast<u_int16_t>(std::stoul(args.value ("port-game-to-matchmaking")));
      auto const PATH_TO_CHAIN_FILE =  args.value ("path-to-chain-file");
      auto const PATH_TO_PRIVATE_FILE =  args.value ("path-to-private-file");
      auto const PATH_TO_DH_File =  args.value ("path-to-dh-file");
      auto const SECRETS_POLLING_SLEEP_TIMER_SECONDS =  std::stoul(args.value ("secrets-polling-sleep-time-seconds"));
      std::string ADDRESS_GAME = args.value ("address-of-game");
      auto const SSL_CONTEXT_VERIFY_NONE= args.isSet("ssl-context-verify-none");
      using namespace boost::asio::experimental::awaitable_operators;
      auto userEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), PORT_USER };
      auto gameMatchmakingEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), PORT_GAME_TO_MATCHMAKING };
      co_spawn (ioContext, server.userMatchmaking (userEndpoint, PATH_TO_CHAIN_FILE, PATH_TO_PRIVATE_FILE, PATH_TO_DH_File, std::chrono::seconds{SECRETS_POLLING_SLEEP_TIMER_SECONDS}, MatchmakingOption{}, ADDRESS_GAME,PORT_MATCHMAKING_TO_GAME, PORT_USER_TO_GAME_VIA_MATCHMAKING,SSL_CONTEXT_VERIFY_NONE) && server.gameMatchmaking (gameMatchmakingEndpoint), printException);
      ioContext.run ();
    }
  catch (std::exception &e)
    {
      std::printf ("Exception: %s\n", e.what ());
    }
}

