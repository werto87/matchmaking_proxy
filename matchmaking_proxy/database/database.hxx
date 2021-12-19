#ifndef B86FE02F_B7D0_4435_9031_A334C305B294
#define B86FE02F_B7D0_4435_9031_A334C305B294

#include "confu_soci/convenienceFunctionForSoci.hxx"
#include "constant.hxx"
#include <boost/optional.hpp>
#include <filesystem>

BOOST_FUSION_DEFINE_STRUCT ((database), Account, (std::string, accountName) (std::string, password) (size_t, rating))

namespace database
{
void inline createEmptyDatabase ()
{
  std::filesystem::remove (databaseName.c_str ());
  if (not std::filesystem::exists ("database"))
    {
      std::filesystem::create_directory ("database");
    }
  using namespace sqlite_api;
  sqlite3 *db{};
  int rc{};
  rc = sqlite3_open (databaseName.c_str (), &db);
  if (rc)
    {
      fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
      return;
    }
  sqlite3_close (db);
}

void inline createDatabaseIfNotExist ()
{
  using namespace sqlite_api;
  if (not std::filesystem::exists ("database"))
    {
      std::filesystem::create_directory ("database");
    }
  sqlite3 *db{};
  int rc{};
  rc = sqlite3_open (databaseName.c_str (), &db);
  if (rc)
    {
      fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
      return;
    }
  sqlite3_close (db);
}

void inline createTables ()
{
  soci::session sql (soci::sqlite3, databaseName);
  try
    {
      confu_soci::createTableForStruct<Account> (sql);
    }
  catch (soci::soci_error const &error)
    {
      std::cout << error.get_error_message () << std::endl;
    }
}

auto constexpr START_RATING = 1500;

boost::optional<Account> inline createAccount (std::string const &accountName, std::string const &password)
{
  soci::session sql (soci::sqlite3, databaseName);
  return confu_soci::findStruct<Account> (sql, "accountName", confu_soci::insertStruct (sql, Account{ accountName, password, START_RATING }, true));
}
}

#endif /* B86FE02F_B7D0_4435_9031_A334C305B294 */
