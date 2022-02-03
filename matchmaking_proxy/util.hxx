#ifndef EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1
#define EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1
#include "confu_json/confu_json.hxx"

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

#endif /* EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1 */
