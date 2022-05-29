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
struct MatchmakingOption;

class Server
{
public:
  Server (boost::asio::io_context &ioContext_, boost::asio::thread_pool &pool_);

  boost::asio::awaitable<void> userMatchmaking (boost::asio::ip::tcp::endpoint userEndpoint, std::filesystem::path const &pathToSecrets, MatchmakingOption const &matchmakingOption, boost::asio::ip::tcp::endpoint matchmakingGameEndpoint, boost::asio::ip::tcp::endpoint userGameViaMatchmakingEndpoint);
  boost::asio::awaitable<void> gameMatchmaking (boost::asio::ip::tcp::endpoint endpoint);

  boost::asio::io_context &ioContext;
  boost::asio::thread_pool &pool;
  std::list<Matchmaking> matchmakings{};
};

#endif /* AD140436_3FBA_4D63_8C0E_9113B92859E0 */
