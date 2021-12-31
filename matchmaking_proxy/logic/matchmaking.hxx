#ifndef D9077A7B_F0F8_4687_B460_C6D43C94F8AF
#define D9077A7B_F0F8_4687_B460_C6D43C94F8AF
#include "../database/database.hxx"
#include "../pw_hash/passwordHash.hxx"
#include "../server/gameLobby.hxx"
#include "../server/user.hxx"
#include "../userMatchmakingSerialization.hxx"
#include "../util.hxx"
#include "matchmaking_proxy/logic/client.hxx"
#include "matchmaking_proxy/logic/myWebsocket.hxx"
#include "matchmaking_proxy/matchmakingGameSerialization.hxx"
#include "rating.hxx"
#include <algorithm>
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/fusion/include/pair.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/support/pair.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/sml.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/type_index.hpp>
#include <boost/uuid/uuid.hpp>
#include <cassert>
#include <cmath>
#include <confu_json/confu_json.hxx>
#include <confu_json/to_json.hxx>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <crypt.h>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <fmt/core.h>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <pipes/filter.hpp>
#include <pipes/pipes.hpp>
#include <pipes/push_back.hpp>
#include <pipes/transform.hpp>
#include <range/v3/algorithm/copy_if.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/all.hpp>
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/range.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/view/filter.hpp>
#include <set>
#include <sodium.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace sml = boost::sml;

struct NotLoggedin
{
};
struct PasswordHashed
{
  std::string hashedPassword{};
};

struct PasswordMatches
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

struct ProxyToGame
{
};
struct Loggedin
{
};

struct ConnectedWithGame
{
};

struct SendMessageToGame
{
  std::string msg{};
};
struct SendMessageToUser
{
  std::string msg{};
};
struct ReciveMessage
{
};

// TODO this leaks using namespace and typedef

typedef boost::beast::websocket::stream<boost::asio::use_awaitable_t<>::as_default_on_t<boost::beast::tcp_stream>> Websocket;
typedef boost::asio::use_awaitable_t<>::as_default_on_t<boost::asio::basic_waitable_timer<boost::asio::chrono::system_clock>> CoroTimer;
constexpr auto use_nothrow_awaitable = boost::asio::experimental::as_tuple (boost::asio::use_awaitable);

auto constexpr ALLOWED_DIFFERENCE_FOR_RANKED_GAME_MATCHMAKING = size_t{ 100 };

bool inline isInRatingrange (size_t userRating, size_t lobbyAverageRating)
{
  auto const difference = userRating > lobbyAverageRating ? userRating - lobbyAverageRating : lobbyAverageRating - userRating;
  return difference < ALLOWED_DIFFERENCE_FOR_RANKED_GAME_MATCHMAKING;
}

bool inline checkRating (size_t userRating, std::vector<std::string> const &accountNames) { return isInRatingrange (userRating, averageRating (accountNames)); }

