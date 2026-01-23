#include "matchmaking_proxy/server/server.hxx"
#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/util.hxx"
#include "networkingUtil.hxx"
#include "util.hxx"
#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>
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
#include <modern_durak_game_shared/modern_durak_game_shared.hxx>
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

TEST_CASE ("user,matchmaking, game", "[matchmaking server]")
{
  if (sodium_init () < 0)
    {
      std::osyncstream (std::cout) << "sodium_init <= 0" << std::endl;
      std::terminate ();
      /* panic! the library couldn't be initialized, it is not safe to use */
    }
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  using namespace boost::asio;
  auto ioContext = io_context{};
  auto pool = thread_pool{ 2 };
  auto const matchmakingGamePort = 4242;
  auto const userGameViaMatchmakingPort = 3232;
  auto matchmakingGame = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), matchmakingGamePort }, { .requestResponse = { { "LeaveGame|{}", "LeaveGameSuccess|{}" } }, .requestStartsWithResponse = { { R"foo(StartGame)foo", R"foo(StartGameSuccess|{"gameName":"7731882c-50cd-4a7d-aa59-8f07989edb18"})foo" } } }, "matchmaking_game", fmt::fg (fmt::color::violet), "0" };
  auto userGameViaMatchmaking = my_web_socket::MockServer{ { boost::asio::ip::make_address ("127.0.0.1"), userGameViaMatchmakingPort }, { .requestResponse = {}, .requestStartsWithResponse = { { R"foo(ConnectToGame)foo", "ConnectToGameSuccess|{}" } } }, "userGameViaMatchmaking", fmt::fg (fmt::color::lawn_green), "0" };
  auto server = Server{ ioContext, pool, { boost::asio::ip::make_address ("127.0.0.1"), 0 }, { boost::asio::ip::make_address ("127.0.0.1"), 0 } };
  auto const userMatchmakingPort = server.userMatchmakingAcceptor.get ()->local_endpoint ().port ();
  auto const PATH_TO_CHAIN_FILE = PATH_TO_SOURCE + std::string{ "/test/cert" } + std::string{ "/localhost.pem" };
  auto const PATH_TO_PRIVATE_File = PATH_TO_SOURCE + std::string{ "/test/cert" } + std::string{ "/localhost-key.pem" };
  auto const PATH_TO_DH_File = PATH_TO_SOURCE + std::string{ "/test/cert" } + std::string{ "/dhparam.pem" };
  auto const POLLING_SLEEP_TIMER = std::chrono::seconds{ 2 };
  using namespace boost::asio::experimental::awaitable_operators;
  auto matchmakingOption = MatchmakingOption{};
  matchmakingOption.timeToAcceptInvite = std::chrono::seconds{ 3333 };
  auto handlecustomMessageCalled = false;
  matchmakingOption.handleCustomMessageFromUser = [&handlecustomMessageCalled, &ioContext, &server] (auto &, auto &, auto &) {
    handlecustomMessageCalled = true;
    co_spawn (ioContext, server.asyncStopRunning (), my_web_socket::printException);
  };
  co_spawn (ioContext, server.userMatchmaking (PATH_TO_CHAIN_FILE, PATH_TO_PRIVATE_File, PATH_TO_DH_File, "matchmaking_proxy.db", POLLING_SLEEP_TIMER, matchmakingOption, "localhost", std::to_string (matchmakingGamePort), std::to_string (userGameViaMatchmakingPort)) || server.gameMatchmaking ("matchmaking_proxy.db"), my_web_socket::printException);
  SECTION ("start, connect, create account, join game, leave", "[matchmaking]")
  {
    auto messagesFromGamePlayer1 = std::vector<std::string>{};
    size_t gameOver = 0;
    auto handleMsgFromGame = [&gameOver] (boost::asio::io_context &, std::string const &msg, std::shared_ptr<my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>> myWebsocket) {
      if (boost::starts_with (msg, "LoginAsGuestSuccess"))
        {
          myWebsocket->queueMessage (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{}));
        }
      else if (boost::starts_with (msg, "AskIfUserWantsToJoinGame"))
        {
          myWebsocket->queueMessage (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true }));
        }
      else if (boost::starts_with (msg, "ProxyStarted"))
        {
          gameOver++;
          if (gameOver == 2)
            {
              myWebsocket->queueMessage (objectToStringWithObjectName (user_matchmaking::CustomMessage ("CombinationSolved", "")));
            }
        }
    };
    co_spawn (ioContext, connectWebsocketSSL (handleMsgFromGame, { { "LoginAsGuest|{}" } }, ioContext, { boost::asio::ip::make_address ("127.0.0.1"), userMatchmakingPort }, messagesFromGamePlayer1), my_web_socket::printException);
    auto messagesFromGamePlayer2 = std::vector<std::string>{};
    co_spawn (ioContext, connectWebsocketSSL (handleMsgFromGame, { { "LoginAsGuest|{}" } }, ioContext, { boost::asio::ip::make_address ("127.0.0.1"), userMatchmakingPort }, messagesFromGamePlayer2), my_web_socket::printException);
    ioContext.run ();
    CHECK (messagesFromGamePlayer1.size () == 4);
    CHECK (boost::starts_with (messagesFromGamePlayer1.at (0), "LoginAsGuestSuccess"));
    CHECK (messagesFromGamePlayer1.at (1) == "JoinMatchMakingQueueSuccess|{}");
    CHECK (messagesFromGamePlayer1.at (2) == "AskIfUserWantsToJoinGame|{}");
    CHECK (messagesFromGamePlayer1.at (3) == "ProxyStarted|{}");
    CHECK (messagesFromGamePlayer2.size () == 5);
    CHECK (boost::starts_with (messagesFromGamePlayer2.at (0), "LoginAsGuestSuccess"));
    CHECK (messagesFromGamePlayer2.at (1) == "JoinMatchMakingQueueSuccess|{}");
    CHECK (messagesFromGamePlayer2.at (2) == "AskIfUserWantsToJoinGame|{}");
    CHECK (messagesFromGamePlayer2.at (3) == "ProxyStarted|{}");
    CHECK (messagesFromGamePlayer2.at (4) == "ProxyStopped|{}");
    CHECK (handlecustomMessageCalled);
  }
  matchmakingGame.shutDownUsingMockServerIoContext ();
  userGameViaMatchmaking.shutDownUsingMockServerIoContext ();
}
BOOST_FUSION_DEFINE_STRUCT ((account_with_combinationsSolved), Account, (std::string, accountName) (std::string, password) (size_t, rating) (size_t, combinationsSolved))
TEST_CASE ("Sandbox", "[.][Sandbox]")
{
  if (sodium_init () < 0)
    {
      std::osyncstream (std::cout) << "sodium_init <= 0" << std::endl;
      std::terminate ();
      /* panic! the library couldn't be initialized, it is not safe to use */
    }
  auto const pathToMatchmakingDatabase = std::filesystem::path{ PATH_TO_BINARY + std::string{ "/matchmaking_proxy.db" } };
  database::createEmptyDatabase (pathToMatchmakingDatabase.string ());
  database::createTables (pathToMatchmakingDatabase.string ());
  using namespace boost::asio;
  auto ioContext = io_context{};
  auto pool = thread_pool{ 2 };
  auto const userMatchmakingPort = 55555;
  auto const gameMatchmakingPort = 12312;
  auto const matchmakingGamePort = 4242;
  auto const userGameViaMatchmakingPort = 3232;
  auto server = Server{ ioContext, pool, { boost::asio::ip::make_address ("127.0.0.1"), userMatchmakingPort }, { boost::asio::ip::make_address ("127.0.0.1"), gameMatchmakingPort } };
  auto const PATH_TO_CHAIN_FILE = PATH_TO_SOURCE + std::string{ "/test/cert" } + std::string{ "/localhost.pem" };
  auto const PATH_TO_PRIVATE_File = PATH_TO_SOURCE + std::string{ "/test/cert" } + std::string{ "/localhost-key.pem" };
  auto const PATH_TO_DH_File = PATH_TO_SOURCE + std::string{ "/test/cert" } + std::string{ "/dhparam.pem" };
  auto const POLLING_SLEEP_TIMER = std::chrono::seconds{ 2 };
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
  auto const &handleMessageFromGame = [] (std::string const &messageType, std::string const &message, MatchmakingGameData &matchmakingGameData) {
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
  };
  co_spawn (ioContext, server.userMatchmaking (PATH_TO_CHAIN_FILE, PATH_TO_PRIVATE_File, PATH_TO_DH_File, pathToMatchmakingDatabase, POLLING_SLEEP_TIMER, matchmakingOption, "localhost", std::to_string (matchmakingGamePort), std::to_string (userGameViaMatchmakingPort)) || server.gameMatchmaking (pathToMatchmakingDatabase, handleMessageFromGame), my_web_socket::printException);
  SECTION ("start connect LoggedInPlayers leave", "[matchmaking]")
  {
    auto messagesFromGamePlayer1 = std::vector<std::string>{};
    size_t gameOver = 0;
    auto handleMsgFromGamePlayer1 = [&gameOver, &server] (boost::asio::io_context &_ioContext, std::string const &msg, std::shared_ptr<my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>>) {
      if (boost::starts_with (msg, "LoggedInPlayers"))
        {
          gameOver++;
        }
      if (gameOver == 3)
        {
          co_spawn (_ioContext, server.asyncStopRunning (), my_web_socket::printException);
        }
    };
    co_spawn (ioContext, connectWebsocketSSL (handleMsgFromGamePlayer1, { objectToStringWithObjectName (user_matchmaking::SubscribeGetLoggedInPlayers{ 42 }), { "LoginAsGuest|{}" } }, ioContext, { boost::asio::ip::make_address ("127.0.0.1"), userMatchmakingPort }, messagesFromGamePlayer1), my_web_socket::printException);
    auto messagesFromGamePlayer2 = std::vector<std::string>{};
    auto handleMsgFromGamePlayer2 = [&gameOver] (boost::asio::io_context &, std::string const &msg, std::shared_ptr<my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>>) {
      if (boost::starts_with (msg, "LoginAsGuestSuccess"))
        {
          throw "Throw something so Server::userMatchmaking clean up code gets called";
        }
    };
    co_spawn (ioContext, connectWebsocketSSL (handleMsgFromGamePlayer2, { { "LoginAsGuest|{}" } }, ioContext, { boost::asio::ip::make_address ("127.0.0.1"), userMatchmakingPort }, messagesFromGamePlayer2), my_web_socket::printException);
    ioContext.run ();
    CHECK (messagesFromGamePlayer1.size () == 4);
    CHECK (boost::starts_with (messagesFromGamePlayer1.at (0), "LoginAsGuestSuccess"));
    CHECK (boost::starts_with (messagesFromGamePlayer1.at (1), "LoggedInPlayers"));
    CHECK (boost::starts_with (messagesFromGamePlayer1.at (2), "LoggedInPlayers"));
    CHECK (boost::starts_with (messagesFromGamePlayer1.at (3), "LoggedInPlayers"));
  }
  SECTION ("just run the server", "[.debuging matchmaking]") { ioContext.run (); }
  std::filesystem::remove (pathToMatchmakingDatabase);
}
