#include "matchmaking_proxy/logic/matchmakingGame.hxx"
#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/logic/matchmakingGameData.hxx"
#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/logic/matchmakingGameAllowedTypes.hxx"
#include "matchmaking_proxy/util.hxx"
#include "rating.hxx"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/fusion/support/is_sequence.hpp>
#include <boost/hana/fwd/for_each.hpp>
#include <boost/system/system_error.hpp>
#include <boost/optional/optional.hpp>
#include <boost/sml.hpp>
#include <confu_json/to_object.hxx>
#include <confu_json/util.hxx>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <iostream>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <ranges>
#include <soci/session.h>
#include <soci/sqlite3/soci-sqlite3.h>
#include <type_traits>
#include <utility>
#include <vector>
// TODO  figure out if we can remove this
namespace meta
{
template <typename T> struct id;
}
// //////////////////////////////////////
using namespace boost::sml;
namespace matchmaking_proxy
{

auto const accountNamesToAccounts = [] (std::string const &fullPathIncludingDatabaseName, std::vector<std::string> const &accountNames) {
  return accountNames | std::ranges::views::transform ([&fullPathIncludingDatabaseName] (auto const &accountName) {
           soci::session sql (soci::sqlite3, fullPathIncludingDatabaseName);
           return confu_soci::findStruct<database::Account> (sql, "accountName", accountName);
         })
         | std::ranges::views::filter ([] (boost::optional<database::Account> const &optionalAccount) { return optionalAccount.has_value (); }) | std::ranges::views::transform ([] (auto const &optionalAccount) { return optionalAccount.value (); }) | std::ranges::to<std::vector<database::Account>> ();
};

void
sendRatingChangeToUserAndUpdateAccountInDatabase (MatchmakingGameData &matchmakingGameData, std::vector<database::Account> const &accounts, const std::vector<database::Account> &accountsWithNewRating)
{
  for (size_t i = 0; i < accounts.size (); ++i)
    {
      if (auto matchmakingItr = std::ranges::find_if (matchmakingGameData.stateMachines, [accountName = accounts.at (i).accountName] (auto const &matchmaking) { return matchmaking->isLoggedInWithAccountName (accountName); }); matchmakingItr != matchmakingGameData.stateMachines.end ())
        { // TODO handle error
          std::ignore = matchmakingItr->get ()->processEvent (objectToStringWithObjectName (user_matchmaking::RatingChanged{ accounts.at (i).rating, accountsWithNewRating.at (i).rating }));
        }
      soci::session sql (soci::sqlite3, matchmakingGameData.fullPathIncludingDatabaseName.string ());
      confu_soci::upsertStruct (sql, accountsWithNewRating.at (i));
    }
}
auto const gameOver = [] (matchmaking_game::GameOver const &_gameOver, MatchmakingGameData &matchmakingGameData) {
  if (_gameOver.ratedGame)
    {
      if (_gameOver.draws.empty ())
        {
          auto losers = accountNamesToAccounts (matchmakingGameData.fullPathIncludingDatabaseName.string (), _gameOver.losers);
          auto winners = accountNamesToAccounts (matchmakingGameData.fullPathIncludingDatabaseName.string (), _gameOver.winners);
          auto [losersWithNewRating, winnersWithNewRating] = calcRatingLoserAndWinner (losers, winners);
          sendRatingChangeToUserAndUpdateAccountInDatabase (matchmakingGameData, winners, winnersWithNewRating);
          sendRatingChangeToUserAndUpdateAccountInDatabase (matchmakingGameData, losers, losersWithNewRating);
        }
      else
        {
          auto draw = accountNamesToAccounts (matchmakingGameData.fullPathIncludingDatabaseName.string (), _gameOver.draws);
          auto drawNewRating = calcRatingDraw (accountNamesToAccounts (matchmakingGameData.fullPathIncludingDatabaseName.string (), _gameOver.draws));
          sendRatingChangeToUserAndUpdateAccountInDatabase (matchmakingGameData, draw, drawNewRating);
        }
    }
  matchmakingGameData.sendToGame (objectToStringWithObjectName (matchmaking_game::GameOverSuccess{}));
};

auto const isLoggedIn = [] (matchmaking_game::UserLeftGame const &userLeftGame, MatchmakingGameData &matchmakingGameData) { return std::ranges::find (matchmakingGameData.stateMachines, true, [accountName = userLeftGame.accountName] (const auto &matchmaking) { return matchmaking->isLoggedInWithAccountName (accountName); }) != matchmakingGameData.stateMachines.end (); };
auto const hasProxy = [] (matchmaking_game::UserLeftGame const &userLeftGame, MatchmakingGameData &matchmakingGameData) { return std::ranges::find (matchmakingGameData.stateMachines, true, [accountName = userLeftGame.accountName] (const auto &matchmaking) { return matchmaking->isLoggedInWithAccountName (accountName) && matchmaking->hasProxyToGame (); }) != matchmakingGameData.stateMachines.end (); };

auto const userLeftGameErrorNotLoggedIn = [] (matchmaking_game::UserLeftGame const &userLeftGame, MatchmakingGameData &matchmakingGameData) { matchmakingGameData.sendToGame (objectToStringWithObjectName (matchmaking_game::UserLeftGameError{ userLeftGame.accountName, "User not logged in" })); };
auto const userLeftGameErrorUserHasNoProxy = [] (matchmaking_game::UserLeftGame const &userLeftGame, MatchmakingGameData &matchmakingGameData) { matchmakingGameData.sendToGame (objectToStringWithObjectName (matchmaking_game::UserLeftGameError{ userLeftGame.accountName, "User not in proxy state" })); };

auto const cancelProxyToGame = [] (matchmaking_game::UserLeftGame const &userLeftGame, MatchmakingGameData &matchmakingGameData) {
  if (auto matchmaking = std::ranges::find (matchmakingGameData.stateMachines, true, [accountName = userLeftGame.accountName] (const auto &_matchmaking) { return _matchmaking->isLoggedInWithAccountName (accountName); }); matchmaking == matchmakingGameData.stateMachines.end ())
    {
      matchmaking->get ()->disconnectFromProxy ();
    }
  matchmakingGameData.sendToGame (objectToStringWithObjectName (matchmaking_game::UserLeftGameSuccess{ userLeftGame.accountName }));
};

auto const customMessage = [] (matchmaking_game::CustomMessage const &_customMessage, MatchmakingGameData &matchmakingGameData) {
  if (matchmakingGameData.handleCustomMessageFromGame)
    {
      matchmakingGameData.handleCustomMessageFromGame (_customMessage.messageType, _customMessage.message, matchmakingGameData);
    }
};

auto const sendTopRatedPlayersToUser = [] (MatchmakingGameData &matchmakingGameData) {
  for (auto &matchmaking : matchmakingGameData.stateMachines)
    {
      matchmaking->proccessSendTopRatedPlayersToUser ();
    }
};

class MatchmakingGameStateMachine
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
* "Default"_s                          + event<m_g::GameOver>                             / (gameOver,sendTopRatedPlayersToUser)
, "Default"_s                          + event<m_g::UserLeftGame>   [isLoggedIn]          / userLeftGameErrorNotLoggedIn
, "Default"_s                          + event<m_g::UserLeftGame>   [hasProxy]            / userLeftGameErrorUserHasNoProxy
, "Default"_s                          + event<m_g::UserLeftGame>                         / cancelProxyToGame
, "Default"_s                          + event<m_g::CustomMessage>                        / customMessage
      );
    }
};



