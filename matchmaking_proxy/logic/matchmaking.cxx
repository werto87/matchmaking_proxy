#include "matchmaking.hxx"
#include "../server/myWebsocket.hxx"
#include "confu_json/confu_json.hxx"
#include "matchmaking_proxy/database/constant.hxx"
#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/rating.hxx"
#include "matchmaking_proxy/matchmakingGameSerialization.hxx"
#include "matchmaking_proxy/pw_hash/passwordHash.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/userMatchmakingSerialization.hxx"
#include "matchmaking_proxy/util.hxx"
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
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
#include <boost/sml.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cassert>
#include <chrono>
#include <confu_json/concept.hxx>
#include <confu_json/util.hxx>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <cstddef>
#include <fmt/color.h>
#include <functional> // for __base
#include <iostream>   // for string
#include <iostream>
#include <map>
#include <memory>
#include <range/v3/algorithm/copy_if.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/view.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/remove_if.hpp>
#include <set>
#include <sstream> // for basic_...
#include <sstream>
#include <stdexcept>
#include <string> // for operat...
#include <string>
#include <type_traits>
#include <utility> // for pair
#include <variant>
#include <vector> // for vector
typedef boost::asio::use_awaitable_t<>::as_default_on_t<boost::asio::basic_waitable_timer<boost::asio::chrono::system_clock>> CoroTimer;
using namespace boost::sml;
typedef boost::beast::websocket::stream<boost::asio::use_awaitable_t<>::as_default_on_t<boost::beast::tcp_stream>> Websocket;

template <typename T> concept hasAccountName = requires(T t) { t.accountName; };

// work around to print type for debuging
template <typename> struct Debug;
// Debug<SomeType> d;

struct MatchmakingData;

void sendToAllAccountsInUsersCreateGameLobby (std::string const &message, MatchmakingData &matchmakingData);

boost::asio::awaitable<void> wantsToJoinGame (user_matchmaking::WantsToJoinGame wantsToJoinGameEv, MatchmakingData &matchmakingData);

struct NotLoggedIn
{
};

BOOST_FUSION_DEFINE_STRUCT ((), PasswordHashed, (std::string, accountName) (std::string, hashedPassword))

BOOST_FUSION_DEFINE_STRUCT ((), PasswordMatches, (std::string, accountName))
struct WaitingForPasswordHashed
{
};

struct WaitingForPasswordCheck
{
};

struct WaitingForUserWantsToRelogGameLobby
{
};
BOOST_FUSION_DEFINE_STRUCT ((), ProxyToGame, )

struct LoggedIn
{
};

struct ConnectedWithGame
{
};

BOOST_FUSION_DEFINE_STRUCT ((), SendMessageToUser, (std::string, msg))

struct ReceiveMessage
{
};

BOOST_FUSION_DEFINE_STRUCT ((), PasswordDoesNotMatch, )
BOOST_FUSION_DEFINE_STRUCT ((), ConnectionToGameLost, )

struct User
{
  std::string accountName{};
  std::set<std::string> communicationChannels{};
};

struct MatchmakingData
{
  MatchmakingData (boost::asio::io_context &ioContext_, std::list<Matchmaking> &stateMachines_, std::function<void (std::string const &msg)> sendMsgToUser_, std::list<GameLobby> &gameLobbies_, boost::asio::thread_pool &pool_, MatchmakingOption const &matchmakingOption_) : ioContext{ ioContext_ }, stateMachines{ stateMachines_ }, sendMsgToUser{ sendMsgToUser_ }, gameLobbies{ gameLobbies_ }, pool{ pool_ }, matchmakingOption{ matchmakingOption_ } { cancelCoroutineTimer->expires_at (std::chrono::system_clock::time_point::max ()); }

  boost::asio::awaitable<std::optional<boost::system::system_error>>
  cancelCoroutine ()
  {
    try
      {
        co_await cancelCoroutineTimer->async_wait ();
        co_return std::optional<boost::system::system_error>{};
      }
    catch (boost::system::system_error &e)
      {
        using namespace boost::system::errc;
        if (operation_canceled == e.code ())
          {
          }
        else
          {
            std::cout << "error in timer boost::system::errc: " << e.code () << std::endl;
            abort ();
          }
        co_return e;
      }
  }
  void
  cancelAndResetTimer ()
  {
    cancelCoroutineTimer->expires_at (std::chrono::system_clock::time_point::max ());
  }

  boost::asio::io_context &ioContext;
  std::unique_ptr<CoroTimer> cancelCoroutineTimer{ std::make_unique<CoroTimer> (CoroTimer{ ioContext }) };
  std::list<Matchmaking> &stateMachines;
  std::function<void (std::string const &msg)> sendMsgToUser{};
  User user{};
  std::list<GameLobby> &gameLobbies;
  boost::asio::thread_pool &pool;
  MyWebsocket<Websocket> matchmakingGame{ {} };
  MatchmakingOption matchmakingOption{};
};

boost::asio::awaitable<void> startGame (GameLobby const &gameLobby, MatchmakingData &matchmakingData);

template <typename Matchmaking, typename Event>
void
process_event (Matchmaking &matchmaking, Event const &event)
{
  matchmaking.sm->impl.process_event (event);
}

boost::asio::awaitable<void>
doHashPassword (user_matchmaking::CreateAccount createAccountObject, auto &sm, auto &&deps, auto &&subs)
{
  auto &matchmakingData = aux::get<MatchmakingData &> (deps);
  using namespace boost::asio::experimental::awaitable_operators;
  std::variant<std::string, std::optional<boost::system::system_error>> hashedPw = co_await(async_hash (matchmakingData.pool, matchmakingData.ioContext, createAccountObject.password, boost::asio::use_awaitable) || matchmakingData.cancelCoroutine ());
  if (std::holds_alternative<std::string> (hashedPw))
    {
      sm.process_event (PasswordHashed{ createAccountObject.accountName, std::get<std::string> (hashedPw) }, deps, subs);
    }
}

