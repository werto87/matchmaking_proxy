#pragma once

#include <login_matchmaking_game_shared/matchmakingGameSerialization.hxx>
// clang-format off
namespace matchmaking_game{
static boost::hana::tuple<
UnhandledMessageError,
StartGameError,
StartGameSuccess,
LeaveGameServer,
LeaveGameSuccess,
LeaveGameError,
GameOver,
StartGame,
GameOverSuccess,
GameOverError,
UserLeftGame,
UserLeftGameSuccess,
UserLeftGameError,
ConnectToGame,
ConnectToGameError,
ConnectToGameSuccess
  >  const matchmakingGame{};
}
// clang-format on
