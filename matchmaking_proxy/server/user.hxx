#ifndef F85705C8_6F01_4F50_98CA_5636F5F5E1C1
#define F85705C8_6F01_4F50_98CA_5636F5F5E1C1

#include "../userMatchmakingSerialization.hxx"
#include "../util.hxx"
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
typedef boost::beast::websocket::stream<boost::asio::use_awaitable_t<>::as_default_on_t<boost::beast::tcp_stream>> Websocket;
struct User
{

  awaitable<std::string>
  my_read (SSLWebsocket &ws_)
  {
    std::cout << "read" << std::endl;
    flat_buffer buffer;
    co_await ws_.async_read (buffer, use_awaitable);
    auto msg = buffers_to_string (buffer.data ());
    std::cout << "number of letters '" << msg.size () << "' msg: '" << msg << "'" << std::endl;
    co_return msg;
  }

  // awaitable<void>
  // readFromClient (std::list<std::shared_ptr<User>>::iterator user, SSLWebsocket &connection)
  // {
  //   try
  //     {
  //       for (;;)
  //         {
  //           auto readResult = co_await my_read (connection);
  //           co_await handleMessageClient (readResult, _io_context, _pool, users, *user, gameLobbies);
  //         }
  //     }
  //   catch (std::exception &e)
  //     {
  //       removeUser (user);
  //       std::cout << "read Exception: " << e.what () << std::endl;
  //     }
  // }

  // bool
  // isRegistered (std::string const &accountName)
  // {
  //   soci::session sql (soci::sqlite3, databaseName);
  //   return confu_soci::findStruct<database::Account> (sql, "accountName", accountName).has_value ();
  // }

  // void
  // removeUserFromLobby (std::shared_ptr<User> user)
  // {
  //   auto const findLobby = [userAccountName = user->accountName] (GameLobby const &gameLobby) {
  //     auto const &accountNamesToCheck = gameLobby.accountNames ();
  //     return ranges::find_if (accountNamesToCheck, [userAccountName] (std::string const &accountNameToCheck) { return userAccountName == accountNameToCheck; }) != accountNamesToCheck.end ();
  //   };
  //   if (auto gameLobbyWithUser = ranges::find_if (gameLobbies, findLobby); gameLobbyWithUser != gameLobbies.end ())
  //     {
  //       gameLobbyWithUser->removeUser (user);
  //       if (gameLobbyWithUser->_users.empty ())
  //         {
  //           gameLobbies.erase (gameLobbyWithUser);
  //         }
  //     }
  // }

  // void
  // removeUser (std::list<std::shared_ptr<User>>::iterator user)
  // {
  //   if (user->get ()->accountName && not isRegistered (user->get ()->accountName.value ()))
  //     {
  //       removeUserFromLobby (*user);
  //     }
  //   user->get ()->communicationChannels.clear ();
  //   user->get ()->ignoreLogin = false;
  //   user->get ()->ignoreCreateAccount = false;
  //   user->get ()->msgQueueClient.clear ();
  //   if (user->get ()->connectionToGame) user->get ()->connectionToGame->close ("Connection lost");
  //   users.erase (user);
  // }

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
                co_await connection.lock ()->async_write (buffer (tmpMsg));
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
    co_await ws_.async_read (buffer);
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
  // TODO make a simple struct out of this and put the complicated things into the matchmaking machine
  std::string accountName{};
  std::set<std::string> communicationChannels{};
  // TODO bool ignoreLogin{};  bool ignoreCreateAccount{}; could be handled in the state machine
  std::deque<std::string> msgQueueClient{};
  std::shared_ptr<CoroTimer> timerClient{};
  std::shared_ptr<Websocket> connectionToGame{};
  std::deque<std::string> msgQueueGame{};
  std::shared_ptr<CoroTimer> timerGame{};
  // TODO rethink the usage of all the smart pointers ;)
};

#endif /* F85705C8_6F01_4F50_98CA_5636F5F5E1C1 */