boost::asio::awaitable<void>
doCheckPassword (auto loginAccountObject, auto &&sm, auto &&deps, auto &&subs)
{
  auto &matchmakingData = aux::get<MatchmakingData &> (deps);
  soci::session sql (soci::sqlite3, databaseName);
  auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName);
  using namespace boost::asio::experimental::awaitable_operators;
  auto passwordMatches = co_await(async_check_hashed_pw (matchmakingData.pool, matchmakingData.ioContext, account->password, loginAccountObject.password, boost::asio::use_awaitable) || matchmakingData.cancelCoroutine ());
  if (std::holds_alternative<bool> (passwordMatches))
    {
      if (std::get<bool> (passwordMatches))
        {
          sm.process_event (PasswordMatches{ loginAccountObject.accountName }, deps, subs);
        }
      else
        {
          sm.process_event (PasswordDoesNotMatch{}, deps, subs);
          co_return;
        }
    }
}

boost::asio::awaitable<void>
connectToGame (matchmaking_game::ConnectToGame connectToGameEv, auto &&sm, auto &&deps, auto &&subs)
{
  auto &matchmakingData = aux::get<MatchmakingData &> (deps);
  auto ws = std::make_shared<Websocket> (Websocket{ matchmakingData.ioContext });
  auto gameEndpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 };
  try
    {
      co_await ws->next_layer ().async_connect (gameEndpoint);
      ws->next_layer ().expires_never ();
      ws->set_option (boost::beast::websocket::stream_base::timeout::suggested (boost::beast::role_type::client));
      ws->set_option (boost::beast::websocket::stream_base::decorator ([] (boost::beast::websocket::request_type &req) { req.set (boost::beast::http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async"); }));
      co_await ws->async_handshake ("localhost:" + std::to_string (gameEndpoint.port ()), "/");
      static size_t id = 0;
      matchmakingData.matchmakingGame = MyWebsocket<Websocket>{ std::move (ws), "connectToGame", fmt::fg (fmt::color::cadet_blue), std::to_string (id++) };
      co_await matchmakingData.matchmakingGame.async_write_one_message (objectToStringWithObjectName (connectToGameEv));
      auto connectToGameResult = co_await matchmakingData.matchmakingGame.async_read_one_message ();
      std::vector<std::string> splitMesssage{};
      boost::algorithm::split (splitMesssage, connectToGameResult, boost::is_any_of ("|"));
      if (splitMesssage.size () == 2)
        {
          auto const &typeToSearch = splitMesssage.at (0);
          auto const &objectAsString = splitMesssage.at (1);
          bool typeFound = false;
          boost::hana::for_each (matchmaking_game::matchmakingGame, [&] (const auto &x) {
            if (typeToSearch == confu_json::type_name<typename std::decay<decltype (x)>::type> ())
              {
                typeFound = true;
                boost::json::error_code ec{};
                sm.process_event (confu_json::to_object<std::decay_t<decltype (x)>> (confu_json::read_json (objectAsString, ec)), deps, subs);
                if (ec) std::cout << "read_json error: " << ec.message () << std::endl;
                return;
              }
          });
          if (not typeFound) std::cout << "could not find a match for typeToSearch in matchmakingGame '" << typeToSearch << "'" << std::endl;
        }
      using namespace boost::asio::experimental::awaitable_operators;
      co_await(matchmakingData.matchmakingGame.readLoop ([&matchmakingData] (std::string const &readResult) { matchmakingData.sendMsgToUser (readResult); }) || matchmakingData.matchmakingGame.writeLoop ());
    }
  catch (std::exception const &e)
    {
      std::cout << "exception: " << e.what ();
      sm.process_event (user_matchmaking::ConnectGameError{ e.what () }, deps, subs);
      throw e;
    }
}

auto hashPassword = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (aux::get<MatchmakingData &> (deps).ioContext, doHashPassword (event, sm, deps, subs), printException); };
auto checkPassword = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (aux::get<MatchmakingData &> (deps).ioContext, doCheckPassword (event, sm, deps, subs), printException); };
auto const wantsToJoinAGameWrapper = [] (user_matchmaking::WantsToJoinGame const &wantsToJoinGameEv, MatchmakingData &matchmakingData) { co_spawn (matchmakingData.ioContext, wantsToJoinGame (wantsToJoinGameEv, matchmakingData), printException); };
auto doConnectToGame = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void {
  //
  boost::asio::co_spawn (aux::get<MatchmakingData &> (deps).ioContext, connectToGame (event, sm, deps, subs), printException);
};

bool
isInRatingrange (size_t userRating, size_t lobbyAverageRating, size_t allowedRatingDifference)
{
  auto const difference = userRating > lobbyAverageRating ? userRating - lobbyAverageRating : lobbyAverageRating - userRating;
  return difference < allowedRatingDifference;
}

bool
checkRating (size_t userRating, std::vector<std::string> const &accountNames, size_t allowedRatingDifference)
{
  return isInRatingrange (userRating, averageRating (accountNames), allowedRatingDifference);
}

bool
matchingLobby (std::string const &accountName, GameLobby const &gameLobby, GameLobby::LobbyType const &lobbyType, size_t allowedRatingDifference)
{
  if (gameLobby.lobbyAdminType == lobbyType && gameLobby.accountNames.size () < gameLobby.maxUserCount ())
    {
      if (lobbyType == GameLobby::LobbyType::MatchMakingSystemRanked)
        {
          soci::session sql (soci::sqlite3, databaseName);
          if (auto userInDatabase = confu_soci::findStruct<database::Account> (sql, "accountName", accountName))
            {
              return checkRating (userInDatabase->rating, gameLobby.accountNames, allowedRatingDifference);
            }
        }
      else
        {
          return true;
        }
    }
  else
    {
      return false;
    }
  return false;
}

auto const sendToUser = [] (SendMessageToUser const &sendMessageToUser, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (sendMessageToUser.msg); };
auto const leaveGame = [] (MatchmakingData &matchmakingData) { matchmakingData.matchmakingGame.close (); };

auto const leaveMatchMakingQueue = [] (MatchmakingData &matchmakingData) {
  if (auto userGameLobby = ranges::find_if (matchmakingData.gameLobbies, [accountName = matchmakingData.user.accountName] (auto const &gameLobby) { return gameLobby.lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers && ranges::find_if (gameLobby.accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != gameLobby.accountNames.end (); }); userGameLobby != matchmakingData.gameLobbies.end ())
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveQuickGameQueueSuccess{}));
      userGameLobby->removeUser (matchmakingData.user.accountName);
      userGameLobby->cancelTimer ();
      if (userGameLobby->accountNames.empty ())
        {
          matchmakingData.gameLobbies.erase (userGameLobby);
        }
      else
        {
          sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}), matchmakingData);
        }
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveQuickGameQueueError{ "User is not in queue" }));
    }
};

