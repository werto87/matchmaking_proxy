#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/database/constant.hxx"
#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingAllowedTypes.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/logic/matchmakingGameAllowedTypes.hxx"
#include "matchmaking_proxy/logic/rating.hxx"
#include "matchmaking_proxy/pw_hash/passwordHash.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/server/myWebsocket.hxx"
#include "matchmaking_proxy/util.hxx"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/sml.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <expected>
#include <login_matchmaking_game_shared/gameOptionAsString.hxx>
#include <login_matchmaking_game_shared/matchmakingGameSerialization.hxx>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <range/v3/to_container.hpp>
#include <range/v3/view/remove_if.hpp>
#include <ranges>

typedef boost::asio::use_awaitable_t<>::as_default_on_t<boost::asio::basic_waitable_timer<boost::asio::chrono::system_clock>> CoroTimer;
using namespace boost::sml;
typedef boost::beast::websocket::stream<boost::asio::use_awaitable_t<>::as_default_on_t<boost::beast::tcp_stream>> Websocket;

BOOST_FUSION_DEFINE_STRUCT ((matchmaking_proxy), PasswordHashed, (std::string, accountName) (std::string, hashedPassword))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_proxy), PasswordMatches, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_proxy), ProxyToGame, )
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_proxy), SendMessageToUser, (std::string, msg))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_proxy), PasswordDoesNotMatch, (std::string, accountName))
BOOST_FUSION_DEFINE_STRUCT ((matchmaking_proxy), ConnectionToGameLost, )
namespace matchmaking_proxy
{
template <typename T>
concept hasAccountName = requires (T t) { t.accountName; };

// work around to print type for debuging
template <typename> struct Debug;
// Debug<SomeType> d;

struct MatchmakingData;

void sendToAllAccountsInUsersCreateGameLobby (std::string const &message, MatchmakingData &matchmakingData);

boost::asio::awaitable<void> wantsToJoinGame (user_matchmaking::WantsToJoinGame wantsToJoinGameEv, MatchmakingData &matchmakingData);

struct NotLoggedIn
{
};

struct WaitingForPasswordHashed
{
};

struct WaitingForPasswordCheck
{
};

struct WaitingForUserWantsToRelogGameLobby
{
};

struct LoggedIn
{
};

struct ConnectedWithGame
{
};

struct GlobalState
{
};

boost::asio::awaitable<void> startGame (GameLobby const &gameLobby, MatchmakingData &matchmakingData);

template <typename Matchmaking, typename Event>
bool
processEvent (Matchmaking &matchmaking, Event const &event)
{
  return matchmaking.sm->impl.process_event (event);
}

boost::asio::awaitable<void>
doHashPassword (user_matchmaking::CreateAccount createAccountObject, auto &sm, auto &&deps, auto &&subs)
{
  auto &matchmakingData = aux::get<MatchmakingData &> (deps);
  using namespace boost::asio::experimental::awaitable_operators;
  std::variant<std::string, std::optional<boost::system::system_error>> hashedPw = co_await (async_hash (matchmakingData.pool, matchmakingData.ioContext, createAccountObject.password, boost::asio::use_awaitable) || matchmakingData.cancelCoroutine ());
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
  auto passwordMatches = co_await (async_check_hashed_pw (matchmakingData.pool, matchmakingData.ioContext, account->password, loginAccountObject.password, boost::asio::use_awaitable) || matchmakingData.cancelCoroutine ());
  if (std::holds_alternative<bool> (passwordMatches))
    {
      if (std::get<bool> (passwordMatches))
        {
          sm.process_event (PasswordMatches{ loginAccountObject.accountName }, deps, subs);
        }
      else
        {
          sm.process_event (PasswordDoesNotMatch{ loginAccountObject.accountName }, deps, subs);
        }
    }
}

boost::asio::awaitable<void>
connectToGame (matchmaking_game::ConnectToGame connectToGameEv, auto &&sm, auto &&deps, auto &&subs)
{
  auto &matchmakingData = aux::get<MatchmakingData &> (deps);
  auto ws = std::make_shared<Websocket> (Websocket{ matchmakingData.ioContext });
  if (auto matchmakingForAccount = std::ranges::find_if (matchmakingData.stateMachines, [accountName = connectToGameEv.accountName] (auto const &matchmaking) { return matchmaking->isLoggedInWithAccountName (accountName); }); matchmakingForAccount != matchmakingData.stateMachines.end ())
    {
      auto matchmakingForAccountSptr = *matchmakingForAccount;
      try
        {
          co_await ws->next_layer ().async_connect (matchmakingData.userGameViaMatchmakingEndpoint);
          ws->next_layer ().expires_never ();
          ws->set_option (boost::beast::websocket::stream_base::timeout::suggested (boost::beast::role_type::client));
          ws->set_option (boost::beast::websocket::stream_base::decorator ([] (boost::beast::websocket::request_type &req) { req.set (boost::beast::http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async"); }));
          co_await ws->async_handshake (matchmakingData.userGameViaMatchmakingEndpoint.address ().to_string () + std::to_string (matchmakingData.userGameViaMatchmakingEndpoint.port ()), "/");
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
                    boost::system::error_code ec{};
                    try
                      {
                        sm.process_event (confu_json::to_object<std::decay_t<decltype (x)>> (confu_json::read_json (objectAsString, ec)), deps, subs);
                      }
                    catch (std::exception const &e)
                      {
                        auto errorHandleMessageFromGame = std::stringstream{};
                        errorHandleMessageFromGame << "exception: " << e.what () << '\n';
                        errorHandleMessageFromGame << "objectAsString: '" << objectAsString << "'\n";
                        errorHandleMessageFromGame << "example for " << confu_json::type_name<std::decay_t<decltype (x)>> () << ": '" << confu_json::to_json<> (x) << "'\n";
                        std::cout << errorHandleMessageFromGame.str () << std::endl;
                        matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::StartGameError{ "Error in Communication between Matchmaking and Game" }));
                      }

                    if (ec) std::cout << "read_json error: " << ec.message () << std::endl;
                  }
              });
              if (not typeFound) std::cout << "could not find a match for typeToSearch in matchmakingGame '" << typeToSearch << "'" << std::endl;
            }

          using namespace boost::asio::experimental::awaitable_operators;
          co_await (matchmakingData.matchmakingGame.readLoop ([&] (std::string const &readResult) {
            if ("LeaveGameSuccess|{}" == readResult)
              {
                sm.process_event (matchmaking_game::LeaveGameSuccess{}, deps, subs);
              }
            else
              {
                matchmakingData.sendMsgToUser (readResult);
              }
          }) && matchmakingData.matchmakingGame.writeLoop ());
        }
      catch (std::exception const &e)
        {
          sm.process_event (ConnectionToGameLost{}, deps, subs);
          std::cout << "exception: " << e.what () << std::endl;
          throw e;
        }
    }
}

