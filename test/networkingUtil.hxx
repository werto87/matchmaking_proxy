#pragma once
#include "catch2/catch.hpp"
#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
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

boost::asio::awaitable<void>
connectWebsocketSSL (auto handleMsgFromGame, std::vector<std::string> messageToSendAfterConnect, boost::asio::io_context &ioContext, boost::asio::ip::tcp::endpoint endpoint, std::vector<std::string> &messagesFromGame)
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
          for (auto const &message : messageToSendAfterConnect)
            {
              co_await connection.async_write (boost::asio::buffer (message), use_awaitable);
            }
          static size_t id = 0;
          auto myWebsocket = std::make_shared<my_web_socket::MyWebSocket<my_web_socket::SSLWebSocket>> (std::move (connection), "connectWebsocketSSL", fmt::fg (fmt::color::chocolate), std::to_string (id++));
          using namespace boost::asio::experimental::awaitable_operators;
          co_await (myWebsocket->readLoop ([myWebsocket, handleMsgFromGame, &ioContext, &messagesFromGame] (const std::string &msg) {
            messagesFromGame.push_back (msg);
            handleMsgFromGame (ioContext, msg, myWebsocket);
          }) && myWebsocket->writeLoop ());
        }
      catch (std::exception const &e)
        {
          std::osyncstream (std::cout) << "connectWebsocketSSL () connect  Exception : " << e.what () << std::endl;
        }
    }
  catch (std::exception const &e)
    {
      std::osyncstream (std::cout) << "exception: " << e.what () << std::endl;
    }
}