#ifndef D9077A7B_F0F8_4687_B460_C6D43C94F8AF
#define D9077A7B_F0F8_4687_B460_C6D43C94F8AF
#include "../database/database.hxx"
#include "../pw_hash/passwordHash.hxx"
#include "../server/gameLobby.hxx"
#include "../server/myWebsocket.hxx"
#include "../server/user.hxx" // for User
#include "../userMatchmakingSerialization.hxx"
#include "../util.hxx"
#include "matchmakingCallbacks.hxx"
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

struct SendMessageToUser
{
  std::string msg{};
};
struct ReciveMessage
{
};
struct NotLoggedinEv
{
};

struct ConnectToGame
{
};

struct ConnectToGameSuccess
{
};
struct ConnectToGameError
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
  Matchmaking (boost::asio::io_context &io_context_, boost::asio::thread_pool &pool_, std::list<GameLobby> &gameLobbies_, MatchmakingCallbacks matchmakingCallbacks_) : matchmakingCallbacks{ matchmakingCallbacks_ }, io_context{ io_context_ }, pool{ pool_ }, gameLobbies{ gameLobbies_ }, cancelCoroutineTimer{ std::make_shared<CoroTimer> (CoroTimer{ io_context_ }) } { cancelCoroutineTimer->expires_after (std::chrono::system_clock::time_point::max () - std::chrono::system_clock::now ()); }

