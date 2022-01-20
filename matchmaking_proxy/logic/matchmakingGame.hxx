#ifndef EFFCC19D_EF93_4BD9_B516_6E5932A5ECA0
#define EFFCC19D_EF93_4BD9_B516_6E5932A5ECA0

#include <memory>

namespace boost::asio
{
class io_context;
class thread_pool;
}

class MatchmakingGame
{
  struct StateMachineWrapper;
  struct StateMachineWrapperDeleter
  {
    void operator() (StateMachineWrapper *p);
  };

public:
  void process_event (std::string const &event);

  std::unique_ptr<StateMachineWrapper, StateMachineWrapperDeleter> sm; // only use this member inside of matchmaking.cxx. reason because of incomplete type
};

#endif /* EFFCC19D_EF93_4BD9_B516_6E5932A5ECA0 */
