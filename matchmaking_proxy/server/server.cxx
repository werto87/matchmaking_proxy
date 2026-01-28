#include "matchmaking_proxy/server/server.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/logic/matchmakingGameData.hxx"
#include "matchmaking_proxy/util.hxx"
#include <algorithm>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#ifdef MATCHMAKING_PROXY_ENABLE_SSL_VERIFICATION
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
#include <certify/https_verification.hpp>
#endif
#include <boost/current_function.hpp>
#include <chrono>
#include <deque>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <matchmaking_proxy/logic/matchmakingAllowedTypes.hxx>
#include <my_web_socket/coSpawnTraced.hxx>
#include <my_web_socket/myWebSocket.hxx>
#include <openssl/ssl3.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
using namespace boost::beast;
using namespace boost::asio;
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
typedef boost::asio::use_awaitable_t<>::as_default_on_t<boost::asio::basic_waitable_timer<boost::asio::chrono::system_clock>> CoroTimer;
namespace matchmaking_proxy
{
Server::Server (boost::asio::io_context &ioContext_, boost::asio::thread_pool &pool_, boost::asio::ip::tcp::endpoint const &userMatchmakingEndpoint, boost::asio::ip::tcp::endpoint const &gameMatchmakingEndpoint) : ioContext{ ioContext_ }, pool{ pool_ }, userMatchmakingAcceptor{ std::make_unique<boost::asio::ip::tcp::acceptor> (ioContext, userMatchmakingEndpoint) }, gameMatchmakingAcceptor{ std::make_unique<boost::asio::ip::tcp::acceptor> (ioContext, gameMatchmakingEndpoint) } {}

boost::asio::awaitable<void>
tryUntilNoException (std::function<void ()> const &fun, std::chrono::seconds const &timeToWaitBeforeCallingFunctionAgain)
{
  for (;;) // try until no exception
    {
      try
        {
          fun ();
          break;
        }
      catch (std::exception &e)
        {
          std::osyncstream (std::cout) << "exception : " << e.what () << std::endl;
        }
      std::osyncstream (std::cout) << "trying again in: " << timeToWaitBeforeCallingFunctionAgain.count () << " seconds" << std::endl;
      auto timer = CoroTimer{ co_await boost::asio::this_coro::executor };
      timer.expires_after (timeToWaitBeforeCallingFunctionAgain);
      co_await timer.async_wait ();
    }
}
bool
messageTypeSupportedByMatchmaking (std::string const &msg)
{
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  auto isSupportedType = false;
  if (splitMesssage.size () == 2)
    {
      auto const &typeToSearch = splitMesssage.at (0);
      boost::hana::for_each (user_matchmaking::userMatchmaking, [&] (const auto &x) {
        if (typeToSearch == confu_json::type_name<typename std::decay<decltype (x)>::type> ())
          {
            isSupportedType = true;
          }
      });
    }
  return isSupportedType;
}

boost::asio::awaitable<void>
Server::userMatchmaking (std::filesystem::path pathToChainFile, std::filesystem::path pathToPrivateFile, std::filesystem::path pathToTmpDhFile, std::filesystem::path fullPathIncludingDatabaseName, std::chrono::seconds pollingSleepTimer, MatchmakingOption matchmakingOption, std::string gameHost, std::string gamePort, std::string userGameViaMatchmakingPort, bool sslContextVerifyNone)
{
  try
    {
      net::ssl::context ctx (net::ssl::context::tls_server);
      if (sslContextVerifyNone)
        {
          ctx.set_verify_mode (ssl::context::verify_none);
        }
      else
        {
          ctx.set_verify_mode (ssl::context::verify_peer);
        }
      ctx.set_default_verify_paths ();
      co_await tryUntilNoException (
          [&pathToChainFile, &ctx] () {
            std::osyncstream (std::cout) << "load fullchain: " << pathToChainFile << std::endl;
            ctx.use_certificate_chain_file (pathToChainFile.string ());
          },
          pollingSleepTimer);
      co_await tryUntilNoException (
          [&pathToPrivateFile, &ctx] () {
            std::osyncstream (std::cout) << "load privkey: " << pathToPrivateFile << std::endl;
            ctx.use_private_key_file (pathToPrivateFile.string (), boost::asio::ssl::context::pem);
          },
          pollingSleepTimer);
      co_await tryUntilNoException (
          [&pathToTmpDhFile, &ctx] () {
            std::osyncstream (std::cout) << "load Diffie-Hellman: " << pathToTmpDhFile << std::endl;
            ctx.use_tmp_dh_file (pathToTmpDhFile.string ());
          },
          pollingSleepTimer);
#ifdef MATCHMAKING_PROXY_ENABLE_SSL_VERIFICATION
      boost::certify::enable_native_https_server_verification (ctx);
#endif
      ctx.set_options (SSL_SESS_CACHE_OFF | SSL_OP_NO_TICKET); //  disable ssl cache. It has a bad support in boost asio/beast and I do not know if it helps in performance in our usecase
      auto gameLobbies = std::make_shared<std::list<GameLobby>> ();
      while (running.load ())
        {
          try
            {
              auto socket = co_await userMatchmakingAcceptor->async_accept ();
              auto connection = my_web_socket::SSLWebSocket{ std::move (socket), ctx };
              connection.set_option (websocket::stream_base::timeout::suggested (role_type::server));
              connection.set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
              co_await connection.next_layer ().async_handshake (ssl::stream_base::server, use_awaitable);
              co_await connection.async_accept (use_awaitable);
              static size_t id = 0;
              auto myWebsocket = std::make_shared<my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>> (std::move (connection), "userMatchmaking", fmt::fg (fmt::color::red), std::to_string (id++));
              sslWebSockets.emplace_back (myWebsocket);
              my_web_socket::coSpawnTraced (ioContext, myWebsocket->sendPingToEndpoint (), "matchmaking_proxy Server::userMatchmaking sendPingToEndpoint");
              tcp::resolver resolv{ ioContext };
              auto resolvedGameMatchmakingEndpoint = co_await resolv.async_resolve (ip::tcp::v4 (), gameHost, gamePort, use_awaitable);
              auto resolvedUserGameViaMatchmakingEndpoint = co_await resolv.async_resolve (ip::tcp::v4 (), gameHost, userGameViaMatchmakingPort, use_awaitable);
              auto matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [myWebsocket] (std::string message) { myWebsocket->queueMessage (std::move (message)); }, gameLobbies, pool, matchmakingOption, resolvedGameMatchmakingEndpoint.begin ()->endpoint (), resolvedUserGameViaMatchmakingEndpoint.begin ()->endpoint (), fullPathIncludingDatabaseName });
              matchmakings.emplace_back (matchmaking);
              using namespace boost::asio::experimental::awaitable_operators;
              my_web_socket::coSpawnTraced (ioContext, myWebsocket->readLoop ([myWebsocket, matchmaking] (const std::string &msg) {
                if (matchmaking->hasProxyToGame () && not messageTypeSupportedByMatchmaking (msg))
                  {
                    matchmaking->sendMessageToGame (msg);
                  }
                else
                  {
                    auto const &processEventResult = matchmaking->processEvent (msg);
                    if (not processEventResult)
                      {
                        myWebsocket->queueMessage (objectToStringWithObjectName (user_matchmaking::UnhandledEventError{ msg, processEventResult.error () }));
                      }
                  }
              }) && myWebsocket->writeLoop (),
                                            "matchmaking_porxy userMatchmaking read && write", [&matchmakings = matchmakings, matchmaking, ex = myWebsocket->webSocket->get_executor ()] (auto) {
                                              auto loggedInPlayerLostConnection = matchmaking->loggedInWithAccountName ().has_value ();
                                              matchmaking->cleanUp ();
                                              if (loggedInPlayerLostConnection)
                                                {
                                                  for (auto matchmakingWeakPtr : matchmakings)
                                                    {
                                                      if (auto otherMatchmaking = matchmakingWeakPtr.lock ()) otherMatchmaking->proccessSendLoggedInPlayersToUser ();
                                                    }
                                                }
                                            });
            }
          catch (std::exception const &e)
            {
              std::osyncstream (std::cout) << "Server::userMatchmaking () connect  Exception : " << e.what () << std::endl;
            }
        }
    }
  catch (std::exception const &e)
    {
      std::osyncstream (std::cout) << "exception: " << e.what () << std::endl;
    }
  std::osyncstream (std::cout) << "exit matchmaking_proxy userMatchmaking \n";
}

