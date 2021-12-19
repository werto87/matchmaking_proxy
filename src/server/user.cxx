#include "src/server/user.hxx"
#include "server.hxx"
#include "src/logic/game.hxx"
#include <iostream>
#include <memory>

using namespace boost::beast;
using namespace boost::asio;

awaitable<void>
User::writeToClient (std::weak_ptr<SSLWebsocket> &connection)
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
User::writeToGame ()
{
  std::weak_ptr<Websocket> connection = connectionToGame;
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
User::readFromGame ()
{
  try
    {
      for (;;)
        {
          auto readResult = co_await my_read (*connectionToGame);
          handleMessageGame (*this, readResult);
        }
    }
  catch (std::exception &e)
    {
      connectionToGame = std::shared_ptr<Websocket>{};
      std::cout << "read Exception: " << e.what () << std::endl;
    }
}

void
User::sendMessageToUser (std::string const &message)
{
  msgQueueClient.push_back (message);
  if (timerClient) timerClient->cancel ();
}
void
User::sendMessageToGame (std::string const &message)
{
  msgQueueGame.push_back (message);
  if (timerGame) timerGame->cancel ();
}
