#ifndef AB446319_39F6_4D7F_9EC5_860337CA5001
#define AB446319_39F6_4D7F_9EC5_860337CA5001

#include "matchmaking_proxy/server/matchmakingOption.hxx"
#include <functional>
#include <list>
#include <memory>
namespace boost::asio
{
class io_context;
class thread_pool;
}

struct GameLobby;

class Matchmaking
{
  struct StateMachineWrapper;
  struct StateMachineWrapperDeleter
  {
    void operator() (StateMachineWrapper *p);
  };

public:
  Matchmaking (boost::asio::io_context &ioContext, std::list<Matchmaking> &stateMachines_, std::function<void (std::string const &msg)> sendMsgToUser, std::list<GameLobby> &gameLobbies, boost::asio::thread_pool &pool, MatchmakingOption const &matchmakingOption);

  void process_event (std::string const &event);

  void sendMessageToGame (std::string const &message);

  bool isLoggedInWithAccountName (std::string const &accountName) const;

  bool isUserInChatChannel (std::string const &channelName) const;

  bool hasProxyToGame () const;

  void disconnectFromProxy ();

  std::vector<std::string> currentStatesAsString () const;

  std::string stateMachineAsString () const;

  std::unique_ptr<StateMachineWrapper, StateMachineWrapperDeleter> sm; // only use this member inside of matchmaking.cxx. reason because of incomplete type
};

#endif /* AB446319_39F6_4D7F_9EC5_860337CA5001 */
