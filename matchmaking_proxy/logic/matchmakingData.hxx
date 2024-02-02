#ifndef C774F1C4_44FA_4B2F_9526_46C43EFDB937
#define C774F1C4_44FA_4B2F_9526_46C43EFDB937

#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/server/matchmakingOption.hxx"
#include "matchmaking_proxy/server/myWebsocket.hxx"
#include "matchmaking_proxy/server/user.hxx"
#include <boost/asio/ip/tcp.hpp>

struct MatchmakingData
{
  MatchmakingData (boost::asio::io_context &ioContext_, std::list<std::shared_ptr<Matchmaking>> &stateMachines_, std::function<void (std::string const &msg)> sendMsgToUser_, std::list<GameLobby> &gameLobbies_, boost::asio::thread_pool &pool_, MatchmakingOption const &matchmakingOption_, boost::asio::ip::tcp::endpoint const &matchmakingGameEndpoint_, boost::asio::ip::tcp::endpoint const &userGameViaMatchmakingEndpoint_) : ioContext{ ioContext_ }, stateMachines{ stateMachines_ }, sendMsgToUser{ sendMsgToUser_ }, gameLobbies{ gameLobbies_ }, pool{ pool_ }, matchmakingOption{ matchmakingOption_ }, matchmakingGameEndpoint{ matchmakingGameEndpoint_ }, userGameViaMatchmakingEndpoint{ userGameViaMatchmakingEndpoint_ } { cancelCoroutineTimer->expires_at (std::chrono::system_clock::time_point::max ()); }

  boost::asio::awaitable<std::optional<boost::system::system_error>>
  cancelCoroutine ()
  {
    try
      {
        co_await cancelCoroutineTimer->async_wait ();
        co_return std::optional<boost::system::system_error>{};
      }
    catch (boost::system::system_error &e)
      {
        using namespace boost::system::errc;
        if (operation_canceled == e.code ())
          {
          }
        else
          {
            std::cout << "error in timer boost::system::errc: " << e.code () << std::endl;
            abort ();
          }
        co_return e;
      }
  }
  void
  cancelAndResetTimer ()
  {
    cancelCoroutineTimer->expires_at (std::chrono::system_clock::time_point::max ());
  }

  boost::asio::io_context &ioContext;
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

#endif /* C774F1C4_44FA_4B2F_9526_46C43EFDB937 */