auto const logoutAccount = [] (MatchmakingData &matchmakingData) {
  if (auto gameLobbyWithAccount = ranges::find_if (matchmakingData.gameLobbies,
                                                   [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames;
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != matchmakingData.gameLobbies.end ())
    {
      gameLobbyWithAccount->removeUser (matchmakingData.user.accountName);
      if (gameLobbyWithAccount->accountNames.empty ())
        {
          if (gameLobbyWithAccount->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers)
            {
              auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
              usersInGameLobby.name = gameLobbyWithAccount->name.value ();
              usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
              ranges::transform (gameLobbyWithAccount->accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
              sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (usersInGameLobby), matchmakingData);
            }
          else
            {
              leaveMatchMakingQueue (matchmakingData);
            }
        }
      else
        {
          matchmakingData.gameLobbies.erase (gameLobbyWithAccount);
        }
    }
  matchmakingData.user = {};
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LogoutAccountSuccess{}));
};

auto const cancelCreateAccount = [] (MatchmakingData &matchmakingData) {
  matchmakingData.cancelAndResetTimer ();
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountCancel{}));
};

auto const joinChannel = [] (user_matchmaking::JoinChannel const &joinChannelObject, MatchmakingData &matchmakingData) {
  matchmakingData.user.communicationChannels.insert (joinChannelObject.channel);
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinChannelSuccess{ joinChannelObject.channel }));
};

auto const joinGameLobby = [] (user_matchmaking::JoinGameLobby const &joinGameLobbyObject, MatchmakingData &matchmakingData) {
  if (auto gameLobby = ranges::find_if (matchmakingData.gameLobbies, [gameLobbyName = joinGameLobbyObject.name, lobbyPassword = joinGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName && _gameLobby.password == lobbyPassword; }); gameLobby != matchmakingData.gameLobbies.end ())
    {
      if (auto error = gameLobby->tryToAddUser (matchmakingData.user))
        {
          matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ joinGameLobbyObject.name, error.value () }));
          return;
        }
      else
        {
          matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbySuccess{}));
          auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobby->maxUserCount ();
          usersInGameLobby.name = gameLobby->name.value ();
          usersInGameLobby.durakGameOption = gameLobby->gameOption;
          ranges::transform (gameLobby->accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
          sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (usersInGameLobby), matchmakingData);
          return;
        }
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ joinGameLobbyObject.name, "wrong password name combination or lobby does not exists" }));
      return;
    }
};

auto const relogToGameLobby = [] (MatchmakingData &matchmakingData) {
  if (auto gameLobbyWithAccount = ranges::find_if (matchmakingData.gameLobbies,
                                                   [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames;
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != matchmakingData.gameLobbies.end ())
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToCreateGameLobbySuccess{}));
      auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
      usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
      usersInGameLobby.name = gameLobbyWithAccount->name.value ();
      usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
      ranges::transform (gameLobbyWithAccount->accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (usersInGameLobby));
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
    }
};

auto const leaveGameLobby = [] (MatchmakingData &matchmakingData) {
  auto gameLobbyWithAccount = ranges::find_if (matchmakingData.gameLobbies, [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
    auto const &accountNames = gameLobby.accountNames;
    return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
  });
  gameLobbyWithAccount->removeUser (matchmakingData.user.accountName);
  if (gameLobbyWithAccount->accountCount () == 0)
    {
      matchmakingData.gameLobbies.erase (gameLobbyWithAccount);
    }
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbySuccess{}));
};

auto const setGameOption = [] (shared_class::GameOption const &gameOption, MatchmakingData &matchmakingData) {
  if (auto gameLobbyWithAccount = ranges::find_if (matchmakingData.gameLobbies,
                                                   [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames;
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != matchmakingData.gameLobbies.end ())
    {
      if (gameLobbyWithAccount->getWaitingForAnswerToStartGame ())
        {
          matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "It is not allowed to change game option while ask to start a game is running" }));
        }
      else
        {
          if (gameLobbyWithAccount->isGameLobbyAdmin (matchmakingData.user.accountName))
            {
              gameLobbyWithAccount->gameOption = gameOption;
              sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (gameOption), matchmakingData);
              return;
            }
          else
            {
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "you need to be admin in the create game lobby to change game option" }));
              return;
            }
        }
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "could not find a game lobby for account" }));
      return;
    }
};

auto const setMaxUserSizeInCreateGameLobby = [] (user_matchmaking::SetMaxUserSizeInCreateGameLobby const &setMaxUserSizeInCreateGameLobbyObject, MatchmakingData &matchmakingData) {
  if (auto gameLobbyWithAccount = ranges::find_if (matchmakingData.gameLobbies,
                                                   [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames;
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != matchmakingData.gameLobbies.end ())
    {
      if (gameLobbyWithAccount->getWaitingForAnswerToStartGame ())
        {
          matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "It is not allowed to change lobby while ask to start a game is running" }));
        }
      else
        {
          if (gameLobbyWithAccount->isGameLobbyAdmin (matchmakingData.user.accountName))
            {
              if (auto errorMessage = gameLobbyWithAccount->setMaxUserCount (setMaxUserSizeInCreateGameLobbyObject.maxUserSize))
                {
                  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ errorMessage.value () }));
                  return;
                }
              else
                {
                  sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (user_matchmaking::MaxUserSizeInCreateGameLobby{ setMaxUserSizeInCreateGameLobbyObject.maxUserSize }), matchmakingData);
                  return;
                }
            }
          else
            {
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "you need to be admin in a game lobby to change the user size" }));
              return;
            }
        }
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "could not find a game lobby for account" }));
      return;
    }
};

