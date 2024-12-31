#ifndef F85705C8_6F01_4F50_98CA_5636F5F5E1C1
#define F85705C8_6F01_4F50_98CA_5636F5F5E1C1

#include <optional>
#include <set>
#include <string>
namespace matchmaking_proxy
{
struct User
{
  std::optional<std::string> accountName{}; // has_value() == true means user is logged in
  std::set<std::string> communicationChannels{};
};
}
#endif /* F85705C8_6F01_4F50_98CA_5636F5F5E1C1 */
