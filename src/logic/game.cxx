#include "game.hxx"

void
handleMessageGame (User &user, std::string const &msg)
{
  user.sendMessageToUser (msg);
}