public:
  auto
  operator() () const noexcept
  {
    using namespace sml;
    auto doCreateAccountAndLogin = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (sml::aux::get<Matchmaking &> (deps).io_context, sml::aux::get<Matchmaking &> (deps).createAccountAndLogin (event, sm, deps, subs), boost::asio::detached); };
    auto doLoginAccount = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (sml::aux::get<Matchmaking &> (deps).io_context, sml::aux::get<Matchmaking &> (deps).loginAccount (event, sm, deps, subs), boost::asio::detached); };
    auto doConnectToGame = [] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (sml::aux::get<Matchmaking &> (deps).io_context, sml::aux::get<Matchmaking &> (deps).connectToGame (event, sm, deps, subs), boost::asio::detached); };
    namespace u_m = user_matchmaking;
    namespace m_g = matchmaking_game;
    // clang-format off
  return make_transition_table(
// NotLoggedIn-----------------------------------------------------------------------------------------------------------------------------------------------------------------
* state<NotLoggedin>                          + event<u_m::CreateAccount>                 [ accountInDatabase ]                   / (&Self::informUserCreateAccountError)
, state<NotLoggedin>                          + event<u_m::CreateAccount>                 [ not accountInDatabase ]               / doCreateAccountAndLogin                   = state<WaitingForPasswordHashed>
, state<NotLoggedin>                          + event<u_m::LoginAccount>                  [ not isRegistered ]                    / (&Self::informUserAccountNotRegistered)
, state<NotLoggedin>                          + event<u_m::LoginAccount>                  [ isLoggedin ]                          / (&Self::informUserAlreadyLoggedin)
, state<NotLoggedin>                          + event<u_m::LoginAccount>                  [ not isLoggedin and isRegistered ]     / doLoginAccount                            = state<WaitingForPasswordCheck>
, state<NotLoggedin>                          + event<u_m::LoginAsGuest>                                                          / (&Self::loginAsGuest)                     = state<Loggedin>
// WaitingForCreateAccount------------------------------------------------------------------------------------------------------------------------------------------------------
, state<WaitingForPasswordHashed>             + event<PasswordHashed>                      [ not accountInDatabase ]              / (&Self::createAccount)                          = state<Loggedin>
, state<WaitingForPasswordHashed>             + event<PasswordHashed>                      [ accountInDatabase ]                  / (&Self::informUserCreateAccountError)           = state<NotLoggedin>
, state<WaitingForPasswordHashed>             + event<u_m::CreateAccountCancel>                                                   / (&Self::cancelCreateAccount)                    = state<NotLoggedin>
// WaitingForLogin--------------------------------------------------------------------------------------------------------------------------------------------------------------
, state<WaitingForPasswordCheck>              + event<PasswordMatches>                     [ userInGameLobby ]                    / (&Self::informUserWantsToRelogToGameLobby)      = state<WaitingForUserWantsToRelogGameLobby>
, state<WaitingForPasswordCheck>              + event<PasswordMatches>                     [ not userInGameLobby ]                                                                  = state<Loggedin>
, state<WaitingForPasswordCheck>              + event<u_m::LoginAccountCancel>                                                    / (&Self::cancelLoginAccount)                     = state<NotLoggedin>
, state<WaitingForPasswordCheck>              + event<NotLoggedinEv>                                                                                                                = state<NotLoggedin>

// WaitingForPasswordCheck---------------------------------------------------------------------------------------------------------------------------------------------------------------------
, state<WaitingForUserWantsToRelogGameLobby>  + event<u_m::RelogTo>                        [ wantsToRelog ]                       / (&Self::relogToGameLobby)                       = state<Loggedin>
, state<WaitingForUserWantsToRelogGameLobby>  + event<u_m::RelogTo>                        [ not wantsToRelog ]                   / (&Self::removeUserFromGameLobby)                = state<Loggedin>
// Loggedin---------------------------------------------------------------------------------------------------------------------------------------------------------------------
, state<Loggedin>                             + on_entry<_>                                                                       / (&Self::informUserLoginAccountSuccess)
, state<Loggedin>                             + event<u_m::JoinChannel>                                                           / (&Self::joinChannel)         
, state<Loggedin>                             + event<u_m::BroadCastMessage>                                                      / (&Self::broadCastMessage)         
, state<Loggedin>                             + event<u_m::LeaveChannel>                                                          / (&Self::leaveChannel)         
, state<Loggedin>                             + event<u_m::LogoutAccount>                                                         / (&Self::logoutAccount)                          = state<NotLoggedin>          
, state<Loggedin>                             + event<u_m::CreateGameLobby>                                                       / (&Self::createGameLobby)          
, state<Loggedin>                             + event<u_m::JoinGameLobby>                                                         / (&Self::joinGameLobby)          
, state<Loggedin>                             + event<u_m::SetMaxUserSizeInCreateGameLobby>                                       / (&Self::setMaxUserSizeInCreateGameLobby)          
, state<Loggedin>                             + event<shared_class::GameOption>                                                   / (&Self::setGameOption)         
, state<Loggedin>                             + event<u_m::LeaveGameLobby>                 [ not userInGameLobby ]                / (&Self::leaveGameLobbyErrorUserNotFoundInLobby)         
, state<Loggedin>                             + event<u_m::LeaveGameLobby>                 [ userInGameLobby 
                                                                                             and not gameLobbyControlledByUsers ] / (&Self::leaveGameLobbyErrorNotControllerByUsers)         
, state<Loggedin>                             + event<u_m::LeaveGameLobby>                 [ userInGameLobby 
                                                                                             and gameLobbyControlledByUsers ]     / (&Self::leaveGameLobby)         
, state<Loggedin>                             + event<u_m::CreateGame>                                                            / (&Self::createGame)         
, state<Loggedin>                             + event<u_m::WantsToJoinGame>                                                       / (&Self::wantsToJoinAGameWrapper)          
, state<Loggedin>                             + event<u_m::LeaveQuickGameQueue>                                                   / (&Self::leaveMatchMakingQueue)          
, state<Loggedin>                             + event<u_m::JoinMatchMakingQueue>                                                  / (&Self::joinMatchMakingQueue)         
, state<Loggedin>                             + event<m_g::StartGameSuccess>                                                                                                        = state<ProxyToGame>          
, state<Loggedin>                             + event<ConnectToGame>                                                              / doConnectToGame
, state<Loggedin>                             + event<ConnectToGameSuccess>                                                                                                         =state<ProxyToGame>
// ProxyToGame------------------------------------------------------------------------------------------------------------------------------------------------------------------  
, state<ProxyToGame>                          +event<m_g::LeaveGameSuccess>                                                                                                         = state<Loggedin>     
, state<ProxyToGame>                          +event<u_m::SendMessageToGame>                                                          / (&Self::sendToGame)
// ReciveMessage------------------------------------------------------------------------------------------------------------------------------------------------------------------  
,*state<ReciveMessage>                        +event<SendMessageToUser>                                                           / (&Self::sendToUser)
  );
    // clang-format on
  }

  MatchmakingCallbacks matchmakingCallbacks{};
  User user{};

