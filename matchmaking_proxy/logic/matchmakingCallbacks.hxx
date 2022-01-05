#ifndef EF055195_9705_4B72_95D6_DE9A39D27574
#define EF055195_9705_4B72_95D6_DE9A39D27574

#include <functional>

struct MatchmakingCallbacks
{
  std::function<void (std::string msgToSend)> sendMsgToUser{};
  std::function<void (std::string const &msgToSend, std::vector<std::string> const &accountsToSendMessageTo)> sendMsgToUsers{};
  std::function<bool (std::string const &accountName)> isLoggedin{};
};

#endif /* EF055195_9705_4B72_95D6_DE9A39D27574 */