auto const createGameLobby = [] (user_matchmaking::CreateGameLobby const &createGameLobbyObject, MatchmakingData &matchmakingData) {
  if (ranges::find_if (matchmakingData.gameLobbies, [gameLobbyName = createGameLobbyObject.name, lobbyPassword = createGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName; }) == matchmakingData.gameLobbies.end ())
    {
      if (auto gameLobbyWithUser = ranges::find_if (matchmakingData.gameLobbies,
                                                    [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                      auto const &accountNames = gameLobby.accountNames;
                                                      return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                    });
          gameLobbyWithUser != matchmakingData.gameLobbies.end ())
        {
          matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameLobbyError{ { "account has already a game lobby with the name: " + gameLobbyWithUser->name.value_or ("Quick Game Lobby") } }));
          return;
        }
      else
        {
          auto &newGameLobby = matchmakingData.gameLobbies.emplace_back (GameLobby{ createGameLobbyObject.name, createGameLobbyObject.password });
          if (newGameLobby.tryToAddUser (matchmakingData.user))
            {
              throw std::logic_error{ "user can not join lobby which he created" };
            }
          else
            {
              auto result = std::vector<std::string>{};
              auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = newGameLobby.maxUserCount ();
              usersInGameLobby.name = newGameLobby.name.value ();
              usersInGameLobby.durakGameOption = newGameLobby.gameOption;
              ranges::transform (newGameLobby.accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbySuccess{}));
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (usersInGameLobby));
              return;
            }
        }
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameLobbyError{ { "lobby already exists with name: " + createGameLobbyObject.name } }));
      return;
    }
};

void sendMessageToUsers (std::string const &message, std::vector<std::string> const &accountNames, MatchmakingData &matchmakingData);

auto const askUsersToJoinGame = [] (std::list<GameLobby>::iterator &gameLobby, MatchmakingData &matchmakingData) {
  if (gameLobby->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers)
    {
      sendMessageToUsers (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGame{}), gameLobby->accountNames | ranges::views::drop (1 /*drop first user because he is the admin and started createGame*/) | ranges::to<std::vector<std::string>> (), matchmakingData);
      gameLobby->readyUsers.push_back (gameLobby->accountNames.at (0));
    }
  else
    {
      sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGame{}), matchmakingData);
    }

  gameLobby->startTimerToAcceptTheInvite (matchmakingData.ioContext, [gameLobby, &matchmakingData] () {
    auto notReadyUsers = std::vector<std::string>{};
    ranges::copy_if (gameLobby->accountNames, ranges::back_inserter (notReadyUsers), [usersWhichAccepted = gameLobby->readyUsers] (std::string const &accountNamesGamelobby) mutable { return ranges::find_if (usersWhichAccepted, [accountNamesGamelobby] (std::string const &userWhoAccepted) { return accountNamesGamelobby == userWhoAccepted; }) == usersWhichAccepted.end (); });
    sendMessageToUsers (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGameTimeOut{}), notReadyUsers, matchmakingData);
    for (auto const &notReadyUser : notReadyUsers)
      {
        if (gameLobby->lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers)
          {
            gameLobby->removeUser (notReadyUser);
          }
      }
    if (gameLobby->accountNames.empty ())
      {
        matchmakingData.gameLobbies.erase (gameLobby);
      }
    else
      {
        gameLobby->readyUsers.clear ();
        sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}), matchmakingData);
      }
  });
};

auto const createGame = [] (MatchmakingData &matchmakingData) {
  if (auto gameLobbyWithUser = ranges::find_if (matchmakingData.gameLobbies,
                                                [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                  auto const &accountNames = gameLobby.accountNames;
                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                });
      gameLobbyWithUser != matchmakingData.gameLobbies.end ())
    {
      if (gameLobbyWithUser->getWaitingForAnswerToStartGame ())
        {
          matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "It is not allowed to start a game while ask to start a game is running" }));
        }
      else
        {
          if (gameLobbyWithUser->isGameLobbyAdmin (matchmakingData.user.accountName))
            {
              if (gameLobbyWithUser->accountNames.size () >= 2)
                {
                  if (auto gameOptionError = errorInGameOption (gameLobbyWithUser->gameOption))
                    {
                      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ gameOptionError.value () }));
                    }
                  else
                    {
                      askUsersToJoinGame (gameLobbyWithUser, matchmakingData);
                    }
                }
              else
                {
                  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "You need atleast two user to create a game" }));
                }
            }
          else
            {
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "you need to be admin in a game lobby to start a game" }));
            }
        }
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "Could not find a game lobby for the user" }));
    }
};

auto const leaveChannel = [] (user_matchmaking::LeaveChannel const &leaveChannelObject, MatchmakingData &matchmakingData) {
  if (matchmakingData.user.communicationChannels.erase (leaveChannelObject.channel))
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveChannelSuccess{ leaveChannelObject.channel }));
      return;
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveChannelError{ leaveChannelObject.channel, { "channel not found" } }));
      return;
    }
};

auto const broadCastMessage = [] (user_matchmaking::BroadCastMessage const &broadCastMessageObject, MatchmakingData &matchmakingData) {
  for (Matchmaking &matchmaking : matchmakingData.stateMachines | ranges::views::filter ([&chatChannel = broadCastMessageObject.channel] (Matchmaking const &matchmaking) { return matchmaking.isUserInChatChannel (chatChannel); }))
    {
      process_event (matchmaking, user_matchmaking::Message{ matchmakingData.user.accountName, broadCastMessageObject.channel, broadCastMessageObject.message });
    }
};

auto const cancelLoginAccount = [] (MatchmakingData &matchmakingData) {
  matchmakingData.cancelAndResetTimer ();
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountCancel{}));
};

auto const createAccount = [] (PasswordHashed const &passwordHash, MatchmakingData &matchmakingData) {
  matchmakingData.user.accountName = passwordHash.accountName;
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountSuccess{ passwordHash.accountName }));
  database::createAccount (matchmakingData.user.accountName, passwordHash.hashedPassword);
};

