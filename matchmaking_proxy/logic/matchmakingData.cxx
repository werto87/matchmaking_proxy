#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include <iostream>
#include <syncstream>
namespace matchmaking_proxy
{
MatchmakingData::MatchmakingData (boost::asio::io_context &ioContext_, std::list<std::weak_ptr<Matchmaking>> &stateMachines_, std::function<void (std::string const &msg)> sendMsgToUser_, std::shared_ptr<std::list<GameLobby>> &gameLobbies_, boost::asio::thread_pool &pool_, MatchmakingOption const &matchmakingOption_, boost::asio::ip::tcp::endpoint const &matchmakingGameEndpoint_, boost::asio::ip::tcp::endpoint const &userGameViaMatchmakingEndpoint_, std::filesystem::path const &fullPathIncludingDatabaseName_) : ioContext{ ioContext_ }, stateMachines{ stateMachines_ }, sendMsgToUser{ sendMsgToUser_ }, gameLobbies{ gameLobbies_ }, pool{ pool_ }, matchmakingOption{ matchmakingOption_ }, matchmakingGameEndpoint{ matchmakingGameEndpoint_ }, userGameViaMatchmakingEndpoint{ userGameViaMatchmakingEndpoint_ }, fullPathIncludingDatabaseName{ fullPathIncludingDatabaseName_ } { cancelCoroutineTimer->expires_at (std::chrono::system_clock::time_point::max ()); }

boost::asio::awaitable<std::optional<boost::system::system_error>>
MatchmakingData::cancelCoroutine ()
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
          std::osyncstream (std::cout) << "error in timer boost::system::errc: " << e.code () << std::endl;
          abort ();
        }
      co_return e;
    }
}
void
MatchmakingData::cancelAndResetTimer ()
{
  cancelCoroutineTimer->expires_at (std::chrono::system_clock::time_point::max ());
}
}