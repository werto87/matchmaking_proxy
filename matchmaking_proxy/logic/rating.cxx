#include "rating.hxx"
#include "matchmaking_proxy/database/constant.hxx"   // for databaseName
#include "matchmaking_proxy/database/database.hxx"   // for Account
#include <algorithm>                                 // for copy
#include <boost/core/addressof.hpp>                  // for addressof
#include <boost/function/function_base.hpp>          // for has_empty_target
#include <boost/iterator/iterator_facade.hpp>        // for operator!=
#include <boost/move/utility_core.hpp>               // for move, forward
#include <boost/numeric/conversion/cast.hpp>         // for numeric_cast
#include <boost/optional/optional.hpp>               // for optional
#include <boost/type_index/type_index_facade.hpp>    // for operator==
#include <confu_soci/convenienceFunctionForSoci.hxx> // for findStruct
#include <iosfwd>                                    // for ostream
#include <iostream>                                  // for operator<<, cout
#include <math.h>                                    // for rintl, lrintl
#include <pipes/for_each.hpp>                        // for for_each_pipeline
#include <pipes/impl/pipes_assembly.hpp>             // for generic_pipeline
#include <pipes/operator.hpp>                        // for operator>>=
#include <pipes/push_back.hpp>                       // for push_back_pipeline
#include <pipes/transform.hpp>                       // for transform_pipe
#include <range/v3/algorithm/find_if.hpp>            // for find_if, find_i...
#include <range/v3/functional/identity.hpp>          // for identity
#include <range/v3/numeric/accumulate.hpp>           // for accumulate, acc...
#include <soci/session.h>                            // for session
#include <soci/sqlite3/soci-sqlite3.h>               // for sqlite3, sqlite...
#include <stdlib.h>                                  // for abort
// In the losing team the user with the most rating loses the most points

size_t
ratingShareLosingTeam (size_t userRating, std::vector<size_t> const &userRatings, size_t ratingChange)
{
  auto const sum = ranges::accumulate (userRatings, boost::numeric_cast<long double> (0), [] (size_t sum_, size_t value) { return sum_ + value; });
  return boost::numeric_cast<size_t> (std::rintl ((userRating / sum) * ratingChange));
}
size_t
averageRating (std::vector<std::string> const &accountNames)
{

  soci::session sql (soci::sqlite3, databaseName);
  auto sumOfRatingInTheLobby = ranges::accumulate (accountNames, size_t{}, [&] (auto x, std::string const &accountToCheck) {
    if (auto userInDatabase = confu_soci::findStruct<database::Account> (sql, "accountName", accountToCheck))
      {
        return x + userInDatabase->rating;
      }
    else
      {
        std::cout << "Can not find user in database but he is in ranked queue";
        abort ();
        return x;
      }
  });
  return boost::numeric_cast<size_t> (std::rintl (boost::numeric_cast<long double> (sumOfRatingInTheLobby) / accountNames.size ()));
}

// In the winning team the user with the most rating gets the least points
size_t
ratingShareWinningTeam (size_t userRating, std::vector<size_t> const &userRatings, size_t ratingChange)
{
  auto const inverseSum = ranges::accumulate (userRatings, boost::numeric_cast<long double> (0), [] (long double sum, long double value) { return sum + (boost::numeric_cast<long double> (1) / value); });
  return boost::numeric_cast<size_t> (std::rintl ((boost::numeric_cast<long double> (1) / (boost::numeric_cast<long double> (userRating)) / inverseSum) * ratingChange));
}

long
ratingChange (size_t userRating, size_t otherUserRating, long double score, size_t ratingChangeFactor)
{
  auto const expectedScore = 1 / (1 + std::pow (10, (boost::numeric_cast<long double> (otherUserRating) - boost::numeric_cast<long double> (userRating)) / 400));
  return std::lrintl (ratingChangeFactor * (score - expectedScore));
}

