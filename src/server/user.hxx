#ifndef F85705C8_6F01_4F50_98CA_5636F5F5E1C1
#define F85705C8_6F01_4F50_98CA_5636F5F5E1C1

#include "src/util.hxx"
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/optional.hpp>
#include <cstddef>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <queue>
#include <set>
#include <string>

using namespace boost::beast;
using namespace boost::asio;

typedef boost::asio::use_awaitable_t<>::as_default_on_t<boost::asio::basic_waitable_timer<boost::asio::chrono::system_clock>> CoroTimer;
typedef boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> SSLWebsocket;
using Websocket = boost::beast::websocket::stream<boost::asio::use_awaitable_t<>::as_default_on_t<boost::beast::tcp_stream>>;
struct User
{

  awaitable<void>
  writeToClient (std::weak_ptr<SSLWebsocket> &connection)
  {
    try
      {
        while (not connection.expired ())
          {
            timerClient = std::make_shared<CoroTimer> (CoroTimer{ co_await this_coro::executor });
            timerClient->expires_after (std::chrono::system_clock::time_point::max () - std::chrono::system_clock::now ());
            try
              {
                co_await timerClient->async_wait ();
              }
            catch (boost::system::system_error &e)
              {
                using namespace boost::system::errc;
                if (operation_canceled == e.code ())
                  {
                    // swallow cancel
                  }
                else
                  {
                    std::cout << "error in timer boost::system::errc: " << e.code () << std::endl;
                    abort ();
                  }
              }
            while (not connection.expired () && not msgQueueClient.empty ())
              {
                auto tmpMsg = std::move (msgQueueClient.front ());
                std::cout << " msg: " << tmpMsg << std::endl;
                msgQueueClient.pop_front ();
                co_await connection.lock ()->async_write (buffer (tmpMsg), use_awaitable);
              }
          }
      }
    catch (std::exception &e)
      {
        std::cout << "write Exception Client: " << e.what () << std::endl;
      }
  }
  boost::asio::awaitable<void>
  writeToGame ()
  {
    std::weak_ptr<Websocket> connection = connectionToGame;
    co_await connection.lock ()->async_write (buffer (objectToStringWithObjectName (shared_class::StartGame{})), use_awaitable);
    try
      {
        while (not connection.expired ())
          {
            timerGame = std::make_shared<CoroTimer> (CoroTimer{ co_await this_coro::executor });
            timerGame->expires_after (std::chrono::system_clock::time_point::max () - std::chrono::system_clock::now ());
            try
              {
                co_await timerGame->async_wait ();
              }
            catch (boost::system::system_error &e)
              {
                using namespace boost::system::errc;
                if (operation_canceled == e.code ())
                  {
                    // swallow cancel
                  }
                else
                  {
                    std::cout << "error in timer boost::system::errc: " << e.code () << std::endl;
                    abort ();
                  }
              }
            while (not connection.expired () && not msgQueueGame.empty ())
              {
                auto tmpMsg = std::move (msgQueueGame.front ());
                std::cout << " msg: " << tmpMsg << std::endl;
                msgQueueGame.pop_front ();
                co_await connection.lock ()->async_write (buffer (tmpMsg), use_awaitable);
              }
          }
      }
    catch (std::exception &e)
      {
        std::cout << "write Exception Game: " << e.what () << std::endl;
      }
  }

  awaitable<std::string>
  my_read (Websocket &ws_)
  {
    std::cout << "read" << std::endl;
    flat_buffer buffer;
    co_await ws_.async_read (buffer, use_awaitable);
    auto msg = buffers_to_string (buffer.data ());
    std::cout << "number of letters '" << msg.size () << "' msg: '" << msg << "'" << std::endl;
    co_return msg;
  }

  boost::asio::awaitable<void>
  readFromGame ()
  {
    try
      {
        for (;;)
          {
            auto readResult = co_await my_read (*connectionToGame);
            handleMessageGame (readResult);
          }
      }
    catch (std::exception &e)
      {
        connectionToGame = std::shared_ptr<Websocket>{};
        std::cout << "read Exception: " << e.what () << std::endl;
      }
  }

  void
  sendMessageToUser (std::string const &message)
  {
    msgQueueClient.push_back (message);
    if (timerClient) timerClient->cancel ();
  }
  void
  sendMessageToGame (std::string const &message)
  {
    msgQueueGame.push_back (message);
    if (timerGame) timerGame->cancel ();
  }

  void
  handleMessageGame (std::string const &msg)
  {
    sendMessageToUser (msg);
  }

  boost::optional<std::string> accountName{}; // has value if user is logged in
  std::set<std::string> communicationChannels{};
  bool ignoreLogin{};
  bool ignoreCreateAccount{};
  std::deque<std::string> msgQueueClient{};
  std::shared_ptr<CoroTimer> timerClient{};
  std::shared_ptr<Websocket> connectionToGame{};
  std::deque<std::string> msgQueueGame{};
  std::shared_ptr<CoroTimer> timerGame{};
};

#endif /* F85705C8_6F01_4F50_98CA_5636F5F5E1C1 */
