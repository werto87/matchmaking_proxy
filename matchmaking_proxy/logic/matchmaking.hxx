#ifndef D9077A7B_F0F8_4687_B460_C6D43C94F8AF
#define D9077A7B_F0F8_4687_B460_C6D43C94F8AF
#include "../database/database.hxx"
#include "../pw_hash/passwordHash.hxx"
#include "../serialization.hxx"
#include "../server/gameLobby.hxx"
#include "../server/user.hxx"
#include "../util.hxx"
#include "matchmaking_proxy/logic/client.hxx"
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

std::set<std::string> inline getApiTypes ()
{
  auto result = std::set<std::string>{};
  boost::hana::for_each (shared_class::sharedClasses, [&] (const auto &x) { result.insert (confu_json::type_name<typename std::decay<decltype (x)>::type> ()); });
  return result;
}

auto const apiTypes = getApiTypes ();

bool inline isInRatingrange (size_t userRating, size_t lobbyAverageRating)
{
  auto const difference = userRating > lobbyAverageRating ? userRating - lobbyAverageRating : lobbyAverageRating - userRating;
  return difference < ALLOWED_DIFFERENCE_FOR_RANKED_GAME_MATCHMAKING;
}

bool inline checkRating (size_t userRating, std::vector<std::string> const &accountNames) { return isInRatingrange (userRating, averageRating (accountNames)); }

