#ifndef D9077A7B_F0F8_4687_B460_C6D43C94F8AF
#define D9077A7B_F0F8_4687_B460_C6D43C94F8AF
#include "../database/database.hxx"
#include "../pw_hash/passwordHash.hxx"
#include "../server/gameLobby.hxx"
#include "../server/user.hxx"
#include "../userMatchmakingSerialization.hxx"
#include "../util.hxx"
#include "matchmaking_proxy/logic/client.hxx"
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
  std::string accountName{};
};
struct WaitingForPasswordHashed
{
};

struct WaitingForPasswordUnHashed
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

typedef boost::asio::use_awaitable_t<>::as_default_on_t<boost::asio::basic_waitable_timer<boost::asio::chrono::system_clock>> CoroTimer;

auto constexpr ALLOWED_DIFFERENCE_FOR_RANKED_GAME_MATCHMAKING = size_t{ 100 };

bool inline isInRatingrange (size_t userRating, size_t lobbyAverageRating)
{
  auto const difference = userRating > lobbyAverageRating ? userRating - lobbyAverageRating : lobbyAverageRating - userRating;
  return difference < ALLOWED_DIFFERENCE_FOR_RANKED_GAME_MATCHMAKING;
}

bool inline checkRating (size_t userRating, std::vector<std::string> const &accountNames) { return isInRatingrange (userRating, averageRating (accountNames)); }

