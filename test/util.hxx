#pragma once

#include "matchmaking_proxy/logic/matchmaking.hxx"
#include "matchmaking_proxy/server/gameLobby.hxx"
#include <iostream> // for operator<<, ostream
#include <login_matchmaking_game_shared/userMatchmakingSerialization.hxx>
#include <my_web_socket/myWebSocket.hxx>
#include <vector> // for allocator
using namespace matchmaking_proxy;
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


inline bool
clearDestinationAndThenCopyFolder (std::filesystem::path const &source, std::filesystem::path const &destination)
{
  std::filesystem::remove_all (destination);
  try
    {
      std::filesystem::copy (source, destination, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
      std::cout << "Folder copied successfully!" << std::endl;
    }
  catch (const std::filesystem::filesystem_error &e)
    {
      std::cerr << "Error copying folder: " << e.what () << std::endl;
      return false;
    }
  return true;
}