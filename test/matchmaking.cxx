// #include "../matchmaking_proxy/logic/OLDmatchmaking.hxx"
// #include "../matchmaking_proxy/userMatchmakingSerialization.hxx" // for Cre...
// #include "matchmaking_proxy/database/database.hxx"               // for cre...
// #include "matchmaking_proxy/server/gameLobby.hxx"                // for Gam...
// #include "matchmaking_proxy/server/user.hxx"                     // for User
// #include <boost/asio/detached.hpp>
// #include <catch2/catch.hpp> // for Ass...
// #include <memory>           // for sha...
// using namespace user_matchmaking;

// TEST_CASE ("matchmaking NotLoggedin -> Loggedin", "[matchmaking]")
// {
//   database::createEmptyDatabase ();
//   database::createTables ();
//   using namespace sml;
//   using namespace boost::asio;
//   auto ioContext = io_context ();
//   auto users_ = std::list<std::shared_ptr<User>>{ std::make_shared<User> (), std::make_shared<User> () };
//   boost::asio::thread_pool pool_{};
//   std::list<GameLobby> gameLobbies_{};
//   typedef sml::sm<Matchmaking> MatchmakingMachine;
//   auto matchmaking = Matchmaking{ ioContext, pool_, gameLobbies_, {} };
//   MatchmakingMachine loginMachine{ matchmaking };
//   SECTION ("CreateAccount", "[matchmaking]")
//   {
//     loginMachine.process_event (CreateAccount{ "newAcc", "abc" });
//     REQUIRE (loginMachine.is (state<WaitingForPasswordHashed>));
//     ioContext.run ();
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("LoginAccount", "[matchmaking]")
//   {
//     database::createAccount ("oldAcc", "$argon2id$v=19$m=8,t=1,p=1$+Z8rjMS3CYbgMdG+JRgc6A$IAmEYrfE66+wsRmzeyPkyZ+xUJn+ybnx0HzKykO9NeY");
//     loginMachine.process_event (LoginAccount{ "oldAcc", "abc" });
//     REQUIRE (loginMachine.is (state<WaitingForPasswordCheck>));
//     ioContext.run ();
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("LoginAsGuest", "[matchmaking]")
//   {
//     loginMachine.process_event (LoginAsGuest{});
//     ioContext.run ();
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   ioContext.stop ();
//   ioContext.reset ();
// }

// TEST_CASE ("matchmaking NotLoggedin -> NotLoggedin", "[matchmaking]")
// {
//   database::createEmptyDatabase ();
//   database::createTables ();
//   typedef sml::sm<Matchmaking> MatchmakingMachine;
//   using namespace sml;
//   using namespace boost::asio;
//   auto ioContext = io_context ();
//   auto users_ = std::list<std::shared_ptr<User>>{ std::make_shared<User> (), std::make_shared<User> () };
//   boost::asio::thread_pool pool_{};
//   std::list<GameLobby> gameLobbies_{};
//   auto matchmaking = Matchmaking{ ioContext, pool_, gameLobbies_, {} };
//   MatchmakingMachine loginMachine{ matchmaking };
//   SECTION ("CreateAccountCancel", "[matchmaking]")
//   {
//     loginMachine.process_event (CreateAccount{ "newAcc", "abc" });
//     loginMachine.process_event (CreateAccountCancel{});
//     ioContext.run ();
//     REQUIRE (loginMachine.is (state<NotLoggedin>));
//   }
//   SECTION ("LoginAccountCancel", "[matchmaking]")
//   {
//     database::createAccount ("oldAcc", "$argon2id$v=19$m=8,t=1,p=1$+Z8rjMS3CYbgMdG+JRgc6A$IAmEYrfE66+wsRmzeyPkyZ+xUJn+ybnx0HzKykO9NeY");
//     loginMachine.process_event (LoginAccount{ "oldAcc", "abc" });
//     loginMachine.process_event (LoginAccountCancel{});
//     ioContext.run ();
//     REQUIRE (loginMachine.is (state<NotLoggedin>));
//   }
//   ioContext.stop ();
//   ioContext.reset ();
// }

