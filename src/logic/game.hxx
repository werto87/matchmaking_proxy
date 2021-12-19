#ifndef C4EAE959_3720_4318_B9FA_254E4BC23432
#define C4EAE959_3720_4318_B9FA_254E4BC23432

#include "../server/gameLobby.hxx"
#include "../server/user.hxx"
#include "src/serialization.hxx"
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <string>
#include <vector>

void handleMessageGame (User &user, std::string const &msg);

#endif /* C4EAE959_3720_4318_B9FA_254E4BC23432 */
