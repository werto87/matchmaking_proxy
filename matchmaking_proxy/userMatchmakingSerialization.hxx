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

BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), JoinChannel, (std::string, channel))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), JoinChannelSuccess, (std::string, channel))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), JoinChannelError, (std::string, channel) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), CreateAccount, (std::string, accountName) (std::string, password))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), CreateAccountCancel, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), CreateAccountSuccess, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), CreateAccountError, (std::string, accountName) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LoginAccount, (std::string, accountName) (std::string, password))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LoginAccountCancel, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LoginAccountSuccess, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LoginAccountError, (std::string, accountName) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LogoutAccount, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LogoutAccountSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LogoutAccountError, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), BroadCastMessage, (std::string, channel) (std::string, message))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), BroadCastMessageSuccess, (std::string, channel) (std::string, message))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), BroadCastMessageError, (std::string, channel) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveChannel, (std::string, channel))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveChannelSuccess, (std::string, channel))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveChannelError, (std::string, channel) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), Message, (std::string, fromAccount) (std::string, channel) (std::string, message))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), CreateGameLobby, (std::string, name) (std::string, password))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), CreateGameLobbySuccess, (std::string, name))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), CreateGameLobbyError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), JoinGameLobby, (std::string, name) (std::string, password))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), JoinGameLobbySuccess, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), JoinGameLobbyError, (std::string, name) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), GameOptionError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), UserInGameLobby, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), UsersInGameLobby, (std::string, name) (std::vector<user_matchmaking::UserInGameLobby>, users) (size_t, maxUserSize) (shared_class::GameOption, durakGameOption))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), SetMaxUserSizeInCreateGameLobby, (size_t, maxUserSize))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), SetMaxUserSizeInCreateGameLobbyError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), MaxUserSizeInCreateGameLobby, (size_t, maxUserSize))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveGame, )
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_game), LeaveGame, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveGameSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveGameError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveGameLobby, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveGameLobbySuccess, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveGameLobbyError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), WantToRelog, (std::string, accountName) (std::string, destination))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), RelogTo, (bool, wantsToRelog))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), RelogToCreateGameLobbySuccess, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), RelogToGameSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), RelogToError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), CreateGame, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), CreateGameError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), StartGame, (std::vector<std::string>, players) (shared_class::GameOption, gameOption))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), StartGameError, (std::string, msg))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), JoinMatchMakingQueue, (bool, isRanked))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), JoinMatchMakingQueueSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), JoinMatchMakingQueueError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), AskIfUserWantsToJoinGame, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), AskIfUserWantsToJoinGameTimeOut, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), GameStartCanceled, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), GameStartCanceledRemovedFromQueue, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), WantsToJoinGame, (bool, answer))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), WantsToJoinGameError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveQuickGameQueue, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveQuickGameQueueSuccess, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LeaveQuickGameQueueError, (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LoginAsGuest, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), LoginAsGuestSuccess, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), RatingChanged, (size_t, oldRating) (size_t, newRating))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), UnhandledMessageError, (std::string, msg) (std::string, error))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), SendMessageToGame, (std::string, msg))
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), ProxyStarted, )
BOOST_FUSION_DEFINE_STRUCT ((user_matchmaking), ProxyStopped, )

// clang-format off
namespace user_matchmaking{
static boost::hana::tuple<
JoinChannel,
JoinChannelSuccess,
JoinChannelError,
CreateAccount,
CreateAccountCancel,
CreateAccountSuccess,
CreateAccountError,
LoginAccount,
LoginAccountCancel,
LoginAccountSuccess,
LoginAccountError,
LogoutAccount,
LogoutAccountSuccess,
LogoutAccountError,
BroadCastMessage,
BroadCastMessageSuccess,
BroadCastMessageError,
LeaveChannel,
LeaveChannelSuccess,
LeaveChannelError,
Message,
CreateGameLobby,
CreateGameLobbySuccess,
CreateGameLobbyError,
JoinGameLobby,
JoinGameLobbySuccess,
JoinGameLobbyError,
GameOptionError,
UserInGameLobby,
UsersInGameLobby,
SetMaxUserSizeInCreateGameLobby,
SetMaxUserSizeInCreateGameLobbyError,
MaxUserSizeInCreateGameLobby,
LeaveGame,
LeaveGameSuccess,
LeaveGameError,
LeaveGameLobby,
LeaveGameLobbySuccess,
LeaveGameLobbyError,
WantToRelog,
RelogTo,
RelogToCreateGameLobbySuccess,
RelogToGameSuccess,
RelogToError,
CreateGame,
CreateGameError,
StartGame,
StartGameError,
JoinMatchMakingQueue,
JoinMatchMakingQueueSuccess,
JoinMatchMakingQueueError,
AskIfUserWantsToJoinGame,
AskIfUserWantsToJoinGameTimeOut,
GameStartCanceled,
GameStartCanceledRemovedFromQueue,
WantsToJoinGame,
WantsToJoinGameError,
LeaveQuickGameQueue,
LeaveQuickGameQueueSuccess,
LeaveQuickGameQueueError,
LoginAsGuest,
LoginAsGuestSuccess,
RatingChanged,
UnhandledMessageError,
SendMessageToGame
  >  const  userMatchmaking{};
}
// clang-format on

#endif /* C02CDB99_AA83_45B0_83E7_8C8BC254A8A2 */
