#ifndef BBA7000A_25AB_49B5_99AD_E99C70321D4E
#define BBA7000A_25AB_49B5_99AD_E99C70321D4E
#include "src/game/rating.hxx"
#include "src/serialization.hxx"
#include "src/server/gameLobby.hxx"
#include "src/server/user.hxx"
#include "src/util.hxx"
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <memory.h>
#include <range/v3/algorithm/find_if.hpp>
#include <vector>
class Game
{
public:
  Game (std::vector<std::shared_ptr<User>> const &users, GameLobby::LobbyType lobbyType, std::function<void ()> gameOver) : users{ users }, lobbyType{ lobbyType }, gameOver{ gameOver } {}

  void relogUser (std::shared_ptr<User> const &user);
  void removeUser (std::shared_ptr<User> const &user);
  std::vector<std::shared_ptr<User>> getWinners () const;
  void gameOverChangeRating ();

  std::vector<std::shared_ptr<User>> users{};
  GameLobby::LobbyType lobbyType{};
  std::function<void ()> gameOver{}; // TODO-TEMPLATE call this inside your game logic if the game is over to remove the game from the games list
};

#endif /* BBA7000A_25AB_49B5_99AD_E99C70321D4E */
