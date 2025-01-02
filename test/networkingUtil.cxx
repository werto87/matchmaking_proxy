#include "catch2/catch.hpp"
#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/logic/matchmakingData.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include "matchmaking_proxy/server/matchmakingOption.hxx"
#include "matchmaking_proxy/util.hxx"
#include "util.hxx"
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <iostream> // for operator<<, ostream
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <my_web_socket/myWebSocket.hxx>
#include <vector> // for allocator
using namespace matchmaking_proxy;

std::shared_ptr<Matchmaking>
createAccountAndJoinMatchmakingQueue (const std::string &playerName, boost::asio::io_context &ioContext, std::vector<std::string> &messages, std::list<GameLobby> &gameLobbies, std::list<std::shared_ptr<Matchmaking>> &matchmakings, boost::asio::thread_pool &pool, const user_matchmaking::JoinMatchMakingQueue &joinMatchMakingQueue)
{
  auto &matchmaking = matchmakings.emplace_back (std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [] (auto) {}, gameLobbies, pool, MatchmakingOption{}, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } }));
  matchmaking = std::make_shared<Matchmaking> (MatchmakingData{ ioContext, matchmakings, [&messages] (std::string msg) { messages.push_back (msg); }, gameLobbies, pool, MatchmakingOption{ .timeToAcceptInvite = std::chrono::milliseconds{ 2222 } }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 }, boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 33333 } });
  ioContext.stop ();
  ioContext.reset ();
  ioContext.restart ();
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (user_matchmaking::CreateAccount{ playerName, "abc" })));
  ioContext.run_for (std::chrono::milliseconds{ 100 });
  ioContext.stop ();
  ioContext.reset ();
  ioContext.restart ();
  REQUIRE (matchmaking->processEvent (objectToStringWithObjectName (joinMatchMakingQueue)));
  ioContext.run_for (std::chrono::milliseconds{ 10 });
  return matchmaking;
}

boost::asio::awaitable<void>
connectWebsocketSSL (auto handleMsgFromGame, boost::asio::io_context &ioContext, boost::asio::ip::tcp::endpoint endpoint, std::vector<std::string> &messagesFromGame)
{
  try
    {
      using namespace boost::asio;
      using namespace boost::beast;
      ssl::context ctx{ ssl::context::tlsv12_client };
      ctx.set_verify_mode (boost::asio::ssl::verify_none); // DO NOT USE THIS IN PRODUCTION THIS WILL IGNORE CHECKING FOR TRUSTFUL CERTIFICATE
      try
        {
          auto connection = my_web_socket::SSLWebSocket{ ioContext, ctx };
          get_lowest_layer (connection).expires_never ();
          connection.set_option (websocket::stream_base::timeout::suggested (role_type::client));
          connection.set_option (websocket::stream_base::decorator ([] (websocket::request_type &req) { req.set (http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl"); }));
          co_await get_lowest_layer (connection).async_connect (endpoint, use_awaitable);
          co_await connection.next_layer ().async_handshake (ssl::stream_base::client, use_awaitable);
          co_await connection.async_handshake ("localhost:" + std::to_string (endpoint.port ()), "/", use_awaitable);
          co_await connection.async_write (boost::asio::buffer (std::string{ "LoginAsGuest|{}" }), use_awaitable);
          static size_t id = 0;
          auto myWebsocket = std::make_shared<my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>> (my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>{ std::move (connection), "connectWebsocketSSL", fmt::fg (fmt::color::chocolate), std::to_string (id++) });
          using namespace boost::asio::experimental::awaitable_operators;
          co_await (myWebsocket->readLoop ([myWebsocket, handleMsgFromGame, &ioContext, &messagesFromGame] (const std::string &msg) {
            messagesFromGame.push_back (msg);
            handleMsgFromGame (ioContext, msg, myWebsocket);
          }) && myWebsocket->writeLoop ());
        }
      catch (std::exception const &e)
        {
          std::cout << "connectWebsocketSSL () connect  Exception : " << e.what () << std::endl;
        }
    }
  catch (std::exception const &e)
    {
      std::cout << "exception: " << e.what () << std::endl;
    }
}

boost::asio::awaitable<void> connectWebsocket (boost::asio::io_context &ioContext, boost::asio::ip::tcp::endpoint const &endpoint, std::vector<std::string> &messageFromMatchmaking, std::vector<std::string> const &sendMessageBeforeStartRead);
