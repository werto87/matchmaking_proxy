#ifndef C774F1C4_44FA_4B2F_9526_46C43EFDB937
#define C774F1C4_44FA_4B2F_9526_46C43EFDB937

#include "matchmaking_proxy/server/user.hxx"
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/websocket.hpp>
#include <list>
#include <matchmaking_proxy/server/matchmakingOption.hxx>
#include <my_web_socket/myWebSocket.hxx>
namespace matchmaking_proxy
{
class Matchmaking;
struct GameLobby;
struct MatchmakingData
{
  MatchmakingData (boost::asio::io_context &ioContext_, std::list<std::shared_ptr<Matchmaking>> &stateMachines_, std::function<void (std::string const &msg)> sendMsgToUser_, std::list<GameLobby> &gameLobbies_, boost::asio::thread_pool &pool_, MatchmakingOption const &matchmakingOption_, boost::asio::ip::tcp::endpoint const &matchmakingGameEndpoint_, boost::asio::ip::tcp::endpoint const &userGameViaMatchmakingEndpoint_);

  boost::asio::awaitable<std::optional<boost::system::system_error>> cancelCoroutine ();
  void cancelAndResetTimer ();

  boost::asio::io_context &ioContext;
  std::unique_ptr<my_web_socket::CoroTimer> cancelCoroutineTimer{ std::make_unique<my_web_socket::CoroTimer> (my_web_socket::CoroTimer{ ioContext }) };
  std::list<std::shared_ptr<Matchmaking>> &stateMachines;
  std::function<void (std::string const &msg)> sendMsgToUser{};
  User user{};
  std::list<GameLobby> &gameLobbies;
  boost::asio::thread_pool &pool;
  my_web_socket::MyWebSocket<my_web_socket::WebSocket> matchmakingGame{};
  MatchmakingOption matchmakingOption{};
  boost::asio::ip::tcp::endpoint matchmakingGameEndpoint{};
  boost::asio::ip::tcp::endpoint userGameViaMatchmakingEndpoint{};
};
}
#endif /* C774F1C4_44FA_4B2F_9526_46C43EFDB937 */
