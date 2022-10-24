#ifndef C7A94DA0_E145_4EDE_9976_E448E01FA587
#define C7A94DA0_E145_4EDE_9976_E448E01FA587

#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <durak/gameData.hxx>
#include <durak/gameOption.hxx>
#include <optional>
#include <string>

typedef std::vector<std::pair<std::string, long long int>> UserTimeMilliseconds;

namespace shared_class
{
enum struct TimerType
{
  noTimer,
  resetTimeOnNewRound,
  addTimeOnNewRound
};
}
BOOST_FUSION_DEFINE_STRUCT ((shared_class), DurakTimers, (shared_class::TimerType, timerType) (UserTimeMilliseconds, runningTimeUserTimePointMilliseconds) (UserTimeMilliseconds, pausedTimeUserDurationMilliseconds))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), TimerOption, (shared_class::TimerType, timerType) (uint64_t, timeAtStartInSeconds) (uint64_t, timeForEachRoundInSeconds))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), GameOption, (durak::GameOption, gameOption) (shared_class::TimerOption, timerOption)) // TODO-TEMPLATE add game options

std::optional<std::string> inline errorInGameOption (shared_class::GameOption const &)
{
  // TODO-TEMPLATE check Game option
  return std::nullopt;
}

#endif /* C7A94DA0_E145_4EDE_9976_E448E01FA587 */
