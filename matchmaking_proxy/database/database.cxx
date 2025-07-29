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
createEmptyDatabase (std::string const &whereToCreateDatabase)
{
  auto const whereToCreateDatabasePath = std::filesystem::path{ whereToCreateDatabase };
  std::filesystem::remove (whereToCreateDatabasePath / std::filesystem::path{ databaseName });
  if (not std::filesystem::exists ((whereToCreateDatabasePath / "database")))
    {
      std::filesystem::create_directory (whereToCreateDatabasePath / "database");
    }
  sqlite3 *db{};
  int rc{};
  rc = sqlite3_open ((whereToCreateDatabasePath / std::filesystem::path{ databaseName }).string ().c_str (), &db);
  if (rc)
    {
      fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
      return;
    }
  sqlite3_close (db);
}

void
createDatabaseIfNotExist (std::string const &whereToCreateDatabase)
{
  auto const whereToCreateDatabasePath = std::filesystem::path{ whereToCreateDatabase };
  if (not std::filesystem::exists ((whereToCreateDatabasePath / "database")))
    {
      std::filesystem::create_directory (whereToCreateDatabasePath / "database");
    }
  sqlite3 *db{};
  int rc{};
  rc = sqlite3_open ((whereToCreateDatabasePath / std::filesystem::path{ databaseName }).string ().c_str (), &db);
  if (rc)
    {
      fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
      return;
    }
  sqlite3_close (db);
}

void
createTables (std::string const &whereToCreateDatabase)
{
  auto const whereToCreateDatabasePath = std::filesystem::path{ whereToCreateDatabase };
  soci::session sql (soci::sqlite3, (whereToCreateDatabasePath / std::filesystem::path{ databaseName }).string ().c_str ());
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
createAccount (std::string const &accountName, std::string const &password, size_t startRating, std::string const &whereToCreateDatabase)
{
  auto const whereToCreateDatabasePath = std::filesystem::path{ whereToCreateDatabase };
  soci::session sql (soci::sqlite3, (whereToCreateDatabasePath / std::filesystem::path{ databaseName }).string ().c_str ());
  return confu_soci::findStruct<Account> (sql, "accountName", confu_soci::insertStruct (sql, Account{ accountName, password, startRating }, true));
}

std::vector<Account>
getTopRatedAccounts (uint64_t count, std::string const &whereToCreateDatabase)
{
  auto const whereToCreateDatabasePath = std::filesystem::path{ whereToCreateDatabase };
  auto sql = soci::session{ soci::sqlite3, (whereToCreateDatabasePath / std::filesystem::path{ databaseName }).string ().c_str () };
  return confu_soci::findStructsOrderBy<database::Account> (sql, count, "rating", confu_soci::OrderMethod::Descending);
}

}
}