private:
  void sendToUser (SendMessageToUser const &sendMessageToUser);

  std::function<bool (user_matchmaking::LoginAccount const &loginAccount)> isRegistered = [] (user_matchmaking::LoginAccount const &loginAccount) -> bool {
    soci::session sql (soci::sqlite3, databaseName);
    return confu_soci::findStruct<database::Account> (sql, "accountName", loginAccount.accountName).has_value ();
  };

  void sendToAllAccountsInUsersCreateGameLobby (std::string const &message);

  void logoutAccount ();

  std::function<bool (user_matchmaking::LoginAccount const &loginAccount, Matchmaking &matchmaking)> isLoggedin = [] (user_matchmaking::LoginAccount const &loginAccount, Matchmaking &matchmaking) -> bool { return matchmaking.matchmakingCallbacks.isLoggedin (loginAccount.accountName); };

  boost::asio::awaitable<std::string> sendStartGameToServer (GameLobby const &gameLobby);

  boost::asio::awaitable<void> startGame (GameLobby const &gameLobby);

  void createAccount (PasswordHashed const &passwordHash);

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
    using namespace boost::asio::experimental::awaitable_operators;
    std::variant<std::string, std::monostate> hashedPw = co_await(async_hash (pool, io_context, createAccountObject.password, boost::asio::use_awaitable) || abortCoroutine ());
    if (std::holds_alternative<std::string> (hashedPw))
      {
        user.accountName = createAccountObject.accountName;
        sm.process_event (PasswordHashed{ std::get<std::string> (hashedPw) }, deps, subs);
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
  std::function<bool (Matchmaking &matchmaking)> gameLobbyControlledByUsers = [] (Matchmaking &matchmaking) -> bool {
    auto gameLobby = ranges::find_if (matchmaking.gameLobbies, [accountName = matchmaking.user.accountName] (auto const &gameLobby) {
      auto const &accountNames = gameLobby.accountNames;
      return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
    });
    return gameLobby->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers;
  };

  std::function<bool (user_matchmaking::RelogTo const &relogTo)> wantsToRelog = [] (user_matchmaking::RelogTo const &relogTo) -> bool { return relogTo.wantsToRelog; };

  boost::asio::awaitable<void>
  loginAccount (auto &&loginAccountObject, auto &&sm, auto &&deps, auto &&subs)
  {
    soci::session sql (soci::sqlite3, databaseName);
    auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName);
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
            matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ loginAccountObject.accountName, "Incorrect Username or Password" }));
            sm.process_event (NotLoggedinEv{}, deps, subs);
            co_return;
          }
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

  void createAccountAndInformUser (PasswordHashed const &passwordHash);

  void informUserAlreadyLoggedin ();

  void informUserAccountNotRegistered ();

  void leaveGameLobbyErrorUserNotFoundInLobby ();

  void leaveGameLobbyErrorNotControllerByUsers ();

  boost::asio::awaitable<void>
  connectToGame (auto &&, auto &&sm, auto &&deps, auto &&subs)
  {
    {
      auto ws = std::make_shared<Websocket> (Websocket{ io_context });
      auto gameEndpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 };
      try
        {
          co_await ws->next_layer ().async_connect (gameEndpoint);
          ws->next_layer ().expires_never ();
          ws->set_option (boost::beast::websocket::stream_base::timeout::suggested (boost::beast::role_type::client));
          ws->set_option (boost::beast::websocket::stream_base::decorator ([] (boost::beast::websocket::request_type &req) { req.set (boost::beast::http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async"); }));
          co_await ws->async_handshake ("localhost:" + std::to_string (gameEndpoint.port ()), "/");
          sm.process_event (ConnectToGameSuccess{}, deps, subs);
          matchmakingGame = std::move (ws);
          co_spawn (io_context, matchmakingGame.readLoop ([&matchmakingCallbacks = matchmakingCallbacks] (std::string const &readResult) { matchmakingCallbacks.sendMsgToUser (readResult); }), boost::asio::detached);
          co_spawn (io_context, matchmakingGame.writeLoop (), boost::asio::detached);
        }
      catch (std::exception &e)
        {
          sm.process_event (ConnectToGameError{}, deps, subs);
        }
    }
  }

  void wantsToJoinAGameWrapper (user_matchmaking::WantsToJoinGame const &wantsToJoinGameEv);

  void sendToGame (user_matchmaking::SendMessageToGame const &sendMessageToGame);

  boost::asio::io_context &io_context;
  boost::asio::thread_pool &pool;
  std::list<GameLobby> &gameLobbies;
  std::shared_ptr<CoroTimer> cancelCoroutineTimer;
  MyWebsocket<Websocket> matchmakingGame{ {} };
};

#endif /* D9077A7B_F0F8_4687_B460_C6D43C94F8AF */
