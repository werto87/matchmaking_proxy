#ifndef C774F1C4_44FA_4B2F_9526_46C43EFDB937
#define C774F1C4_44FA_4B2F_9526_46C43EFDB937

#include "matchmaking_proxy/server/myWebsocket.hxx"
#include "matchmaking_proxy/server/user.hxx"
#include <boost/beast/websocket.hpp>
#include <list>
#include <matchmaking_proxy/server/matchmakingOption.hxx>
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

  typedef boost::asio::use_awaitable_t<>::as_default_on_t<boost::asio::basic_waitable_timer<boost::asio::chrono::system_clock>> CoroTimer;
  std::unique_ptr<CoroTimer> cancelCoroutineTimer{ std::make_unique<CoroTimer> (CoroTimer{ ioContext }) };
  std::list<std::shared_ptr<Matchmaking>> &stateMachines;
  std::function<void (std::string const &msg)> sendMsgToUser{};
  User user{};
  std::list<GameLobby> &gameLobbies;
  boost::asio::thread_pool &pool;
  typedef boost::beast::websocket::stream<boost::asio::use_awaitable_t<>::as_default_on_t<boost::beast::tcp_stream>> Websocket;
  MyWebsocket<Websocket> matchmakingGame{ {} };
  MatchmakingOption matchmakingOption{};
  boost::asio::ip::tcp::endpoint matchmakingGameEndpoint{};
  boost::asio::ip::tcp::endpoint userGameViaMatchmakingEndpoint{};
};
}
#endif /* C774F1C4_44FA_4B2F_9526_46C43EFDB937 */
