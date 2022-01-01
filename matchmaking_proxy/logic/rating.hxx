#ifndef BE8478DC_CF98_4643_A3A0_36C1F7C77C87
#define BE8478DC_CF98_4643_A3A0_36C1F7C77C87

#include "../database/database.hxx"
#include <boost/numeric/conversion/cast.hpp>
#include <cstddef>
#include <pipes/pipes.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <utility>
#include <vector>

// In the losing team the user with the most rating loses the most points
size_t ratingShareLosingTeam (size_t userRating, std::vector<size_t> const &userRatings, size_t ratingChange);
size_t averageRating (std::vector<std::string> const &accountNames);
// In the winning team the user with the most rating gets the least points
size_t ratingShareWinningTeam (size_t userRating, std::vector<size_t> const &userRatings, size_t ratingChange);
long ratingChange (size_t userRating, size_t otherUserRating, long double score, size_t ratingChangeFactor);
size_t averageRating (std::vector<size_t> const &ratings);
size_t averageRating (size_t sum, size_t elements);
std::pair<std::vector<database::Account>, std::vector<database::Account>> calcRatingLoserAndWinner (std::vector<database::Account> losers, std::vector<database::Account> winners);
std::vector<database::Account> calcRatingDraw (std::vector<database::Account> accounts);

#endif /* BE8478DC_CF98_4643_A3A0_36C1F7C77C87 */