boost::asio::awaitable<std::string>
sendStartGameToServer (GameLobby const &gameLobby, MatchmakingData &matchmakingData)
{
  auto ws = std::make_shared<Websocket> (matchmakingData.ioContext);
  auto gameEndpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 };
  co_await ws->next_layer ().async_connect (gameEndpoint);
  ws->next_layer ().expires_never ();
  ws->set_option (boost::beast::websocket::stream_base::timeout::suggested (boost::beast::role_type::client));
  ws->set_option (boost::beast::websocket::stream_base::decorator ([] (boost::beast::websocket::request_type &req) { req.set (boost::beast::http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async"); }));
  co_await ws->async_handshake ("localhost:" + std::to_string (gameEndpoint.port ()), "/");
  static size_t id = 0;
  auto myWebsocket = MyWebsocket<Websocket>{ std::move (ws), "sendStartGameToServer", fmt::fg (fmt::color::cornflower_blue), std::to_string (id++) };
  auto startGame = matchmaking_game::StartGame{};
  startGame.players = gameLobby.accountNames;
  startGame.gameOption = gameLobby.gameOption;
  startGame.ratedGame = gameLobby.lobbyAdminType == GameLobby::LobbyType::MatchMakingSystemRanked;
  co_await myWebsocket.async_write_one_message (objectToStringWithObjectName (startGame));
  auto msg = co_await myWebsocket.async_read_one_message ();
  co_return msg;
}

boost::asio::awaitable<void>
wantsToJoinGame (user_matchmaking::WantsToJoinGame wantsToJoinGameEv, MatchmakingData &matchmakingData)
{
  if (auto userGameLobby = ranges::find_if (matchmakingData.gameLobbies,
                                            [accountName = matchmakingData.user.accountName] (GameLobby const &gameLobby) {
                                              auto const &accountNames = gameLobby.accountNames;
                                              return gameLobby.getWaitingForAnswerToStartGame () && ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                            });
      userGameLobby != matchmakingData.gameLobbies.end ())
    {
      if (wantsToJoinGameEv.answer)
        {
          if (ranges::find_if (userGameLobby->readyUsers, [accountName = matchmakingData.user.accountName] (std::string const &readyUserAccountName) { return readyUserAccountName == accountName; }) == userGameLobby->readyUsers.end ())
            {
              userGameLobby->readyUsers.push_back (matchmakingData.user.accountName);
              if (userGameLobby->readyUsers.size () == userGameLobby->accountNames.size ())
                {
                  try
                    {
                      co_await startGame (*userGameLobby, matchmakingData);
                      matchmakingData.gameLobbies.erase (userGameLobby);
                    }
                  catch (std::exception const &e)
                    {
                      userGameLobby->cancelTimer ();
                      sendMessageToUsers (objectToStringWithObjectName (user_matchmaking::StartGameError{ "Can not connect to game: " + std::string{ e.what () } }), userGameLobby->accountNames, matchmakingData);
                      sendMessageToUsers (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}), userGameLobby->accountNames, matchmakingData);
                    }
                }
            }
          else
            {
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::WantsToJoinGameError{ "You already accepted to join the game" }));
            }
        }
      else
        {
          userGameLobby->cancelTimer ();
          if (userGameLobby->lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers)
            {
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameStartCanceledRemovedFromQueue{}));
              userGameLobby->removeUser (matchmakingData.user.accountName);
              sendMessageToUsers (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}), userGameLobby->accountNames, matchmakingData);
              if (userGameLobby->accountNames.empty ())
                {
                  matchmakingData.gameLobbies.erase (userGameLobby);
                }
            }
        }
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::WantsToJoinGameError{ "No game to join" }));
    }
}

auto const removeUserFromGameLobby = [] (MatchmakingData &matchmakingData) {
  if (auto gameLobbyWithAccount = ranges::find_if (matchmakingData.gameLobbies,
                                                   [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames;
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != matchmakingData.gameLobbies.end ())
    {
      gameLobbyWithAccount->removeUser (matchmakingData.user.accountName);
      if (gameLobbyWithAccount->accountCount () == 0)
        {
          matchmakingData.gameLobbies.erase (gameLobbyWithAccount);
        }
      else
        {
          auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
          usersInGameLobby.name = gameLobbyWithAccount->name.value ();
          usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
          ranges::transform (gameLobbyWithAccount->accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
          sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (usersInGameLobby), matchmakingData);
        }
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
    }
};

auto const joinMatchMakingQueue = [] (user_matchmaking::JoinMatchMakingQueue const &joinMatchMakingQueueEv, MatchmakingData &matchmakingData) {
  auto lobbyType = (joinMatchMakingQueueEv.isRanked) ? GameLobby::LobbyType::MatchMakingSystemRanked : GameLobby::LobbyType::MatchMakingSystemUnranked;
  if (ranges::find_if (matchmakingData.gameLobbies, [accountName = matchmakingData.user.accountName] (auto const &gameLobby) { return ranges::find_if (gameLobby.accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != gameLobby.accountNames.end (); }) == matchmakingData.gameLobbies.end ())
    {
      if (auto gameLobbyToAddUser = ranges::find_if (matchmakingData.gameLobbies, [lobbyType = (joinMatchMakingQueueEv.isRanked) ? GameLobby::LobbyType::MatchMakingSystemRanked : GameLobby::LobbyType::MatchMakingSystemUnranked, accountName = matchmakingData.user.accountName, &matchmakingData] (GameLobby const &gameLobby) { return matchingLobby (accountName, gameLobby, lobbyType, matchmakingData.matchmakingOption.allowedRatingDifference); }); gameLobbyToAddUser != matchmakingData.gameLobbies.end ())
        {
          if (auto error = gameLobbyToAddUser->tryToAddUser (matchmakingData.user))
            {
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ matchmakingData.user.accountName, error.value () }));
            }
          else
            {
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueSuccess{}));
              if (gameLobbyToAddUser->accountNames.size () == gameLobbyToAddUser->maxUserCount ())
                {
                  askUsersToJoinGame (gameLobbyToAddUser, matchmakingData);
                }
            }
        }
      else
        {
          auto gameLobby = GameLobby{};
          if (auto error = gameLobby.setMaxUserCount ((lobbyType == GameLobby::LobbyType::MatchMakingSystemUnranked) ? matchmakingData.matchmakingOption.usersNeededToStartQuickGame : matchmakingData.matchmakingOption.usersNeededToStartRankedGame))
            {
              throw std::logic_error{ "Configuration Error. Please check MatchmakingOption. Error: " + error.value () };
            }
          gameLobby.lobbyAdminType = lobbyType;
          if (auto error = gameLobby.tryToAddUser (matchmakingData.user))
            {
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ matchmakingData.user.accountName, error.value () }));
            }
          matchmakingData.gameLobbies.emplace_back (gameLobby);
          matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueSuccess{}));
        }
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueError{ "User is allready in gamelobby" }));
    }
};

