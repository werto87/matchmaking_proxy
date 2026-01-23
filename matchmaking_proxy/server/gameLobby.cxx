#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/server/user.hxx"
#include "matchmaking_proxy/util.hxx"
#include <algorithm>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/system_error.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <my_web_socket/coSpawnPrintException.hxx>
#include <string>
namespace boost
{
namespace asio
{
class io_context;
}
}
namespace matchmaking_proxy
{
GameLobby::GameLobby (std::string name_, std::string password_) : name{ std::move (name_) }, password (std::move (password_)) {}

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
      if (std::ranges::none_of (accountNames, [&accountName = user.accountName] (std::string const &accountInGameLobby) { return accountInGameLobby == accountName; }))
        {
          accountNames.push_back (user.accountName.value ());
          return {};
        }
      else
        {
          return "User allready in lobby with user name: " + user.accountName.value ();
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
      if (auto userToRemoveFromLobby = std::ranges::find_if (accountNames, [&userToRemoveName] (auto const &accountName) { return userToRemoveName == accountName; }); userToRemoveFromLobby != accountNames.end ())
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

void
GameLobby::removeUser (std::string const &accountNameToRemove)
{
  accountNames.erase (std::remove_if (accountNames.begin (), accountNames.end (), [&accountNameToRemove] (auto const &accountNameInGameLobby) { return accountNameToRemove == accountNameInGameLobby; }), accountNames.end ());
}

size_t
GameLobby::accountCount () const
{
  return accountNames.size ();
}

boost::asio::awaitable<void>
GameLobby::runTimer (std::shared_ptr<boost::asio::system_timer> timer, std::function<void ()> gameInviteOver)
{
  try
    {
      co_await timer->async_wait (boost::asio::use_awaitable);
      waitingForAnswerToStartGame = false;
      gameInviteOver ();
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
          std::osyncstream (std::cout) << "error in timer boost::system::errc: " << e.code () << std::endl;
          abort ();
        }
    }
  catch (std::exception &e)
    {
      std::osyncstream (std::cout) << "runTimer exception: " << e.what () << std::endl;
      abort ();
    }
}

void
GameLobby::startTimerToAcceptTheInvite (boost::asio::io_context &io_context, std::function<void ()> gameInviteOver, std::chrono::milliseconds const &timeToAcceptInvite)
{
  waitingForAnswerToStartGame = true;
  _timer = std::make_shared<boost::asio::system_timer> (io_context);
  _timer->expires_after (timeToAcceptInvite);
  co_spawn (_timer->get_executor (), [=, this] () { return runTimer (_timer, gameInviteOver); }, my_web_socket::printException);
}

void
GameLobby::cancelTimer ()
{
  if (waitingForAnswerToStartGame)
    {
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
}