bool inline matchingLobby (std::string const &accountName, GameLobby const &gameLobby, GameLobby::LobbyType const &lobbyType)
{
  if (gameLobby.lobbyAdminType == lobbyType && gameLobby.accountNames.size () < gameLobby.maxUserCount ())
    {
      if (lobbyType == GameLobby::LobbyType::MatchMakingSystemRanked)
        {
          soci::session sql (soci::sqlite3, databaseName);
          if (auto userInDatabase = confu_soci::findStruct<database::Account> (sql, "accountName", accountName))
            {
              return checkRating (userInDatabase->rating, gameLobby.accountNames);
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

struct Matchmaking
{
  using Self = Matchmaking;
  Matchmaking (boost::asio::io_context &io_context_, std::list<std::shared_ptr<User>> &users_, boost::asio::thread_pool &pool_, std::list<GameLobby> &gameLobbies_, std::function<void (std::string msgToSend)> sendMsg_) : sendMsgToUser{ sendMsg_ }, io_context{ io_context_ }, users{ users_ }, pool{ pool_ }, gameLobbies{ gameLobbies_ }, cancelCoroutineTimer{ std::make_shared<CoroTimer> (CoroTimer{ io_context_ }) } { cancelCoroutineTimer->expires_after (std::chrono::system_clock::time_point::max () - std::chrono::system_clock::now ()); }

public:
  auto
  operator() () const noexcept
  {
    using namespace sml;
    auto doCreateAccountAndLogin = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (sml::aux::get<Matchmaking &> (deps).io_context, sml::aux::get<Matchmaking &> (deps).createAccountAndLogin (event, sm, deps, subs), boost::asio::detached); };
    auto doLoginAccount = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (sml::aux::get<Matchmaking &> (deps).io_context, sml::aux::get<Matchmaking &> (deps).loginAccount (event, sm, deps, subs), boost::asio::detached); };
    namespace u_m = user_matchmaking;
    namespace m_g = matchmaking_game;
    // clang-format off
    return make_transition_table(
// NotLoggedIn-----------------------------------------------------------------------------------------------------------------------------------------------------------------
* state<NotLoggedin>                          + event<u_m::CreateAccount>                                             / doCreateAccountAndLogin                   = state<WaitingForPasswordHashed>
, state<NotLoggedin>                          + event<u_m::LoginAccount>                                              / doLoginAccount                            = state<WaitingForPasswordCheck>
, state<NotLoggedin>                          + event<u_m::LoginAsGuest>                                              / (&Self::loginAsGuest)                     = state<Loggedin>
// WaitingForCreateAccount------------------------------------------------------------------------------------------------------------------------------------------------------
, state<WaitingForPasswordHashed>             + event<PasswordHashed>                      [ not accountInDatabase ]  / (&Self::createAccount)                    = state<Loggedin>
, state<WaitingForPasswordHashed>             + event<PasswordHashed>                      [ accountInDatabase ]      / (&Self::informUserCreateAccountError)     = state<NotLoggedin>
, state<WaitingForPasswordHashed>             + event<u_m::CreateAccountCancel>                                       / (&Self::cancelCreateAccount)              = state<NotLoggedin>
// WaitingForLogin--------------------------------------------------------------------------------------------------------------------------------------------------------------
, state<WaitingForPasswordCheck>              + event<PasswordMatches>                     [ userInGameLobby ]        / (&Self::informUserWantsToRelogToGameLobby)= state<WaitingForUserWantsToRelogGameLobby>
, state<WaitingForPasswordCheck>              + event<PasswordMatches>                     [ not userInGameLobby ]                                                = state<Loggedin>
, state<WaitingForPasswordCheck>              + event<u_m::LoginAccountCancel>                                        / (&Self::cancelLoginAccount)               = state<NotLoggedin>
// WaitingForPasswordCheck---------------------------------------------------------------------------------------------------------------------------------------------------------------------
, state<WaitingForUserWantsToRelogGameLobby>  + event<u_m::RelogTo>                        [ wantsToRelog ]           / (&Self::relogToGameLobby)                 = state<Loggedin>
, state<WaitingForUserWantsToRelogGameLobby>  + event<u_m::RelogTo>                        [ not wantsToRelog ]       / (&Self::removeUserFromGameLobby)          = state<Loggedin>
// Loggedin---------------------------------------------------------------------------------------------------------------------------------------------------------------------
, state<Loggedin>                             + on_entry<_>                                                           / (&Self::informUserLoginAccountSuccess)
, state<Loggedin>                             + event<u_m::JoinChannel>                                               / (&Self::joinChannel)         
, state<Loggedin>                             + event<u_m::BroadCastMessage>                                          / (&Self::broadCastMessage)         
, state<Loggedin>                             + event<u_m::LeaveChannel>                                              / (&Self::leaveChannel)         
, state<Loggedin>                             + event<u_m::LogoutAccount>                                             / (&Self::logoutAccount)                    = state<NotLoggedin>          
, state<Loggedin>                             + event<u_m::CreateGameLobby>                                           / (&Self::createGameLobby)          
, state<Loggedin>                             + event<u_m::JoinGameLobby>                                             / (&Self::joinGameLobby)          
, state<Loggedin>                             + event<u_m::SetMaxUserSizeInCreateGameLobby>                           / (&Self::setMaxUserSizeInCreateGameLobby)          
, state<Loggedin>                             + event<shared_class::GameOption>                                       / (&Self::setGameOption)         
, state<Loggedin>                             + event<u_m::LeaveGameLobby>                                            / (&Self::leaveGameLobby)         
, state<Loggedin>                             + event<u_m::CreateGame>                                                / (&Self::createGame)         
, state<Loggedin>                             + event<u_m::WantsToJoinGame>                                           / (&Self::wantsToJoinGame)          
, state<Loggedin>                             + event<u_m::LeaveQuickGameQueue>                                       / (&Self::leaveMatchMakingQueue)          
, state<Loggedin>                             + event<u_m::JoinMatchMakingQueue>                                      / (&Self::joinMatchMakingQueue)         
, state<Loggedin>                             + event<m_g::StartGameSuccess>                                                                                      = state<ProxyToGame>          
// ProxyToGame------------------------------------------------------------------------------------------------------------------------------------------------------------------  
, state<ProxyToGame>                          +event<m_g::LeaveGameSuccess>                                                                                       = state<Loggedin>     
// ReciveMessage------------------------------------------------------------------------------------------------------------------------------------------------------------------  
,*state<ReciveMessage>                        +event<SendMessageToUser>                                               / (&Self::sendToUser)
    );
    // clang-format on
  }

  std::function<void (std::string msgToSend)> sendMsgToUser{};

private:
  void
  sendToUser (SendMessageToUser const &sendMessageToUser)
  {
    sendMsgToUser (std::move (sendMessageToUser.msg));
  }

  bool
  isRegistered (std::string const &accountName)
  {
    soci::session sql (soci::sqlite3, databaseName);
    return confu_soci::findStruct<database::Account> (sql, "accountName", accountName).has_value ();
  }

  void
  logoutAccount ()
  {
    if (isRegistered (user.accountName))
      {
        // TODO find a way to remove user from gamelobby
        // removeUserFromLobby ();
      }
    user = {};
    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LogoutAccountSuccess{}));
  }

  boost::asio::awaitable<std::string>
  sendStartGameToServer (GameLobby const &gameLobby)
  {
    auto ws = Websocket{ io_context };
    auto gameEndpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 };
    co_await ws.next_layer ().async_connect (gameEndpoint);
    ws.next_layer ().expires_never ();
    ws.set_option (boost::beast::websocket::stream_base::timeout::suggested (boost::beast::role_type::client));
    ws.set_option (boost::beast::websocket::stream_base::decorator ([] (boost::beast::websocket::request_type &req) { req.set (boost::beast::http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async"); }));
    co_await ws.async_handshake ("localhost:" + std::to_string (gameEndpoint.port ()), "/");
    auto startGame = user_matchmaking::StartGame{};
    startGame.players = gameLobby.accountNames;
    startGame.gameOption = gameLobby.gameOption;
    co_await ws.async_write (boost::asio::buffer (objectToStringWithObjectName (startGame)));
    boost::beast::flat_buffer buffer;
    co_await ws.async_read (buffer);
    auto msg = boost::beast::buffers_to_string (buffer.data ());
    co_return msg;
  }

  boost::asio::awaitable<void>
  startGame (GameLobby const &gameLobby)
  {
    try
      {
        auto startServerAnswer = co_await sendStartGameToServer (gameLobby);
        std::vector<std::string> splitMesssage{};
        boost::algorithm::split (splitMesssage, startServerAnswer, boost::is_any_of ("|"));
        if (splitMesssage.size () == 2)
          {
            auto const &typeToSearch = splitMesssage.at (0);
            if (typeToSearch == "GameStarted")
              {
                // TODO send to the user state machines start game so they connect like here
                // for (auto &user_ : gameLobby._users)
                //   {

                //     user_->connectionToGame = std::make_shared<Websocket> (io_context);
                //     auto gameEndpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 };
                //     co_await user_->connectionToGame->next_layer ().async_connect (gameEndpoint);
                //     user_->connectionToGame->next_layer ().expires_never ();
                //     user_->connectionToGame->set_option (boost::beast::websocket::stream_base::timeout::suggested (boost::beast::role_type::client));
                //     user_->connectionToGame->set_option (boost::beast::websocket::stream_base::decorator ([] (boost::beast::websocket::request_type &req) { req.set (boost::beast::http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async"); }));
                //     co_await user_->connectionToGame->async_handshake ("localhost:" + std::to_string (gameEndpoint.port ()), "/");
                //     co_spawn (
                //         io_context, [user_] { return user_->readFromGame (); }, boost::asio::detached);
                //     co_spawn (
                //         io_context, [user_] { return user_->writeToGame (); }, boost::asio::detached);
                //     user_->sendMessageToUser (startServerAnswer);
                //   }
              }
            else if (typeToSearch == "StartGameError")
              {
                // TODO send to the user state machines StartGameError
                // for (auto &user_ : gameLobby._users)
                //   {
                //     user_->sendMessageToUser (startServerAnswer);
                //   }
              }
          }
      }
    catch (std::exception &e)
      {
        std::cout << "Start Game exception: " << e.what () << std::endl;
      }
  }

  bool
  createAccount (PasswordHashed const &passwordHash)
  {
    return database::createAccount (user.accountName, passwordHash.hashedPassword).has_value ();
  }

  std::function<bool (Matchmaking &matchmaking)> accountInDatabase = [] (Matchmaking &matchmaking) -> bool {
    soci::session sql (soci::sqlite3, databaseName);
    return confu_soci::findStruct<database::Account> (sql, "accountName", matchmaking.user.accountName).has_value ();
  };

  void
  cancelCreateAccount ()
  {
    cancelCoroutineTimer->cancel ();
    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountCancel{}));
  }

  void
  cancelLoginAccount ()
  {
    cancelCoroutineTimer->cancel ();
    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountCancel{}));
  }

  boost::asio::awaitable<void>
  abortCoroutine ()
  {
    try
      {
        co_await cancelCoroutineTimer->async_wait ();
      }
    catch (boost::system::system_error &e)
      {
        using namespace boost::system::errc;
        if (operation_canceled == e.code ())
          {
            cancelCoroutineTimer->expires_after (std::chrono::system_clock::time_point::max () - std::chrono::system_clock::now ());
            co_return;
          }
        else
          {
            std::cout << "error in timer boost::system::errc: " << e.code () << std::endl;
            abort ();
          }
      }
  }

  boost::asio::awaitable<void>
  createAccountAndLogin (user_matchmaking::CreateAccount const &createAccountObject, auto &sm, auto &&deps, auto &&subs)
  {
    soci::session sql (soci::sqlite3, databaseName);
    if (confu_soci::findStruct<database::Account> (sql, "accountName", createAccountObject.accountName))
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountError{ createAccountObject.accountName, "account already created" }));
        co_return;
      }
    else
      {
        using namespace boost::asio::experimental::awaitable_operators;
        std::variant<std::string, std::monostate> hashedPw = co_await(async_hash (pool, io_context, createAccountObject.password, boost::asio::use_awaitable) || abortCoroutine ());
        if (std::holds_alternative<std::string> (hashedPw))
          {
            user.accountName = createAccountObject.accountName;
            sm.process_event (PasswordHashed{ std::get<std::string> (hashedPw) }, deps, subs);
          }
      }
  }

  void
  informUserWantsToRelogToGameLobby ()
  {
    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::WantToRelog{ user.accountName, "Create Game Lobby" }));
  }

  std::function<bool (Matchmaking &matchmaking)> userInGameLobby = [] (Matchmaking &matchmaking) -> bool {
    return ranges::find_if (matchmaking.gameLobbies,
                            [accountName = matchmaking.user.accountName] (auto const &gameLobby) {
                              auto const &accountNames = gameLobby.accountNames;
                              return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                            })
           != matchmaking.gameLobbies.end ();
  };

  std::function<bool (user_matchmaking::RelogTo const &relogTo)> wantsToRelog = [] (user_matchmaking::RelogTo const &relogTo) -> bool { return relogTo.wantsToRelog; };

  boost::asio::awaitable<void>
  loginAccount (auto &&loginAccountObject, auto &&sm, auto &&deps, auto &&subs)
  {
    soci::session sql (soci::sqlite3, databaseName);
    if (auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName))
      {
        if (std::find_if (users.begin (), users.end (), [accountName = account->accountName] (auto const &u) { return accountName == u->accountName; }) != users.end ())
          {
            sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ loginAccountObject.accountName, "Account already logged in" }));
            co_return;
          }
        else
          {
            using namespace boost::asio::experimental::awaitable_operators;
            auto passwordMatches = co_await(async_check_hashed_pw (pool, io_context, account->password, loginAccountObject.password, boost::asio::use_awaitable) || abortCoroutine ());
            if (std::holds_alternative<bool> (passwordMatches))
              {
                if (std::get<bool> (passwordMatches))
                  {
                    user.accountName = loginAccountObject.accountName;
                    sm.process_event (PasswordMatches{}, deps, subs);
                  }
                else
                  {
                    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ loginAccountObject.accountName, "Incorrect Username or Password" }));
                    co_return;
                  }
              }
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ loginAccountObject.accountName, "Incorrect username or password" }));
        co_return;
      }
  }

  void
  broadCastMessage (user_matchmaking::BroadCastMessage const &broadCastMessageObject)
  {
    // TODO send to all users which are in the channel
    // for (auto &user_ : users | ranges::views::filter ([channel = broadCastMessageObject.channel, accountName = user.accountName] (auto const &user_) { return user_->communicationChannels.find (channel) != user_->communicationChannels.end (); }))
    //   {
    //     soci::session sql (soci::sqlite3, databaseName);
    //     auto message = user_matchmaking::Message{ user_->accountName, broadCastMessageObject.channel, broadCastMessageObject.message };
    //     user_->sendMessageToUser (objectToStringWithObjectName (std::move (message)));
    //   }
  }

  void
  informUserLoginAccountSuccess ()
  {
    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountSuccess{ user.accountName }));
  }

  void
  informUserCreateAccountError ()
  {
    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountError{ user.accountName, "Account already Created" }));
  }

  void
  joinChannel (user_matchmaking::JoinChannel const &joinChannelObject)
  {
    user.communicationChannels.insert (joinChannelObject.channel);
    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinChannelSuccess{ joinChannelObject.channel }));
  }

  void
  leaveChannel (user_matchmaking::LeaveChannel const &leaveChannelObject)
  {
    if (user.communicationChannels.erase (leaveChannelObject.channel))
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveChannelSuccess{ leaveChannelObject.channel }));
        return;
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveChannelError{ leaveChannelObject.channel, { "channel not found" } }));
        return;
      }
  }

  void
  askUsersToJoinGame (std::list<GameLobby>::iterator &gameLobby)
  {
    // TODO do something so we can send to all accounts in game lobby
    // gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGame{}));
    gameLobby->startTimerToAcceptTheInvite (io_context, [gameLobby, &gameLobbies = gameLobbies] () {
      auto notReadyUsers = std::vector<std::string>{};
      ranges::copy_if (gameLobby->accountNames, ranges::back_inserter (notReadyUsers), [usersWhichAccepted = gameLobby->readyUsers] (std::string const &accountNamesGamelobby) mutable { return ranges::find_if (usersWhichAccepted, [accountNamesGamelobby] (std::string const &userWhoAccepted) { return accountNamesGamelobby == userWhoAccepted; }) == usersWhichAccepted.end (); });
      for (auto const &notReadyUser : notReadyUsers)
        {
          // TODO send a msg to not ready users
          // notReadysendMsgToUser (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGameTimeOut{}));
          if (gameLobby->lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers)
            {
              gameLobby->removeUser (notReadyUser);
            }
        }
      if (gameLobby->accountNames.empty ())
        {
          gameLobbies.erase (gameLobby);
        }
      else
        {
          gameLobby->readyUsers.clear ();
          // TODO do something so we can send to all accounts in game lobby
          // gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}));
        }
    });
  }

  void
  createGame ()
  {
    if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                  [accountName = user.accountName] (auto const &gameLobby) {
                                                    auto const &accountNames = gameLobby.accountNames;
                                                    return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                  });
        gameLobbyWithUser != gameLobbies.end ())
      {
        if (gameLobbyWithUser->getWaitingForAnswerToStartGame ())
          {
            sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "It is not allowed to start a game while ask to start a game is running" }));
          }
        else
          {
            if (gameLobbyWithUser->isGameLobbyAdmin (user.accountName))
              {
                if (gameLobbyWithUser->accountNames.size () >= 2)
                  {
                    if (auto gameOptionError = errorInGameOption (gameLobbyWithUser->gameOption))
                      {
                        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ gameOptionError.value () }));
                      }
                    else
                      {
                        askUsersToJoinGame (gameLobbyWithUser);
                      }
                  }
                else
                  {
                    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "You need atleast two user to create a game" }));
                  }
              }
            else
              {
                sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "you need to be admin in a game lobby to start a game" }));
              }
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "Could not find a game lobby for the user" }));
      }
  }

  void
  createGameLobby (user_matchmaking::CreateGameLobby const &createGameLobbyObject)
  {
    if (ranges::find_if (gameLobbies, [gameLobbyName = createGameLobbyObject.name, lobbyPassword = createGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName; }) == gameLobbies.end ())
      {
        if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                      [accountName = user.accountName] (auto const &gameLobby) {
                                                        auto const &accountNames = gameLobby.accountNames;
                                                        return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                      });
            gameLobbyWithUser != gameLobbies.end ())
          {
            sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameLobbyError{ { "account has already a game lobby with the name: " + gameLobbyWithUser->name.value_or ("Quick Game Lobby") } }));
            return;
          }
        else
          {
            // TODO place a useful lambda to send msg to all users in game lobby
            auto &newGameLobby = gameLobbies.emplace_back (GameLobby{ createGameLobbyObject.name, createGameLobbyObject.password, [] (auto, auto) {} });
            if (newGameLobby.tryToAddUser (user))
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
                sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbySuccess{}));
                sendMsgToUser (objectToStringWithObjectName (usersInGameLobby));
                return;
              }
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameLobbyError{ { "lobby already exists with name: " + createGameLobbyObject.name } }));
        return;
      }
  }

  void
  joinGameLobby (user_matchmaking::JoinGameLobby const &joinGameLobbyObject)
  {
    if (auto gameLobby = ranges::find_if (gameLobbies, [gameLobbyName = joinGameLobbyObject.name, lobbyPassword = joinGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName && _gameLobby.password == lobbyPassword; }); gameLobby != gameLobbies.end ())
      {
        if (auto error = gameLobby->tryToAddUser (user))
          {
            sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ joinGameLobbyObject.name, error.value () }));
            return;
          }
        else
          {
            sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbySuccess{}));
            auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
            usersInGameLobby.maxUserSize = gameLobby->maxUserCount ();
            usersInGameLobby.name = gameLobby->name.value ();
            usersInGameLobby.durakGameOption = gameLobby->gameOption;
            ranges::transform (gameLobby->accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
            // TODO do something so we can send to all accounts in game lobby
            // gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
            return;
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ joinGameLobbyObject.name, "wrong password name combination or lobby does not exists" }));
        return;
      }
  }

  void
  setMaxUserSizeInCreateGameLobby (user_matchmaking::SetMaxUserSizeInCreateGameLobby const &setMaxUserSizeInCreateGameLobbyObject)
  {
    auto accountNameToSearch = user.accountName;
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user.accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames;
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {
        if (gameLobbyWithAccount->getWaitingForAnswerToStartGame ())
          {
            sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "It is not allowed to change lobby while ask to start a game is running" }));
          }
        else
          {
            if (gameLobbyWithAccount->isGameLobbyAdmin (user.accountName))
              {
                if (auto errorMessage = gameLobbyWithAccount->setMaxUserCount (setMaxUserSizeInCreateGameLobbyObject.maxUserSize))
                  {
                    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ errorMessage.value () }));
                    return;
                  }
                else
                  {
                    // TODO do something so we can send to all accounts in game lobby
                    // gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (user_matchmaking::MaxUserSizeInCreateGameLobby{ setMaxUserSizeInCreateGameLobbyObject.maxUserSize }));
                    return;
                  }
              }
            else
              {
                sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "you need to be admin in a game lobby to change the user size" }));
                return;
              }
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "could not find a game lobby for account" }));
        return;
      }
  }

  void
  setGameOption (shared_class::GameOption const &gameOption)
  {
    auto accountNameToSearch = user.accountName;
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user.accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames;
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {
        if (gameLobbyWithAccount->getWaitingForAnswerToStartGame ())
          {
            sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "It is not allowed to change game option while ask to start a game is running" }));
          }
        else
          {
            if (gameLobbyWithAccount->isGameLobbyAdmin (user.accountName))
              {
                gameLobbyWithAccount->gameOption = gameOption;
                // TODO do something so we can send to all accounts in game lobby
                // gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (gameOption));
                return;
              }
            else
              {
                sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "you need to be admin in the create game lobby to change game option" }));
                return;
              }
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "could not find a game lobby for account" }));
        return;
      }
  }

  void
  leaveGameLobby ()
  {
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user.accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames;
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {
        if (gameLobbyWithAccount->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers)
          {
            gameLobbyWithAccount->removeUser (user.accountName);
            if (gameLobbyWithAccount->accountCount () == 0)
              {
                gameLobbies.erase (gameLobbyWithAccount);
              }
            sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbySuccess{}));
            return;
          }
        else
          {
            sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbyError{ "not allowed to leave a game lobby which is controlled by the matchmaking system with leave game lobby" }));
            return;
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbyError{ "could not remove user from lobby user not found in lobby" }));
        return;
      }
  }

  void
  relogToGameLobby ()
  {
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user.accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames;
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {

        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToCreateGameLobbySuccess{}));
        auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
        usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
        usersInGameLobby.name = gameLobbyWithAccount->name.value ();
        usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
        ranges::transform (gameLobbyWithAccount->accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
        sendMsgToUser (objectToStringWithObjectName (usersInGameLobby));
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
      }
  }

  void
  removeUserFromGameLobby ()
  {
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user.accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames;
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {
        gameLobbyWithAccount->removeUser (user.accountName);
        if (gameLobbyWithAccount->accountCount () == 0)
          {
            gameLobbies.erase (gameLobbyWithAccount);
          }
        else
          {
            auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
            usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
            usersInGameLobby.name = gameLobbyWithAccount->name.value ();
            usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
            ranges::transform (gameLobbyWithAccount->accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
            // TODO do something so we can send to all accounts in game lobby
            // gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
      }
  }

  void
  joinMatchMakingQueue (GameLobby::LobbyType const &lobbyType)
  {
    if (ranges::find_if (gameLobbies,
                         [accountName = user.accountName] (auto const &gameLobby) {
                           auto const &accountNames = gameLobby.accountNames;
                           return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                         })
        == gameLobbies.end ())
      {
        if (auto gameLobbyToAddUser = ranges::find_if (gameLobbies, [lobbyType, accountName = user.accountName] (GameLobby const &gameLobby) { return matchingLobby (accountName, gameLobby, lobbyType); }); gameLobbyToAddUser != gameLobbies.end ())
          {
            if (auto error = gameLobbyToAddUser->tryToAddUser (user))
              {
                sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ user.accountName, error.value () }));
              }
            else
              {
                sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueSuccess{}));
                if (gameLobbyToAddUser->accountNames.size () == gameLobbyToAddUser->maxUserCount ())
                  {
                    askUsersToJoinGame (gameLobbyToAddUser);
                  }
              }
          }
        else
          {
            auto gameLobby = GameLobby{};
            gameLobby.lobbyAdminType = lobbyType;
            if (auto error = gameLobby.tryToAddUser (user))
              {
                sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ user.accountName, error.value () }));
              }
            gameLobbies.emplace_back (gameLobby);
            sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueSuccess{}));
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueError{ "User is allready in gamelobby" }));
      }
  }

  boost::asio::awaitable<void>
  wantsToJoinGame (user_matchmaking::WantsToJoinGame const &wantsToJoinGameEv)
  {
    if (auto gameLobby = ranges::find_if (gameLobbies,
                                          [accountName = user.accountName] (auto const &gameLobby) {
                                            auto const &accountNames = gameLobby.accountNames;
                                            return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                          });
        gameLobby != gameLobbies.end ())
      {
        if (wantsToJoinGameEv.answer)
          {
            if (ranges::find_if (gameLobby->readyUsers, [accountName = user.accountName] (std::string const &readyUserAccountName) { return readyUserAccountName == accountName; }) == gameLobby->readyUsers.end ())
              {
                gameLobby->readyUsers.push_back (user.accountName);
                if (gameLobby->readyUsers.size () == gameLobby->accountNames.size ())
                  {
                    co_await startGame (*gameLobby);
                    gameLobbies.erase (gameLobby);
                  }
              }
            else
              {
                sendMsgToUser (objectToStringWithObjectName (user_matchmaking::WantsToJoinGameError{ "You already accepted to join the game" }));
              }
          }
        else
          {
            gameLobby->cancelTimer ();
            if (gameLobby->lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers)
              {
                sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameStartCanceledRemovedFromQueue{}));
                gameLobby->removeUser (user.accountName);
                if (gameLobby->accountNames.empty ())
                  {
                    gameLobbies.erase (gameLobby);
                  }
              }
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::WantsToJoinGameError{ "No game to join" }));
      }
  }

  void
  leaveMatchMakingQueue ()
  {
    if (auto gameLobby = ranges::find_if (gameLobbies,
                                          [accountName = user.accountName] (auto const &gameLobby) {
                                            auto const &accountNames = gameLobby.accountNames;
                                            return gameLobby.lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers && ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                          });
        gameLobby != gameLobbies.end ())
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveQuickGameQueueSuccess{}));
        gameLobby->removeUser (user.accountName);
        gameLobby->cancelTimer ();
        if (gameLobby->accountNames.empty ())
          {
            gameLobbies.erase (gameLobby);
          }
      }
    else
      {
        sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveQuickGameQueueError{ "User is not in queue" }));
      }
  }

  void
  loginAsGuest ()
  {
    user.accountName = boost::uuids::to_string (boost::uuids::random_generator () ());
    sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAsGuestSuccess{ user.accountName }));
  }

  boost::asio::io_context &io_context;
  User user{};
  std::list<std::shared_ptr<User>> &users;
  boost::asio::thread_pool &pool;
  std::list<GameLobby> &gameLobbies;
  std::shared_ptr<CoroTimer> cancelCoroutineTimer;
  MyWebsocket<Websocket> matchmakingGame{ std::make_shared<Websocket> (io_context) };
};

#endif /* D9077A7B_F0F8_4687_B460_C6D43C94F8AF */
