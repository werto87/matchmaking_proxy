#include "matchmaking_proxy/util.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/server/matchmakingOption.hxx"
#include <boost/asio/ip/tcp.hpp>
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
std::shared_ptr<Matchmaking>
createAccountAndJoinMatchmakingGame (const std::string &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<std::shared_ptr<Matchmaking>> &matchmakings, boost::asio::thread_pool &pool, const user_matchmaking::JoinMatchMakingQueue &joinMatchMakingQueue, int &proxyStartedCalled)
{
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [] (auto) {}, gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings,
                                                                [&messages, &ioContext, &proxyStartedCalled] (std::string msg) {
                                                                  messages.push_back (msg);
                                                                  if (msg == "ProxyStarted|{}")
                                                                    {
                                                                      proxyStartedCalled++;
                                                                      if (proxyStartedCalled == 2)
                                                                        {
                                                                          ioContext.stop ();
                                                                        }
                                                                    }
                                                                },
                                                                gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } });
  matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ playerName, "abc" }));
  ioContext.run_for (std::chrono::seconds{ 5 });
  ioContext.stop ();
  ioContext.reset ();
  matchmaking->processEvent (objectToStringWithObjectName (joinMatchMakingQueue));
  return matchmaking;
}
