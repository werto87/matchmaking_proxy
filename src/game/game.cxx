#include "src/game/game.hxx"
#include "pipes/pipes.hpp"
void
Game::relogUser (std::shared_ptr<User> const &)
{
  // TODO-TEMPLATE add logic here
}

void
Game::removeUser (std::shared_ptr<User> const &)
{
  // TODO-TEMPLATE add logic here
}

std::vector<std::shared_ptr<User>>
Game::getWinners () const
{
  // TODO-TEMPLATE add logic here
  return {};
}

void
updateAccountsOnDatabaseAndSendDataToClient (std::vector<database::Account> const &accountsBeforeUpdate, std::vector<database::Account> const &accountsAfterUpdate, std::vector<std::shared_ptr<User>> users)
{
  soci::session sql (soci::sqlite3, databaseName);
  for (size_t i = 0; i < accountsBeforeUpdate.size (); i++)
    {
      confu_soci::upsertStruct (sql, accountsBeforeUpdate.at (i));
      if (auto user = ranges::find_if (users, [accountName = accountsBeforeUpdate.at (i).accountName] (std::shared_ptr<User> user) { return user->accountName.value () == accountName; }); user != users.end ())
        {
          user->get ()->sendMessageToUser (objectToStringWithObjectName (shared_class::RatingChanged{ accountsAfterUpdate.at (i).rating, accountsBeforeUpdate.at (i).rating }));
        }
    }
}

void
Game::gameOverChangeRating ()
{
  auto winners = getWinners ();
  if (winners.empty ())
    {
      auto accounts = std::vector<database::Account>{};
      soci::session sql (soci::sqlite3, databaseName);
      users >>= pipes::transform ([&sql] (auto const &user) { return confu_soci::findStruct<database::Account> (sql, "accountName", user->accountName.value ()).value (); }) >>= pipes::push_back (accounts);
      auto updatedAccounts = calcRatingDraw (accounts);
      updateAccountsOnDatabaseAndSendDataToClient (accounts, updatedAccounts, users);
    }
  else
    {
      soci::session sql (soci::sqlite3, databaseName);
      auto winnerAccounts = std::vector<database::Account>{};
      auto loserAccounts = std::vector<database::Account>{};
      users >>= pipes::transform ([&sql] (auto const &user) { return confu_soci::findStruct<database::Account> (sql, "accountName", user->accountName.value ()).value (); }) >>= pipes::partition ([&winners] (database::Account const &account) { return ranges::find_if (winners, [accountName = account.accountName] (std::shared_ptr<User> user) { return user->accountName == accountName; }) != winners.end (); }, pipes::push_back (winnerAccounts), pipes::push_back (loserAccounts));
      auto [updatedLoserAccounts, updatedWinnerAccounts] = calcRatingLoserAndWinner (loserAccounts, winnerAccounts);
      updateAccountsOnDatabaseAndSendDataToClient (loserAccounts, updatedLoserAccounts, users);
      updateAccountsOnDatabaseAndSendDataToClient (winnerAccounts, updatedWinnerAccounts, users);
    }
}
