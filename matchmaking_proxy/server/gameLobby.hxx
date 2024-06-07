#ifndef DBE82937_D6AB_4777_A3C8_A62B68300AA3
#define DBE82937_D6AB_4777_A3C8_A62B68300AA3

#include <boost/asio/awaitable.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#include <boost/asio/system_timer.hpp>
#pragma GCC diagnostic pop
#include <cstddef>
#include <functional>
#include <login_matchmaking_game_shared/gameOptionBase.hxx>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct User;

namespace boost
{
namespace asio
{
class io_context;
}
}

struct GameLobby
{
  GameLobby () = default;
  ~GameLobby () { cancelTimer (); }
  GameLobby (std::string name_, std::string password_);

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

  void removeUser (std::string const &accountNameToRemove);

  size_t accountCount () const;

  boost::asio::awaitable<void> runTimer (std::shared_ptr<boost::asio::system_timer> timer, std::function<void ()> gameInviteOver);

  void startTimerToAcceptTheInvite (boost::asio::io_context &io_context, std::function<void ()> gameInviteOver, std::chrono::milliseconds const &timeToAcceptInvite = std::chrono::milliseconds{ 10'000 });

  void cancelTimer ();
  bool getWaitingForAnswerToStartGame () const;

  std::vector<std::string> accountNames{};
  user_matchmaking_game::GameOptionAsString gameOptionAsString{};
  std::vector<std::string> readyUsers{};
  LobbyType lobbyAdminType = LobbyType::FirstUserInLobbyUsers;
  std::optional<std::string> name{};
  std::string password{};

private:
  std::shared_ptr<boost::asio::system_timer> _timer;
  bool waitingForAnswerToStartGame = false;
  size_t _maxUserCount{ 2 };
};

#endif /* DBE82937_D6AB_4777_A3C8_A62B68300AA3 */
