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

struct GameLobby
{

  GameLobby () = default;
  GameLobby (std::string name, std::string password, std::function<void (std::vector<std::string> const &accountNames, std::string msg)> sendToUsersInGameLobby_);
  ~GameLobby ();

  enum struct LobbyType
  {
    FirstUserInLobbyUsers,
    MatchMakingSystemUnranked,
    MatchMakingSystemRanked
  };

  std::optional<std::string> setMaxUserCount (size_t userMaxCount);

  bool isGameLobbyAdmin (std::string const &accountName) const;

  size_t maxUserCount () const;

  std::optional<std::string> tryToAddUser (User const &user);

  bool tryToRemoveUser (std::string const &userWhoTriesToRemove, std::string const &userToRemoveName);

  bool tryToRemoveAdminAndSetNewAdmin ();

  bool removeUser (std::string const &accountNameToRemove);

  size_t accountCount ();

  boost::asio::awaitable<void> runTimer (std::shared_ptr<boost::asio::system_timer> timer, std::function<void ()> gameOverCallback);

  void startTimerToAcceptTheInvite (boost::asio::io_context &io_context, std::function<void ()> gameInviteOver);

  void cancelTimer ();
  bool getWaitingForAnswerToStartGame () const;

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