auto const loginAsGuest = [] (MatchmakingData &matchmakingData) {
  matchmakingData.user.accountName = boost::uuids::to_string (boost::uuids::random_generator () ());
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAsGuestSuccess{ matchmakingData.user.accountName }));
};

auto const accountInDatabase = [] (auto const &typeWithAccountName) -> bool {
  soci::session sql (soci::sqlite3, databaseName);
  return confu_soci::findStruct<database::Account> (sql, "accountName", typeWithAccountName.accountName).has_value ();
};

auto const wantsToRelog = [] (user_matchmaking::RelogTo const &relogTo) -> bool { return relogTo.wantsToRelog; };

std::string
getAccountName (auto const &typeWithAccountName, MatchmakingData &matchmakingData)
{
  if constexpr (hasAccountName<typename std::decay<decltype (typeWithAccountName)>::type>)
    {
      return typeWithAccountName.accountName;
    }
  else
    {
      return matchmakingData.user.accountName;
    }
}

auto const userInGameLobby = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) -> bool {
  return ranges::find_if (matchmakingData.gameLobbies,
                          [accountName = getAccountName (typeWithAccountName, matchmakingData)] (auto const &gameLobby) {
                            auto const &accountNames = gameLobby.accountNames;
                            return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                          })
         != matchmakingData.gameLobbies.end ();
};

auto const alreadyLoggedIn = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) -> bool { return ranges::find (matchmakingData.stateMachines, true, [accountName = typeWithAccountName.accountName] (const Matchmaking &matchmaking) { return matchmaking.isLoggedInWithAccountName (accountName); }) != matchmakingData.stateMachines.end (); };

auto const gameLobbyControlledByUsers = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) -> bool {
  auto userGameLobby = ranges::find_if (matchmakingData.gameLobbies, [accountName = getAccountName (typeWithAccountName, matchmakingData)] (auto const &gameLobby) {
    auto const &accountNames = gameLobby.accountNames;
    return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
  });
  return userGameLobby->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers;
};

auto const loginAccountErrorPasswordAccountName = [] (user_matchmaking::LoginAccount const &loginAccount, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ loginAccount.accountName, "Incorrect Username or Password" })); };
auto const loginAccountSuccess = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) {
  matchmakingData.user.accountName = typeWithAccountName.accountName;
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountSuccess{ matchmakingData.user.accountName }));
};
auto const createAccountErrorAccountAlreadyCreated = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountError{ typeWithAccountName.accountName, "Account already Created" })); };
auto const proxyStarted = [] (MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::ProxyStarted{})); };
auto const proxyStopped = [] (MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::ProxyStopped{})); };
auto const loginAccountErrorAccountAlreadyLoggedIn = [] (user_matchmaking::LoginAccount const &loginAccount, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ loginAccount.accountName, "Account already logged in" })); };
auto const wantsToRelogToGameLobby = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::WantToRelog{ typeWithAccountName.accountName, "Create Game Lobby" })); };
auto const connectToGameError = [] (user_matchmaking::ConnectGameError const &connectGameError, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (connectGameError)); };
auto const leaveGameLobbyErrorUserNotInGameLobby = [] (MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbyError{ "could not remove user from lobby user not found in lobby" })); };
auto const leaveGameLobbyErrorControlledByMatchmaking = [] (MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbyError{ "not allowed to leave a game lobby which is controlled by the matchmaking system with leave game lobby" })); };
auto const sendMessageToUser = [] (user_matchmaking::Message const &message, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (message)); };
// TODO find a way to inform user if he sends something which does not get handled
// TODO this could be usefull for printing not handled events
// auto const stateCanNotHandleEvent = [] (auto const &event, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::UnhandledEventError{ "event not handled: '" + confu_json::type_name<typename std::decay<std::remove_cvref_t<decltype (event)>>> () + "'" })); };