std::set<std::string> inline getBlockedApiFromClientToGame ()
{
  auto result = std::set<std::string>{};
  boost::hana::for_each (shared_class::blacklistClientToServer, [&] (const auto &x) { result.insert (confu_json::type_name<typename std::decay<decltype (x)>::type> ()); });
  return result;
}
auto const blockedApiFromClientToGame = getBlockedApiFromClientToGame ();

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

  void
  removeUserFromLobbyAndGame ()
  {
    auto const findLobby = [userAccountName = user->accountName] (GameLobby const &gameLobby) {
      auto const &accountNamesToCheck = gameLobby.accountNames ();
      return ranges::find_if (accountNamesToCheck, [userAccountName] (std::string const &accountNameToCheck) { return userAccountName == accountNameToCheck; }) != accountNamesToCheck.end ();
    };
    auto gameLobbyWithUser = ranges::find_if (gameLobbies, findLobby);
    while (gameLobbyWithUser != gameLobbies.end ())
      {
        gameLobbyWithUser->removeUser (user);
        if (gameLobbyWithUser->_users.empty ())
          {
            gameLobbies.erase (gameLobbyWithUser);
          }
        gameLobbyWithUser = ranges::find_if (gameLobbies, findLobby);
      }
  }

  void
  logoutAccount ()
  {
    removeUserFromLobbyAndGame ();
    user->accountName = {};
    user->msgQueueClient.clear ();
    user->communicationChannels.clear ();
    user->ignoreLogin = false;
    user->ignoreCreateAccount = false;
    user->sendMessageToUser (objectToStringWithObjectName (shared_class::LogoutAccountSuccess{}));
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
    auto startGame = shared_class::StartGame{};
    ranges::transform (gameLobby._users, ranges::back_inserter (startGame.players), [] (std::shared_ptr<User> user_) { return user_->accountName.value (); });
    startGame.gameOption = gameLobby.gameOption;
    co_await ws.async_write (buffer (objectToStringWithObjectName (startGame)));
    flat_buffer buffer;
    co_await ws.async_read (buffer, use_awaitable);
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

  bool
  allowedToSendToGameFromClient (std::string const &typeToSearch)
  {
    return not blockedApiFromClientToGame.contains (typeToSearch);
  }

  boost::asio::awaitable<void>
  createAccountAndLogin (auto &&createAccountObject, auto &sm, auto &&deps, auto &&subs)
  {
    if (user->accountName)
      {
        logoutAccount ();
      }
    soci::session sql (soci::sqlite3, databaseName);
    if (confu_soci::findStruct<database::Account> (sql, "accountName", createAccountObject.accountName))
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "account already created" }));
        co_return;
      }
    else
      {
        auto hashedPw = co_await async_hash (pool, io_context, createAccountObject.password, boost::asio::use_awaitable);
        if (user->ignoreCreateAccount)
          {
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "Canceled by User Request" }));
            co_return;
          }
        else
          {
            if (auto account = database::createAccount (createAccountObject.accountName, hashedPw))
              {
                user->accountName = account->accountName;
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::LoginAccountSuccess{ createAccountObject.accountName }));
                sm.process_event (shared_class::LoginAccountSuccess{}, deps, subs);
                co_return;
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "account already created" }));
                co_return;
              }
          }
      }
  }

  boost::asio::awaitable<void>
  loginAccount (auto &&loginAccountObject, auto &&sm, auto &&deps, auto &&subs)
  {
    if (user->accountName)
      {
        logoutAccount ();
      }
    soci::session sql (soci::sqlite3, databaseName);
    if (auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName))
      {
        if (std::find_if (users.begin (), users.end (), [accountName = account->accountName] (auto const &u) { return accountName == u->accountName; }) != users.end ())
          {
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Account already logged in" }));
            co_return;
          }
        else
          {
            if (co_await async_check_hashed_pw (pool, io_context, account->password, loginAccountObject.password, boost::asio::use_awaitable))
              {
                if (user->ignoreLogin)
                  {
                    user->sendMessageToUser (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Canceled by User Request" }));
                    co_return;
                  }
                else
                  {
                    user->accountName = account->accountName;
                    if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                                  [accountName = user->accountName] (auto const &gameLobby) {
                                                                    auto const &accountNames = gameLobby.accountNames ();
                                                                    return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                                  });
                        gameLobbyWithUser != gameLobbies.end ())
                      {
                        if (gameLobbyWithUser->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers)
                          {
                            user->sendMessageToUser (objectToStringWithObjectName (shared_class::WantToRelog{ loginAccountObject.accountName, "Create Game Lobby" }));
                          }
                        else
                          {
                            gameLobbyWithUser->removeUser (user);
                            if (gameLobbyWithUser->accountCount () == 0)
                              {
                                gameLobbies.erase (gameLobbyWithUser);
                              }
                            user->sendMessageToUser (objectToStringWithObjectName (shared_class::LoginAccountSuccess{ loginAccountObject.accountName }));
                            sm.process_event (shared_class::LoginAccountSuccess{}, deps, subs);
                          }
                        co_return;
                      }
                    else
                      {
                        user->sendMessageToUser (objectToStringWithObjectName (shared_class::LoginAccountSuccess{ loginAccountObject.accountName }));
                        sm.process_event (shared_class::LoginAccountSuccess{}, deps, subs);
                        co_return;
                      }
                  }
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Incorrect Username or Password" }));
                co_return;
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Incorrect username or password" }));
        co_return;
      }
  }

  void
  broadCastMessage (shared_class::BroadCastMessage const &broadCastMessageObject)
  {
    if (user->accountName)
      {
        for (auto &user_ : users | ranges::views::filter ([channel = broadCastMessageObject.channel, accountName = user->accountName] (auto const &user_) { return user_->communicationChannels.find (channel) != user_->communicationChannels.end (); }))
          {
            soci::session sql (soci::sqlite3, databaseName);
            auto message = shared_class::Message{ user_->accountName.value (), broadCastMessageObject.channel, broadCastMessageObject.message };
            user_->sendMessageToUser (objectToStringWithObjectName (std::move (message)));
          }
        return;
      }
    else
      {
        user->msgQueueClient.push_back (objectToStringWithObjectName (shared_class::BroadCastMessageError{ broadCastMessageObject.channel, "account not logged in" }));
        return;
      }
  }

  void
  joinChannel (shared_class::JoinChannel const &joinChannelObject)
  {
    if (user->accountName)
      {
        user->communicationChannels.insert (joinChannelObject.channel);
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinChannelSuccess{ joinChannelObject.channel }));
        return;
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinChannelError{ joinChannelObject.channel, { "user not logged in" } }));
        return;
      }
  }

  void
  leaveChannel (shared_class::LeaveChannel const &leaveChannelObject)
  {
    if (user->accountName)
      {
        if (user->communicationChannels.erase (leaveChannelObject.channel))
          {
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::LeaveChannelSuccess{ leaveChannelObject.channel }));
            return;
          }
        else
          {
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::LeaveChannelError{ leaveChannelObject.channel, { "channel not found" } }));
            return;
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::LeaveChannelError{ leaveChannelObject.channel, { "user not logged in" } }));
        return;
      }
  }

  void
  askUsersToJoinGame (std::list<GameLobby>::iterator &gameLobby)
  {
    gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::AskIfUserWantsToJoinGame{}));
    gameLobby->startTimerToAcceptTheInvite (io_context, [gameLobby, &gameLobbies = gameLobbies] () {
      auto notReadyUsers = std::vector<std::shared_ptr<User>>{};
      ranges::copy_if (gameLobby->_users, ranges::back_inserter (notReadyUsers), [usersWhichAccepted = gameLobby->readyUsers] (std::shared_ptr<User> const &user_) mutable { return ranges::find_if (usersWhichAccepted, [user_] (std::shared_ptr<User> const &userWhoAccepted) { return user_->accountName.value () == userWhoAccepted->accountName.value (); }) == usersWhichAccepted.end (); });
      for (auto const &notReadyUser : notReadyUsers)
        {
          notReadyUser->sendMessageToUser (objectToStringWithObjectName (shared_class::AskIfUserWantsToJoinGameTimeOut{}));
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
          gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::GameStartCanceled{}));
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
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::CreateGameError{ "It is not allowed to start a game while ask to start a game is running" }));
          }
        else
          {
            if (gameLobbyWithUser->isGameLobbyAdmin (user->accountName.value ()))
              {
                if (gameLobbyWithUser->accountNames ().size () >= 2)
                  {
                    if (auto gameOptionError = errorInGameOption (gameLobbyWithUser->gameOption))
                      {
                        user->sendMessageToUser (objectToStringWithObjectName (shared_class::GameOptionError{ gameOptionError.value () }));
                      }
                    else
                      {
                        askUsersToJoinGame (gameLobbyWithUser);
                      }
                  }
                else
                  {
                    user->sendMessageToUser (objectToStringWithObjectName (shared_class::CreateGameError{ "You need atleast two user to create a game" }));
                  }
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::CreateGameError{ "you need to be admin in a game lobby to start a game" }));
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::CreateGameError{ "Could not find a game lobby for the user" }));
      }
  }

  void
  createGameLobby (shared_class::CreateGameLobby const &createGameLobbyObject)
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
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::CreateGameLobbyError{ { "account has already a game lobby with the name: " + gameLobbyWithUser->name.value_or ("Quick Game Lobby") } }));
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
                auto usersInGameLobby = shared_class::UsersInGameLobby{};
                usersInGameLobby.maxUserSize = newGameLobby.maxUserCount ();
                usersInGameLobby.name = newGameLobby.name.value ();
                usersInGameLobby.durakGameOption = newGameLobby.gameOption;
                ranges::transform (newGameLobby.accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinGameLobbySuccess{}));
                user->sendMessageToUser (objectToStringWithObjectName (usersInGameLobby));
                return;
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::CreateGameLobbyError{ { "lobby already exists with name: " + createGameLobbyObject.name } }));
        return;
      }
  }

  void
  joinGameLobby (shared_class::JoinGameLobby const &joinGameLobbyObject)
  {
    if (auto gameLobby = ranges::find_if (gameLobbies, [gameLobbyName = joinGameLobbyObject.name, lobbyPassword = joinGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName && _gameLobby.password == lobbyPassword; }); gameLobby != gameLobbies.end ())
      {
        if (auto error = gameLobby->tryToAddUser (user))
          {
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinGameLobbyError{ joinGameLobbyObject.name, error.value () }));
            return;
          }
        else
          {
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinGameLobbySuccess{}));
            auto usersInGameLobby = shared_class::UsersInGameLobby{};
            usersInGameLobby.maxUserSize = gameLobby->maxUserCount ();
            usersInGameLobby.name = gameLobby->name.value ();
            usersInGameLobby.durakGameOption = gameLobby->gameOption;
            ranges::transform (gameLobby->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
            gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
            return;
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinGameLobbyError{ joinGameLobbyObject.name, "wrong password name combination or lobby does not exists" }));
        return;
      }
  }

  void
  setMaxUserSizeInCreateGameLobby (shared_class::SetMaxUserSizeInCreateGameLobby const &setMaxUserSizeInCreateGameLobbyObject)
  {
    auto accountNameToSearch = user->accountName.value ();
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user->accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames ();
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {
        if (gameLobbyWithAccount->getWaitingForAnswerToStartGame ())
          {
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ "It is not allowed to change lobby while ask to start a game is running" }));
          }
        else
          {
            if (gameLobbyWithAccount->isGameLobbyAdmin (user->accountName.value ()))
              {
                if (auto errorMessage = gameLobbyWithAccount->setMaxUserCount (setMaxUserSizeInCreateGameLobbyObject.maxUserSize))
                  {
                    user->sendMessageToUser (objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ errorMessage.value () }));
                    return;
                  }
                else
                  {
                    gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::MaxUserSizeInCreateGameLobby{ setMaxUserSizeInCreateGameLobbyObject.maxUserSize }));
                    return;
                  }
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ "you need to be admin in a game lobby to change the user size" }));
                return;
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ "could not find a game lobby for account" }));
        return;
      }
  }

  void
  setGameOption (shared_class::GameOption const &gameOption)
  {
    auto accountNameToSearch = user->accountName.value ();
    if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                     [accountName = user->accountName] (auto const &gameLobby) {
                                                       auto const &accountNames = gameLobby.accountNames ();
                                                       return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                     });
        gameLobbyWithAccount != gameLobbies.end ())
      {
        if (gameLobbyWithAccount->getWaitingForAnswerToStartGame ())
          {
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::GameOptionError{ "It is not allowed to change game option while ask to start a game is running" }));
          }
        else
          {
            if (gameLobbyWithAccount->isGameLobbyAdmin (user->accountName.value ()))
              {
                gameLobbyWithAccount->gameOption = gameOption;
                gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (gameOption));
                return;
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::GameOptionError{ "you need to be admin in the create game lobby to change game option" }));
                return;
              }
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::GameOptionError{ "could not find a game lobby for account" }));
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
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::LeaveGameLobbySuccess{}));
            return;
          }
        else
          {
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::LeaveGameLobbyError{ "not allowed to leave a game lobby which is controlled by the matchmaking system with leave game lobby" }));
            return;
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::LeaveGameLobbyError{ "could not remove user from lobby user not found in lobby" }));
        return;
      }
  }

  void
  relogTo (shared_class::RelogTo const &relogToObject)
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
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::RelogToCreateGameLobbySuccess{}));
            auto usersInGameLobby = shared_class::UsersInGameLobby{};
            usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
            usersInGameLobby.name = gameLobbyWithAccount->name.value ();
            usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
            ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
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
                auto usersInGameLobby = shared_class::UsersInGameLobby{};
                usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
                usersInGameLobby.name = gameLobbyWithAccount->name.value ();
                usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
                ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
                gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
                return;
              }
          }
      }
    else if (relogToObject.wantsToRelog)
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
        return;
      }
    return;
  }

  void
  loginAccountCancel ()
  {
    if (not user->accountName)
      {
        user->ignoreLogin = true;
      }
  }

  void
  createAccountCancel ()
  {
    if (not user->accountName)
      {
        user->ignoreCreateAccount = true;
      }
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
        if (auto gameLobbyToAddUser = ranges::find_if (gameLobbies, [lobbyType, accountName = user->accountName.value ()] (GameLobby const &gameLobby) { return matchingLobby (accountName, gameLobby, lobbyType); }); gameLobbyToAddUser != gameLobbies.end ())
          {
            if (auto error = gameLobbyToAddUser->tryToAddUser (user))
              {
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinGameLobbyError{ user->accountName.value (), error.value () }));
              }
            else
              {
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinMatchMakingQueueSuccess{}));
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
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinGameLobbyError{ user->accountName.value (), error.value () }));
              }
            gameLobbies.emplace_back (gameLobby);
            user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinMatchMakingQueueSuccess{}));
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::JoinMatchMakingQueueError{ "User is allready in gamelobby" }));
      }
  }

  boost::asio::awaitable<void>
  wantsToJoinGame (shared_class::WantsToJoinGame const &wantsToJoinGameEv)
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
            if (ranges::find_if (gameLobby->readyUsers, [accountName = user->accountName.value ()] (std::shared_ptr<User> _user) { return _user->accountName == accountName; }) == gameLobby->readyUsers.end ())
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
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::WantsToJoinGameError{ "You already accepted to join the game" }));
              }
          }
        else
          {
            gameLobby->cancelTimer ();
            if (gameLobby->lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers)
              {
                user->sendMessageToUser (objectToStringWithObjectName (shared_class::GameStartCanceledRemovedFromQueue{}));
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
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::WantsToJoinGameError{ "No game to join" }));
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
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::LeaveQuickGameQueueSuccess{}));
        gameLobby->removeUser (user);
        gameLobby->cancelTimer ();
        if (gameLobby->_users.empty ())
          {
            gameLobbies.erase (gameLobby);
          }
      }
    else
      {
        user->sendMessageToUser (objectToStringWithObjectName (shared_class::LeaveQuickGameQueueError{ "User is not in queue" }));
      }
  }

  void
  loginAsGuest ()
  {
    user->accountName = to_string (boost::uuids::random_generator () ());
    user->sendMessageToUser (objectToStringWithObjectName (shared_class::LoginAsGuestSuccess{ user->accountName.value () }));
  }
  auto
  operator() ()
  {
    using namespace sml;
    auto doCreateAccountAndLogin = [this] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (io_context, createAccountAndLogin (event, sm, deps, subs), boost::asio::detached); };
    auto doLoginAccount = [this] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (io_context, loginAccount (event, sm, deps, subs), boost::asio::detached); };
    auto doWantsToJoinGame = [this] (auto &&event, auto &&sm, auto &&deps, auto &&subs) -> void { boost::asio::co_spawn (io_context, wantsToJoinGame (event, sm, deps, subs), boost::asio::detached); };
    using namespace shared_class;
    // clang-format off
    return make_transition_table(
      // NotLoggedIn-------------------------------------------------------------------------------------------------------
      * state<NotLoggedin>  + event<CreateAccount>                    / doCreateAccountAndLogin
      , state<NotLoggedin>  + event<LoginAccount>                     / doLoginAccount
      , state<NotLoggedin>  + event<LoginAccountSuccess>                                                                =state<Loggedin>
      , state<NotLoggedin>  + event<LoginAsGuest>                     / (&Matchmaking::loginAsGuest)                    =state<Loggedin>
      , state<NotLoggedin>  + event<CreateAccountCancel>              / (&Matchmaking::createAccountCancel)
      , state<NotLoggedin>  + event<LoginAccountCancel>               / (&Matchmaking::loginAccountCancel) 
      // LoggedIn-------------------------------------------------------------------------------------------------------
      , state<Loggedin>     + event<JoinChannel>                      / (&Matchmaking::joinChannel)         
      , state<Loggedin>     + event<BroadCastMessage>                 / (&Matchmaking::broadCastMessage)         
      , state<Loggedin>     + event<LeaveChannel>                     / (&Matchmaking::leaveChannel)         
      , state<Loggedin>     + event<LogoutAccount>                    / (&Matchmaking::logoutAccount)                   =state<NotLoggedin>          
      , state<Loggedin>     + event<CreateGameLobby>                  / (&Matchmaking::createGameLobby)          
      , state<Loggedin>     + event<JoinGameLobby>                    / (&Matchmaking::joinGameLobby)          
      , state<Loggedin>     + event<SetMaxUserSizeInCreateGameLobby>  / (&Matchmaking::setMaxUserSizeInCreateGameLobby)          
      , state<Loggedin>     + event<GameOption>                       / (&Matchmaking::setGameOption)         
      , state<Loggedin>     + event<LeaveGameLobby>                   / (&Matchmaking::leaveGameLobby)         
      , state<Loggedin>     + event<RelogTo>                          / (&Matchmaking::relogTo)          
      , state<Loggedin>     + event<CreateGame>                       / (&Matchmaking::createGame)         
      , state<Loggedin>     + event<WantsToJoinGame>                  / (&Matchmaking::wantsToJoinGame)          
      , state<Loggedin>     + event<LeaveQuickGameQueue>              / (&Matchmaking::leaveMatchMakingQueue)          
      , state<Loggedin>     + event<JoinMatchMakingQueue>             / (&Matchmaking::joinMatchMakingQueue)         
      // ProxyToGame-------------------------------------------------------------------------------------------------------  
      // , state<ProxyToGame>  +event<LeaveGame>                       / //Todo Handle message to game do not forget to add a guard so  the user does not call messages which are only allowed to send from matchmaking to game for example: the user should not be allowed to send StartGame{} to game via matchmaking maybe use a guard with a black list
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
