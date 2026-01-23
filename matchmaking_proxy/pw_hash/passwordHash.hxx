#ifndef AD3B06C2_4AC7_438D_8907_4643053A4E7E
#define AD3B06C2_4AC7_438D_8907_4643053A4E7E
#include "constant.hxx"
#include <algorithm>
#include <array>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/execution/outstanding_work.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/saved_handler.hpp>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <sodium.h>
#include <string>
#include <syncstream>
#include <thread>
#include <utility>

namespace matchmaking_proxy
{
std::string inline pw_to_hash (std::string const &password)
{
  auto hashed_password = std::array<char, crypto_pwhash_STRBYTES>{};
  // TODO find a way to cancel this
  if (crypto_pwhash_str (hashed_password.data (), password.data (), password.size (), hash_opt, hash_memory) != 0)
    {
      std::osyncstream (std::cout) << "out of memory" << std::endl;
      std::terminate ();
    }
  auto result = std::string{};
  std::copy_if (hashed_password.begin (), hashed_password.end (), std::back_inserter (result), [] (auto value) { return value != 0; });
  return result;
}

bool inline check_hashed_pw (std::string const &hashedPassword, std::string const &password)
{
  // TODO find a way to cancel this
  return crypto_pwhash_str_verify (hashedPassword.data (), password.data (), password.size ()) == 0;
}

template <boost::asio::completion_token_for<void (std::string)> CompletionToken>
auto
async_hash (boost::asio::thread_pool &pool, boost::asio::io_context &io_context, std::string const &password, CompletionToken &&token)
{
  return boost::asio::async_initiate<CompletionToken, void (std::string)> (
      [&] (auto completion_handler, std::string const &passwordToHash) {
        // TODO find a way to cancel this
        auto io_eq = boost::asio::prefer (io_context.get_executor (), boost::asio::execution::outstanding_work.tracked);
        boost::asio::post (pool, [&, io_eq = std::move (io_eq), completion_handler = std::move (completion_handler), passwordToHash] () mutable {
          auto hashedPw = pw_to_hash (passwordToHash);
          boost::asio::post (io_eq, [hashedPw = std::move (hashedPw), completion_handler = std::move (completion_handler)] () mutable { completion_handler (hashedPw); });
        });
      },
      token, password);
}

template <boost::asio::completion_token_for<void (bool)> CompletionToken>
auto
async_check_hashed_pw (boost::asio::thread_pool &pool, boost::asio::io_context &io_context, std::string const &password, std::string const &hashedPassword, CompletionToken &&token)
{
  return boost::asio::async_initiate<CompletionToken, void (bool)> (
      [&] (auto completion_handler, std::string const &passwordToCheck, std::string const &hashedPw) {
        // TODO find a way to cancel this
        auto io_eq = boost::asio::prefer (io_context.get_executor (), boost::asio::execution::outstanding_work.tracked);
        boost::asio::post (pool, [&, io_eq = std::move (io_eq), completion_handler = std::move (completion_handler), passwordToCheck, hashedPw] () mutable {
          auto isCorrectPw = check_hashed_pw (passwordToCheck, hashedPw);
          boost::asio::post (io_eq, [isCorrectPw, completion_handler = std::move (completion_handler)] () mutable { completion_handler (isCorrectPw); });
        });
      },
      token, password, hashedPassword);
}
}
#endif /* AD3B06C2_4AC7_438D_8907_4643053A4E7E */
