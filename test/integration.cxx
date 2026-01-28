#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/server/server.hxx"
#include "matchmaking_proxy/util.hxx"
#include "networkingUtil.hxx"
#include "util.hxx"
#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>

#ifdef MATCHMAKING_PROXY_ENABLE_SSL_VERIFICATION
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
#endif
#include <catch2/catch.hpp>
#include <cstddef>
#include <deque>
#include <exception>
#include <filesystem>
#include <fmt/color.h>
#include <functional>
#include <iostream>
#include <iterator>
#include <login_matchmaking_game_shared/matchmakingGameSerialization.hxx>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <matchmaking_proxy/server/matchmakingOption.hxx>
#include <modern_durak_game/server/server.hxx>
#include <modern_durak_game_shared/modern_durak_game_shared.hxx>
#include <my_web_socket/coSpawnTraced.hxx>
#include <my_web_socket/mockServer.hxx>
#include <my_web_socket/myWebSocket.hxx>
#include <openssl/ssl3.h>
#include <sodium/core.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

using namespace matchmaking_proxy;
using namespace boost::asio;

BOOST_FUSION_DEFINE_STRUCT ((shared_class), Account, (std::string, accountName) (std::string, password) (size_t, rating))
BOOST_FUSION_DEFINE_STRUCT ((account_with_combinationsSolved), Account, (std::string, accountName) (std::string, password) (size_t, rating) (size_t, combinationsSolved))

struct LocalBackend
{
  std::unique_ptr<boost::asio::io_context> matchmakingServerContext = std::make_unique<boost::asio::io_context> ();
  std::unique_ptr<boost::asio::io_context> gameServerContext = std::make_unique<boost::asio::io_context> ();
  std::unique_ptr<boost::asio::thread_pool> pool = std::make_unique<boost::asio::thread_pool> ();
  std::shared_ptr<matchmaking_proxy::Server> matchmakingServer{};
  std::shared_ptr<modern_durak_game::Server> gameServer{};
  boost::asio::ip::tcp::endpoint matchmakingEndpoint{};
  boost::asio::ip::tcp::endpoint matchmakingToGameEndpoint{};
  boost::asio::ip::tcp::endpoint userToGameViaMatchmakingEndpoint{};
  boost::asio::ip::tcp::endpoint gameToMatchmakingEndpoint{};
};

using Login = std::variant<user_matchmaking::LoginAsGuest, user_matchmaking::CreateAccount>;

