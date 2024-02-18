#ifndef EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1
#define EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1
#include <boost/json/system_error.hpp>
#include <confu_json/to_json.hxx>
#include <confu_json/to_object.hxx>
#include <confu_json/util.hxx>
#include <exception>
#include <iostream>
#include <random>
#include <string>

template <typename TypeToSend>
std::string
objectToStringWithObjectName (TypeToSend const &typeToSend)
{
  std::stringstream ss{};
  ss << confu_json::type_name<TypeToSend> () << '|' << confu_json::to_json (typeToSend);
#ifdef LOG_OBJECT_TO_STRING_WITH_OBJECT_NAME
  std::cout << "objectToStringWithObjectName: " << ss.str () << std::endl;
#endif
  return ss.str ();
}

template <typename T>
T
stringToObject (std::string const &objectAsString)
{
  T t{};
  boost::json::error_code ec{};
  auto jsonValue = confu_json::read_json (objectAsString, ec);
  if (ec)
    {
      std::cerr << "error while parsing string: error code: " << ec << std::endl;
      std::cerr << "error while parsing string: stringToParse: " << objectAsString << std::endl;
    }
  else
    {
      t = confu_json::to_object<T> (jsonValue);
    }
  return t;
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

template <typename T>
T
rndNumber ()
{
  static std::random_device rd;       // Get a random seed from the OS entropy device, or whatever
  static std::mt19937_64 eng (rd ()); // Use the 64-bit Mersenne Twister 19937 generator
  std::uniform_int_distribution<T> distr{};
  return distr (eng);
}

#endif /* EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1 */
