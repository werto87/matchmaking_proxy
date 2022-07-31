// #include "matchmaking_proxy/database/database.hxx"
// #include "matchmaking_proxy/logic/matchmakingGame.hxx"
// #include "matchmaking_proxy/matchmakingGameSerialization.hxx"
// #include "matchmaking_proxy/server/matchmakingOption.hxx"
// #include "matchmaking_proxy/server/myWebsocket.hxx"
// #include "matchmaking_proxy/server/server.hxx"
// #include "matchmaking_proxy/userMatchmakingSerialization.hxx"
// #include "matchmaking_proxy/util.hxx"
// #include "test/mockserver.hxx"
// #include <algorithm> // for max
// #include <boost/algorithm/string/predicate.hpp>
// #include <boost/asio/co_spawn.hpp>
// #include <boost/asio/detached.hpp>
// #include <boost/asio/experimental/awaitable_operators.hpp>
// #include <boost/asio/use_awaitable.hpp>
// #include <boost/beast/core/flat_buffer.hpp>
// #include <boost/certify/extensions.hpp>
// #include <boost/certify/https_verification.hpp>
// #include <catch2/catch.hpp>
// #include <deque>
// #include <exception>
// #include <filesystem>
// #include <functional>
// #include <iostream>
// #include <iterator> // for next
// #include <openssl/ssl3.h>
// #include <range/v3/algorithm/find_if.hpp>
// #include <range/v3/view.hpp>
// #include <sodium/core.h>
// #include <stdexcept>
// #include <string>
// #include <type_traits>
// #include <utility> // for pair

// using namespace boost::asio;

// boost::asio::awaitable<void>
// connectWebsocketSSL2 (auto handleMsgFromGame, io_context &ioContext, boost::asio::ip::tcp::endpoint const &endpoint, std::vector<std::string> sendMessageBeforeStartRead = {}, std::optional<std::string> connectionName = {})
// {
//   try
//     {
//       using namespace boost::asio;
//       using namespace boost::beast;
//       ssl::context ctx{ ssl::context::tlsv12_client };
//       ctx.set_verify_mode (boost::asio::ssl::verify_none); // DO NOT USE THIS IN PRODUCTION THIS WILL IGNORE CHECKING FOR TRUSTFUL CERTIFICATE
//       try
//         {
//           typedef boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> SSLWebsocket;
//           auto connection = std::make_shared<SSLWebsocket> (SSLWebsocket{ ioContext, ctx });
//           get_lowest_layer (*connection).expires_never ();
//           connection->set_option (websocket::stream_base::timeout::suggested (role_type::client));
//           connection->set_option (websocket::stream_base::decorator ([] (websocket::request_type &req) { req.set (http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl"); }));
//           co_await get_lowest_layer (*connection).async_connect (endpoint, use_awaitable);
//           co_await connection->next_layer ().async_handshake (ssl::stream_base::client, use_awaitable);
//           co_await connection->async_handshake ("localhost:" + std::to_string (endpoint.port ()), "/", use_awaitable);
//           static size_t id = 0;
//           auto myWebsocket = std::make_shared<MyWebsocket<SSLWebsocket>> (MyWebsocket<SSLWebsocket>{ std::move (connection), connectionName ? connectionName.value () : std::string{ "connectWebsocket" }, fmt::fg (fmt::color::chocolate), std::to_string (id++) });
//           for (auto message : sendMessageBeforeStartRead)
//             {
//               co_await myWebsocket->async_write_one_message (message);
//             }
//           using namespace boost::asio::experimental::awaitable_operators;
//           co_await(myWebsocket->readLoop ([myWebsocket, handleMsgFromGame, &ioContext] (const std::string &msg) { handleMsgFromGame (ioContext, msg, myWebsocket); }) && myWebsocket->writeLoop ());
//         }
//       catch (std::exception const &e)
//         {
//           std::cout << "connectWebsocketSSL () connect  Exception : " << e.what () << std::endl;
//         }
//     }
//   catch (std::exception const &e)
//     {
//       std::cout << "exception: " << e.what () << std::endl;
//     }
// }

// TEST_CASE ("integration test", "[integration]")
// {
//   SECTION ("start the server to play around", "[matchmaking]")
//   {
//     if (sodium_init () < 0)
//       {
//         std::cout << "sodium_init <= 0" << std::endl;
//         std::terminate ();
//         /* panic! the library couldn't be initialized, it is not safe to use */
//       }
//     database::createEmptyDatabase ();
//     database::createTables ();
//     using namespace boost::asio;
//     io_context ioContext (1);
//     signal_set signals (ioContext, SIGINT, SIGTERM);
//     signals.async_wait ([&] (auto, auto) { ioContext.stop (); });
//     thread_pool pool{ 2 };
//     auto server = Server{ ioContext, pool };
//     auto const userPort = 55555;
//     auto const gamePort = 22222;
//     // TODO create some test certificates and share them on git
//     // TODO run mock server which reads the game over messages and stops instead of the current stopping logic
//     auto mockserver = Mockserver{ { ip::tcp::v4 (), 12312 }, { .callOnMessageStartsWith{ { "GameOver", [&ioContext] () { ioContext.stop (); } } } }, "GameToMatchmaking" };
//     auto const PATH_TO_CHAIN_FILE = std::string{ "/etc/letsencrypt/live/test-name/fullchain.pem" };
//     auto const PATH_TO_PRIVATE_File = std::string{ "/etc/letsencrypt/live/test-name/privkey.pem" };
//     auto const PATH_TO_DH_File = std::string{ "/etc/letsencrypt/dhparams/dhparam.pem" };
//     auto const POLLING_SLEEP_TIMER = std::chrono::seconds{ 2 };
//     auto userEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), userPort };
//     auto gameEndpoint = boost::asio::ip::tcp::endpoint{ ip::tcp::v4 (), gamePort };
//     auto matchmakingGameEndpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 4242 };
//     auto userGameViaMatchmakingEndpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 3232 };
//     using namespace boost::asio::experimental::awaitable_operators;
//     co_spawn (ioContext, server.userMatchmaking (userEndpoint, PATH_TO_CHAIN_FILE, PATH_TO_PRIVATE_File, PATH_TO_DH_File, POLLING_SLEEP_TIMER, MatchmakingOption{ .usersNeededToStartQuickGame = 2 }, "localhost", "4242", "3232") || server.gameMatchmaking (gameEndpoint), printException);

//     auto sendAfterConnect = std::vector<std::string>{ { "LoginAsGuest|{}", objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueue{}) } };
//     auto const joinGameLogic = [] (auto &&, auto const &msg, auto &&myWebsocket) {
//       std::vector<std::string> splitMessage{};
//       boost::algorithm::split (splitMessage, msg, boost::is_any_of ("|"));
//       if (splitMessage.size () == 2)
//         {
//           auto const &typeToSearch = splitMessage.at (0);
//           if (typeToSearch == "AskIfUserWantsToJoinGame")
//             {
//               myWebsocket->sendMessage (objectToStringWithObjectName (user_matchmaking::WantsToJoinGame{ true }));
//             }
//           else if (typeToSearch == "ProxyStarted")
//             {
//               myWebsocket->sendMessage ("DurakLeaveGame|{}");
//             }
//         }
//     };
//     co_spawn (ioContext, connectWebsocketSSL2 (joinGameLogic, ioContext, userEndpoint, sendAfterConnect, "user"), printException);
//     co_spawn (ioContext, connectWebsocketSSL2 (joinGameLogic, ioContext, userEndpoint, sendAfterConnect, "user"), printException);
//     ioContext.run ();
//   }
// }
