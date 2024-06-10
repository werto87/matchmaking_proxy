#ifndef F85705C8_6F01_4F50_98CA_5636F5F5E1C1
#define F85705C8_6F01_4F50_98CA_5636F5F5E1C1

#include <set>
#include <string>
namespace matchmaking_proxy
{
struct User
{
  std::string accountName{};
  std::set<std::string> communicationChannels{};
};
}
#endif /* F85705C8_6F01_4F50_98CA_5636F5F5E1C1 */