// TODO make it build with gcc
class StateMachineImpl
{
public:
  auto
  operator() () const noexcept
  {
    namespace u_m = user_matchmaking;
    namespace m_g = matchmaking_game;
    namespace s_c = shared_class;
    // clang-format off
  return make_transition_table(
// NotLoggedIn-----------------------------------------------------------------------------------------------------------------------------------------------------------------
* state<NotLoggedIn>                          + event<u_m::CreateAccount>                                                         / hashPassword                            = state<WaitingForPasswordHashed>
, state<NotLoggedIn>                          + event<u_m::LoginAsGuest>                                                          / loginAsGuest                            = state<LoggedIn>
, state<NotLoggedIn>                          + event<u_m::LoginAccount>                   [ not accountInDatabase ]              / loginAccountErrorPasswordAccountName 
, state<NotLoggedIn>                          + event<u_m::LoginAccount>                                                          / checkPassword                           = state<WaitingForPasswordCheck>
// WaitingForPasswordHashed---------------------------------------------------------------------------------------------------------------------------------------------------
, state<WaitingForPasswordHashed>             + event<PasswordHashed>                      [ accountInDatabase ]                  / createAccountErrorAccountAlreadyCreated = state<NotLoggedIn>
, state<WaitingForPasswordHashed>             + event<PasswordHashed>                                                             / createAccount                           = state<LoggedIn>
, state<WaitingForPasswordHashed>             + event<u_m::CreateAccountCancel>                                                   / cancelCreateAccount                     = state<NotLoggedIn>
// WaitingForPasswordCheck-----------------------------------------------------------------------------------------------------------------------------------------------------------
, state<WaitingForPasswordCheck>              + event<PasswordMatches>                     [ alreadyLoggedIn ]                    / loginAccountErrorAccountAlreadyLoggedIn = state<WaitingForUserWantsToRelogGameLobby>
, state<WaitingForPasswordCheck>              + event<PasswordMatches>                     [ userInGameLobby ]                    / wantsToRelogToGameLobby                 = state<WaitingForUserWantsToRelogGameLobby>
, state<WaitingForPasswordCheck>              + event<PasswordMatches>                                                            / loginAccountSuccess                     = state<LoggedIn>
, state<WaitingForPasswordCheck>              + event<u_m::LoginAccountCancel>                                                    / cancelLoginAccount                      = state<NotLoggedIn>
, state<WaitingForPasswordCheck>              + event<PasswordDoesNotMatch>                                                       / loginAccountErrorPasswordAccountName    = state<NotLoggedIn>
// WaitingForUserWantsToRelogGameLobby------------------------------------------------------------------------------------------------------------------------------------------------------------------
, state<WaitingForUserWantsToRelogGameLobby>  + event<u_m::RelogTo>                        [ wantsToRelog ]                       / relogToGameLobby                        = state<LoggedIn>
, state<WaitingForUserWantsToRelogGameLobby>  + event<u_m::RelogTo>                                                               / removeUserFromGameLobby                 = state<LoggedIn>
// LoggedIn---------------------------------------------------------------------------------------------------------------------------------------------------------------------
, state<LoggedIn>                             + event<u_m::CreateAccount>                                                         / (logoutAccount,hashPassword)            = state<WaitingForPasswordHashed>
, state<LoggedIn>                             + event<u_m::LoginAccount>                                                          / (logoutAccount,checkPassword)           = state<WaitingForPasswordCheck>
, state<LoggedIn>                             + event<u_m::JoinChannel>                                                           / joinChannel         
, state<LoggedIn>                             + event<u_m::BroadCastMessage>                                                      / broadCastMessage         
, state<LoggedIn>                             + event<u_m::Message>                                                               / sendMessageToUser         
, state<LoggedIn>                             + event<u_m::LeaveChannel>                                                          / leaveChannel         
, state<LoggedIn>                             + event<u_m::LogoutAccount>                                                         / logoutAccount                           = state<NotLoggedIn>          
, state<LoggedIn>                             + event<u_m::CreateGameLobby>                                                       / createGameLobby          
, state<LoggedIn>                             + event<u_m::JoinGameLobby>                                                         / joinGameLobby          
, state<LoggedIn>                             + event<u_m::SetMaxUserSizeInCreateGameLobby>                                       / setMaxUserSizeInCreateGameLobby          
, state<LoggedIn>                             + event<s_c::GameOption>                                                            / setGameOption         
, state<LoggedIn>                             + event<u_m::LeaveGameLobby>                 [ not gameLobbyControlledByUsers ]     / leaveGameLobbyErrorControlledByMatchmaking         
, state<LoggedIn>                             + event<u_m::LeaveGameLobby>                 [ not userInGameLobby ]                / leaveGameLobbyErrorUserNotInGameLobby         
, state<LoggedIn>                             + event<u_m::LeaveGameLobby>                                                        / leaveGameLobby         
, state<LoggedIn>                             + event<u_m::CreateGame>                                                            / createGame         
, state<LoggedIn>                             + event<u_m::WantsToJoinGame>                                                       / wantsToJoinAGameWrapper          
, state<LoggedIn>                             + event<u_m::LeaveQuickGameQueue>                                                   / leaveMatchMakingQueue          
, state<LoggedIn>                             + event<u_m::JoinMatchMakingQueue>                                                  / joinMatchMakingQueue         
, state<LoggedIn>                             + event<m_g::ConnectToGame>                                                         / doConnectToGame
, state<LoggedIn>                             + event<u_m::ConnectGameError>                                                      / connectToGameError                      
, state<LoggedIn>                             + event<m_g::ConnectToGameSuccess>                                                       / proxyStarted                            = state<ProxyToGame>
// ProxyToGame------------------------------------------------------------------------------------------------------------------------------------------------------------------  
, state<ProxyToGame>                          + event<ConnectionToGameLost>                                                       / proxyStopped                            = state<LoggedIn>     
, state<ProxyToGame>                          + event<m_g::LeaveGameSuccess>                                                      / leaveGame                               
// ReceiveMessage------------------------------------------------------------------------------------------------------------------------------------------------------------------  
,*state<ReceiveMessage>                        + event<SendMessageToUser>                                                          / sendToUser
        // clang-format on
    );
  }
};

struct my_logger
{
  template <class SM, class TEvent>
  void
  log_process_event (const TEvent &event)
  {
    if constexpr (confu_json::is_adapted_struct<TEvent>::value)
      {
        std::cout << "\n[" << aux::get_type_name<SM> () << "]"
                  << "[process_event] '" << objectToStringWithObjectName (event) << "'" << std::endl;
      }
    else
      {
        printf ("[%s][process_event] %s\n", aux::get_type_name<SM> (), aux::get_type_name<TEvent> ());
      }
  }

  template <class SM, class TGuard, class TEvent>
  void
  log_guard (const TGuard &, const TEvent &, bool result)
  {
    printf ("[%s][guard]\t  '%s' %s\n", aux::get_type_name<SM> (), aux::get_type_name<TGuard> (), (result ? "[OK]" : "[Reject]"));
  }

  template <class SM, class TAction, class TEvent>
  void
  log_action (const TAction &, const TEvent &)
  {
    printf ("[%s][action]\t '%s' \n", aux::get_type_name<SM> (), aux::get_type_name<TAction> ());
  }

