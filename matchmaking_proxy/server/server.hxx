#ifndef AD140436_3FBA_4D63_8C0E_9113B92859E0
#define AD140436_3FBA_4D63_8C0E_9113B92859E0

#include "../database/database.hxx"
#include "../logic/client.hxx"
#include "gameLobby.hxx"
#include "user.hxx"
#include <algorithm>
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/type_index.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstddef>
#include <filesystem>
#include <list>
#include <memory>
#include <queue>
#include <string>
#ifdef BOOST_ASIO_HAS_CLANG_LIBCXX
#include <experimental/coroutine>
#endif
#include <iostream>
#include <memory>
#include <openssl/ssl.h>
#include <sodium.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

using namespace boost::beast;
using namespace boost::asio;
namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using tcp_acceptor = use_awaitable_t<>::as_default_on_t<tcp::acceptor>;

class Server
{
public:
  Server (boost::asio::io_context &io_context, boost::asio::thread_pool &pool) : _io_context{ io_context }, _pool{ pool } {}

  awaitable<void>
  listener (boost::asio::ip::tcp::endpoint const &endpoint, std::filesystem::path const &pathToSecrets)
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
        catch (std::exception &e)
          {
            std::cout << "load fullchain: " << pathToSecrets / "fullchain.pem"
                      << " exception : " << e.what () << std::endl;
          }
        try
          {
            ctx.use_private_key_file (pathToSecrets / "privkey.pem", boost::asio::ssl::context::pem);
          }
        catch (std::exception &e)
          {
            std::cout << "load privkey: " << pathToSecrets / "privkey.pem"
                      << " exception : " << e.what () << std::endl;
          }
        try
          {
            ctx.use_tmp_dh_file (pathToSecrets / "dh2048.pem");
          }
        catch (std::exception &e)
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
                auto socket = co_await acceptor.async_accept ();
                auto connection = std::make_shared<SSLWebsocket> (SSLWebsocket{ std::move (socket), ctx });
                users.emplace_back (std::make_shared<User> (User{}));
                // TODO make user a member of state machine to fight the problems with circular dependencies
                std::list<std::shared_ptr<User>>::iterator user = std::next (users.end (), -1);
                connection->set_option (websocket::stream_base::timeout::suggested (role_type::server));
                connection->set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
                co_await connection->next_layer ().async_handshake (ssl::stream_base::server, use_awaitable);
                co_await connection->async_accept (use_awaitable);
                // TODO instead of detached we can try to remove the state machine for matchmaking
                // TODO read from client
                // co_spawn (
                //     executor, [connection, this, &user] () mutable { return readFromClient (user, *connection); }, detached);
                co_spawn (
                    executor, [connectionWeakPointer = std::weak_ptr<SSLWebsocket>{ connection }, &user] () mutable { return user->get ()->writeToClient (connectionWeakPointer); }, detached);
              }
            catch (std::exception &e)
              {
                std::cout << "Server::listener () connect  Exception : " << e.what () << std::endl;
              }
          }
      }
    catch (std::exception &e)
      {
        std::cout << "exception: " << e.what () << std::endl;
      }
  }

  boost::asio::io_context &_io_context;
  boost::asio::thread_pool &_pool;
  //   TODO instead of users we can hold the matchmaking statemachine which is the new user and has the connection and the logic
  //  we could move the connections into the state machine and so we have a simpler user!!!!
  std::list<std::shared_ptr<User>> users{};
  std::list<GameLobby> gameLobbies{};
};

#endif /* AD140436_3FBA_4D63_8C0E_9113B92859E0 */
