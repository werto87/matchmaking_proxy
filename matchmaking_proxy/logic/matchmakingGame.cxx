#include "matchmakingGame.hxx"
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
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
using namespace boost::sml;

struct MatchmakingGameData
{
};

auto const accountNamesToAccounts = [] (std::vector<std::string> const &accountNames) {
  return accountNames | ranges::views::transform ([] (auto const &accountName) {
           soci::session sql (soci::sqlite3, databaseName);
           return confu_soci::findStruct<database::Account> (sql, "accountName", accountName);
         })
         | ranges::views::filter ([] (boost::optional<database::Account> const &optionalAccount) { return optionalAccount.has_value (); }) | ranges::views::transform ([] (auto const &optionalAccount) { return optionalAccount.value (); }) | ranges::to<std::vector<database::Account>> ();
};

auto const gameOver = [] (matchmaking_game::GameOver const &gameOver) {
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
  * "Default"_s                          + event<m_g::GameOver>                                           / gameOver                                          = X
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
  explicit StateMachineWrapper (MatchmakingGame *owner) :
  impl (owner,
#ifdef LOGGING_FOR_STATE_MACHINE
                                                                                              logger,
#endif
                                                                                              matchmakingData){}

  MatchmakingGameData matchmakingData;

#ifdef LOGGING_FOR_STATE_MACHINE
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
          if (not typeFound) std::cout << "could not find a match for typeToSearch in userMatchmaking '" << typeToSearch << "'" << std::endl;
      
    }
  else
    {
      std::cout << "Not supported event. event syntax: EventName|JsonObject" << std::endl;
    }
}

}
