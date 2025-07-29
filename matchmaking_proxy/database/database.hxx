#ifndef B86FE02F_B7D0_4435_9031_A334C305B294
#define B86FE02F_B7D0_4435_9031_A334C305B294

#include "confu_soci/convenienceFunctionForSoci.hxx"
#include <boost/optional.hpp>
#include <filesystem>

BOOST_FUSION_DEFINE_STRUCT ((matchmaking_proxy::database), Account, (std::string, accountName) (std::string, password) (size_t, rating))
namespace matchmaking_proxy
{
namespace database
{
void createEmptyDatabase (std::string const &fullPathIncludingDatabaseName);

void createDatabaseIfNotExist (std::string const &fullPathIncludingDatabaseName);

void createTables (std::string const &fullPathIncludingDatabaseName);

std::vector<Account> getTopRatedAccounts (uint64_t count, std::string const &fullPathIncludingDatabaseName);

boost::optional<Account> createAccount (std::string const &accountName, std::string const &password,std::string const& fullPathIncludingDatabaseName, size_t startRating = 1500);
}
}
#endif /* B86FE02F_B7D0_4435_9031_A334C305B294 */
