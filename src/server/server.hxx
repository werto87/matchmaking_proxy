#ifndef AD140436_3FBA_4D63_8C0E_9113B92859E0
#define AD140436_3FBA_4D63_8C0E_9113B92859E0

#include "../database/database.hxx"
#include "gameLobby.hxx"
#include "user.hxx"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <cstddef>
#include <list>
#include <memory>
#include <queue>
#include <string>

class Server
{
public:
  Server (boost::asio::io_context &io_context, boost::asio::thread_pool &pool);

  boost::asio::awaitable<void> listener (boost::asio::ip::tcp::endpoint const &endpoint, std::filesystem::path const &pathToSecrets);

private:
  void removeUser (std::list<std::shared_ptr<User>>::iterator user);
  boost::asio::awaitable<std::string> my_read (SSLWebsocket &ws_);
  boost::asio::awaitable<void> readFromClient (std::list<std::shared_ptr<User>>::iterator user, SSLWebsocket &connection);

  boost::asio::io_context &_io_context;
  boost::asio::thread_pool &_pool;
  std::list<std::shared_ptr<User>> users{};
  std::list<GameLobby> gameLobbies{};
};

#endif /* AD140436_3FBA_4D63_8C0E_9113B92859E0 */