void sendMessageToUsers (std::string const &message, std::vector<std::string> const &accountNames, MatchmakingData &matchmakingData);

auto const askUsersToJoinGame = [] (std::list<GameLobby>::iterator &gameLobby, MatchmakingData &matchmakingData) {
  if (gameLobby->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers)
    {
      sendMessageToUsers (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGame{}), gameLobby->accountNames | std::ranges::views::drop (1 /*drop first user because he is the admin and started createGame*/) | ranges::to<std::vector<std::string>> (), matchmakingData);
      gameLobby->readyUsers.push_back (gameLobby->accountNames.at (0));
    }
  else
    {
      sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGame{}), matchmakingData);
    }
  gameLobby->startTimerToAcceptTheInvite (
      matchmakingData.ioContext,
      [gameLobby, &matchmakingData] () {
        auto notReadyUsers = std::vector<std::string>{};
        std::ranges::copy_if (gameLobby->accountNames, std::back_inserter (notReadyUsers), [usersWhichAccepted = gameLobby->readyUsers] (std::string const &accountNamesGamelobby) mutable { return std::ranges::find_if (usersWhichAccepted, [accountNamesGamelobby] (std::string const &userWhoAccepted) { return accountNamesGamelobby == userWhoAccepted; }) == usersWhichAccepted.end (); });
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
            sendMessageToUsers (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}), gameLobby->readyUsers, matchmakingData);
            gameLobby->readyUsers.clear ();
          }
      },
      matchmakingData.matchmakingOption.timeToAcceptInvite);
};

