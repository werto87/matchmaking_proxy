#ifndef B5E08F8D_3CB1_4FDC_BCF6_F4AEEBFBD52B
#define B5E08F8D_3CB1_4FDC_BCF6_F4AEEBFBD52B

#include <chrono>
#include <cstddef>
namespace matchmaking_proxy
{
struct MatchmakingOption
{
  size_t usersNeededToStartQuickGame{ 2 };
  size_t usersNeededToStartRankedGame{ 2 };
  size_t allowedRatingDifference{ 100 };
  std::chrono::milliseconds timeToAcceptInvite{ 10'000 };
};
}
#endif /* B5E08F8D_3CB1_4FDC_BCF6_F4AEEBFBD52B */
