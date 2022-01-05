#include "gameLobby.hxx"
#include "matchmaking_proxy/server/user.hxx"                  // for User
#include "matchmaking_proxy/userMatchmakingSerialization.hxx" // for UsersI...
#include "matchmaking_proxy/util.hxx"                         // for object...
#include <algorithm>                                          // for remove_if
#include <chrono>                                             // for seconds
#include <experimental/coroutine>                             // for suspen...
#include <iostream>                                           // for string
#include <new>                                                // for operat...
#include <range/v3/algorithm/find_if.hpp>                     // for find_if
#include <range/v3/algorithm/none_of.hpp>                     // for none_of
#include <range/v3/algorithm/transform.hpp>                   // for transform
#include <range/v3/functional/identity.hpp>                   // for identity
#include <range/v3/iterator/insert_iterators.hpp>             // for back_i...
#include <ratio>                                              // for ratio
#include <stdlib.h>                                           // for abort
#include <string>                                             // for operat...
#include <type_traits>                                        // for move
#include <utility>                                            // for pair

GameLobby::GameLobby (std::string name, std::string password, std::function<void (std::string const &msgToSend, std::vector<std::string> const &accountsToSendMessageTo)> sendToUsersInGameLobby_) : name{ std::move (name) }, password (std::move (password)), sendToUsersInGameLobby{ sendToUsersInGameLobby_ } {}

GameLobby::~GameLobby ()
{
  if (_timer) _timer->cancel ();
}

std::optional<std::string>
GameLobby::setMaxUserCount (size_t userMaxCount)
{
  if (userMaxCount < 1)
    {
      return "userMaxCount < 1";
    }
  else
    {
      if (userMaxCount < accountNames.size ())
        {
          return "userMaxCount < accountNames.size ()";
        }
      else
        {
          _maxUserCount = userMaxCount;
        }
    }
  return {};
}

bool
GameLobby::isGameLobbyAdmin (std::string const &accountName) const
{
  return lobbyAdminType == LobbyType::FirstUserInLobbyUsers && accountNames.front () == accountName;
}

size_t
GameLobby::maxUserCount () const
{
  return _maxUserCount;
}

std::optional<std::string>
GameLobby::tryToAddUser (User const &user)
{
  if (_maxUserCount > accountNames.size ())
    {
      if (ranges::none_of (accountNames, [&accountName = user.accountName] (std::string const &accountInGameLobby) { return accountInGameLobby == accountName; }))
        {
          accountNames.push_back (user.accountName);
          return {};
        }
      else
        {
          return "User allready in lobby with user name: " + user.accountName;
        }
    }
  else
    {
      return "Lobby full";
    }
}

bool
GameLobby::tryToRemoveUser (std::string const &userWhoTriesToRemove, std::string const &userToRemoveName)
{
  if (isGameLobbyAdmin (userWhoTriesToRemove) && userWhoTriesToRemove != userToRemoveName)
    {
      if (auto userToRemoveFromLobby = ranges::find_if (accountNames, [&userToRemoveName] (auto const &accountName) { return userToRemoveName == accountName; }); userToRemoveFromLobby != accountNames.end ())
        {
          accountNames.erase (userToRemoveFromLobby);
          return true;
        }
    }
  return false;
}

bool
GameLobby::tryToRemoveAdminAndSetNewAdmin ()
{
  if (not accountNames.empty ())
    {
      accountNames.erase (accountNames.begin ());
      return true;
    }
  else
    {
      return false;
    }
}

bool
GameLobby::removeUser (std::string const &accountNameToRemove)
{
  accountNames.erase (std::remove_if (accountNames.begin (), accountNames.end (), [&accountNameToRemove] (auto const &accountNameInGameLobby) { return accountNameToRemove == accountNameInGameLobby; }), accountNames.end ());
  if (lobbyAdminType == LobbyType::FirstUserInLobbyUsers)
    {
      auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
      usersInGameLobby.maxUserSize = maxUserCount ();
      usersInGameLobby.name = name.value ();
      usersInGameLobby.durakGameOption = gameOption;
      ranges::transform (accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
      sendToUsersInGameLobby (objectToStringWithObjectName (usersInGameLobby), accountNames);
    }
  return accountNames.empty ();
}

size_t
GameLobby::accountCount ()
{
  return accountNames.size ();
}

boost::asio::awaitable<void>
GameLobby::runTimer (std::shared_ptr<boost::asio::system_timer> timer, std::function<void ()> gameOverCallback)
{
  try
    {
      co_await timer->async_wait (boost::asio::use_awaitable);
      waitingForAnswerToStartGame = false;
      gameOverCallback ();
    }
  catch (boost::system::system_error &e)
    {
      using namespace boost::system::errc;
      if (operation_canceled == e.code ())
        {
          // swallow cancel
        }
      else
        {
          std::cout << "error in timer boost::system::errc: " << e.code () << std::endl;
          abort ();
        }
    }
}

auto constexpr TIME_TO_ACCEPT_THE_INVITE = std::chrono::seconds{ 10 };

void
GameLobby::startTimerToAcceptTheInvite (boost::asio::io_context &io_context, std::function<void ()> gameInviteOver)
{
  waitingForAnswerToStartGame = true;
  _timer = std::make_shared<boost::asio::system_timer> (io_context);
  _timer->expires_after (TIME_TO_ACCEPT_THE_INVITE);
  co_spawn (
      _timer->get_executor (), [=] () { return runTimer (_timer, gameInviteOver); }, boost::asio::detached);
}

void
GameLobby::cancelTimer ()
{
  if (waitingForAnswerToStartGame)
    {
      sendToUsersInGameLobby (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}), accountNames);
      readyUsers.clear ();
      _timer->cancel ();
      waitingForAnswerToStartGame = false;
    }
}

bool
GameLobby::getWaitingForAnswerToStartGame () const
{
  return waitingForAnswerToStartGame;
}