#include "matchmakingGame.hxx"
#include "matchmaking.hxx"
#include "matchmaking_proxy/database/constant.hxx"
#include "matchmaking_proxy/database/database.hxx" // for Account
#include "matchmaking_proxy/logic/rating.hxx"
#include "matchmaking_proxy/matchmakingGameSerialization.hxx"
#include "matchmaking_proxy/userMatchmakingSerialization.hxx"
#include "matchmaking_proxy/util.hxx"
#include <boost/sml.hpp>
#include <confu_json/concept.hxx>
#include <confu_json/confu_json.hxx>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
using namespace boost::sml;

struct MatchmakingGameDependencies
{
  std::list<Matchmaking> &stateMachines;
  std::function<void (std::string const &)> sendToGame{};
};

auto const accountNamesToAccounts = [] (std::vector<std::string> const &accountNames) {
  return accountNames | ranges::views::transform ([] (auto const &accountName) {
           soci::session sql (soci::sqlite3, databaseName);
           return confu_soci::findStruct<database::Account> (sql, "accountName", accountName);
         })
         | ranges::views::filter ([] (boost::optional<database::Account> const &optionalAccount) { return optionalAccount.has_value (); }) | ranges::views::transform ([] (auto const &optionalAccount) { return optionalAccount.value (); }) | ranges::to<std::vector<database::Account>> ();
};

auto const gameOver = [] (matchmaking_game::GameOver const &gameOver, MatchmakingGameDependencies &matchmakingGameDependencies) {
  if (gameOver.ratedGame)
    {
      if (gameOver.draws.empty ())
        {
          auto [winners, losers] = calcRatingLoserAndWinner (accountNamesToAccounts (gameOver.losers), accountNamesToAccounts (gameOver.winners));
          for (auto const &account : winners)
            {
              soci::session sql (soci::sqlite3, databaseName);
              confu_soci::upsertStruct (sql, account);
            }
          for (auto const &account : losers)
            {
              soci::session sql (soci::sqlite3, databaseName);
              confu_soci::upsertStruct (sql, account);
            }
        }
      else
        {
          for (auto const &account : calcRatingDraw (accountNamesToAccounts (gameOver.draws)))
            {
              soci::session sql (soci::sqlite3, databaseName);
              confu_soci::upsertStruct (sql, account);
            }
        }
    }
  matchmakingGameDependencies.sendToGame (objectToStringWithObjectName (matchmaking_game::GameOverSuccess{}));
};

auto const isLoggedIn = [] (matchmaking_game::UserLeftGame const &userLeftGame, MatchmakingGameDependencies &matchmakingGameDependencies) { return ranges::find (matchmakingGameDependencies.stateMachines, true, [accountName = userLeftGame.accountName] (const Matchmaking &matchmaking) { return matchmaking.isLoggedInWithAccountName (accountName); }) != matchmakingGameDependencies.stateMachines.end (); };
auto const hasProxy = [] (matchmaking_game::UserLeftGame const &userLeftGame, MatchmakingGameDependencies &matchmakingGameDependencies) { return ranges::find (matchmakingGameDependencies.stateMachines, true, [accountName = userLeftGame.accountName] (const Matchmaking &matchmaking) { return matchmaking.isLoggedInWithAccountName (accountName) && matchmaking.hasProxyToGame (); }) != matchmakingGameDependencies.stateMachines.end (); };

auto const userLeftGameErrorNotLoggedIn = [] (matchmaking_game::UserLeftGame const &userLeftGame, MatchmakingGameDependencies &matchmakingGameDependencies) { matchmakingGameDependencies.sendToGame (objectToStringWithObjectName (matchmaking_game::UserLeftGameError{ userLeftGame.accountName, "User not logged in" })); };
auto const userLeftGameErrorUserHasNoProxy = [] (matchmaking_game::UserLeftGame const &userLeftGame, MatchmakingGameDependencies &matchmakingGameDependencies) { matchmakingGameDependencies.sendToGame (objectToStringWithObjectName (matchmaking_game::UserLeftGameError{ userLeftGame.accountName, "User not in proxy state" })); };

auto const cancelProxyToGame = [] (matchmaking_game::UserLeftGame const &userLeftGame, MatchmakingGameDependencies &matchmakingGameDependencies) {
  if (auto matchmaking = ranges::find (matchmakingGameDependencies.stateMachines, true, [accountName = userLeftGame.accountName] (const Matchmaking &matchmaking) { return matchmaking.isLoggedInWithAccountName (accountName); }); matchmaking == matchmakingGameDependencies.stateMachines.end ())
    {
      matchmaking->disconnectFromProxy ();
    }
  matchmakingGameDependencies.sendToGame (objectToStringWithObjectName (matchmaking_game::UserLeftGameSuccess{ userLeftGame.accountName }));
};

