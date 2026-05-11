#include "matchmaking_proxy/database/database.hxx"
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/make_tuple.hpp>
#include <boost/fusion/include/tuple.hpp>
#include <boost/fusion/tuple.hpp>
#include <catch2/catch_all.hpp>
#include <iostream>
#include <soci/soci.h>
#include <string>
#include <vector>

namespace matchmaking_proxy::database
{
bool
operator== (Account const &lhs, Account const &rhs)
{
  return lhs.accountName == rhs.accountName && lhs.password == rhs.password && lhs.rating == rhs.rating;
}
}

using namespace matchmaking_proxy;
TEST_CASE ("find top rated accounts", "[database]")
{
  auto const fullPathToDatabase = std::string{ PATH_TO_BINARY } + std::format ("/{}.db", boost::uuids::to_string (boost::uuids::random_generator () ()));
  {
    database::createEmptyDatabase (fullPathToDatabase);
    database::createTables (fullPathToDatabase);
    database::createAccount ("aa", "myPw", fullPathToDatabase, 1600);
    database::createAccount ("bb", "myPw", fullPathToDatabase, 1300);
    database::createAccount ("cc", "myPw", fullPathToDatabase, 1500);
    auto sql = soci::session{ soci::sqlite3, fullPathToDatabase };
    auto result = confu_soci::findStructsOrderBy<database::Account> (sql, 100, "rating", confu_soci::OrderMethod::Descending);
    auto expectedResult = std::vector<database::Account>{ { "aa", "myPw", 1600 }, { "cc", "myPw", 1500 }, { "bb", "myPw", 1300 } };
    CHECK (result == expectedResult);
  }
  std::filesystem::remove (fullPathToDatabase);
}