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
void createEmptyDatabase ();

void createDatabaseIfNotExist ();

void createTables ();

std::vector<Account> getTopRatedAccounts (uint64_t count);

boost::optional<Account> createAccount (std::string const &accountName, std::string const &password, size_t startRating = 1500);
}
}
#endif /* B86FE02F_B7D0_4435_9031_A334C305B294 */