void
someTestCase (auto handleMessageFromMatchmaking, Login const &login)
{
  using namespace boost::asio;
  using namespace boost::beast;
  auto localBackend = LocalBackend{};
  auto const localAddressPortZeroEndpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 0 };
  localBackend.matchmakingServer = std::make_shared<matchmaking_proxy::Server> (*localBackend.matchmakingServerContext, *localBackend.pool, localAddressPortZeroEndpoint, localAddressPortZeroEndpoint);
  localBackend.gameServer = std::make_shared<modern_durak_game::Server> (*localBackend.gameServerContext, localAddressPortZeroEndpoint, localAddressPortZeroEndpoint);
  localBackend.matchmakingEndpoint = { boost::asio::ip::make_address ("127.0.0.1"), localBackend.matchmakingServer->userMatchmakingAcceptor->local_endpoint ().port () };
  localBackend.matchmakingToGameEndpoint = { boost::asio::ip::make_address ("127.0.0.1"), localBackend.matchmakingServer->gameMatchmakingAcceptor->local_endpoint ().port () };
  localBackend.userToGameViaMatchmakingEndpoint = { boost::asio::ip::make_address ("127.0.0.1"), localBackend.gameServer->userToGameViaMatchmakingAcceptor->local_endpoint ().port () };
  localBackend.gameToMatchmakingEndpoint = { boost::asio::ip::make_address ("127.0.0.1"), localBackend.gameServer->matchmakingGameAcceptor->local_endpoint ().port () };
  using namespace matchmaking_proxy;
  auto const pathToMatchmakingDatabase = std::filesystem::path{ PATH_TO_BINARY + std::string{ "/matchmaking_proxy.db" } };
  database::createEmptyDatabase (pathToMatchmakingDatabase.string ());
  database::createTables (pathToMatchmakingDatabase.string ());
  auto ioContext = boost::asio::io_context{};
  ssl::context ctx{ ssl::context::tlsv12_client };
  auto myWebSocket = std::shared_ptr<my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>>{};
  auto matchmakingFinished = false;
  auto tearDownSignal = std::make_unique<boost::asio::experimental::channel<boost::asio::any_io_executor, void (boost::system::error_code)>> (ioContext, 1);
  boost::asio::post (*localBackend.pool, [&, matchmakingProxyServer = localBackend.matchmakingServer] () {
    auto const PATH_TO_CHAIN_FILE = PATH_TO_SOURCE + std::string{ "/test/cert" } + std::string{ "/localhost.pem" };
    auto const PATH_TO_PRIVATE_FILE = PATH_TO_SOURCE + std::string{ "/test/cert" } + std::string{ "/localhost-key.pem" };
    auto const PATH_TO_DH_File = PATH_TO_SOURCE + std::string{ "/test/cert" } + std::string{ "/dhparam.pem" };
    auto const SECRETS_POLLING_SLEEP_TIMER_SECONDS = int64_t{ 2 };
    using namespace boost::asio;
    if (sodium_init () < 0)
      {
        std::osyncstream (std::cout) << "sodium_init <= 0" << std::endl;
        std::terminate ();
        /* panic! the library couldn't be initialized, it is not safe to use */
      }
    try
      {
        using namespace boost::asio::experimental::awaitable_operators;
        auto matchmakingOption = MatchmakingOption{};
        matchmakingOption.handleCustomMessageFromUser = [] (std::string const &messageType, std::string const &message, MatchmakingData &matchmakingData) {
          boost::system::error_code ec{};
          auto messageAsObject = confu_json::read_json (message, ec);
          if (ec) std::osyncstream (std::cout) << "no handle for custom message: '" << message << "'" << std::endl;
          else if (messageType == "GetCombinationSolved")
            {
              auto combinationSolved = confu_json::to_object<shared_class::GetCombinationSolved> (messageAsObject);
              soci::session sql (soci::sqlite3, matchmakingData.fullPathIncludingDatabaseName.string ().c_str ());
              bool columnExists = false;
              soci::rowset<soci::row> rs = (sql.prepare << "PRAGMA table_info(Account)");
              for (auto const &row : rs)
                {
                  std::string name = row.get<std::string> (1);
                  if (name == "combinationsSolved")
                    {
                      columnExists = true;
                      break;
                    }
                }
              if (!columnExists) sql << "ALTER TABLE Account ADD COLUMN combinationsSolved INTEGER NOT NULL DEFAULT 0";
              if (auto result = confu_soci::findStruct<account_with_combinationsSolved::Account> (sql, "accountName", combinationSolved.accountName))
                {
                  matchmakingData.sendMsgToUser (objectToStringWithObjectName (shared_class::CombinationsSolved{ result->accountName, result->combinationsSolved }));
                }
            }
        };
        std::osyncstream (std::cout) << "gameMatchmaking called" << std::endl;
        my_web_socket::coSpawnTraced (*localBackend.matchmakingServerContext,
                                      matchmakingProxyServer->userMatchmaking (PATH_TO_CHAIN_FILE, PATH_TO_PRIVATE_FILE, PATH_TO_DH_File, pathToMatchmakingDatabase, std::chrono::seconds{ SECRETS_POLLING_SLEEP_TIMER_SECONDS }, matchmakingOption, localBackend.gameToMatchmakingEndpoint.address ().to_string (), std::to_string (localBackend.gameToMatchmakingEndpoint.port ()), std::to_string (localBackend.userToGameViaMatchmakingEndpoint.port ()), true)
                                          && matchmakingProxyServer->gameMatchmaking (pathToMatchmakingDatabase,
                                                                                      [] (std::string const &messageType, std::string const &message, MatchmakingGameData &matchmakingGameData) {
                                                                                        boost::system::error_code ec{};
                                                                                        auto messageAsObject = confu_json::read_json (message, ec);
                                                                                        if (ec) std::osyncstream (std::cout) << "no handle for custom message: '" << message << "'" << std::endl;
                                                                                        else if (messageType == "CombinationSolved")
                                                                                          {
                                                                                            auto combinationSolved = confu_json::to_object<shared_class::CombinationSolved> (messageAsObject);
                                                                                            soci::session sql (soci::sqlite3, matchmakingGameData.fullPathIncludingDatabaseName.string ().c_str ());
                                                                                            bool columnExists = false;
                                                                                            soci::rowset<soci::row> rs = (sql.prepare << "PRAGMA table_info(Account)");
                                                                                            for (auto const &row : rs)
                                                                                              {
                                                                                                std::string name = row.get<std::string> (1);
                                                                                                if (name == "combinationsSolved")
                                                                                                  {
                                                                                                    columnExists = true;
                                                                                                    break;
                                                                                                  }
                                                                                              }
                                                                                            if (!columnExists) sql << "ALTER TABLE Account ADD COLUMN combinationsSolved INTEGER NOT NULL DEFAULT 0";
                                                                                            if (auto accountResult = confu_soci::findStruct<account_with_combinationsSolved::Account> (sql, "accountName", combinationSolved.accountName)) confu_soci::updateStruct (sql, account_with_combinationsSolved::Account{ accountResult.value ().accountName, accountResult.value ().password, accountResult.value ().rating, accountResult.value ().combinationsSolved + 1 });
                                                                                          }
                                                                                        else
                                                                                          std::osyncstream (std::cout) << "no handle for custom message: '" << message << "'" << std::endl;
                                                                                      }),
                                      "modern_durak_unreal_cxx MatchmakingServer", [] (auto) {});
        localBackend.matchmakingServerContext->run ();
        std::cout << "matchmaking server finished" << std::endl;
        matchmakingFinished = true;
      }
    catch (std::exception &e)
      {
        std::printf ("Exception: %s\n", e.what ());
      }
  });
  auto gameFinished = false;
  boost::asio::post (*localBackend.pool, [&, gameServer = localBackend.gameServer] () {
    auto const PATH_TO_COMBINATION_DATABASE = PATH_TO_SOURCE + std::string{ "/test/database/diamond_combinations.db" };
    if (not std::filesystem::exists (PATH_TO_COMBINATION_DATABASE))
      {
        std::osyncstream (std::cout) << "combination.db not found at: '" + PATH_TO_COMBINATION_DATABASE + "' please provide it or create it by running create_combination_database executable. Consider building create_combination_database in release mode it is around 15 times faster than debug." << std::endl;
        std::terminate ();
      }
    else
      {
        std::osyncstream (std::cout) << "starting modern_durak_game" << std::endl;
        try
          {
            using namespace boost::asio;
            using namespace boost::asio::experimental::awaitable_operators;
            my_web_socket::coSpawnTraced (*localBackend.gameServerContext, gameServer->listenerUserToGameViaMatchmaking (localBackend.matchmakingToGameEndpoint.address ().to_string (), std::to_string (localBackend.matchmakingToGameEndpoint.port ()), PATH_TO_COMBINATION_DATABASE) && gameServer->listenerMatchmakingToGame (localBackend.matchmakingEndpoint.address ().to_string (), std::to_string (localBackend.matchmakingEndpoint.port ()), PATH_TO_COMBINATION_DATABASE), "modern_durak_unreal_cxx gameServer", [] (auto) {});
            localBackend.gameServerContext->run ();
            std::cout << "game server finished" << std::endl;
            gameFinished = true;
          }
        catch (std::exception &e)
          {
            std::printf ("Exception: %s\n", e.what ());
          }
      }
  });
  try
    {
      ctx.set_verify_mode (boost::asio::ssl::verify_none); // DO NOT USE THIS IN PRODUCTION THIS WILL IGNORE CHECKING FOR TRUSTFUL CERTIFICATE
      using ip::tcp;
      my_web_socket::coSpawnTraced (
          ioContext,
          [&] () -> boost::asio::awaitable<void> {
            auto connection = my_web_socket::SSLWebSocket{ ioContext, ctx };
            get_lowest_layer (connection).expires_never ();
            connection.set_option (websocket::stream_base::timeout::suggested (role_type::client));
            connection.set_option (websocket::stream_base::decorator ([] (websocket::request_type &req) { req.set (http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl"); }));
            co_await get_lowest_layer (connection).async_connect (localBackend.matchmakingEndpoint, use_awaitable);
            co_await connection.next_layer ().async_handshake (ssl::stream_base::client, use_awaitable);
            co_await connection.async_handshake (localBackend.matchmakingEndpoint.address ().to_string () + ":" + std::to_string (localBackend.matchmakingEndpoint.port ()), "/", use_awaitable);
            static size_t id = 0;
            myWebSocket = std::make_shared<my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>> (std::move (connection), "connectWebsocket", fg (fmt::color::chocolate), std::to_string (id++));
            using namespace boost::asio::experimental::awaitable_operators;
            my_web_socket::coSpawnTraced (ioContext, myWebSocket->readLoop ([&] (auto const &message) { handleMessageFromMatchmaking (message, tearDownSignal, myWebSocket); }) && myWebSocket->writeLoop (), "test userMatchmaking read && write", [myWebSocket] (auto) {});
            if (std::holds_alternative<user_matchmaking::LoginAsGuest> (login))
              {
                myWebSocket->queueMessage (objectToStringWithObjectName (user_matchmaking::LoginAsGuest{}));
              }
            else if (std::holds_alternative<user_matchmaking::CreateAccount> (login))
              {
                myWebSocket->queueMessage (objectToStringWithObjectName (std::get<user_matchmaking::CreateAccount> (login)));
              }
          },
          "test");
    }
  catch (std::exception const &e)
    {
      std::osyncstream (std::cout) << "Exception :" << e.what () << std::endl;
    }

  my_web_socket::coSpawnTraced (
      ioContext,
      [&] () -> boost::asio::awaitable<void> {
        co_await tearDownSignal->async_receive (boost::asio::use_awaitable);
        co_await myWebSocket->asyncClose ();
        my_web_socket::coSpawnTraced (*localBackend.gameServerContext, localBackend.gameServer->asyncStopRunning (), "matchmaking asyncStopRunning ()");
        my_web_socket::coSpawnTraced (*localBackend.matchmakingServerContext, localBackend.matchmakingServer->asyncStopRunning (), "matchmaking asyncStopRunning ()");
        localBackend.pool->join ();
      },
      "matchmaking asyncStopRunning ()");
  ioContext.run ();
  REQUIRE (matchmakingFinished);
  REQUIRE (gameFinished);
}

TEST_CASE ("matchmaking server and game server tear down after")
{
  SECTION ("start")
  {
    someTestCase ([] (auto, auto &doneSignal, auto) { doneSignal->try_send (boost::system::error_code{}); }, user_matchmaking::LoginAsGuest{});
  }
  SECTION ("LoginAsGuestSuccess")
  {
    someTestCase (
        [] (auto message, auto &doneSignal, auto) {
          if (boost::starts_with (message, "LoginAsGuestSuccess|"))
            {
              doneSignal->try_send (boost::system::error_code{});
            }
        },
        user_matchmaking::LoginAsGuest{});
  }
  SECTION ("JoinMatchMakingQueueSuccess")
  {
    someTestCase (
        [] (auto message, auto &doneSignal, auto myWebSocket) {
          if (boost::starts_with (message, "LoginAsGuestSuccess|"))
            {
              myWebSocket->queueMessage (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{}));
            }
          else if (boost::starts_with (message, "JoinMatchMakingQueueSuccess|"))
            {
              doneSignal->try_send (boost::system::error_code{});
            }
        },
        user_matchmaking::LoginAsGuest{});
  }
  SECTION ("GameOptionAsString")
  {
    someTestCase (
        [] (auto message, auto &doneSignal, auto myWebSocket) {
          if (boost::starts_with (message, "LoginAsGuestSuccess|"))
            {
              myWebSocket->queueMessage (objectToStringWithObjectName (user_matchmaking::CreateGameLobby{ "abc", "" }));
            }
          else if (boost::starts_with (message, "JoinGameLobbySuccess|"))
            {
              auto gameOption = shared_class::GameOption{};
              gameOption.create3CardsVs3CardsPuzzle = true;
              myWebSocket->queueMessage (objectToStringWithObjectName (user_matchmaking_game::GameOptionAsString{ boost::json::serialize (confu_json::to_json (gameOption)) }));
            }
          else if (boost::starts_with (message, "GameOptionAsString|"))
            {
              doneSignal->try_send (boost::system::error_code{});
            }
        },
        user_matchmaking::LoginAsGuest{});
  }
  SECTION ("ProxyStarted")
  {
    someTestCase (
        [] (auto message, auto &doneSignal, auto myWebSocket) {
          if (boost::starts_with (message, "LoginAccountSuccess|"))
            {
              myWebSocket->queueMessage (objectToStringWithObjectName (user_matchmaking::CreateGameLobby{ "abc", "" }));
            }
          else if (boost::starts_with (message, "JoinGameLobbySuccess|"))
            {
              auto gameOption = shared_class::GameOption{};
              gameOption.create3CardsVs3CardsPuzzle = true;
              myWebSocket->queueMessage (objectToStringWithObjectName (user_matchmaking_game::GameOptionAsString{ boost::json::serialize (confu_json::to_json (gameOption)) }));
            }
          else if (boost::starts_with (message, "GameOptionAsString|"))
            {
              myWebSocket->queueMessage (objectToStringWithObjectName (user_matchmaking::CreateGame{}));
            }
          else if (boost::starts_with (message, "ProxyStarted|"))
            {
              doneSignal->try_send (boost::system::error_code{});
            }
        },
        user_matchmaking::CreateAccount{ "abc", "abc" });
  }
  SECTION ("StartGameSuccess")
  {
    someTestCase (
        [] (auto message, auto &doneSignal, auto myWebSocket) {
          if (boost::starts_with (message, "LoginAccountSuccess|"))
            {
              myWebSocket->queueMessage (objectToStringWithObjectName (user_matchmaking::CreateGameLobby{ "abc", "" }));
            }
          else if (boost::starts_with (message, "JoinGameLobbySuccess|"))
            {
              auto gameOption = shared_class::GameOption{};
              gameOption.create3CardsVs3CardsPuzzle = true;
              myWebSocket->queueMessage (objectToStringWithObjectName (user_matchmaking_game::GameOptionAsString{ boost::json::serialize (confu_json::to_json (gameOption)) }));
            }
          else if (boost::starts_with (message, "GameOptionAsString|"))
            {
              myWebSocket->queueMessage (objectToStringWithObjectName (user_matchmaking::CreateGame{}));
            }
          else if (boost::starts_with (message, "StartGameSuccess|"))
            {
              doneSignal->try_send (boost::system::error_code{});
            }
        },
        user_matchmaking::CreateAccount{ "abc", "abc" });
  }
}