boost::asio::awaitable<void>
createGame (user_matchmaking::CreateGame, auto &&, auto &&deps, auto &&)
{
  auto &matchmakingData = aux::get<MatchmakingData &> (deps);
  if (auto gameLobbyWithUser = std::ranges::find_if (matchmakingData.gameLobbies,
                                                     [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames;
                                                       return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
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
              if (auto errorInGameOptionResult = user_matchmaking_game::errorInGameOption (gameLobbyWithUser->gameOptionAsString))
                {
                  if (gameLobbyWithUser->accountNames.size () > 1)
                    {
                      askUsersToJoinGame (gameLobbyWithUser, matchmakingData);
                    }
                  else
                    {
                      co_await startGame (*gameLobbyWithUser, matchmakingData);
                      matchmakingData.gameLobbies.erase (gameLobbyWithUser);
                    }
                }
              else
                {
                  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ errorInGameOptionResult.error () }));
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

auto hashPassword = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void {
  boost::asio::co_spawn (aux::get<MatchmakingData &> (deps).ioContext, doHashPassword (event, sm, deps, subs), printException); // NOLINT(clang-analyzer-core.NullDereference) //TODO check if this is really a false positive
};
auto checkPassword = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (aux::get<MatchmakingData &> (deps).ioContext, doCheckPassword (event, sm, deps, subs), printException); };
auto const wantsToJoinAGameWrapper = [] (user_matchmaking::WantsToJoinGame const &wantsToJoinGameEv, MatchmakingData &matchmakingData) { co_spawn (matchmakingData.ioContext, wantsToJoinGame (wantsToJoinGameEv, matchmakingData), printException); };
auto doConnectToGame = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (aux::get<MatchmakingData &> (deps).ioContext, connectToGame (event, sm, deps, subs), printException); };
auto createGameWrapper = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (aux::get<MatchmakingData &> (deps).ioContext, createGame (event, sm, deps, subs), printException); };

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
auto const ratingChanged = [] (user_matchmaking::RatingChanged const &ratingChangedEv, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (ratingChangedEv)); };

auto const leaveGame = [] (MatchmakingData &matchmakingData) { matchmakingData.matchmakingGame.close (); };

auto const leaveMatchMakingQueue = [] (MatchmakingData &matchmakingData) {
  if (auto userGameLobby = std::ranges::find_if (matchmakingData.gameLobbies, [accountName = matchmakingData.user.accountName] (auto const &gameLobby) { return gameLobby.lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers && std::ranges::find_if (gameLobby.accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != gameLobby.accountNames.end (); }); userGameLobby != matchmakingData.gameLobbies.end ())
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveQuickGameQueueSuccess{}));
      userGameLobby->removeUser (matchmakingData.user.accountName);
      if (userGameLobby->accountNames.empty ())
        {
          matchmakingData.gameLobbies.erase (userGameLobby);
        }
      else
        {
          sendMessageToUsers (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}), userGameLobby->accountNames, matchmakingData);
          userGameLobby->cancelTimer ();
        }
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveQuickGameQueueError{ "User is not in queue" }));
    }
};

auto const logoutAccount = [] (MatchmakingData &matchmakingData) {
  if (auto gameLobbyWithAccount = std::ranges::find_if (matchmakingData.gameLobbies,
                                                        [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                          auto const &accountNames = gameLobby.accountNames;
                                                          return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
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
              usersInGameLobby.gameOptionAsString = gameLobbyWithAccount->gameOptionAsString;
              std::ranges::transform (gameLobbyWithAccount->accountNames, std::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
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
  if (auto gameLobby = std::ranges::find_if (matchmakingData.gameLobbies, [gameLobbyName = joinGameLobbyObject.name, lobbyPassword = joinGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName && _gameLobby.password == lobbyPassword; }); gameLobby != matchmakingData.gameLobbies.end ())
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
          usersInGameLobby.gameOptionAsString = gameLobby->gameOptionAsString;
          std::ranges::transform (gameLobby->accountNames, std::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
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
  if (auto gameLobbyWithAccount = std::ranges::find_if (matchmakingData.gameLobbies,
                                                        [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                          auto const &accountNames = gameLobby.accountNames;
                                                          return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                        });
      gameLobbyWithAccount != matchmakingData.gameLobbies.end ())
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToCreateGameLobbySuccess{}));
      auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
      usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
      usersInGameLobby.name = gameLobbyWithAccount->name.value ();
      usersInGameLobby.gameOptionAsString = gameLobbyWithAccount->gameOptionAsString;
      std::ranges::transform (gameLobbyWithAccount->accountNames, std::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (usersInGameLobby));
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
    }
};

auto const leaveGameLobby = [] (MatchmakingData &matchmakingData) {
  auto gameLobbyWithAccount = std::ranges::find_if (matchmakingData.gameLobbies, [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
    auto const &accountNames = gameLobby.accountNames;
    return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
  });
  gameLobbyWithAccount->removeUser (matchmakingData.user.accountName);
  if (gameLobbyWithAccount->accountCount () == 0)
    {
      matchmakingData.gameLobbies.erase (gameLobbyWithAccount);
    }
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbySuccess{}));
};