size_t
averageRating (std::vector<size_t> const &ratings)
{
  auto accountsRatingSum = size_t{ 0 };
  ratings >>= pipes::for_each ([&accountsRatingSum] (auto rating) { accountsRatingSum += rating; });
  return boost::numeric_cast<size_t> (std::rintl (boost::numeric_cast<long double> (accountsRatingSum) / ratings.size ()));
}

size_t
averageRating (size_t sum, size_t elements)
{
  return boost::numeric_cast<size_t> (std::rintl (boost::numeric_cast<long double> (sum) / elements));
}
auto constexpr SCORE_WON = 1;
auto const SCORE_DRAW = boost::numeric_cast<long double> (0.5);
auto constexpr RATING_CHANGE_FACTOR = 20;
std::pair<std::vector<database::Account>, std::vector<database::Account>>
calcRatingLoserAndWinner (std::vector<database::Account> losers, std::vector<database::Account> winners)
{
  auto losersRatings = std::vector<size_t>{};
  losers >>= pipes::transform ([] (database::Account const &account) { return account.rating; }) >>= pipes::push_back (losersRatings);
  auto const averageRatingLosers = averageRating (losersRatings);
  auto winnersRatings = std::vector<size_t>{};
  winners >>= pipes::transform ([] (database::Account const &account) { return account.rating; }) >>= pipes::push_back (winnersRatings);
  auto const averageRatingWinners = averageRating (winnersRatings);
  auto totalRatingWon = ratingChange (averageRatingWinners, averageRatingLosers, SCORE_WON, RATING_CHANGE_FACTOR);
  for (auto &winner : winners)
    {
      winner.rating = winner.rating + ratingShareWinningTeam (winner.rating, winnersRatings, boost::numeric_cast<size_t> (totalRatingWon));
    }
  for (auto &loser : losers)
    {
      auto const newRating = loser.rating - ratingShareLosingTeam (loser.rating, losersRatings, boost::numeric_cast<size_t> (totalRatingWon));
      loser.rating = (newRating <= 0) ? 1 : newRating;
    }
  return std::make_pair (losers, winners);
}

std::vector<database::Account>
calcRatingDraw (std::vector<database::Account> accounts)
{
  auto ratingSum = ranges::accumulate (accounts, size_t{}, [] (size_t sum, database::Account const &account) { return sum + account.rating; });
  long double totalRatingWon = 0;
  auto losers = std::vector<database::Account>{};
  auto winners = std::vector<database::Account>{};
  for (auto &account : accounts)
    {
      if (auto ratingChanged = ratingChange (account.rating, averageRating (ratingSum - account.rating, accounts.size () - 1), SCORE_DRAW, RATING_CHANGE_FACTOR); ratingChanged < 0)
        {
          totalRatingWon -= ratingChanged;
          losers.push_back (account);
        }
      else
        {
          winners.push_back (account);
        }
    }
  auto losersRatings = std::vector<size_t>{};
  losers >>= pipes::transform ([] (database::Account const &account) { return account.rating; }) >>= pipes::push_back (losersRatings);
  auto winnersRatings = std::vector<size_t>{};
  winners >>= pipes::transform ([] (database::Account const &account) { return account.rating; }) >>= pipes::push_back (winnersRatings);
  for (auto &winner : winners)
    {
      if (auto accountToUpdateRating = ranges::find_if (accounts, [accountName = winner.accountName] (auto const &account) { return account.accountName == accountName; }); accountToUpdateRating != accounts.end ())
        {
          accountToUpdateRating->rating = winner.rating + ratingShareWinningTeam (winner.rating, winnersRatings, boost::numeric_cast<size_t> (totalRatingWon));
        }
    }
  for (auto &loser : losers)
    {
      if (auto accountToUpdateRating = ranges::find_if (accounts, [accountName = loser.accountName] (auto const &account) { return account.accountName == accountName; }); accountToUpdateRating != accounts.end ())
        {
          auto const newRating = loser.rating - ratingShareLosingTeam (loser.rating, losersRatings, boost::numeric_cast<size_t> (totalRatingWon));
          accountToUpdateRating->rating = (newRating <= 0) ? 1 : newRating;
        }
    }
  return accounts;
}