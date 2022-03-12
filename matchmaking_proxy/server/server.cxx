#include "server.hxx"
#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/userMatchmakingSerialization.hxx"
#include "matchmaking_proxy/util.hxx"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <range/v3/algorithm/find_if.hpp>
#ifdef BOOST_ASIO_HAS_CLANG_LIBCXX
#include <experimental/coroutine>
#endif
#include "matchmakingOption.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "myWebsocket.hxx"
#include "server.hxx"
#include <algorithm> // for max
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
#include <boost/current_function.hpp>
#include <deque>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator> // for next
#include <openssl/ssl3.h>
#include <range/v3/view.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility> // for pair
using namespace boost::beast;
using namespace boost::asio;
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using tcp_acceptor = use_awaitable_t<>::as_default_on_t<tcp::acceptor>;

Server::Server (boost::asio::io_context &ioContext_, boost::asio::thread_pool &pool_) : ioContext{ ioContext_ }, pool{ pool_ } {}

boost::asio::awaitable<void>
Server::userMatchmaking (boost::asio::ip::tcp::endpoint const &endpoint, std::filesystem::path const &pathToSecrets, MatchmakingOption const &matchmakingOption)
{
  try
    {
      auto executor = co_await this_coro::executor;
      tcp_acceptor acceptor (executor, endpoint);
      net::ssl::context ctx (net::ssl::context::tls_server);
      ctx.set_verify_mode (ssl::context::verify_peer);
      ctx.set_default_verify_paths ();
      try
        {
          ctx.use_certificate_chain_file (pathToSecrets / "fullchain.pem");
        }
      catch (std::exception const &e)
        {
          std::cout << "load fullchain: " << pathToSecrets / "fullchain.pem"
                    << " exception : " << e.what () << std::endl;
        }
      try
        {
          ctx.use_private_key_file (pathToSecrets / "privkey.pem", boost::asio::ssl::context::pem);
        }
      catch (std::exception const &e)
        {
          std::cout << "load privkey: " << pathToSecrets / "privkey.pem"
                    << " exception : " << e.what () << std::endl;
        }
      try
        {
          ctx.use_tmp_dh_file (pathToSecrets / "dh2048.pem");
        }
      catch (std::exception const &e)
        {
          std::cout << "load dh2048: " << pathToSecrets / "dh2048.pem"
                    << " exception : " << e.what () << std::endl;
        }
      boost::certify::enable_native_https_server_verification (ctx);
      ctx.set_options (SSL_SESS_CACHE_OFF | SSL_OP_NO_TICKET); //  disable ssl cache. It has a bad support in boost asio/beast and I do not know if it helps in performance in our usecase
      for (;;)
        {
          try
            {
              typedef boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> SSLWebsocket;
              auto socket = co_await acceptor.async_accept ();
              auto connection = std::make_shared<SSLWebsocket> (SSLWebsocket{ std::move (socket), ctx });
              connection->set_option (websocket::stream_base::timeout::suggested (role_type::server));
              connection->set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
              co_await connection->next_layer ().async_handshake (ssl::stream_base::server, use_awaitable);
              co_await connection->async_accept (use_awaitable);
              static size_t id = 0;
              auto myWebsocket = std::make_shared<MyWebsocket<SSLWebsocket>> (MyWebsocket<SSLWebsocket>{ connection, "userMatchmaking", fmt::fg (fmt::color::red), std::to_string (id++) });
              matchmakings.emplace_back (Matchmaking{ MatchmakingData{ ioContext, matchmakings, [myWebsocket] (std::string message) { myWebsocket->sendMessage (std::move (message)); }, gameLobbies, pool, matchmakingOption } });
              std::list<Matchmaking>::iterator matchmaking = std::prev (matchmakings.end ());
              using namespace boost::asio::experimental::awaitable_operators;
              co_spawn (ioContext, myWebsocket->readLoop ([matchmaking, myWebsocket] (const std::string &msg) {
                if (matchmaking->hasProxyToGame ())
                  {
                    matchmaking->sendMessageToGame (msg);
                  }
                else
                  {
                    if (auto const &error = matchmaking->processEvent (msg))
                      {
                        myWebsocket->sendMessage (objectToStringWithObjectName (user_matchmaking::UnhandledEventError{ msg, error.value () }));
                      }
                  }
              }) && myWebsocket->writeLoop (),
                        [&matchmakings = matchmakings, matchmaking] (auto eptr) {
                          printException (eptr);
                          matchmakings.erase (matchmaking);
                        });
            }
          catch (std::exception const &e)
            {
              std::cout << "Server::userMatchmaking () connect  Exception : " << e.what () << std::endl;
            }
        }
    }
  catch (std::exception const &e)
    {
      std::cout << "exception: " << e.what () << std::endl;
    }
}

boost::asio::awaitable<void>
Server::gameMatchmaking (boost::asio::ip::tcp::endpoint const &endpoint)
{
  try
    {
      auto executor = co_await this_coro::executor;
      tcp_acceptor acceptor (executor, endpoint);
      for (;;)
        {
          try
            {
              auto socket = co_await acceptor.async_accept ();
              typedef boost::beast::websocket::stream<boost::asio::use_awaitable_t<>::as_default_on_t<boost::beast::tcp_stream>> Websocket;
              auto connection = std::make_shared<Websocket> (Websocket{ std::move (socket) });
              connection->set_option (websocket::stream_base::timeout::suggested (role_type::server));
              connection->set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
              co_await connection->async_accept ();
              static size_t id = 0;
              auto myWebsocket = std::make_shared<MyWebsocket<Websocket>> (MyWebsocket<Websocket>{ connection, "gameMatchmaking", fmt::fg (fmt::color::blue_violet), std::to_string (id++) });
              using namespace boost::asio::experimental::awaitable_operators;
              co_await(myWebsocket->readLoop ([myWebsocket, &matchmakings = matchmakings] (const std::string &msg) {
                auto matchmakingGame = MatchmakingGame{ matchmakings, [myWebsocket] (std::string const &msg) { myWebsocket->sendMessage (msg); } };
                matchmakingGame.process_event (msg);
              }) || myWebsocket->writeLoop ());
            }
          catch (std::exception const &e)
            {
              std::cout << "Server::gameMatchmaking () connect  Exception : " << e.what () << std::endl;
            }
        }
    }
  catch (std::exception const &e)
    {
      std::cout << "exception: " << e.what () << std::endl;
    }
}