class StateMachineImpl
{
public:
  auto
  operator() () const noexcept
  {
    namespace u_m = user_matchmaking;
    namespace m_g = matchmaking_game;
    // clang-format off
    return make_transition_table(
  // Default-----------------------------------------------------------------------------------------------------------------------------------------------------------------
* "Default"_s                          + event<m_g::GameOver>                             / gameOver
, "Default"_s                          + event<m_g::UserLeftGame>   [isLoggedIn]          / userLeftGameErrorNotLoggedIn
, "Default"_s                          + event<m_g::UserLeftGame>   [hasProxy]            / userLeftGameErrorUserHasNoProxy
, "Default"_s                          + event<m_g::UserLeftGame>                         / cancelProxyToGame
      );
    }
};



struct my_logger
{
  template <class SM, class TEvent>
  void
  log_process_event (const TEvent &event)
  {
    if constexpr (confu_json::is_adapted_struct<TEvent>::value)
      {
        std::cout << "\n[" << aux::get_type_name<SM> () << "]"
                  << "[process_event] '" << objectToStringWithObjectName (event) << "'" << std::endl;
      }
    else
      {
        printf ("[%s][process_event] %s\n", aux::get_type_name<SM> (), aux::get_type_name<TEvent> ());
      }
  }

  template <class SM, class TGuard, class TEvent>
  void
  log_guard (const TGuard &, const TEvent &, bool result)
  {
    printf ("[%s][guard]\t  '%s' %s\n", aux::get_type_name<SM> (), aux::get_type_name<TGuard> (), (result ? "[OK]" : "[Reject]"));
  }

  template <class SM, class TAction, class TEvent>
  void
  log_action (const TAction &, const TEvent &)
  {
    printf ("[%s][action]\t '%s' \n", aux::get_type_name<SM> (), aux::get_type_name<TAction> ());
  }

  template <class SM, class TSrcState, class TDstState>
  void
  log_state_change (const TSrcState &src, const TDstState &dst)
  {
    printf ("[%s][transition]\t  '%s' -> '%s'\n", aux::get_type_name<SM> (), src.c_str (), dst.c_str ());
  }
};
struct MatchmakingGame::StateMachineWrapper
{
  StateMachineWrapper (MatchmakingGame *owner,MatchmakingGameDependencies matchmakingGameDependencies_) : matchmakingGameDependencies{std::move(matchmakingGameDependencies_)},
  impl (owner,
#ifdef LOG_FOR_STATE_MACHINE
                                                                                              logger,
#endif
                                                                                              matchmakingGameDependencies){}

  MatchmakingGameDependencies matchmakingGameDependencies;

#ifdef LOG_FOR_STATE_MACHINE
  my_logger logger;
  boost::sml::sm<StateMachineImpl, boost::sml::logger<my_logger>> impl;
#else
  boost::sml::sm<StateMachineImpl> impl;
#endif
};

void // has to be after YourClass::StateMachineWrapper definition
MatchmakingGame::StateMachineWrapperDeleter::operator() (StateMachineWrapper *p)
{
  delete p;
}


MatchmakingGame::MatchmakingGame(std::list<Matchmaking> &stateMachines_, std::function<void (std::string const &)> sendToGame): sm{ new StateMachineWrapper{this, MatchmakingGameDependencies{stateMachines_,sendToGame}} } {}



void MatchmakingGame::process_event (std::string const &event) {
{
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, event, boost::is_any_of ("|"));
  if (splitMesssage.size () == 2)
    {
      auto const &typeToSearch = splitMesssage.at (0);
      auto const &objectAsString = splitMesssage.at (1);
      bool typeFound = false;
      boost::hana::for_each (matchmaking_game::matchmakingGame, [&] (const auto &x) {
            if (typeToSearch == confu_json::type_name<typename std::decay<decltype (x)>::type> ())
              {
                typeFound = true;
                boost::json::error_code ec{};
                sm->impl.process_event (confu_json::to_object<std::decay_t<decltype (x)>> (confu_json::read_json (objectAsString, ec)));
                if (ec) std::cout << "read_json error: " << ec.message () << std::endl;
                return;
              }
          });
          if (not typeFound) std::cout << "could not find a match for typeToSearch in matchmakingGame '" << typeToSearch << "'" << std::endl;
    }
  else
    {
      std::cout << "Not supported event. event syntax: EventName|JsonObject" << std::endl;
    }
}

}
