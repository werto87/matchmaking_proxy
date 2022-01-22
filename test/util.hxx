#ifndef F913C042_CAFF_4558_AE02_952AE3C4F17A
#define F913C042_CAFF_4558_AE02_952AE3C4F17A

#include <iostream> // for operator<<, ostream
#include <vector>   // for allocator

template <typename T, template <typename ELEM, typename ALLOC = std::allocator<ELEM>> class Container>
std::ostream &
operator<< (std::ostream &o, const Container<T> &container)
{
  typename Container<T>::const_iterator beg = container.begin ();
  while (beg != container.end ())
    {
      o << "\n" << *beg++; // 2
    }
  return o;
}

void printExceptionHelper (std::exception_ptr eptr);

template <class... Fs> struct overloaded : Fs...
{
  using Fs::operator()...;
};

template <class... Fs> overloaded (Fs...) -> overloaded<Fs...>;

auto const printException1 = [] (std::exception_ptr eptr) { printExceptionHelper (eptr); };

auto const printException2 = [] (std::exception_ptr eptr, auto) { printExceptionHelper (eptr); };

auto const printException = overloaded{ printException1, printException2 };

#endif /* F913C042_CAFF_4558_AE02_952AE3C4F17A */
