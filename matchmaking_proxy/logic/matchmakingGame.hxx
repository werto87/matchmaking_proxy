#ifndef EFFCC19D_EF93_4BD9_B516_6E5932A5ECA0
#define EFFCC19D_EF93_4BD9_B516_6E5932A5ECA0

#include <functional>
#include <list>
#include <memory>
#include <string>
#include "matchmaking_proxy/logic/matchmakingGameData.hxx"
namespace matchmaking_proxy
{
class Matchmaking;

class MatchmakingGame
{
  struct StateMachineWrapper;
  struct StateMachineWrapperDeleter
  {
    void operator() (StateMachineWrapper *p) const;
  };

public:
  MatchmakingGame (MatchmakingGameData matchmakingGameData);

  void process_event (std::string const &event);

  std::unique_ptr<StateMachineWrapper, StateMachineWrapperDeleter> sm; // only use this member inside of ".cxx". reason because of incomplete type
};
}
#endif /* EFFCC19D_EF93_4BD9_B516_6E5932A5ECA0 */