auto const gameOptionValid = [] (user_matchmaking_game::GameOptionAsString const &gameOptionAsString, MatchmakingData &matchmakingData) {
  if (auto errorInGameOptionResult = user_matchmaking_game::errorInGameOption (gameOptionAsString))
    {
      return true;
    }
  else
    {
      matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ errorInGameOptionResult.error () }));
      return false;
    }
};

auto const setGameOption = [] (user_matchmaking_game::GameOptionAsString const &gameOptionAsString, MatchmakingData &matchmakingData) {
  if (auto gameLobbyWithAccount = std::ranges::find_if (matchmakingData.gameLobbies,
                                                        [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                          auto const &accountNames = gameLobby.accountNames;
                                                          return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
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
              gameLobbyWithAccount->gameOptionAsString = gameOptionAsString;
              sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (gameLobbyWithAccount->gameOptionAsString), matchmakingData);
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
  if (auto gameLobbyWithAccount = std::ranges::find_if (matchmakingData.gameLobbies,
                                                        [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                          auto const &accountNames = gameLobby.accountNames;
                                                          return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
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
  if (std::ranges::find_if (matchmakingData.gameLobbies, [gameLobbyName = createGameLobbyObject.name, lobbyPassword = createGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName; }) == matchmakingData.gameLobbies.end ())
    {
      if (auto gameLobbyWithUser = std::ranges::find_if (matchmakingData.gameLobbies,
                                                         [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                           auto const &accountNames = gameLobby.accountNames;
                                                           return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                         });
          gameLobbyWithUser != matchmakingData.gameLobbies.end ())
        {
          matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameLobbyError{ { "account has already a game lobby with the name: " + gameLobbyWithUser->name.value_or ("Quick Game Lobby") } }));
          return;
        }
      else
        {
          auto &newGameLobby = matchmakingData.gameLobbies.emplace_back (createGameLobbyObject.name, createGameLobbyObject.password);
          if (newGameLobby.tryToAddUser (matchmakingData.user))
            {
              throw std::logic_error{ "user can not join lobby which he created" };
            }
          else
            {
              auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = newGameLobby.maxUserCount ();
              usersInGameLobby.name = newGameLobby.name.value ();
              usersInGameLobby.gameOptionAsString = newGameLobby.gameOptionAsString;
              std::ranges::transform (newGameLobby.accountNames, std::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
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
  for (auto &matchmaking : matchmakingData.stateMachines | std::ranges::views::filter ([&chatChannel = broadCastMessageObject.channel] (auto const &matchmaking) { return matchmaking->isUserInChatChannel (chatChannel); }))
    {
      // TODO handle error
      std::ignore = processEvent (*matchmaking, user_matchmaking::Message{ matchmakingData.user.accountName, broadCastMessageObject.channel, broadCastMessageObject.message });
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
  co_await ws->next_layer ().async_connect (matchmakingData.matchmakingGameEndpoint);
  ws->next_layer ().expires_never ();
  ws->set_option (boost::beast::websocket::stream_base::timeout::suggested (boost::beast::role_type::client));
  ws->set_option (boost::beast::websocket::stream_base::decorator ([] (boost::beast::websocket::request_type &req) { req.set (boost::beast::http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async"); }));
  co_await ws->async_handshake (matchmakingData.matchmakingGameEndpoint.address ().to_string () + std::to_string (matchmakingData.matchmakingGameEndpoint.port ()), "/");
  static size_t id = 0;
  auto myWebsocket = MyWebsocket<Websocket>{ std::move (ws), "sendStartGameToServer", fmt::fg (fmt::color::cornflower_blue), std::to_string (id++) };
  auto startGame = matchmaking_game::StartGame{};
  startGame.players = gameLobby.accountNames;
  startGame.gameOptionAsString = gameLobby.gameOptionAsString;
  startGame.ratedGame = gameLobby.lobbyAdminType == GameLobby::LobbyType::MatchMakingSystemRanked;
  co_await myWebsocket.async_write_one_message (objectToStringWithObjectName (startGame));
  auto msg = co_await myWebsocket.async_read_one_message ();
  co_return msg;
}

boost::asio::awaitable<void>
wantsToJoinGame (user_matchmaking::WantsToJoinGame wantsToJoinGameEv, MatchmakingData &matchmakingData)
{
  if (auto userGameLobby = std::ranges::find_if (matchmakingData.gameLobbies,
                                                 [accountName = matchmakingData.user.accountName] (GameLobby const &gameLobby) {
                                                   auto const &accountNames = gameLobby.accountNames;
                                                   return gameLobby.getWaitingForAnswerToStartGame () && std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                 });
      userGameLobby != matchmakingData.gameLobbies.end ())
    {
      if (wantsToJoinGameEv.answer)
        {
          if (std::ranges::find_if (userGameLobby->readyUsers, [accountName = matchmakingData.user.accountName] (std::string const &readyUserAccountName) { return readyUserAccountName == accountName; }) == userGameLobby->readyUsers.end ())
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
          if (userGameLobby->lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers)
            {
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameStartCanceledRemovedFromQueue{}));
              userGameLobby->removeUser (matchmakingData.user.accountName);
              if (userGameLobby->accountNames.empty ())
                {
                  matchmakingData.gameLobbies.erase (userGameLobby);
                }
              else
                {
                  sendMessageToUsers (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}), userGameLobby->accountNames, matchmakingData);
                  userGameLobby->cancelTimer ();
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
  if (auto gameLobbyWithAccount = std::ranges::find_if (matchmakingData.gameLobbies,
                                                        [accountName = matchmakingData.user.accountName] (auto const &gameLobby) {
                                                          auto const &accountNames = gameLobby.accountNames;
                                                          return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
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
          usersInGameLobby.gameOptionAsString = gameLobbyWithAccount->gameOptionAsString;
          std::ranges::transform (gameLobbyWithAccount->accountNames, std::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
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
  if (std::ranges::find_if (matchmakingData.gameLobbies, [accountName = matchmakingData.user.accountName] (auto const &gameLobby) { return std::ranges::find_if (gameLobby.accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != gameLobby.accountNames.end (); }) == matchmakingData.gameLobbies.end ())
    {
      if (auto gameLobbyToAddUser = std::ranges::find_if (matchmakingData.gameLobbies, [lobbyType = (joinMatchMakingQueueEv.isRanked) ? GameLobby::LobbyType::MatchMakingSystemRanked : GameLobby::LobbyType::MatchMakingSystemUnranked, accountName = matchmakingData.user.accountName, &matchmakingData] (GameLobby const &gameLobby) { return matchingLobby (accountName, gameLobby, lobbyType, matchmakingData.matchmakingOption.allowedRatingDifference); }); gameLobbyToAddUser != matchmakingData.gameLobbies.end ())
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
          auto &gameLobby = matchmakingData.gameLobbies.emplace_back ();
          if (auto error = gameLobby.setMaxUserCount ((lobbyType == GameLobby::LobbyType::MatchMakingSystemUnranked) ? matchmakingData.matchmakingOption.usersNeededToStartQuickGame : matchmakingData.matchmakingOption.usersNeededToStartRankedGame))
            {
              throw std::logic_error{ "Configuration Error. Please check MatchmakingOption. Error: " + error.value () };
            }
          gameLobby.lobbyAdminType = lobbyType;
          if (auto error = gameLobby.tryToAddUser (matchmakingData.user))
            {
              matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ matchmakingData.user.accountName, error.value () }));
            }
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
  return std::ranges::find_if (matchmakingData.gameLobbies,
                               [accountName = getAccountName (typeWithAccountName, matchmakingData)] (auto const &gameLobby) {
                                 auto const &accountNames = gameLobby.accountNames;
                                 return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                               })
         != matchmakingData.gameLobbies.end ();
};

auto const alreadyLoggedIn = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) -> bool { return std::ranges::find (matchmakingData.stateMachines, true, [accountName = typeWithAccountName.accountName] (const auto &matchmaking) { return matchmaking->isLoggedInWithAccountName (accountName); }) != matchmakingData.stateMachines.end (); };

auto const gameLobbyControlledByUsers = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) -> bool {
  auto userGameLobby = std::ranges::find_if (matchmakingData.gameLobbies, [accountName = getAccountName (typeWithAccountName, matchmakingData)] (auto const &gameLobby) {
    auto const &accountNames = gameLobby.accountNames;
    return std::ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
  });
  return userGameLobby->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers;
};

auto const loginAccountErrorPasswordAccountName = [] (auto const &objectWithAccountName, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ objectWithAccountName.accountName, "Incorrect Username or Password" })); };
auto const loginAccountSuccess = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) {
  matchmakingData.user.accountName = typeWithAccountName.accountName;
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountSuccess{ matchmakingData.user.accountName }));
};
auto const createAccountErrorAccountAlreadyCreated = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountError{ typeWithAccountName.accountName, "Account already Created" })); };
auto const proxyStarted = [] (MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::ProxyStarted{})); };
auto const proxyStopped = [] (MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::ProxyStopped{})); };
auto const loginAccountErrorAccountAlreadyLoggedIn = [] (PasswordMatches const &passwordMatches, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ passwordMatches.accountName, "Account already logged in" })); };
auto const wantsToRelogToGameLobby = [] (auto const &typeWithAccountName, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::WantToRelog{ typeWithAccountName.accountName, "Create Game Lobby" })); };
auto const connectToGameError = [] (user_matchmaking::ConnectGameError const &connectGameError, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (connectGameError)); };
auto const leaveGameLobbyErrorUserNotInGameLobby = [] (MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbyError{ "could not remove user from lobby user not found in lobby" })); };
auto const leaveGameLobbyErrorControlledByMatchmaking = [] (MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbyError{ "not allowed to leave a game lobby which is controlled by the matchmaking system with leave game lobby" })); };
auto const sendMessageToUser = [] (user_matchmaking::Message const &message, MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (message)); };

