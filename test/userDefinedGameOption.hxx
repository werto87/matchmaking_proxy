#ifndef C7A94DA0_E145_4EDE_9976_E448E01FA587
#define C7A94DA0_E145_4EDE_9976_E448E01FA587

#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <optional>
#include <string>

BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), GameOption, (bool, someBool) (std::string, someString)) // TODO-TEMPLATE add game options

std::optional<std::string> inline errorInGameOption (user_matchmaking::GameOption const &)
{
  // TODO-TEMPLATE check Game option
  return std::nullopt;
}

#endif /* C7A94DA0_E145_4EDE_9976_E448E01FA587 */
