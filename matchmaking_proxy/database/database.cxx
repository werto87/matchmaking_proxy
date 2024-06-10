#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/database/constant.hxx"
#include <boost/iterator/iterator_facade.hpp>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <filesystem>
#include <iostream>
#include <soci/error.h>
#include <soci/session.h>
#include <soci/sqlite3/soci-sqlite3.h>
#include <sqlite3.h>
#include <stdio.h>

namespace matchmaking_proxy
{
namespace database
{
void
createEmptyDatabase ()
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

void
createDatabaseIfNotExist ()
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

void
createTables ()
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

boost::optional<Account>
createAccount (std::string const &accountName, std::string const &password, size_t startRating)
{
  soci::session sql (soci::sqlite3, databaseName);
  return confu_soci::findStruct<Account> (sql, "accountName", confu_soci::insertStruct (sql, Account{ accountName, password, startRating }, true));
}
}
}