auto const userStatistics = [] (user_matchmaking::GetUserStatistics const &, MatchmakingData &matchmakingData) {
  auto result = user_matchmaking::UserStatistics{};
  // TODO use std::ranges::left_fold when it is implemented in libc++
  for (auto const &gameLobby : matchmakingData.gameLobbies)
    {
      switch (gameLobby.lobbyAdminType)
        {
        case GameLobby::LobbyType::FirstUserInLobbyUsers:
          {
            result.userInCreateCustomGameLobby += gameLobby.accountCount ();
            break;
          }
        case GameLobby::LobbyType::MatchMakingSystemRanked:
          {
            result.userInRankedQueue += gameLobby.accountCount ();
            break;
          }
        case GameLobby::LobbyType::MatchMakingSystemUnranked:
          {
            result.userInUnRankedQueue += gameLobby.accountCount ();
            break;
          }
        }
    }
  result.userInGame = boost::numeric_cast<size_t> (std::ranges::count_if (matchmakingData.stateMachines, [] (auto const &stateMachine) { return std::ranges::contains (stateMachine->currentStatesAsString (), "matchmaking_proxy::ProxyToGame"); }));
  matchmakingData.sendMsgToUser (objectToStringWithObjectName (result));
};

template <class T>
void
dump_transition (std::stringstream &ss) noexcept
{
  auto src_state = std::string{ boost::sml::aux::string<typename T::src_state>{}.c_str () };
  auto dst_state = std::string{ boost::sml::aux::string<typename T::dst_state>{}.c_str () };
  if (dst_state == "X")
    {
      dst_state = "[*]";
    }

  if (T::initial)
    {
      ss << "[*] --> " << src_state << std::endl;
    }

  ss << src_state << " --> " << dst_state;

  const auto has_event = !boost::sml::aux::is_same<typename T::event, boost::sml::anonymous>::value;
  const auto has_guard = !boost::sml::aux::is_same<typename T::guard, boost::sml::front::always>::value;
  const auto has_action = !boost::sml::aux::is_same<typename T::action, boost::sml::front::none>::value;

  if (has_event || has_guard || has_action)
    {
      ss << " :";
    }

  if (has_event)
    {
      ss << " " << boost::sml::aux::get_type_name<typename T::event> ();
    }

  if (has_guard)
    {
      ss << " [" << boost::sml::aux::get_type_name<typename T::guard::type> () << "]";
    }

  if (has_action)
    {
      ss << " / " << boost::sml::aux::get_type_name<typename T::action::type> ();
    }

  ss << std::endl;
}

