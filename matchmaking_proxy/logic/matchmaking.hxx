#ifndef D9077A7B_F0F8_4687_B460_C6D43C94F8AF
#define D9077A7B_F0F8_4687_B460_C6D43C94F8AF
#include "../database/database.hxx"
#include "../pw_hash/passwordHash.hxx"
#include "../server/gameLobby.hxx"
#include "../server/myWebsocket.hxx"
#include "../server/user.hxx" // for User
#include "../userMatchmakingSerialization.hxx"
#include "../util.hxx"
#include "matchmaking_proxy/database/constant.hxx"
#include "matchmaking_proxy/matchmakingGameSerialization.hxx"
#include <algorithm>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/sml.hpp>
#include <chrono>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <cstddef>
#include <functional>
#include <iostream>
#include <list> // for list
#include <memory>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/functional/identity.hpp>
#include <range/v3/range/dangling.hpp>
#include <soci/session.h>
#include <soci/sqlite3/soci-sqlite3.h>
#include <string>
#include <type_traits> // for move
#include <variant>     // for get
#include <vector>

namespace boost
{
namespace asio
{
class thread_pool;
}
}

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

bool isInRatingrange (size_t userRating, size_t lobbyAverageRating);

bool checkRating (size_t userRating, std::vector<std::string> const &accountNames);

bool matchingLobby (std::string const &accountName, GameLobby const &gameLobby, GameLobby::LobbyType const &lobbyType);

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
  void sendToUser (SendMessageToUser const &sendMessageToUser);

  bool isRegistered (std::string const &accountName);

  void logoutAccount ();

  boost::asio::awaitable<std::string> sendStartGameToServer (GameLobby const &gameLobby);

  boost::asio::awaitable<void> startGame (GameLobby const &gameLobby);

  bool createAccount (PasswordHashed const &passwordHash);

  std::function<bool (Matchmaking &matchmaking)> accountInDatabase = [] (Matchmaking &matchmaking) -> bool {
    soci::session sql (soci::sqlite3, databaseName);
    return confu_soci::findStruct<database::Account> (sql, "accountName", matchmaking.user.accountName).has_value ();
  };

  void cancelCreateAccount ();

  void cancelLoginAccount ();

  boost::asio::awaitable<void> abortCoroutine ();

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

  void informUserWantsToRelogToGameLobby ();

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

  void broadCastMessage (user_matchmaking::BroadCastMessage const &broadCastMessageObject);

  void informUserLoginAccountSuccess ();

  void informUserCreateAccountError ();

  void joinChannel (user_matchmaking::JoinChannel const &joinChannelObject);

  void leaveChannel (user_matchmaking::LeaveChannel const &leaveChannelObject);

  void askUsersToJoinGame (std::list<GameLobby>::iterator &gameLobby);

  void createGame ();

  void createGameLobby (user_matchmaking::CreateGameLobby const &createGameLobbyObject);

  void joinGameLobby (user_matchmaking::JoinGameLobby const &joinGameLobbyObject);

  void setMaxUserSizeInCreateGameLobby (user_matchmaking::SetMaxUserSizeInCreateGameLobby const &setMaxUserSizeInCreateGameLobbyObject);

  void setGameOption (shared_class::GameOption const &gameOption);

  void leaveGameLobby ();

  void relogToGameLobby ();

  void removeUserFromGameLobby ();

  void joinMatchMakingQueue (GameLobby::LobbyType const &lobbyType);

  boost::asio::awaitable<void> wantsToJoinGame (user_matchmaking::WantsToJoinGame const &wantsToJoinGameEv);

  void leaveMatchMakingQueue ();

  void loginAsGuest ();

  boost::asio::io_context &io_context;
  User user{};
  std::list<std::shared_ptr<User>> &users;
  boost::asio::thread_pool &pool;
  std::list<GameLobby> &gameLobbies;
  std::shared_ptr<CoroTimer> cancelCoroutineTimer;
  MyWebsocket<Websocket> matchmakingGame{ std::make_shared<Websocket> (io_context) };
};

#endif /* D9077A7B_F0F8_4687_B460_C6D43C94F8AF */
