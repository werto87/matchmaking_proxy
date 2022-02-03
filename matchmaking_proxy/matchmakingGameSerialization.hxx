#ifndef F7525A03_A98E_4EEF_964B_E02274116B7D
#define F7525A03_A98E_4EEF_964B_E02274116B7D

#include PATH_TO_USER_DEFINED_GAME_OPTION
#include <boost/algorithm/string.hpp>
#include <boost/fusion/adapted/struct/adapt_struct.hpp>
#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <boost/fusion/algorithm/query/count.hpp>
#include <boost/fusion/functional.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/algorithm.hpp>
#include <boost/fusion/include/at.hpp>
#include <boost/fusion/include/count.hpp>
#include <boost/fusion/include/define_struct.hpp>
#include <boost/fusion/sequence/intrinsic/at.hpp>
#include <boost/fusion/sequence/intrinsic_fwd.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/find.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
#include <boost/json.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/range_c.hpp>
#include <cstddef>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <variant>

BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), UnhandledMessageError, (std::string, msg) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), StartGame, (std::vector<std::string>, players) (user_matchmaking::GameOption, gameOption) (bool, ratedGame))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), StartGameError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), StartGameSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), LeaveGameServer, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), LeaveGameSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), LeaveGameError, )
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), GameOver, (bool, ratedGame) (std::vector<std::string>, winners) (std::vector<std::string>, losers) (std::vector<std::string>, draws))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), GameOverSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), GameOverError, )
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), UserLeftGame, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), UserLeftGameSuccess, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), UserLeftGameError, (std::string, accountName) (std::string, error))

// clang-format off
namespace matchmaking_game{
static boost::hana::tuple<
  UnhandledMessageError,
  StartGame,
  StartGameError,
  StartGameSuccess,
  LeaveGameServer,
  LeaveGameSuccess,
  LeaveGameError,
  GameOver
  >  const matchmakingGame{};
}
// clang-format on

#endif /* F7525A03_A98E_4EEF_964B_E02274116B7D */