template <template <class...> class T, class... Ts>
std::string
dump_transitions (const T<Ts...> &) noexcept
{
  auto ss = std::stringstream{};
  int _[]{ 0, (dump_transition<Ts> (ss), 0)... };
  (void)_;
  return ss.str ();
}

template <class SM>
std::string
dump () noexcept
{
  auto ss = std::stringstream{};
  ss << "@startuml" << std::endl << std::endl;
  ss << dump_transitions (typename SM::transitions{});
  ss << std::endl << "@enduml" << std::endl;
  return ss.str ();
}

std::string dumpStateMachine ();

auto const matchmakingLogic = [] (MatchmakingData &matchmakingData) { matchmakingData.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::MatchmakingLogic{ dumpStateMachine () })); };

class MatchmakingStateMachine
{
public:
  auto
  operator() () const noexcept
  {
    namespace u_m = user_matchmaking;
    namespace m_g = matchmaking_game;
    namespace u_m_g = user_matchmaking_game;
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
, state<LoggedIn>                             + event<u_m::LoginAccount>                   [ not accountInDatabase ]              / loginAccountErrorPasswordAccountName
, state<LoggedIn>                             + event<u_m::LoginAccount>                                                          / (logoutAccount,checkPassword)           = state<WaitingForPasswordCheck>
, state<LoggedIn>                             + event<u_m::JoinChannel>                                                           / joinChannel
, state<LoggedIn>                             + event<u_m::BroadCastMessage>                                                      / broadCastMessage
, state<LoggedIn>                             + event<u_m::Message>                                                               / sendMessageToUser
, state<LoggedIn>                             + event<u_m::LeaveChannel>                                                          / leaveChannel
, state<LoggedIn>                             + event<u_m::LogoutAccount>                                                         / logoutAccount                           = state<NotLoggedIn>
, state<LoggedIn>                             + event<u_m::CreateGameLobby>                                                       / createGameLobby
, state<LoggedIn>                             + event<u_m::JoinGameLobby>                                                         / joinGameLobby
, state<LoggedIn>                             + event<u_m::SetMaxUserSizeInCreateGameLobby>                                       / setMaxUserSizeInCreateGameLobby
, state<LoggedIn>                             + event<u_m_g::GameOptionAsString>           [ gameOptionValid ]                    / setGameOption
, state<LoggedIn>                             + event<u_m::LeaveGameLobby>                 [ not gameLobbyControlledByUsers ]     / leaveGameLobbyErrorControlledByMatchmaking
, state<LoggedIn>                             + event<u_m::LeaveGameLobby>                 [ not userInGameLobby ]                / leaveGameLobbyErrorUserNotInGameLobby
, state<LoggedIn>                             + event<u_m::LeaveGameLobby>                                                        / leaveGameLobby
, state<LoggedIn>                             + event<u_m::CreateGame>                                                            / createGameWrapper
, state<LoggedIn>                             + event<u_m::WantsToJoinGame>                                                       / wantsToJoinAGameWrapper
, state<LoggedIn>                             + event<u_m::LeaveQuickGameQueue>                                                   / leaveMatchMakingQueue
, state<LoggedIn>                             + event<u_m::JoinMatchMakingQueue>                                                  / joinMatchMakingQueue
, state<LoggedIn>                             + event<m_g::ConnectToGame>                                                         / doConnectToGame
, state<LoggedIn>                             + event<u_m::ConnectGameError>                                                      / connectToGameError
, state<LoggedIn>                             + event<m_g::ConnectToGameSuccess>                                                  / proxyStarted                            = state<ProxyToGame>
// ProxyToGame------------------------------------------------------------------------------------------------------------------------------------------------------------------
, state<ProxyToGame>                          + event<ConnectionToGameLost>                                                       / proxyStopped                            = state<LoggedIn>
, state<ProxyToGame>                          + event<m_g::LeaveGameSuccess>                                                      / leaveGame
// GlobalState------------------------------------------------------------------------------------------------------------------------------------------------------------------
,*state<GlobalState>                          + event<SendMessageToUser>                                                          / sendToUser
, state<GlobalState>                          + event<u_m::GetMatchmakingLogic>                                                   / matchmakingLogic
, state<GlobalState>                          + event<u_m::RatingChanged>                                                         / ratingChanged
, state<GlobalState>                          + event<u_m::GetUserStatistics>                                                     / userStatistics

        // clang-format on
    );
  }
};

