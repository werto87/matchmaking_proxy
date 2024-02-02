#ifndef F913C042_CAFF_4558_AE02_952AE3C4F17A
#define F913C042_CAFF_4558_AE02_952AE3C4F17A

#include "logic/matchmaking.hxx"
#include "server/gameLobby.hxx"
#include "util.hxx"
#include <iostream> // for operator<<, ostream
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <vector> // for allocator
template <typename T, template <typename ELEM, typename ALLOC = std::allocator<ELEM>> class Container>
std::ostream &
operator<< (std::ostream &o, const Container<T> &container)
{
  typename Container<T>::const_iterator beg = container.begin ();
  while (beg != container.end ())
    {
      o << "\n" << *beg++; // 2
    }
  return o;
}

std::shared_ptr<Matchmaking> createAccountAndJoinMatchmakingGame (std::string const &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<std::shared_ptr<Matchmaking>> &matchmakings, boost::asio::thread_pool &pool, user_matchmaking::JoinMatchMakingQueue const &joinMatchMakingQueue, int &proxyStartedCalled);

#endif /* F913C042_CAFF_4558_AE02_952AE3C4F17A */
