#include "database.hxx"
#include "matchmaking_proxy/database/constant.hxx"   // for databaseName
#include <confu_soci/convenienceFunctionForSoci.hxx> // for createTableForS...
#include <filesystem>                                // for create_directory
#include <iostream>                                  // for endl, basic_ost...
#include <soci/error.h>                              // for soci_error
#include <soci/session.h>                            // for session
#include <soci/sqlite3/soci-sqlite3.h>               // for sqlite3, sqlite...
#include <sqlite3.h>                                 // for sqlite3_close
#include <stdio.h>                                   // for fprintf, stderr
#include <vector>                                    // for vector

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

auto constexpr START_RATING = 1500;

boost::optional<Account>
createAccount (std::string const &accountName, std::string const &password)
{
  soci::session sql (soci::sqlite3, databaseName);
  return confu_soci::findStruct<Account> (sql, "accountName", confu_soci::insertStruct (sql, Account{ accountName, password, START_RATING }, true));
}
}