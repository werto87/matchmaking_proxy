#ifndef C02CDB99_AA83_45B0_83E7_8C8BC254A8A2
#define C02CDB99_AA83_45B0_83E7_8C8BC254A8A2

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

BOOST_FUSION_DEFINE_STRUCT ((shared_class), JoinChannel, (std::string, channel))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), JoinChannelSuccess, (std::string, channel))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), JoinChannelError, (std::string, channel) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), CreateAccount, (std::string, accountName) (std::string, password))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), CreateAccountCancel, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), CreateAccountSuccess, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), CreateAccountError, (std::string, accountName) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LoginAccount, (std::string, accountName) (std::string, password))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LoginAccountCancel, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LoginAccountSuccess, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LoginAccountError, (std::string, accountName) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LogoutAccount, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LogoutAccountSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LogoutAccountError, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), BroadCastMessage, (std::string, channel) (std::string, message))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), BroadCastMessageSuccess, (std::string, channel) (std::string, message))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), BroadCastMessageError, (std::string, channel) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveChannel, (std::string, channel))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveChannelSuccess, (std::string, channel))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveChannelError, (std::string, channel) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), Message, (std::string, fromAccount) (std::string, channel) (std::string, message))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), CreateGameLobby, (std::string, name) (std::string, password))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), CreateGameLobbySuccess, (std::string, name))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), CreateGameLobbyError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), JoinGameLobby, (std::string, name) (std::string, password))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), JoinGameLobbySuccess, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), JoinGameLobbyError, (std::string, name) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), GameOptionError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), UserInGameLobby, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), UsersInGameLobby, (std::string, name) (std::vector<shared_class::UserInGameLobby>, users) (size_t, maxUserSize) (shared_class::GameOption, durakGameOption))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), SetMaxUserSizeInCreateGameLobby, (size_t, maxUserSize))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), SetMaxUserSizeInCreateGameLobbyError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), MaxUserSizeInCreateGameLobby, (size_t, maxUserSize))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveGame, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveGameServer, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveGameSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveGameError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveGameLobby, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveGameLobbySuccess, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveGameLobbyError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), WantToRelog, (std::string, accountName) (std::string, destination))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), RelogTo, (bool, wantsToRelog))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), RelogToCreateGameLobbySuccess, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), RelogToGameSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), RelogToError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), CreateGame, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), CreateGameError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), StartGame, (std::vector<std::string>, players) (shared_class::GameOption, gameOption))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), StartGameError, (std::string, msg))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), JoinMatchMakingQueue, (bool, isRanked))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), JoinMatchMakingQueueSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), JoinMatchMakingQueueError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), AskIfUserWantsToJoinGame, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), AskIfUserWantsToJoinGameTimeOut, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), GameStartCanceled, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), GameStartCanceledRemovedFromQueue, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), WantsToJoinGame, (bool, answer))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), WantsToJoinGameError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveQuickGameQueue, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveQuickGameQueueSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LeaveQuickGameQueueError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LoginAsGuest, )
BOOST_FUSION_DEFINE_STRUCT ((shared_class), LoginAsGuestSuccess, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), RatingChanged, (size_t, oldRating) (size_t, newRating))
BOOST_FUSION_DEFINE_STRUCT ((shared_class), UnhandledMessageError, (std::string, msg) (std::string, error))

// clang-format off
namespace shared_class{
    // TODO-TEMPLATE add new type to handle in server and client here
static boost::hana::tuple<
CreateAccount,
LoginAccount,
LoginAsGuest,
CreateAccountCancel,
LoginAccountCancel,
JoinChannel,
BroadCastMessage,
LeaveChannel,
LogoutAccount,
CreateGameLobby,
JoinGameLobby,
SetMaxUserSizeInCreateGameLobby,
GameOption,
LeaveGame,
LeaveGameLobby,
RelogTo,
CreateGame,
WantsToJoinGame,
LeaveQuickGameQueue,
JoinMatchMakingQueue
  >  const sharedClasses{};
}
// clang-format on

// clang-format off
namespace shared_class{
    // TODO-TEMPLATE add new type to handle in server and client here
static boost::hana::tuple<
StartGame
  >  const blacklistClientToServer{};
}
// clang-format on

#endif /* C02CDB99_AA83_45B0_83E7_8C8BC254A8A2 */
