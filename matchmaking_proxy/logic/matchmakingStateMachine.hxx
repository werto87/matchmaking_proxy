#ifndef FC9C2A0E_0B1D_4FE6_B776_3235987CF58C
#define FC9C2A0E_0B1D_4FE6_B776_3235987CF58C
#include "matchmaking.hxx"
#include "myWebsocket.hxx"
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <memory>
typedef boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> SSLWebsocket;
struct MatchmakingStateMachine
{
  MatchmakingStateMachine (Matchmaking data_) : data{ std::move (data_) } {}

  void
  init (std::shared_ptr<MyWebsocket<SSLWebsocket>> myWebsocket, auto &executor, std::list<MatchmakingStateMachine>::iterator matchmaking, std::list<MatchmakingStateMachine> &matchmakings)
  {
    co_spawn (
        executor,
        [myWebsocket, matchmaking] () mutable {
          using namespace boost::asio::experimental::awaitable_operators;
          return myWebsocket->readLoop ([matchmaking] (std::string msg) {
            std::vector<std::string> splitMesssage{};
            boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
            if (splitMesssage.size () == 2)
              {
                auto const &typeToSearch = splitMesssage.at (0);
                auto const &objectAsString = splitMesssage.at (1);
                bool typeFound = false;
                auto objectAsStringStream = std::stringstream{};
                objectAsStringStream << objectAsString;
                boost::hana::for_each (user_matchmaking::userMatchmaking, [&] (const auto &x) {
                  if (typeToSearch == confu_json::type_name<typename std::decay<decltype (x)>::type> ())
                    {
                      typeFound = true;
                      boost::json::error_code ec{};
                      matchmaking->matchmakingStateMachine.process_event (confu_json::to_object<std::decay_t<decltype (x)>> (confu_json::read_json (objectAsStringStream, ec)));
                      if (ec) std::cout << "ec.message () " << ec.message () << std::endl;
                      return;
                    }
                });
                if (not typeFound) std::cout << "could not find a match for typeToSearch '" << typeToSearch << "'" << std::endl;
              }
          }) || myWebsocket->writeLoop ();
        },
        [matchmaking, &matachmakings = matchmakings] (auto, auto) { matachmakings.erase (matchmaking); });
  }

  Matchmaking data;
  sml::sm<Matchmaking> matchmakingStateMachine{ data }; // using unique pointer to late init this member. late init this member because we need to change something on data after construction and for some reason this does not update so we need to throw away the old object and set the new one but the copy assigment of sml::sm<Matchmaking> does not work
};

#endif /* FC9C2A0E_0B1D_4FE6_B776_3235987CF58C */