// TEST_CASE ("matchmaking Loggedin -> Loggedin", "[matchmaking]")
// {
//   database::createEmptyDatabase ();
//   database::createTables ();
//   typedef sml::sm<Matchmaking> MatchmakingMachine;
//   using namespace sml;
//   using namespace boost::asio;
//   auto ioContext = io_context ();
//   auto users_ = std::list<std::shared_ptr<User>>{ std::make_shared<User> (), std::make_shared<User> () };
//   boost::asio::thread_pool pool_{};
//   std::list<GameLobby> gameLobbies_{};
//   auto matchmaking = Matchmaking{ ioContext, pool_, gameLobbies_, {} };
//   MatchmakingMachine loginMachine{ matchmaking };
//   loginMachine.process_event (CreateAccount{ "newAcc", "abc" });
//   ioContext.run ();
//   ioContext.stop ();
//   ioContext.reset ();
//   REQUIRE (loginMachine.is (state<Loggedin>));
//   SECTION ("JoinChannel", "[matchmaking]")
//   {
//     loginMachine.process_event (JoinChannel{ "my channel" });
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("BroadCastMessage", "[matchmaking]")
//   {
//     loginMachine.process_event (BroadCastMessage{ "my channel", "Hello World!" });
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("LeaveChannel", "[matchmaking]")
//   {
//     loginMachine.process_event (LeaveChannel{ "my channel" });
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("CreateGameLobby", "[matchmaking]")
//   {
//     loginMachine.process_event (CreateGameLobby{ "my channel", "" });
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("JoinGameLobby", "[matchmaking]")
//   {
//     loginMachine.process_event (JoinGameLobby{ "my channel", "" });
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("SetMaxUserSizeInCreateGameLobby", "[matchmaking]")
//   {
//     loginMachine.process_event (SetMaxUserSizeInCreateGameLobby{ 42 });
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("GameOption", "[matchmaking]")
//   {
//     loginMachine.process_event (shared_class::GameOption{ true, "some string" });
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("LeaveGameLobby", "[matchmaking]")
//   {
//     loginMachine.process_event (LeaveGameLobby{});
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("RelogTo", "[matchmaking]")
//   {
//     loginMachine.process_event (RelogTo{});
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("CreateGame", "[matchmaking]")
//   {
//     loginMachine.process_event (CreateGame{});
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("WantsToJoinGame", "[matchmaking]")
//   {
//     loginMachine.process_event (WantsToJoinGame{});
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("LeaveQuickGameQueue", "[matchmaking]")
//   {
//     loginMachine.process_event (LeaveQuickGameQueue{});
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   SECTION ("JoinMatchMakingQueue", "[matchmaking]")
//   {
//     loginMachine.process_event (JoinMatchMakingQueue{});
//     REQUIRE (loginMachine.is (state<Loggedin>));
//   }
//   ioContext.stop ();
//   ioContext.reset ();
// }

// TEST_CASE ("matchmaking Loggedin -> NotLoggedin", "[matchmaking]")
// {
//   database::createEmptyDatabase ();
//   database::createTables ();
//   typedef sml::sm<Matchmaking> MatchmakingMachine;
//   using namespace sml;
//   using namespace boost::asio;
//   auto ioContext = io_context ();
//   auto users_ = std::list<std::shared_ptr<User>>{ std::make_shared<User> (), std::make_shared<User> () };
//   boost::asio::thread_pool pool_{};
//   std::list<GameLobby> gameLobbies_{};
//   auto matchmaking = Matchmaking{ ioContext, pool_, gameLobbies_, {} };
//   MatchmakingMachine loginMachine{ matchmaking };
//   loginMachine.process_event (CreateAccount{ "newAcc", "abc" });
//   ioContext.run ();
//   ioContext.stop ();
//   ioContext.reset ();
//   REQUIRE (loginMachine.is (state<Loggedin>));
//   SECTION ("LogoutAccount", "[matchmaking]")
//   {
//     loginMachine.process_event (LogoutAccount{});
//     REQUIRE (loginMachine.is (state<NotLoggedin>));
//   }
//   ioContext.stop ();
//   ioContext.reset ();
// }

// // Test for play
// // TEST_CASE ("remove login machine while coroutine is running", "[matchmaking]")
// // {
// //   database::createEmptyDatabase ();
// //   database::createTables ();
// //   typedef sml::sm<Matchmaking> MatchmakingMachine;
// //   using namespace sml;
// //   using namespace boost::asio;
// //   auto ioContext = io_context ();
// //   auto users_ = std::list<std::shared_ptr<User>>{ std::make_shared<User> (), std::make_shared<User> () };
// //   boost::asio::thread_pool pool_{};
// //   std::list<GameLobby> gameLobbies_{};
// //   auto matchmaking = Matchmaking{ ioContext,   pool_, gameLobbies_ };
// //   auto loginMachine = std::make_unique<MatchmakingMachine> (matchmaking);
// //   SECTION ("LoginAccount", "[matchmaking]")
// //   {
// //     database::createAccount ("oldAcc", "$argon2id$v=19$m=8,t=1,p=1$+Z8rjMS3CYbgMdG+JRgc6A$IAmEYrfE66+wsRmzeyPkyZ+xUJn+ybnx0HzKykO9NeY");
// //     loginMachine->process_event (LoginAccount{ "oldAcc", "abc" });
// //     auto timer = std::make_shared<CoroTimer> (CoroTimer{ ioContext });
// //     timer->expires_after (std::chrono::seconds{ 5 });
// //     co_spawn (ioContext, timer->async_wait (), [&loginMachine] (auto) {
// //       // DO NOT FORGET TO SLOW DOWN THE ASNYC_HASH FUNCTION SO THIS FINISHES FIRST
// //       loginMachine.reset ();
// //       std::cout << "reset called!" << std::endl;
// //     });
// //     std::cout << "run called!" << std::endl;
// //     ioContext.run ();
// //   }
// //   ioContext.stop ();
// //   ioContext.reset ();
// // }