std::string
dumpStateMachine ()
{
  return dump<boost::sml::sm<MatchmakingStateMachine>> ();
}

struct my_logger
{
  template <class SM, class TEvent>
  void
  log_process_event (const TEvent &event)
  {
    if constexpr (boost::fusion::traits::is_sequence<TEvent>::value)
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
  StateMachineWrapper (Matchmaking *owner, MatchmakingData &&matchmakingData_)
      : matchmakingData{ std::move (matchmakingData_) }, impl (owner,
#ifdef MATCHMAKING_PROXY_LOG_FOR_STATE_MACHINE
                                                               logger,
#endif
                                                               matchmakingData)
  {
  }
  MatchmakingData matchmakingData;

#ifdef MATCHMAKING_PROXY_LOG_FOR_STATE_MACHINE
  my_logger logger;
  boost::sml::sm<MatchmakingStateMachine, boost::sml::logger<my_logger>> impl;
#else
  boost::sml::sm<MatchmakingStateMachine> impl;
#endif
};

void // has to be after YourClass::StateMachineWrapper definition
Matchmaking::StateMachineWrapperDeleter::operator() (StateMachineWrapper *p)
{
  delete p;
}

Matchmaking::Matchmaking (MatchmakingData &&matchmakingData) : sm{ new StateMachineWrapper{ this, std::move (matchmakingData) } } {}

std::expected<void, std::string>
Matchmaking::processEvent (std::string const &event)
{
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, event, boost::is_any_of ("|"));
  auto result = std::expected<void, std::string>{};
  if (splitMesssage.size () == 2)
    {
      auto const &typeToSearch = splitMesssage.at (0);
      auto const &objectAsString = splitMesssage.at (1);
      bool typeFound = false;
      boost::hana::for_each (user_matchmaking::userMatchmaking, [&] (const auto &x) {
        if (typeToSearch == confu_json::type_name<typename std::decay<decltype (x)>::type> ())
          {
            typeFound = true;
            boost::system::error_code ec{};
            auto messageAsObject = confu_json::read_json (objectAsString, ec);
            if (ec) result = std::unexpected ("read_json error: " + ec.message ());
            else
              {
                try
                  {
                    if (not sm->impl.process_event (confu_json::to_object<std::decay_t<decltype (x)>> (messageAsObject)))
                      {
                        result = std::unexpected ("No transition found");
                      }
                  }
                catch (std::exception const &e)
                  {
                    auto messageForUser = std::stringstream{};
                    messageForUser << "exception: " << e.what () << '\n';
                    messageForUser << "messageAsObject: " << messageAsObject << '\n';
                    messageForUser << "example for " << confu_json::type_name<std::decay_t<decltype (x)>> () << " : '" << confu_json::to_json<> (x) << "'" << '\n';
                    result = std::unexpected (messageForUser.str ());
                  }
              }
          }
      });
      if (not typeFound) result = std::unexpected ("could not find a match for typeToSearch in shared_class::gameTypes '" + typeToSearch + "'");
    }
  else
    {
      result = std::unexpected ("Not supported event. event syntax: EventName|JsonObject");
    }
  return result;
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

std::vector<std::string>
Matchmaking::currentStatesAsString () const
{
  auto results = std::vector<std::string>{};
  sm->impl.visit_current_states ([&results] (auto state) { results.push_back (state.c_str ()); });
  return results;
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
              if (auto matchmakingItr = std::ranges::find_if (matchmakingData.stateMachines, [&accountName] (auto const &matchmaking) { return matchmaking->isLoggedInWithAccountName (accountName); }); matchmakingItr != matchmakingData.stateMachines.end ())
                {
                  matchmakingItr->get ()->sm->impl.process_event (matchmaking_game::ConnectToGame{ accountName, std::move (stringToObject<matchmaking_game::StartGameSuccess> (objectAsString).gameName) });
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
      for (auto &matchmaking : matchmakingData.stateMachines | ranges::views::remove_if ([&accountToSendMessageTo] (auto const &matchmaking) { return matchmaking->sm->matchmakingData.user.accountName != accountToSendMessageTo; }))
        {
          matchmaking->sm->impl.process_event (SendMessageToUser{ message });
        }
    }
}

void
sendToAllAccountsInUsersCreateGameLobby (std::string const &message, MatchmakingData &matchmakingData)
{
  if (auto userGameLobby = std::ranges::find_if (matchmakingData.gameLobbies, [&accountName = matchmakingData.user.accountName] (GameLobby const &gameLobby) { return std::ranges::find (gameLobby.accountNames, accountName) != gameLobby.accountNames.end (); }); userGameLobby != matchmakingData.gameLobbies.end ())
    {
      sendMessageToUsers (message, userGameLobby->accountNames, matchmakingData);
    }
}

void
Matchmaking::cleanUp ()
{
  disconnectFromProxy ();
  if (auto userGameLobby = std::ranges::find_if (sm->matchmakingData.gameLobbies, [&accountName = sm->matchmakingData.user.accountName] (GameLobby const &gameLobby) { return std::ranges::find (gameLobby.accountNames, accountName) != gameLobby.accountNames.end (); }); userGameLobby != sm->matchmakingData.gameLobbies.end ())
    {
      userGameLobby->removeUser (sm->matchmakingData.user.accountName);
      if (userGameLobby->accountNames.empty ())
        {
          sm->matchmakingData.gameLobbies.erase (userGameLobby);
        }
      else
        {
          sendMessageToUsers (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}), userGameLobby->accountNames, sm->matchmakingData);
          userGameLobby->cancelTimer ();
        }
    }
}
}