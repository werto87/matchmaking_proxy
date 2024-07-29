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

namespace matchmaking_proxy
{

template <typename TypeToSend>
std::string
objectToStringWithObjectName (TypeToSend const &typeToSend)
{
  std::stringstream ss{};
  ss << confu_json::type_name<TypeToSend> () << '|' << confu_json::to_json (typeToSend);
#ifdef MATCHMAKING_PROXY_LOG_OBJECT_TO_STRING_WITH_OBJECT_NAME
  std::cout << "objectToStringWithObjectName: " << ss.str () << std::endl;
#endif
  return ss.str ();
}

template <typename T>
T
stringToObject (std::string const &objectAsString)
{
  T t{};
  boost::system::error_code ec{};
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

}
#endif /* EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1 */