bool inline matchingLobby (std::string const &accountName, GameLobby const &gameLobby, GameLobby::LobbyType const &lobbyType)
{
  if (gameLobby.lobbyAdminType == lobbyType && gameLobby._users.size () < gameLobby.maxUserCount ())
    {
      if (lobbyType == GameLobby::LobbyType::MatchMakingSystemRanked)
        {
          soci::session sql (soci::sqlite3, databaseName);
          if (auto userInDatabase = confu_soci::findStruct<database::Account> (sql, "accountName", accountName))
            {
              return checkRating (userInDatabase->rating, gameLobby.accountNames ());
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
  Matchmaking (boost::asio::io_context &io_context_, std::shared_ptr<User> user_, std::list<std::shared_ptr<User>> &users_, boost::asio::thread_pool &pool_, std::list<GameLobby> &gameLobbies_) : io_context{ io_context_ }, user{ user_ }, users{ users_ }, pool{ pool_ }, gameLobbies{ gameLobbies_ } {}

  bool
  isRegistered (std::string const &accountName)
  {
    soci::session sql (soci::sqlite3, databaseName);
    return confu_soci::findStruct<database::Account> (sql, "accountName", accountName).has_value ();
  }

  void
  logoutAccount ()
  {
    if (isRegistered (user->accountName))
      {
        // TODO find a way to remove user from gamelobby
        // removeUserFromLobby ();
      }
    // TODO we do not need this optional account name because we know if user is logged in based on state machine state
    user->accountName = {};
    user->msgQueueClient.clear ();
    user->communicationChannels.clear ();
    user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LogoutAccountSuccess{}));
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
    ranges::transform (gameLobby._users, ranges::back_inserter (startGame.players), [] (std::shared_ptr<User> user_) { return user_->accountName; });
    startGame.gameOption = gameLobby.gameOption;
    co_await ws.async_write (buffer (objectToStringWithObjectName (startGame)));
    flat_buffer buffer;
    co_await ws.async_read (buffer);
    auto msg = buffers_to_string (buffer.data ());
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
                for (auto &user_ : gameLobby._users)
                  {
                    user_->connectionToGame = std::make_shared<Websocket> (io_context);
                    auto gameEndpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 };
                    co_await user_->connectionToGame->next_layer ().async_connect (gameEndpoint, boost::asio::use_awaitable);
                    user_->connectionToGame->next_layer ().expires_never ();
                    user_->connectionToGame->set_option (boost::beast::websocket::stream_base::timeout::suggested (boost::beast::role_type::client));
                    user_->connectionToGame->set_option (boost::beast::websocket::stream_base::decorator ([] (boost::beast::websocket::request_type &req) { req.set (boost::beast::http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async"); }));
                    co_await user_->connectionToGame->async_handshake ("localhost:" + std::to_string (gameEndpoint.port ()), "/");
                    co_spawn (
                        io_context, [user_] { return user_->readFromGame (); }, boost::asio::detached);
                    co_spawn (
                        io_context, [user_] { return user_->writeToGame (); }, boost::asio::detached);
                    user_->sendMessageToUser (startServerAnswer);
                  }
              }
            else if (typeToSearch == "StartGameError")
              {
                for (auto &user_ : gameLobby._users)
                  {
                    user_->sendMessageToUser (startServerAnswer);
                  }
              }
          }
      }
    catch (std::exception &e)
      {
        std::cout << "Start Game exception: " << e.what () << std::endl;
      }
  }

  void
  passwordHashed (PasswordHashed const &passwordHash)
  {
    // if (true) // depends on state
    //   {
    if (auto account = database::createAccount (user->accountName, passwordHash.hashedPassword))
      {
        user->accountName = account->accountName;
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountSuccess{ user->accountName }));
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountError{ user->accountName, "account already created" }));
      }
    // }
    // else
    //   {
    //     user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountError{ user->accountName, "Canceled by User Request" }));
    //   }
  }

  boost::asio::awaitable<void>
  createAccountAndLogin (auto &&createAccountObject, auto &sm, auto &&deps, auto &&subs)
  {
    soci::session sql (soci::sqlite3, databaseName);
    if (confu_soci::findStruct<database::Account> (sql, "accountName", createAccountObject.accountName))
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountError{ createAccountObject.accountName, "account already created" }));
        co_return;
      }
    else
      {
        auto hashedPw = co_await async_hash (pool, io_context, createAccountObject.password, boost::asio::use_awaitable);
        sm.process_event (PasswordHashed{ hashedPw }, deps, subs);
      }
  }

  void
  informUserAboutCancelCreateAccount ()
  {
    user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountCancel{}));
  }
  void
  informUserAboutCancelLogin ()
  {
    user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountCancel{}));
  }

  void
  passwordMatches (PasswordMatches const &passwordMatchesEv)
  {
    user->accountName = passwordMatchesEv.accountName;
    if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                  [accountName = user->accountName] (auto const &gameLobby) {
                                                    auto const &accountNames = gameLobby.accountNames ();
                                                    return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                  });
        gameLobbyWithUser != gameLobbies.end ())
      {
        if (gameLobbyWithUser->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers)
          {
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::WantToRelog{ user->accountName, "Create Game Lobby" }));
          }
        else
          {
            gameLobbyWithUser->removeUser (user);
            if (gameLobbyWithUser->accountCount () == 0)
              {
                gameLobbies.erase (gameLobbyWithUser);
              }
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountSuccess{ user->accountName }));
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountSuccess{ user->accountName }));
      }
  }

  boost::asio::awaitable<void>
  loginAccount (auto &&loginAccountObject, auto &&sm, auto &&deps, auto &&subs)
  {
    soci::session sql (soci::sqlite3, databaseName);
    if (auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName))
      {
        if (std::find_if (users.begin (), users.end (), [accountName = account->accountName] (auto const &u) { return accountName == u->accountName; }) != users.end ())
          {
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ loginAccountObject.accountName, "Account already logged in" }));
            co_return;
          }
        else
          {
            if (co_await async_check_hashed_pw (pool, io_context, account->password, loginAccountObject.password, boost::asio::use_awaitable))
              {
                sm.process_event (PasswordMatches{ account->accountName }, deps, subs);
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ loginAccountObject.accountName, "Incorrect Username or Password" }));
                co_return;
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ loginAccountObject.accountName, "Incorrect username or password" }));
        co_return;
      }
  }

  void
  broadCastMessage (user_matchmaking::BroadCastMessage const &broadCastMessageObject)
  {
    for (auto &user_ : users | ranges::views::filter ([channel = broadCastMessageObject.channel, accountName = user->accountName] (auto const &user_) { return user_->communicationChannels.find (channel) != user_->communicationChannels.end (); }))
      {
        soci::session sql (soci::sqlite3, databaseName);
        auto message = user_matchmaking::Message{ user_->accountName, broadCastMessageObject.channel, broadCastMessageObject.message };
        user_->sendMessageToUser (objectToStringWithObjectName (std::move (message)));
      }
  }

  void
  joinChannel (user_matchmaking::JoinChannel const &joinChannelObject)
  {
    user->communicationChannels.insert (joinChannelObject.channel);
    user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::JoinChannelSuccess{ joinChannelObject.channel }));
  }

  void
  leaveChannel (user_matchmaking::LeaveChannel const &leaveChannelObject)
  {
    if (user->communicationChannels.erase (leaveChannelObject.channel))
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LeaveChannelSuccess{ leaveChannelObject.channel }));
        return;
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LeaveChannelError{ leaveChannelObject.channel, { "channel not found" } }));
        return;
      }
  }

  void
  askUsersToJoinGame (std::list<GameLobby>::iterator &gameLobby)
  {
    gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGame{}));
    gameLobby->startTimerToAcceptTheInvite (io_context, [gameLobby, &gameLobbies = gameLobbies] () {
      auto notReadyUsers = std::vector<std::shared_ptr<User>>{};
      ranges::copy_if (gameLobby->_users, ranges::back_inserter (notReadyUsers), [usersWhichAccepted = gameLobby->readyUsers] (std::shared_ptr<User> const &user_) mutable { return ranges::find_if (usersWhichAccepted, [user_] (std::shared_ptr<User> const &userWhoAccepted) { return user_->accountName == userWhoAccepted->accountName; }) == usersWhichAccepted.end (); });
      for (auto const &notReadyUser : notReadyUsers)
        {
          notReadyUser->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGameTimeOut{}));
          if (gameLobby->lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers)
            {
              gameLobby->removeUser (notReadyUser);
            }
        }
      if (gameLobby->_users.empty ())
        {
          gameLobbies.erase (gameLobby);
        }
      else
        {
          gameLobby->readyUsers.clear ();
          gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}));
        }
    });
  }

  void
  createGame ()
  {
    if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                  [accountName = user->accountName] (auto const &gameLobby) {
                                                    auto const &accountNames = gameLobby.accountNames ();
                                                    return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                  });
        gameLobbyWithUser != gameLobbies.end ())
      {
        if (gameLobbyWithUser->getWaitingForAnswerToStartGame ())
          {
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "It is not allowed to start a game while ask to start a game is running" }));
          }
        else
          {
            if (gameLobbyWithUser->isGameLobbyAdmin (user->accountName))
              {
                if (gameLobbyWithUser->accountNames ().size () >= 2)
                  {
                    if (auto gameOptionError = errorInGameOption (gameLobbyWithUser->gameOption))
                      {
                        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ gameOptionError.value () }));
                      }
                    else
                      {
                        askUsersToJoinGame (gameLobbyWithUser);
                      }
                  }
                else
                  {
                    user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "You need atleast two user to create a game" }));
                  }
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "you need to be admin in a game lobby to start a game" }));
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "Could not find a game lobby for the user" }));
      }
  }

  void
  createGameLobby (user_matchmaking::CreateGameLobby const &createGameLobbyObject)
  {
    if (ranges::find_if (gameLobbies, [gameLobbyName = createGameLobbyObject.name, lobbyPassword = createGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName; }) == gameLobbies.end ())
      {
        if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                      [accountName = user->accountName] (auto const &gameLobby) {
                                                        auto const &accountNames = gameLobby.accountNames ();
                                                        return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                      });
            gameLobbyWithUser != gameLobbies.end ())
          {
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::CreateGameLobbyError{ { "account has already a game lobby with the name: " + gameLobbyWithUser->name.value_or ("Quick Game Lobby") } }));
            return;
          }
        else
          {
            auto &newGameLobby = gameLobbies.emplace_back (GameLobby{ createGameLobbyObject.name, createGameLobbyObject.password });
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
                ranges::transform (newGameLobby.accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
                user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbySuccess{}));
                user->sendMessageToUser (objectToStringWithObjectName (usersInGameLobby));
                return;
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::CreateGameLobbyError{ { "lobby already exists with name: " + createGameLobbyObject.name } }));
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
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ joinGameLobbyObject.name, error.value () }));
            return;
          }
        else
          {
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbySuccess{}));
            auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
            usersInGameLobby.maxUserSize = gameLobby->maxUserCount ();
            usersInGameLobby.name = gameLobby->name.value ();
            usersInGameLobby.durakGameOption = gameLobby->gameOption;
            ranges::transform (gameLobby->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
            gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
            return;
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ joinGameLobbyObject.name, "wrong password name combination or lobby does not exists" }));
        return;
      }
  }

  void
  setMaxUserSizeInCreateGameLobby (user_matchmaking::SetMaxUserSizeInCreateGameLobby const &setMaxUserSizeInCreateGameLobbyObject)
  {
    auto accountNameToSearch = user->accountName;
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user->accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames ();
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {
        if (gameLobbyWithAccount->getWaitingForAnswerToStartGame ())
          {
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "It is not allowed to change lobby while ask to start a game is running" }));
          }
        else
          {
            if (gameLobbyWithAccount->isGameLobbyAdmin (user->accountName))
              {
                if (auto errorMessage = gameLobbyWithAccount->setMaxUserCount (setMaxUserSizeInCreateGameLobbyObject.maxUserSize))
                  {
                    user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ errorMessage.value () }));
                    return;
                  }
                else
                  {
                    gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (user_matchmaking::MaxUserSizeInCreateGameLobby{ setMaxUserSizeInCreateGameLobbyObject.maxUserSize }));
                    return;
                  }
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "you need to be admin in a game lobby to change the user size" }));
                return;
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "could not find a game lobby for account" }));
        return;
      }
  }

  void
  setGameOption (shared_class::GameOption const &gameOption)
  {
    auto accountNameToSearch = user->accountName;
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user->accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames ();
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {
        if (gameLobbyWithAccount->getWaitingForAnswerToStartGame ())
          {
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "It is not allowed to change game option while ask to start a game is running" }));
          }
        else
          {
            if (gameLobbyWithAccount->isGameLobbyAdmin (user->accountName))
              {
                gameLobbyWithAccount->gameOption = gameOption;
                gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (gameOption));
                return;
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "you need to be admin in the create game lobby to change game option" }));
                return;
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "could not find a game lobby for account" }));
        return;
      }
  }

  void
  leaveGameLobby ()
  {
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user->accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames ();
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {
        if (gameLobbyWithAccount->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers)
          {
            gameLobbyWithAccount->removeUser (user);
            if (gameLobbyWithAccount->accountCount () == 0)
              {
                gameLobbies.erase (gameLobbyWithAccount);
              }
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbySuccess{}));
            return;
          }
        else
          {
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbyError{ "not allowed to leave a game lobby which is controlled by the matchmaking system with leave game lobby" }));
            return;
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbyError{ "could not remove user from lobby user not found in lobby" }));
        return;
      }
  }

  void
  relogTo (user_matchmaking::RelogTo const &relogToObject)
  {
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user->accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames ();
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {
        if (relogToObject.wantsToRelog)
          {
            gameLobbyWithAccount->relogUser (user);
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::RelogToCreateGameLobbySuccess{}));
            auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
            usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
            usersInGameLobby.name = gameLobbyWithAccount->name.value ();
            usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
            ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
            user->sendMessageToUser (objectToStringWithObjectName (usersInGameLobby));
            return;
          }
        else
          {
            gameLobbyWithAccount->removeUser (user);
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
                ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
                gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
                return;
              }
          }
      }
    else if (relogToObject.wantsToRelog)
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
        return;
      }
    return;
  }

  void
  joinMatchMakingQueue (GameLobby::LobbyType const &lobbyType)
  {
    if (ranges::find_if (gameLobbies,
                         [accountName = user->accountName] (auto const &gameLobby) {
                           auto const &accountNames = gameLobby.accountNames ();
                           return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                         })
        == gameLobbies.end ())
      {
        if (auto gameLobbyToAddUser = ranges::find_if (gameLobbies, [lobbyType, accountName = user->accountName] (GameLobby const &gameLobby) { return matchingLobby (accountName, gameLobby, lobbyType); }); gameLobbyToAddUser != gameLobbies.end ())
          {
            if (auto error = gameLobbyToAddUser->tryToAddUser (user))
              {
                user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ user->accountName, error.value () }));
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueSuccess{}));
                if (gameLobbyToAddUser->_users.size () == gameLobbyToAddUser->maxUserCount ())
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
                user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ user->accountName, error.value () }));
              }
            gameLobbies.emplace_back (gameLobby);
            user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueSuccess{}));
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueError{ "User is allready in gamelobby" }));
      }
  }

  boost::asio::awaitable<void>
  wantsToJoinGame (user_matchmaking::WantsToJoinGame const &wantsToJoinGameEv)
  {
    if (auto gameLobby = ranges::find_if (gameLobbies,
                                          [accountName = user->accountName] (auto const &gameLobby) {
                                            auto const &accountNames = gameLobby.accountNames ();
                                            return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                          });
        gameLobby != gameLobbies.end ())
      {
        if (wantsToJoinGameEv.answer)
          {
            if (ranges::find_if (gameLobby->readyUsers, [accountName = user->accountName] (std::shared_ptr<User> _user) { return _user->accountName == accountName; }) == gameLobby->readyUsers.end ())
              {
                gameLobby->readyUsers.push_back (user);
                if (gameLobby->readyUsers.size () == gameLobby->_users.size ())
                  {
                    co_await startGame (*gameLobby);
                    gameLobbies.erase (gameLobby);
                  }
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::WantsToJoinGameError{ "You already accepted to join the game" }));
              }
          }
        else
          {
            gameLobby->cancelTimer ();
            if (gameLobby->lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers)
              {
                user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::GameStartCanceledRemovedFromQueue{}));
                gameLobby->removeUser (user);
                if (gameLobby->_users.empty ())
                  {
                    gameLobbies.erase (gameLobby);
                  }
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::WantsToJoinGameError{ "No game to join" }));
      }
  }

  void
  leaveMatchMakingQueue ()
  {
    if (auto gameLobby = ranges::find_if (gameLobbies,
                                          [accountName = user->accountName] (auto const &gameLobby) {
                                            auto const &accountNames = gameLobby.accountNames ();
                                            return gameLobby.lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers && ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                          });
        gameLobby != gameLobbies.end ())
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LeaveQuickGameQueueSuccess{}));
        gameLobby->removeUser (user);
        gameLobby->cancelTimer ();
        if (gameLobby->_users.empty ())
          {
            gameLobbies.erase (gameLobby);
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LeaveQuickGameQueueError{ "User is not in queue" }));
      }
  }

  void
  loginAsGuest ()
  {
    user->accountName = to_string (boost::uuids::random_generator () ());
    user->sendMessageToUser (objectToStringWithObjectName (user_matchmaking::LoginAsGuestSuccess{ user->accountName }));
  }
  auto
  operator() ()
  {
    using namespace sml;
    auto doCreateAccountAndLogin = [this] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (io_context, createAccountAndLogin (event, sm, deps, subs), boost::asio::detached); };
    auto doLoginAccount = [this] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (io_context, loginAccount (event, sm, deps, subs), boost::asio::detached); };
    // clang-format off
    return make_transition_table(
      // NotLoggedIn-----------------------------------------------------------------------------------------------------------------------------------------------------------------
      * state<NotLoggedin>                + event<user_matchmaking::CreateAccount>                    / doCreateAccountAndLogin                           = state<WaitingForPasswordHashed>
      , state<NotLoggedin>                + event<user_matchmaking::LoginAccount>                     / doLoginAccount                                    = state<WaitingForPasswordUnHashed>
      , state<NotLoggedin>                + event<user_matchmaking::LoginAsGuest>                     / (&Matchmaking::loginAsGuest)                      = state<Loggedin>
      , state<NotLoggedin>                + event<PasswordHashed>                                     / (&Matchmaking::informUserAboutCancelCreateAccount)
      , state<NotLoggedin>                + event<PasswordMatches>                                    / (&Matchmaking::informUserAboutCancelLogin)        
      // WaitingForCreateAccount------------------------------------------------------------------------------------------------------------------------------------------------------
      , state<WaitingForPasswordHashed>   + event<PasswordHashed>                                     / (&Matchmaking::passwordHashed)                    = state<Loggedin>
      , state<WaitingForPasswordHashed>   + event<user_matchmaking::CreateAccountCancel>                                                                  = state<NotLoggedin>
      // WaitingForLogin--------------------------------------------------------------------------------------------------------------------------------------------------------------
      , state<WaitingForPasswordUnHashed> + event<PasswordMatches>                                    / (&Matchmaking::passwordMatches)                   = state<Loggedin>
      , state<WaitingForPasswordUnHashed> + event<user_matchmaking::LoginAccountCancel>                                                                   = state<NotLoggedin>
      // Loggedin---------------------------------------------------------------------------------------------------------------------------------------------------------------------
      , state<Loggedin>                   + event<user_matchmaking::JoinChannel>                      / (&Matchmaking::joinChannel)         
      , state<Loggedin>                   + event<user_matchmaking::BroadCastMessage>                 / (&Matchmaking::broadCastMessage)         
      , state<Loggedin>                   + event<user_matchmaking::LeaveChannel>                     / (&Matchmaking::leaveChannel)         
      , state<Loggedin>                   + event<user_matchmaking::LogoutAccount>                    / (&Matchmaking::logoutAccount)                     = state<NotLoggedin>          
      , state<Loggedin>                   + event<user_matchmaking::CreateGameLobby>                  / (&Matchmaking::createGameLobby)          
      , state<Loggedin>                   + event<user_matchmaking::JoinGameLobby>                    / (&Matchmaking::joinGameLobby)          
      , state<Loggedin>                   + event<user_matchmaking::SetMaxUserSizeInCreateGameLobby>  / (&Matchmaking::setMaxUserSizeInCreateGameLobby)          
      , state<Loggedin>                   + event<shared_class::GameOption>                           / (&Matchmaking::setGameOption)         
      , state<Loggedin>                   + event<user_matchmaking::LeaveGameLobby>                   / (&Matchmaking::leaveGameLobby)         
      , state<Loggedin>                   + event<user_matchmaking::RelogTo>                          / (&Matchmaking::relogTo)          
      , state<Loggedin>                   + event<user_matchmaking::CreateGame>                       / (&Matchmaking::createGame)         
      , state<Loggedin>                   + event<user_matchmaking::WantsToJoinGame>                  / (&Matchmaking::wantsToJoinGame)          
      , state<Loggedin>                   + event<user_matchmaking::LeaveQuickGameQueue>              / (&Matchmaking::leaveMatchMakingQueue)          
      , state<Loggedin>                   + event<user_matchmaking::JoinMatchMakingQueue>             / (&Matchmaking::joinMatchMakingQueue)         
      , state<Loggedin>                   + event<matchmaking_game::StartGameSuccess>                                                                     = state<ProxyToGame>          
      // ProxyToGame------------------------------------------------------------------------------------------------------------------------------------------------------------------  
      , state<ProxyToGame>                +event<matchmaking_game::LeaveGameSuccess>                                                                      = state<Loggedin>     
    );
    // clang-format on
  }

private:
  boost::asio::io_context &io_context;
  std::shared_ptr<User> user{};
  std::list<std::shared_ptr<User>> &users;
  boost::asio::thread_pool &pool;
  std::list<GameLobby> &gameLobbies;
};

#endif /* D9077A7B_F0F8_4687_B460_C6D43C94F8AF */
