#ifndef C7A94DA0_E145_4EDE_9976_E448E01FA587
#define C7A94DA0_E145_4EDE_9976_E448E01FA587

#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <optional>
#include <string>
/*
 * These values are automatically set according to their cmake variables.
 */
BOOST_FUSION_DEFINE_STRUCT ((shared_class), GameOption, )

std::optional<std::string> inline errorInGameOption (shared_class::GameOption const &)
{
  // TODO-TEMPLATE check Game option
  return std::nullopt;
}

#endif /* C7A94DA0_E145_4EDE_9976_E448E01FA587 */