struct my_logger
{
  template <class SM, class TEvent>
  void
  log_process_event (const TEvent &event)
  {
    if constexpr (boost::fusion::traits::is_sequence<TEvent>::value)
      {
        std::osyncstream (std::cout) << "\n[" << aux::get_type_name<SM> () << "]"
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
  StateMachineWrapper (MatchmakingGame *owner,MatchmakingGameData matchmakingGameDependencies_) : matchmakingGameData{std::move(matchmakingGameDependencies_)},
  impl (owner,
#ifdef MATCHMAKING_PROXY_LOG_FOR_STATE_MACHINE
                                                                                              logger,
#endif
                                                                                              matchmakingGameData){}

  MatchmakingGameData matchmakingGameData;

#ifdef MATCHMAKING_PROXY_LOG_FOR_STATE_MACHINE
  my_logger logger;
  boost::sml::sm<MatchmakingGameStateMachine, boost::sml::logger<my_logger>> impl;
#else
  boost::sml::sm<MatchmakingGameStateMachine> impl;
#endif
};

void // has to be after YourClass::StateMachineWrapper definition
MatchmakingGame::StateMachineWrapperDeleter::operator() (StateMachineWrapper *p)const
{
  delete p;
}


MatchmakingGame::MatchmakingGame(MatchmakingGameData matchmakingGameData): sm{ new StateMachineWrapper{this, matchmakingGameData} } {}

// clang-format on

void
MatchmakingGame::process_event (std::string const &event)
{
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
              boost::system::error_code ec{};
              sm->impl.process_event (confu_json::to_object<std::decay_t<decltype (x)>> (confu_json::read_json (objectAsString, ec)));
              if (ec) std::osyncstream (std::cout) << "read_json error: " << ec.message () << std::endl;
              return;
            }
        });
        if (not typeFound) std::osyncstream (std::cout) << "could not find a match for typeToSearch in matchmakingGame '" << typeToSearch << "'" << std::endl;
      }
    else
      {
        std::osyncstream (std::cout) << "Not supported event. event syntax: EventName|JsonObject" << std::endl;
      }
  }
}
}