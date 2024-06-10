#ifndef EFFCC19D_EF93_4BD9_B516_6E5932A5ECA0
#define EFFCC19D_EF93_4BD9_B516_6E5932A5ECA0

#include <functional>
#include <list>
#include <memory>
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
  MatchmakingGame (std::list<std::shared_ptr<Matchmaking>> &stateMachines_, std::function<void (std::string const &)> sendToGame);

  void process_event (std::string const &event);

  std::unique_ptr<StateMachineWrapper, StateMachineWrapperDeleter> sm; // only use this member inside of ".cxx". reason because of incomplete type
};
}
#endif /* EFFCC19D_EF93_4BD9_B516_6E5932A5ECA0 */
