#ifndef B5E08F8D_3CB1_4FDC_BCF6_F4AEEBFBD52B
#define B5E08F8D_3CB1_4FDC_BCF6_F4AEEBFBD52B

#include <chrono>
#include <confu_algorithm/constrainedNumber.hxx>
#include <cstddef>
namespace matchmaking_proxy
{
struct MatchmakingData;
struct MatchmakingGameData;

struct MatchmakingOption
{
  confu_algorithm::ConstrainedNumber<uint64_t, confu_algorithm::notZero, 2> usersNeededToStartQuickGame{};
  confu_algorithm::ConstrainedNumber<uint64_t, confu_algorithm::notZero, 2> usersNeededToStartRankedGame{};
  size_t allowedRatingDifference{ 100 };
  std::chrono::milliseconds timeToAcceptInvite{ 10'000 };
  std::function<void (std::string const &customMessage, MatchmakingData &matchmakingData)> handleCustomMessageFromUser{};
};
}
#endif /* B5E08F8D_3CB1_4FDC_BCF6_F4AEEBFBD52B */
