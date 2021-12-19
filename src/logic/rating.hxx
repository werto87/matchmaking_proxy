#ifndef BE8478DC_CF98_4643_A3A0_36C1F7C77C87
#define BE8478DC_CF98_4643_A3A0_36C1F7C77C87

#include "src/database/database.hxx"
#include <cstddef>
#include <utility>
#include <vector>

long ratingChange (size_t userRating, size_t otherUserRating, long double score, size_t ratingChangeFactor);

size_t averageRating (std::vector<size_t> const &ratings);

size_t ratingShareWinningTeam (size_t userRating, std::vector<size_t> const &userRatings, size_t ratingChange);

size_t ratingShareLosingTeam (size_t userRating, std::vector<size_t> const &userRatings, size_t ratingChange);

size_t averageRating (size_t sum, size_t elements);

std::pair<std::vector<database::Account>, std::vector<database::Account>> calcRatingLoserAndWinner (std::vector<database::Account> losers, std::vector<database::Account> winners);

std::vector<database::Account> calcRatingDraw (std::vector<database::Account> accounts);

size_t averageRating (std::vector<std::string> const &accountNames);
#endif /* BE8478DC_CF98_4643_A3A0_36C1F7C77C87 */