#ifndef FC9C2A0E_0B1D_4FE6_B776_3235987CF58C
#define FC9C2A0E_0B1D_4FE6_B776_3235987CF58C
#include "matchmaking.hxx"
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <deque>
#include <list> // for list
#include <memory>
#include <type_traits> // for move
namespace boost
{
namespace asio
{
class io_context;
}
}
template <class T> class MyWebsocket;

typedef boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> SSLWebsocket;
struct MatchmakingStateMachine
{
  MatchmakingStateMachine (Matchmaking data_) : data{ std::move (data_) } {}

  void init (std::shared_ptr<MyWebsocket<SSLWebsocket>> myWebsocket, boost::asio::io_context &executor, std::list<MatchmakingStateMachine>::iterator matchmaking, std::list<MatchmakingStateMachine> &matchmakings);

  Matchmaking data;
  sml::sm<Matchmaking> matchmakingStateMachine{ data };
};

#endif /* FC9C2A0E_0B1D_4FE6_B776_3235987CF58C */
