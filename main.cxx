#include "src/database/database.hxx"
#include "src/server/server.hxx"
#include <Corrade/Utility/Arguments.h>
#include <boost/bind/bind.hpp>
#include <boost/json/src.hpp>
#include <exception>
#include <iostream>
#include <sodium.h>
#include <stdexcept>

int
main (int argc, char *argv[])
{
#ifdef DEBUG
  std::cout << "DEBUG" << std::endl;
#else
  std::cout << "NO DEBUG" << std::endl;
#endif
  auto args = Corrade::Utility::Arguments{};
  // clang-format off
    args
    .addNamedArgument('p', "port").setHelp("port", "port to listen")
    .addNamedArgument('s', "pathToSecrets").setHelp("pathToSecrets", "path to folder with fullchain.pem, privkey.pem and dh2048.pem")
    .setGlobalHelp("A brief description")
    .parse(argc, argv);
  // clang-format on
  try
    {
      if (sodium_init () < 0)
        {
          std::cout << "sodium_init <= 0" << std::endl;
          std::terminate ();
          /* panic! the library couldn't be initialized, it is not safe to use */
        }

#ifdef DEBUG
      database::createEmptyDatabase ();
#else
      database::createDatabaseIfNotExist ();
#endif
      database::createTables ();
      using namespace boost::asio;
      io_context io_context (1);
      signal_set signals (io_context, SIGINT, SIGTERM);
      signals.async_wait ([&] (auto, auto) { io_context.stop (); });
      thread_pool pool{ 2 };
      auto server = Server{ io_context, pool };
      co_spawn (
          io_context,
          [&server, &args] {
            return server.listener ({ ip::tcp::v4 (), boost::lexical_cast<u_int16_t> (args.value ("port")) }, args.value ("pathToSecrets"));
          },
          detached);
      io_context.run ();
    }
  catch (std::exception &e)
    {
      std::printf ("Exception: %s\n", e.what ());
    }
  return 0;
}