#include "matchmaking_proxy/database/database.hxx"
#include "matchmaking_proxy/server/server.hxx"
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/bind/bind.hpp>
#include <catch2/catch.hpp>
#include <exception>
#include <iostream>
#include <sodium.h>
#include <stdexcept>

TEST_CASE ("integration test", "[integration]")
{

  SECTION ("start the server to play around", "[matchmaking]")
  {
    if (sodium_init () < 0)
      {
        std::cout << "sodium_init <= 0" << std::endl;
        std::terminate ();
        /* panic! the library couldn't be initialized, it is not safe to use */
      }
    database::createEmptyDatabase ();
    database::createTables ();
    using namespace boost::asio;
    io_context io_context (1);
    signal_set signals (io_context, SIGINT, SIGTERM);
    signals.async_wait ([&] (auto, auto) { io_context.stop (); });
    thread_pool pool{ 2 };
    auto server = Server{ io_context, pool };
    auto const port = 55555;
    auto const pathToSecrets = std::filesystem::path{ "/home/walde/certificate/otherTestCert" };
    co_spawn (
        io_context, [&server, pathToSecrets, endpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), port }] { return server.listener (endpoint, pathToSecrets); }, detached);
    io_context.run ();
  }
}