boost::asio::awaitable<void>
Server::asyncStopRunning ()
{
  running.store (false, std::memory_order_release);
  auto keepMatchmakingsAliveUntilStopRunningReturns = matchmakings;
  for (auto matchmakingWeakPtr : keepMatchmakingsAliveUntilStopRunningReturns)
    {
      if (auto matchmaking = matchmakingWeakPtr.lock ()) co_await matchmaking->asyncCloseMatchmakingToGame ();
    }
  boost::system::error_code ec;
  userMatchmakingAcceptor->cancel (ec);
  userMatchmakingAcceptor->close (ec);
  gameMatchmakingAcceptor->cancel (ec);
  gameMatchmakingAcceptor->close (ec);
  auto keepWebsocketsAliveUntilStopRunningReturns = webSockets;
  for (auto webSocketWeakPtr : keepWebsocketsAliveUntilStopRunningReturns)
    {
      if (auto webSocket = webSocketWeakPtr.lock ()) co_await webSocket->asyncClose ();
    }
  auto keepSSLWebsocketsAliveUntilStopRunningReturns = sslWebSockets;
  for (auto sslWebSocketWeakPtr : keepSSLWebsocketsAliveUntilStopRunningReturns)
    {
      if (auto webSocket = sslWebSocketWeakPtr.lock ()) co_await webSocket->asyncClose ();
    }
}

