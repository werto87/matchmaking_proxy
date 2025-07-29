#include "matchmaking_proxy/database/database.hxx"
#include "catch2/catch.hpp"
#include "util.hxx"
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/make_tuple.hpp>
#include <boost/fusion/include/tuple.hpp>
#include <boost/fusion/tuple.hpp>
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
TEST_CASE ("find top rated accounts", "[database]")
{
  database::createEmptyDatabase ("matchmaking_proxy.db");
  database::createTables ("matchmaking_proxy.db");
  database::createAccount ("aa", "myPw","matchmaking_proxy.db", 1600);
  database::createAccount ("bb", "myPw","matchmaking_proxy.db", 1300);
  database::createAccount ("cc", "myPw","matchmaking_proxy.db", 1500);
  auto sql = soci::session{ soci::sqlite3, "matchmaking_proxy.db" };
  auto result = confu_soci::findStructsOrderBy<database::Account> (sql, 100, "rating", confu_soci::OrderMethod::Descending);
  auto expectedResult = std::vector<database::Account>{ { "aa", "myPw", 1600 }, { "cc", "myPw", 1500 }, { "bb", "myPw", 1300 } };
  REQUIRE (result == expectedResult);
}