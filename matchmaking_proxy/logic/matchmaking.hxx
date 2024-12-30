#ifndef AB446319_39F6_4D7F_9EC5_860337CA5001
#define AB446319_39F6_4D7F_9EC5_860337CA5001

#include <expected>
#include <memory>
#include <string>
#include <vector>
namespace matchmaking_proxy
{
struct MatchmakingData;

class Matchmaking
{
  struct StateMachineWrapper;
  struct StateMachineWrapperDeleter
  {
    void operator() (StateMachineWrapper *p);
  };

public:
  explicit Matchmaking (MatchmakingData &&matchmakingData);

  [[nodiscard]] std::expected<void, std::string> processEvent (std::string const &event);

  void sendMessageToGame (std::string const &message);

  bool isLoggedInWithAccountName (std::string const &accountName) const;

  bool isUserInChatChannel (std::string const &channelName) const;

  void disconnectFromProxy ();

  bool hasProxyToGame () const;

  void cleanUp ();

  void proccessSendTopRatedPlayersToUser ();

  std::vector<std::string> currentStatesAsString () const;

  std::unique_ptr<StateMachineWrapper, StateMachineWrapperDeleter> sm; // only use this member inside of matchmaking.cxx. reason because of incomplete type
};
}
#endif /* AB446319_39F6_4D7F_9EC5_860337CA5001 */