boost::asio::awaitable<void>
Server::gameMatchmaking (std::filesystem::path fullPathIncludingDatabaseName, std::function<void (std::string const &messageType, std::string const &message, MatchmakingGameData &matchmakingGameData)> handleCustomMessageFromGame)
{
  try
    {
      while (running.load ())
        {
          try
            {
              std::osyncstream (std::cout) << "wait for game over" << std::endl;
              auto socket = co_await gameMatchmakingAcceptor->async_accept ();
              auto connection = my_web_socket::WebSocket{ std::move (socket) };
              connection.set_option (websocket::stream_base::timeout::suggested (role_type::server));
              connection.set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
              co_await connection.async_accept ();
              static size_t id = 0;
              auto myWebSocket = std::make_shared<my_web_socket::MyWebSocket<my_web_socket::WebSocket>> (std::move (connection), "gameMatchmaking", fmt::fg (fmt::color::blue_violet), std::to_string (id++));
              webSockets.emplace_back (myWebSocket);
              using namespace boost::asio::experimental::awaitable_operators;
              my_web_socket::coSpawnTraced (ioContext, myWebSocket->readLoop ([myWebSocket, &matchmakings = matchmakings, fullPathIncludingDatabaseName, handleCustomMessageFromGame] (const std::string &msg) {
                auto matchmakingGameData = MatchmakingGameData{ fullPathIncludingDatabaseName, matchmakings, [myWebSocket] (std::string const &_msg) { myWebSocket->queueMessage (_msg); }, handleCustomMessageFromGame };
                auto matchmakingGame = MatchmakingGame{ matchmakingGameData };
                matchmakingGame.process_event (msg);
              }) && myWebSocket->writeLoop (),
                                            "matchmaking_porxy gameMatchmaking read && write");
            }
          catch (std::exception const &e)
            {
              std::osyncstream (std::cout) << "Server::gameMatchmaking () connect  Exception : " << e.what () << std::endl;
            }
        }
    }
  catch (std::exception const &e)
    {
      std::osyncstream (std::cout) << "exception: " << e.what () << std::endl;
    }
  std::osyncstream (std::cout) << "exit matchmaking_proxy gameMatchmaking \n";
}
}