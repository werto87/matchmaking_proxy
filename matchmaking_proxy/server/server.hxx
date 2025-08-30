#ifndef AD140436_3FBA_4D63_8C0E_9113B92859E0
#define AD140436_3FBA_4D63_8C0E_9113B92859E0

#include "matchmaking_proxy/logic/matchmaking.hxx" // for Match...
#include "matchmaking_proxy/server/gameLobby.hxx"  // for GameL...
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

namespace matchmaking_proxy
{
struct MatchmakingGameData;
struct MatchmakingOption;
class Server
{
public:
  Server (boost::asio::io_context &ioContext_, boost::asio::thread_pool &pool_, boost::asio::ip::tcp::endpoint const& userMatchmakingEndpoint, boost::asio::ip::tcp::endpoint const& gameMatchmakingEndpoint);
  ~Server ();
  boost::asio::awaitable<void> userMatchmaking (std::filesystem::path pathToChainFile, std::filesystem::path pathToPrivateFile, std::filesystem::path pathToTmpDhFile, std::filesystem::path fullPathIncludingDatabaseName, std::chrono::seconds pollingSleepTimer, MatchmakingOption matchmakingOption, std::string gameHost, std::string gamePort, std::string userGameViaMatchmakingPort, bool sslContextVerifyNone = false);
  boost::asio::awaitable<void> gameMatchmaking (std::filesystem::path fullPathIncludingDatabaseName, std::function<void (std::string const &type ,std::string const &message,  MatchmakingGameData &matchmakingGameData)> handleCustomMessageFromGame = {});
  boost::asio::io_context &ioContext;
  boost::asio::thread_pool &pool;
  std::list<std::shared_ptr<Matchmaking>> matchmakings{};

  std::unique_ptr<boost::asio::ip::tcp::acceptor> userMatchmakingAcceptor{};
  std::unique_ptr<boost::asio::ip::tcp::acceptor> gameMatchmakingAcceptor{};
};
}
#endif /* AD140436_3FBA_4D63_8C0E_9113B92859E0 */
