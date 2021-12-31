#ifndef DBE82937_D6AB_4777_A3C8_A62B68300AA3
#define DBE82937_D6AB_4777_A3C8_A62B68300AA3

#include "../userMatchmakingSerialization.hxx"
#include "../util.hxx"
#include "user.hxx"
#include <algorithm>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <range/v3/algorithm.hpp>
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/range.hpp>
#include <stdexcept>
#include <string>

auto constexpr TIME_TO_ACCEPT_THE_INVITE = std::chrono::seconds{ 10 };

struct GameLobby
{

  GameLobby () = default;
  GameLobby (std::string name, std::string password, std::function<void (std::vector<std::string> const &accountNames, std::string msg)> sendToUsersInGameLobby_) : name{ std::move (name) }, password (std::move (password)), sendToUsersInGameLobby{ sendToUsersInGameLobby_ } {}
  ~GameLobby ()
  {
    if (_timer) _timer->cancel ();
  }

  enum struct LobbyType
  {
    FirstUserInLobbyUsers,
    MatchMakingSystemUnranked,
    MatchMakingSystemRanked
  };

  std::optional<std::string>
  setMaxUserCount (size_t userMaxCount)
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
  isGameLobbyAdmin (std::string const &accountName) const
  {
    return lobbyAdminType == LobbyType::FirstUserInLobbyUsers && accountNames.front () == accountName;
  }

  size_t
  maxUserCount () const
  {
    return _maxUserCount;
  }

  std::optional<std::string>
  tryToAddUser (User const &user)
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
  tryToRemoveUser (std::string const &userWhoTriesToRemove, std::string const &userToRemoveName)
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
  tryToRemoveAdminAndSetNewAdmin ()
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
  removeUser (std::string const &accountNameToRemove)
  {
    accountNames.erase (std::remove_if (accountNames.begin (), accountNames.end (), [&accountNameToRemove] (auto const &accountNameInGameLobby) { return accountNameToRemove == accountNameInGameLobby; }), accountNames.end ());
    if (lobbyAdminType == LobbyType::FirstUserInLobbyUsers)
      {
        auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
        usersInGameLobby.maxUserSize = maxUserCount ();
        usersInGameLobby.name = name.value ();
        usersInGameLobby.durakGameOption = gameOption;
        ranges::transform (accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
        sendToUsersInGameLobby (accountNames, objectToStringWithObjectName (usersInGameLobby));
      }
    return accountNames.empty ();
  }

  size_t
  accountCount ()
  {
    return accountNames.size ();
  }

  boost::asio::awaitable<void>
  runTimer (std::shared_ptr<boost::asio::system_timer> timer, std::function<void ()> gameOverCallback)
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

  void
  startTimerToAcceptTheInvite (boost::asio::io_context &io_context, std::function<void ()> gameInviteOver)
  {
    waitingForAnswerToStartGame = true;
    _timer = std::make_shared<boost::asio::system_timer> (io_context);
    _timer->expires_after (TIME_TO_ACCEPT_THE_INVITE);
    co_spawn (
        _timer->get_executor (), [=] () { return runTimer (_timer, gameInviteOver); }, boost::asio::detached);
  }

  void
  cancelTimer ()
  {
    if (waitingForAnswerToStartGame)
      {
        sendToUsersInGameLobby (accountNames, objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}));
        readyUsers.clear ();
        _timer->cancel ();
        waitingForAnswerToStartGame = false;
      }
  }

  bool
  getWaitingForAnswerToStartGame () const
  {
    return waitingForAnswerToStartGame;
  }

  std::vector<std::string> accountNames{};
  shared_class::GameOption gameOption{};
  std::vector<std::string> readyUsers{};
  LobbyType lobbyAdminType = LobbyType::FirstUserInLobbyUsers;
  std::optional<std::string> name{};
  std::string password{};

private:
  std::shared_ptr<boost::asio::system_timer> _timer;
  bool waitingForAnswerToStartGame = false;
  size_t _maxUserCount{ 2 };
  std::function<void (std::vector<std::string> const &accountNames, std::string msg)> sendToUsersInGameLobby{};
};

#endif /* DBE82937_D6AB_4777_A3C8_A62B68300AA3 */
