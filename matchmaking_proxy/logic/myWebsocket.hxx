#ifndef FDE41782_20C3_436A_B415_E198F593F0AE
#define FDE41782_20C3_436A_B415_E198F593F0AE

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
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <queue>
#include <set>
#include <string>

typedef boost::asio::use_awaitable_t<>::as_default_on_t<boost::asio::basic_waitable_timer<boost::asio::chrono::system_clock>> CoroTimer;

template <class T> class MyWebsocket
{
public:
  MyWebsocket (std::shared_ptr<T> webSocket_) : webSocket{ webSocket_ } {}
  ~MyWebsocket () {}
  boost::asio::awaitable<std::string>
  async_read_one_message ()
  {

    boost::beast::flat_buffer buffer;
    try
      {
        co_await webSocket->async_read (buffer, boost::asio::use_awaitable);
      }
    catch (boost::system::system_error &e)
      {
        webSocket.reset ();
        if (timer) timer->cancel ();
        throw e;
      }
    auto msg = boost::beast::buffers_to_string (buffer.data ());
    co_return msg;
  }

  boost::asio::awaitable<void>
  readLoop (std::function<void (std::string readResult)> onRead)
  {
    try
      {
        for (;;)
          {
            auto oneMsg = co_await async_read_one_message ();
            std::cout << "read: "
                      << "'" << oneMsg << "'" << std::endl;
            onRead (std::move (oneMsg));
          }
      }
    catch (boost::system::system_error &e)
      {
        std::cout << "read Exception: " << e.what () << std::endl;
        webSocket.reset ();
        if (timer) timer->cancel ();
        throw e;
      }
  }

  boost::asio::awaitable<void>
  writeLoop ()
  {
    auto connection = std::weak_ptr<T>{ webSocket };
    try
      {
        while (not connection.expired ())
          {
            timer = std::make_shared<CoroTimer> (CoroTimer{ co_await boost::asio::this_coro::executor });
            timer->expires_after (std::chrono::system_clock::time_point::max () - std::chrono::system_clock::now ());
            try
              {
                co_await timer->async_wait ();
              }
            catch (boost::system::system_error &e)
              {
                using namespace boost::system::errc;
                if (operation_canceled == e.code ())
                  {
                    //  swallow cancel
                  }
                else
                  {
                    std::cout << "error in timer boost::system::errc: " << e.code () << std::endl;
                    abort ();
                  }
              }
            while (not connection.expired () && not msgQueue.empty ())
              {
                auto tmpMsg = std::move (msgQueue.front ());
                msgQueue.pop_front ();
                co_await connection.lock ()->async_write (boost::asio::buffer (tmpMsg), boost::asio::use_awaitable);
                std::cout << "write: "
                          << "'" << tmpMsg << "'" << std::endl;
              }
          }
      }
    catch (std::exception &e)
      {
        std::cout << "write Exception: " << e.what () << std::endl;
        webSocket.reset ();
        if (timer) timer->cancel ();
        throw e;
      }
  }

  void
  sendMessage (std::string message)
  {
    msgQueue.size ();
    msgQueue.push_back (std::move (message));
    if (timer) timer->cancel ();
  }

private:
  std::shared_ptr<T> webSocket{};
  std::deque<std::string> msgQueue{};
  std::shared_ptr<CoroTimer> timer{};
};

#endif /* FDE41782_20C3_436A_B415_E198F593F0AE */
