#include "matchmaking_proxy/logic/rating.hxx"
#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/server/server.hxx"
#include "matchmaking_proxy/server/user.hxx"
#include "test/util.hxx"
#include <catch2/catch.hpp>
#include <chrono>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <cstddef>
#include <iterator>

TEST_CASE ("updateRatingWinLose", "[rating]")
{
  SECTION ("updateRatingWinLose 2 player", "[rating]")
  {
    auto const losers = std::vector<database::Account>{ { "player1", "", 1500 } };
    auto const winners = std::vector<database::Account>{ { "player2", "", 2000 } };
    auto const [updatedLosers, updatedWinners] = calcRatingLoserAndWinner (losers, winners);
    CHECK (updatedLosers.at (0).rating == 1499);
    CHECK (updatedWinners.at (0).rating == 2001);
  }
  SECTION ("updateRatingWinLose 4 player 1 heigh 3 low", "[rating]")
  {
    auto const losers = std::vector<database::Account>{ { "player1", "", 2000 } };
    auto const winners = std::vector<database::Account>{ { "player2", "", 1000 }, { "player3", "", 1000 }, { "player4", "", 1000 } };
    auto const [updatedLosers, updatedWinners] = calcRatingLoserAndWinner (losers, winners);
    CHECK (updatedLosers.at (0).rating == 1980);
    CHECK (updatedWinners.at (0).rating == 1007);
    CHECK (updatedWinners.at (1).rating == 1007);
    CHECK (updatedWinners.at (2).rating == 1007);
  }
}

TEST_CASE ("updateRatingDraw", "[rating]")
{
  SECTION ("updateRatingDraw 2 players", "[rating]")
  {
    auto const accounts = std::vector<database::Account>{ { "player1", "", 1500 }, { "player2", "", 2000 } };
    auto const updatedAccounts = calcRatingDraw (accounts);
    CHECK (updatedAccounts.at (0).rating == 1509);
    CHECK (updatedAccounts.at (1).rating == 1991);
  }
  SECTION ("updateRatingDraw 3 players", "[rating]")
  {
    auto const accounts = std::vector<database::Account>{ { "player1", "", 1500 }, { "player2", "", 2000 }, { "player3", "", 2000 } };
    auto const updatedAccounts = calcRatingDraw (accounts);
    CHECK (updatedAccounts.at (0).rating == 1512);
    CHECK (updatedAccounts.at (1).rating == 1994);
    CHECK (updatedAccounts.at (2).rating == 1994);
  }
  SECTION ("updateRatingDraw 4 players 3 low 1 high", "[rating]")
  {
    auto const accounts = std::vector<database::Account>{ { "player1", "", 1000 }, { "player2", "", 2000 }, { "player3", "", 1000 }, { "player4", "", 1000 } };
    auto const updatedAccounts = calcRatingDraw (accounts);
    CHECK (updatedAccounts.at (0).rating == 1003);
    CHECK (updatedAccounts.at (1).rating == 1990);
    CHECK (updatedAccounts.at (2).rating == 1003);
    CHECK (updatedAccounts.at (3).rating == 1003);
  }
  SECTION ("updateRatingDraw 4 players 1 low 3 high", "[rating]")
  {
    auto const accounts = std::vector<database::Account>{ { "player1", "", 2000 }, { "player2", "", 2000 }, { "player3", "", 2000 }, { "player4", "", 1000 } };
    auto const updatedAccounts = calcRatingDraw (accounts);
    CHECK (updatedAccounts.at (0).rating == 1993);
    CHECK (updatedAccounts.at (1).rating == 1993);
    CHECK (updatedAccounts.at (2).rating == 1993);
    CHECK (updatedAccounts.at (3).rating == 1021);
  }
  SECTION ("updateRatingDraw 4 players 2 low 2 high", "[rating]")
  {
    auto const accounts = std::vector<database::Account>{ { "player1", "", 2000 }, { "player2", "", 2000 }, { "player3", "", 1 }, { "player4", "", 1 } };
    auto const updatedAccounts = calcRatingDraw (accounts);
    CHECK (updatedAccounts.at (0).rating == 1990);
    CHECK (updatedAccounts.at (1).rating == 1990);
    CHECK (updatedAccounts.at (2).rating == 11);
    CHECK (updatedAccounts.at (3).rating == 11);
  }
}