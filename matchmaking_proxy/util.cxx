#include "util.hxx"
#include <iostream>

#ifdef LOG_CO_SPAWN_PRINT_EXCEPTIONS
void
printExceptionHelper (std::exception_ptr eptr)
{
  try
    {
      if (eptr)
        {
          std::rethrow_exception (eptr);
        }
    }
  catch (std::exception const &e)
    {
      std::cout << "unhandled exception: '" << e.what () << "'" << std::endl;
    }
}
#else

#endif
void printExceptionHelper (std::exception_ptr) {}