  template <class SM, class TSrcState, class TDstState>
  void
  log_state_change (const TSrcState &src, const TDstState &dst)
  {
    printf ("[%s][transition]\t  '%s' -> '%s'\n", aux::get_type_name<SM> (), src.c_str (), dst.c_str ());
  }
};
struct Matchmaking::StateMachineWrapper
{
  StateMachineWrapper (Matchmaking *owner, boost::asio::io_context &ioContext, std::list<Matchmaking> &stateMachines_, std::function<void (std::string const &msg)> sendMsgToUser, std::list<GameLobby> &gameLobbies, boost::asio::thread_pool &pool, MatchmakingOption const &matchmakingOption_)
      : matchmakingData{ ioContext, stateMachines_, sendMsgToUser, gameLobbies, pool, matchmakingOption_ }, impl (owner,
#ifdef LOG_FOR_STATE_MACHINE
                                                                                                                  logger,
#endif
                                                                                                                  matchmakingData)
  {
  }
  MatchmakingData matchmakingData;

#ifdef LOG_FOR_STATE_MACHINE
  my_logger logger;
  boost::sml::sm<StateMachineImpl, boost::sml::logger<my_logger>> impl;
#else
  boost::sml::sm<StateMachineImpl> impl;
#endif
};

void // has to be after YourClass::StateMachineWrapper definition
Matchmaking::StateMachineWrapperDeleter::operator() (StateMachineWrapper *p)
{
  delete p;
}

Matchmaking::Matchmaking (boost::asio::io_context &ioContext, std::list<Matchmaking> &stateMachines_, std::function<void (std::string const &msg)> sendMsgToUser, std::list<GameLobby> &gameLobbies, boost::asio::thread_pool &pool, MatchmakingOption const &matchmakingOption_) : sm{ new StateMachineWrapper{ this, ioContext, stateMachines_, sendMsgToUser, gameLobbies, pool, matchmakingOption_ } } {}

void
Matchmaking::process_event (std::string const &event)
{
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, event, boost::is_any_of ("|"));
  if (splitMesssage.size () == 2)
    {
      auto const &typeToSearch = splitMesssage.at (0);
      auto const &objectAsString = splitMesssage.at (1);
      bool typeFound = false;
      boost::hana::for_each (user_matchmaking::userMatchmaking, [&] (const auto &x) {
        if (typeToSearch == confu_json::type_name<typename std::decay<decltype (x)>::type> ())
          {
            typeFound = true;
            boost::json::error_code ec{};
            sm->impl.process_event (confu_json::to_object<std::decay_t<decltype (x)>> (confu_json::read_json (objectAsString, ec)));
            if (ec) std::cout << "read_json error: " << ec.message () << std::endl;
            return;
          }
      });
      if (not typeFound) std::cout << "could not find a match for typeToSearch in userMatchmaking '" << typeToSearch << "'" << std::endl;
    }
  else
    {
      std::cout << "Not supported event. event syntax: EventName|JsonObject"
                << " msg: '" << event << "'" << std::endl;
    }
}

void
Matchmaking::sendMessageToGame (std::string const &message)
{
  sm->matchmakingData.matchmakingGame.sendMessage (message);
}

bool
Matchmaking::isLoggedInWithAccountName (std::string const &accountName) const
{
  return sm->matchmakingData.user.accountName == accountName;
}

bool
Matchmaking::isUserInChatChannel (std::string const &channelName) const
{
  return sm->matchmakingData.user.communicationChannels.count (channelName);
}

bool
Matchmaking::hasProxyToGame () const
{
  return sm->impl.is (state<ProxyToGame>);
}

void
Matchmaking::disconnectFromProxy ()
{
  sm->impl.process_event (matchmaking_game::LeaveGameSuccess{});
}

boost::asio::awaitable<void>
startGame (GameLobby const &gameLobby, MatchmakingData &matchmakingData)
{
  auto startServerAnswer = co_await sendStartGameToServer (gameLobby, matchmakingData);
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, startServerAnswer, boost::is_any_of ("|"));
  if (splitMesssage.size () == 2)
    {
      auto const &typeToSearch = splitMesssage.at (0);
      auto const &objectAsString = splitMesssage.at (1);
      if (typeToSearch == "StartGameSuccess")
        {
          for (auto const &accountName : gameLobby.accountNames)
            {
              if (auto matchmakingItr = ranges::find_if (matchmakingData.stateMachines, [&accountName] (Matchmaking const &matchmaking) { return matchmaking.isLoggedInWithAccountName (accountName); }); matchmakingItr != matchmakingData.stateMachines.end ())
                {
                  matchmakingItr->sm->impl.process_event (matchmaking_game::ConnectToGame{ accountName, std::move (stringToObject<matchmaking_game::StartGameSuccess> (objectAsString).gameName) });
                }
            }
        }
      else if (typeToSearch == "StartGameError")
        {
          sendToAllAccountsInUsersCreateGameLobby (startServerAnswer, matchmakingData);
        }
      else
        {
          std::cout << "Game server answered with: " << startServerAnswer << " expected StartGameSuccess|{} or StartGameError|{} " << std::endl;
        }
    }
  else
    {
      std::cout << "Game server answered with: " << startServerAnswer << " expected StartGameSuccess|{} or StartGameError|{} " << std::endl;
    }
}

void
sendMessageToUsers (std::string const &message, std::vector<std::string> const &accountNames, MatchmakingData &matchmakingData)
{
  for (auto const &accountToSendMessageTo : accountNames)
    {
      for (auto &matchmaking : matchmakingData.stateMachines | ranges::views::remove_if ([&accountToSendMessageTo] (Matchmaking const &matchmaking) { return matchmaking.sm->matchmakingData.user.accountName != accountToSendMessageTo; }))
        {
          matchmaking.sm->impl.process_event (SendMessageToUser{ message });
        }
    }
}

void
sendToAllAccountsInUsersCreateGameLobby (std::string const &message, MatchmakingData &matchmakingData)
{
  if (auto userGameLobby = ranges::find_if (matchmakingData.gameLobbies, [&accountName = matchmakingData.user.accountName] (GameLobby const &gameLobby) { return ranges::find (gameLobby.accountNames, accountName) != gameLobby.accountNames.end (); }); userGameLobby != matchmakingData.gameLobbies.end ())
    {
      sendMessageToUsers (message, userGameLobby->accountNames, matchmakingData);
    }
}