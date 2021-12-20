#ifndef DBE82937_D6AB_4777_A3C8_A62B68300AA3
#define DBE82937_D6AB_4777_A3C8_A62B68300AA3

#include "../serialization.hxx"
#include "../util.hxx"
#include "user.hxx"
#include <algorithm>
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
  GameLobby (std::string name, std::string password) : name{ std::move (name) }, password (std::move (password)) {}
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
        if (userMaxCount < _users.size ())
          {
            return "userMaxCount < _users.size ()";
          }
        else
          {
            _maxUserCount = userMaxCount;
          }
      }
    return {};
  }

  std::vector<std::string>
  accountNames () const
  {
    auto result = std::vector<std::string>{};
    ranges::transform (_users, ranges::back_inserter (result), [] (auto const &user) { return user->accountName.value_or ("Error User is not Logged  in but still in GameLobby"); });
    return result;
  }

  bool
  isGameLobbyAdmin (std::string const &accountName) const
  {
    return lobbyAdminType == LobbyType::FirstUserInLobbyUsers && _users.front ()->accountName.value () == accountName;
  }

  size_t
  maxUserCount () const
  {
    return _maxUserCount;
  }

  std::optional<std::string>
  tryToAddUser (std::shared_ptr<User> const &user)
  {
    if (_maxUserCount > _users.size ())
      {
        if (ranges::none_of (_users, [accountName = user->accountName.value ()] (std::shared_ptr<User> const &user) { return user->accountName == accountName; }))
          {
            _users.push_back (user);
            return {};
          }
        else
          {
            return "User allready in lobby with user name: " + user->accountName.value ();
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
        if (auto userToRemoveFromLobby = ranges::find_if (_users, [&userToRemoveName] (auto const &user) { return userToRemoveName == user->accountName; }); userToRemoveFromLobby != _users.end ())
          {
            _users.erase (userToRemoveFromLobby);
            return true;
          }
      }
    return false;
  }

  bool
  tryToRemoveAdminAndSetNewAdmin ()
  {
    if (not _users.empty ())
      {
        _users.erase (_users.begin ());
        return true;
      }
    else
      {
        return false;
      }
  }

  void
  sendToAllAccountsInGameLobby (std::string const &message)
  {
    // TODO do not push messages in a queue with offline user
    // if user is offline but in lobby user message queue gets filled but the messages get never send because of relogUser() overrides user with another user before the messages gets send. This is fine but we still push messages in a queue and override it later
    ranges::for_each (_users, [&message] (auto &user) { user->sendMessageToUser (message); });
  }

  bool
  removeUser (std::shared_ptr<User> const &user)
  {
    _users.erase (std::remove_if (_users.begin (), _users.end (), [accountName = user->accountName.value ()] (auto const &_user) { return accountName == _user->accountName.value (); }), _users.end ());
    if (lobbyAdminType == LobbyType::FirstUserInLobbyUsers)
      {
        auto usersInGameLobby = shared_class::UsersInGameLobby{};
        usersInGameLobby.maxUserSize = maxUserCount ();
        usersInGameLobby.name = name.value ();
        usersInGameLobby.durakGameOption = gameOption;
        ranges::transform (accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
        sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
      }
    return _users.empty ();
  }

  size_t
  accountCount ()
  {
    return _users.size ();
  }

  void
  relogUser (std::shared_ptr<User> &user)
  {
    if (auto oldLogin = ranges::find_if (_users, [accountName = user->accountName.value ()] (auto const &_user) { return accountName == _user->accountName.value (); }); oldLogin != _users.end ())
      {
        *oldLogin = user;
      }
    else
      {
        throw std::logic_error{ "can not relog user beacuse he is not logged in the create game lobby" };
      }
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
        sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::GameStartCanceled{}));
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

  std::vector<std::shared_ptr<User>> _users{};
  shared_class::GameOption gameOption{};
  std::vector<std::shared_ptr<User>> readyUsers{};
  LobbyType lobbyAdminType = LobbyType::FirstUserInLobbyUsers;
  std::optional<std::string> name{};
  std::string password{};

private:
  std::shared_ptr<boost::asio::system_timer> _timer;
  bool waitingForAnswerToStartGame = false;
  size_t _maxUserCount{ 2 };
};

#endif /* DBE82937_D6AB_4777_A3C8_A62B68300AA3 */
