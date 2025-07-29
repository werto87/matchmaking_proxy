#pragma once
#include <functional>
#include <list>
#include <string>
#include <filesystem>

namespace matchmaking_proxy
{
class Matchmaking;
struct MatchmakingGameData
{
  std::filesystem::path fullPathIncludingDatabaseName{};
  std::list<std::shared_ptr<Matchmaking>> &stateMachines;
  std::function<void (std::string const &)> sendToGame;
};
}
