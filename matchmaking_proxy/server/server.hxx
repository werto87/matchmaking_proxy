#ifndef AD140436_3FBA_4D63_8C0E_9113B92859E0
#define AD140436_3FBA_4D63_8C0E_9113B92859E0

#include "gameLobby.hxx"                           // for GameL...
#include "matchmaking_proxy/logic/matchmaking.hxx" // for Match...
#include <boost/asio/awaitable.hpp>                // for await...
#include <boost/asio/ip/tcp.hpp>                   // for tcp
#include <filesystem>                              // for path
#include <list>                                    // for list
#include <memory>                                  // for share...

namespace boost::asio
{

class thread_pool;
class io_context;
}

struct User;

class Server
{
public:
  Server (boost::asio::io_context &io_context, boost::asio::thread_pool &pool);

  boost::asio::awaitable<void> listener (boost::asio::ip::tcp::endpoint const &endpoint, std::filesystem::path const &pathToSecrets);

  boost::asio::io_context &_io_context;
  boost::asio::thread_pool &_pool;
  std::list<Matchmaking> matchmakings{};
  std::list<GameLobby> gameLobbies{};
};

#endif /* AD140436_3FBA_4D63_8C0E_9113B92859E0 */
