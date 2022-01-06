#include "matchmaking.hxx"
#include "matchmaking_proxy/logic/rating.hxx"
#include "matchmaking_proxy/userMatchmakingSerialization.hxx"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <memory>
#include <range/v3/algorithm/copy_if.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/iterator/insert_iterators.hpp>

auto constexpr ALLOWED_DIFFERENCE_FOR_RANKED_GAME_MATCHMAKING = size_t{ 100 };

bool
isInRatingrange (size_t userRating, size_t lobbyAverageRating)
{
  auto const difference = userRating > lobbyAverageRating ? userRating - lobbyAverageRating : lobbyAverageRating - userRating;
  return difference < ALLOWED_DIFFERENCE_FOR_RANKED_GAME_MATCHMAKING;
}

bool
checkRating (size_t userRating, std::vector<std::string> const &accountNames)
{
  return isInRatingrange (userRating, averageRating (accountNames));
}

bool
matchingLobby (std::string const &accountName, GameLobby const &gameLobby, GameLobby::LobbyType const &lobbyType)
{
  if (gameLobby.lobbyAdminType == lobbyType && gameLobby.accountNames.size () < gameLobby.maxUserCount ())
    {
      if (lobbyType == GameLobby::LobbyType::MatchMakingSystemRanked)
        {
          soci::session sql (soci::sqlite3, databaseName);
          if (auto userInDatabase = confu_soci::findStruct<database::Account> (sql, "accountName", accountName))
            {
              return checkRating (userInDatabase->rating, gameLobby.accountNames);
            }
        }
      else
        {
          return true;
        }
    }
  else
    {
      return false;
    }
  return false;
}

void
Matchmaking::sendToUser (SendMessageToUser const &sendMessageToUser)
{
  matchmakingCallbacks.sendMsgToUser (sendMessageToUser.msg);
}

void
Matchmaking::sendToGame (user_matchmaking::SendMessageToGame const &sendMessageToGame)
{
  std::cout << "msg to server: " << sendMessageToGame.msg << std::endl;
  matchmakingGame.sendMessage (std::move (sendMessageToGame.msg));
}

void
Matchmaking::sendToAllAccountsInUsersCreateGameLobby (std::string const &message)
{
  if (auto gameLobby = ranges::find_if (gameLobbies, [&accountName = user.accountName] (GameLobby const &gameLobby) { return ranges::find (gameLobby.accountNames, accountName) != gameLobby.accountNames.end (); }); gameLobby != gameLobbies.end ())
    {
      matchmakingCallbacks.sendMsgToUsers (message, gameLobby->accountNames);
    }
}

void
Matchmaking::leaveGame ()
{
  matchmakingGame.close ();
}

void
Matchmaking::logoutAccount ()
{
  // if (isRegistered (user.accountName))
  //   {
  // TODO find a way to remove user from gamelobby
  // removeUserFromLobby ();
  // }
  user = {};
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LogoutAccountSuccess{}));
}

void
Matchmaking::cancelCreateAccount ()
{
  cancelCoroutineTimer->cancel ();
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountCancel{}));
}

void
Matchmaking::informUserWantsToRelogToGameLobby ()
{
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::WantToRelog{ user.accountName, "Create Game Lobby" }));
}

void
Matchmaking::leaveGameLobbyErrorUserNotFoundInLobby ()
{
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbyError{ "could not remove user from lobby user not found in lobby" }));
}

void
Matchmaking::leaveGameLobbyErrorNotControllerByUsers ()
{
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbyError{ "not allowed to leave a game lobby which is controlled by the matchmaking system with leave game lobby" }));
}

void
Matchmaking::joinChannel (user_matchmaking::JoinChannel const &joinChannelObject)
{
  user.communicationChannels.insert (joinChannelObject.channel);
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinChannelSuccess{ joinChannelObject.channel }));
}

void
Matchmaking::joinGameLobby (user_matchmaking::JoinGameLobby const &joinGameLobbyObject)
{
  if (auto gameLobby = ranges::find_if (gameLobbies, [gameLobbyName = joinGameLobbyObject.name, lobbyPassword = joinGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName && _gameLobby.password == lobbyPassword; }); gameLobby != gameLobbies.end ())
    {
      if (auto error = gameLobby->tryToAddUser (user))
        {
          matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ joinGameLobbyObject.name, error.value () }));
          return;
        }
      else
        {
          matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbySuccess{}));
          auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobby->maxUserCount ();
          usersInGameLobby.name = gameLobby->name.value ();
          usersInGameLobby.durakGameOption = gameLobby->gameOption;
          ranges::transform (gameLobby->accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
          sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (usersInGameLobby));
          return;
        }
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ joinGameLobbyObject.name, "wrong password name combination or lobby does not exists" }));
      return;
    }
}

void
Matchmaking::relogToGameLobby ()
{
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                   [accountName = user.accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames;
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbies.end ())
    {

      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToCreateGameLobbySuccess{}));
      auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
      usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
      usersInGameLobby.name = gameLobbyWithAccount->name.value ();
      usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
      ranges::transform (gameLobbyWithAccount->accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (usersInGameLobby));
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
    }
}

void
Matchmaking::leaveGameLobby ()
{
  auto gameLobbyWithAccount = ranges::find_if (gameLobbies, [accountName = user.accountName] (auto const &gameLobby) {
    auto const &accountNames = gameLobby.accountNames;
    return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
  });
  gameLobbyWithAccount->removeUser (user.accountName);
  if (gameLobbyWithAccount->accountCount () == 0)
    {
      gameLobbies.erase (gameLobbyWithAccount);
    }
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveGameLobbySuccess{}));
  return;
}

void
Matchmaking::setGameOption (shared_class::GameOption const &gameOption)
{
  auto accountNameToSearch = user.accountName;
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                   [accountName = user.accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames;
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbies.end ())
    {
      if (gameLobbyWithAccount->getWaitingForAnswerToStartGame ())
        {
          matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "It is not allowed to change game option while ask to start a game is running" }));
        }
      else
        {
          if (gameLobbyWithAccount->isGameLobbyAdmin (user.accountName))
            {
              gameLobbyWithAccount->gameOption = gameOption;
              sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (gameOption));
              return;
            }
          else
            {
              matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "you need to be admin in the create game lobby to change game option" }));
              return;
            }
        }
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ "could not find a game lobby for account" }));
      return;
    }
}

void
Matchmaking::setMaxUserSizeInCreateGameLobby (user_matchmaking::SetMaxUserSizeInCreateGameLobby const &setMaxUserSizeInCreateGameLobbyObject)
{
  auto accountNameToSearch = user.accountName;
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                   [accountName = user.accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames;
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbies.end ())
    {
      if (gameLobbyWithAccount->getWaitingForAnswerToStartGame ())
        {
          matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "It is not allowed to change lobby while ask to start a game is running" }));
        }
      else
        {
          if (gameLobbyWithAccount->isGameLobbyAdmin (user.accountName))
            {
              if (auto errorMessage = gameLobbyWithAccount->setMaxUserCount (setMaxUserSizeInCreateGameLobbyObject.maxUserSize))
                {
                  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ errorMessage.value () }));
                  return;
                }
              else
                {
                  sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (user_matchmaking::MaxUserSizeInCreateGameLobby{ setMaxUserSizeInCreateGameLobbyObject.maxUserSize }));
                  return;
                }
            }
          else
            {
              matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "you need to be admin in a game lobby to change the user size" }));
              return;
            }
        }
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::SetMaxUserSizeInCreateGameLobbyError{ "could not find a game lobby for account" }));
      return;
    }
}

void
Matchmaking::createGameLobby (user_matchmaking::CreateGameLobby const &createGameLobbyObject)
{
  if (ranges::find_if (gameLobbies, [gameLobbyName = createGameLobbyObject.name, lobbyPassword = createGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName; }) == gameLobbies.end ())
    {
      if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                    [accountName = user.accountName] (auto const &gameLobby) {
                                                      auto const &accountNames = gameLobby.accountNames;
                                                      return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                    });
          gameLobbyWithUser != gameLobbies.end ())
        {
          matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameLobbyError{ { "account has already a game lobby with the name: " + gameLobbyWithUser->name.value_or ("Quick Game Lobby") } }));
          return;
        }
      else
        {
          auto &newGameLobby = gameLobbies.emplace_back (GameLobby{ createGameLobbyObject.name, createGameLobbyObject.password, matchmakingCallbacks.sendMsgToUsers });
          if (newGameLobby.tryToAddUser (user))
            {
              throw std::logic_error{ "user can not join lobby which he created" };
            }
          else
            {
              auto result = std::vector<std::string>{};
              auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = newGameLobby.maxUserCount ();
              usersInGameLobby.name = newGameLobby.name.value ();
              usersInGameLobby.durakGameOption = newGameLobby.gameOption;
              ranges::transform (newGameLobby.accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
              matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbySuccess{}));
              matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (usersInGameLobby));
              return;
            }
        }
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameLobbyError{ { "lobby already exists with name: " + createGameLobbyObject.name } }));
      return;
    }
}

void
Matchmaking::createGame ()
{
  if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                [accountName = user.accountName] (auto const &gameLobby) {
                                                  auto const &accountNames = gameLobby.accountNames;
                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                });
      gameLobbyWithUser != gameLobbies.end ())
    {
      if (gameLobbyWithUser->getWaitingForAnswerToStartGame ())
        {
          matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "It is not allowed to start a game while ask to start a game is running" }));
        }
      else
        {
          if (gameLobbyWithUser->isGameLobbyAdmin (user.accountName))
            {
              if (gameLobbyWithUser->accountNames.size () >= 2)
                {
                  if (auto gameOptionError = errorInGameOption (gameLobbyWithUser->gameOption))
                    {
                      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameOptionError{ gameOptionError.value () }));
                    }
                  else
                    {
                      askUsersToJoinGame (gameLobbyWithUser);
                    }
                }
              else
                {
                  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "You need atleast two user to create a game" }));
                }
            }
          else
            {
              matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "you need to be admin in a game lobby to start a game" }));
            }
        }
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateGameError{ "Could not find a game lobby for the user" }));
    }
}

void
Matchmaking::askUsersToJoinGame (std::list<GameLobby>::iterator &gameLobby)
{
  sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGame{}));
  gameLobby->startTimerToAcceptTheInvite (io_context, [gameLobby, this] () {
    auto notReadyUsers = std::vector<std::string>{};
    ranges::copy_if (gameLobby->accountNames, ranges::back_inserter (notReadyUsers), [usersWhichAccepted = gameLobby->readyUsers] (std::string const &accountNamesGamelobby) mutable { return ranges::find_if (usersWhichAccepted, [accountNamesGamelobby] (std::string const &userWhoAccepted) { return accountNamesGamelobby == userWhoAccepted; }) == usersWhichAccepted.end (); });
    matchmakingCallbacks.sendMsgToUsers (objectToStringWithObjectName (user_matchmaking::AskIfUserWantsToJoinGameTimeOut{}), notReadyUsers);
    for (auto const &notReadyUser : notReadyUsers)
      {
        if (gameLobby->lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers)
          {
            gameLobby->removeUser (notReadyUser);
          }
      }
    if (gameLobby->accountNames.empty ())
      {
        gameLobbies.erase (gameLobby);
      }
    else
      {
        gameLobby->readyUsers.clear ();
        sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (user_matchmaking::GameStartCanceled{}));
      }
  });
}

void
Matchmaking::leaveChannel (user_matchmaking::LeaveChannel const &leaveChannelObject)
{
  if (user.communicationChannels.erase (leaveChannelObject.channel))
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveChannelSuccess{ leaveChannelObject.channel }));
      return;
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveChannelError{ leaveChannelObject.channel, { "channel not found" } }));
      return;
    }
}

void
Matchmaking::informUserCreateAccountError ()
{
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::CreateAccountError{ user.accountName, "Account already Created" }));
}

void
Matchmaking::informUserAccountNotRegistered ()
{
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ user.accountName, "Incorrect Username or Password" }));
}

void
Matchmaking::informUserLoginAccountSuccess ()
{
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountSuccess{ user.accountName }));
}

void
Matchmaking::broadCastMessage (user_matchmaking::BroadCastMessage const &broadCastMessageObject)
{
  // TODO send to all users which are in the channel
  // for (auto &user_ : users | ranges::views::filter ([channel = broadCastMessageObject.channel, accountName = user.accountName] (auto const &user_) { return user_->communicationChannels.find (channel) != user_->communicationChannels.end (); }))
  //   {
  //     soci::session sql (soci::sqlite3, databaseName);
  //     auto message = user_matchmaking::Message{ user_->accountName, broadCastMessageObject.channel, broadCastMessageObject.message };
  //     user_->sendMessageToUser (objectToStringWithObjectName (std::move (message)));
  //   }
}

boost::asio::awaitable<void>
Matchmaking::abortCoroutine ()
{
  try
    {
      co_await cancelCoroutineTimer->async_wait ();
    }
  catch (boost::system::system_error &e)
    {
      using namespace boost::system::errc;
      if (operation_canceled == e.code ())
        {
          cancelCoroutineTimer->expires_after (std::chrono::system_clock::time_point::max () - std::chrono::system_clock::now ());
          co_return;
        }
      else
        {
          std::cout << "error in timer boost::system::errc: " << e.code () << std::endl;
          abort ();
        }
    }
}

void
Matchmaking::cancelLoginAccount ()
{
  cancelCoroutineTimer->cancel ();
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountCancel{}));
}

void
Matchmaking::createAccount (PasswordHashed const &passwordHash)
{
  database::createAccount (user.accountName, passwordHash.hashedPassword);
}

boost::asio::awaitable<void>
Matchmaking::startGame (GameLobby const &gameLobby)
{
  try
    {
      auto startServerAnswer = co_await sendStartGameToServer (gameLobby);
      std::cout << "msg from server when creating game: " << startServerAnswer << std::endl;
      std::vector<std::string> splitMesssage{};
      boost::algorithm::split (splitMesssage, startServerAnswer, boost::is_any_of ("|"));
      if (splitMesssage.size () == 2)
        {
          auto const &typeToSearch = splitMesssage.at (0);
          if (typeToSearch == "StartGameSuccess")
            {
              matchmakingCallbacks.connectToGame (gameLobby.accountNames);
            }
          else if (typeToSearch == "StartGameError")
            {
              sendToAllAccountsInUsersCreateGameLobby (startServerAnswer);
            }
        }
    }
  catch (std::exception &e)
    {
      std::cout << "Start Game exception: " << e.what () << std::endl;
    }
}

boost::asio::awaitable<std::string>
Matchmaking::sendStartGameToServer (GameLobby const &gameLobby)
{
  auto ws = Websocket{ io_context };
  auto gameEndpoint = boost::asio::ip::tcp::endpoint{ boost::asio::ip::tcp::v4 (), 44444 };
  co_await ws.next_layer ().async_connect (gameEndpoint);
  ws.next_layer ().expires_never ();
  ws.set_option (boost::beast::websocket::stream_base::timeout::suggested (boost::beast::role_type::client));
  ws.set_option (boost::beast::websocket::stream_base::decorator ([] (boost::beast::websocket::request_type &req) { req.set (boost::beast::http::field::user_agent, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-async"); }));
  co_await ws.async_handshake ("localhost:" + std::to_string (gameEndpoint.port ()), "/");
  auto startGame = user_matchmaking::StartGame{};
  startGame.players = gameLobby.accountNames;
  startGame.gameOption = gameLobby.gameOption;
  co_await ws.async_write (boost::asio::buffer (objectToStringWithObjectName (startGame)));
  boost::beast::flat_buffer buffer;
  co_await ws.async_read (buffer);
  auto msg = boost::beast::buffers_to_string (buffer.data ());
  co_return msg;
}

void
Matchmaking::removeUserFromGameLobby ()
{
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                   [accountName = user.accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames;
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbies.end ())
    {
      gameLobbyWithAccount->removeUser (user.accountName);
      if (gameLobbyWithAccount->accountCount () == 0)
        {
          gameLobbies.erase (gameLobbyWithAccount);
        }
      else
        {
          auto usersInGameLobby = user_matchmaking::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
          usersInGameLobby.name = gameLobbyWithAccount->name.value ();
          usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
          ranges::transform (gameLobbyWithAccount->accountNames, ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return user_matchmaking::UserInGameLobby{ accountName }; });
          sendToAllAccountsInUsersCreateGameLobby (objectToStringWithObjectName (usersInGameLobby));
        }
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
    }
}

void
Matchmaking::joinMatchMakingQueue (GameLobby::LobbyType const &lobbyType)
{
  if (ranges::find_if (gameLobbies,
                       [accountName = user.accountName] (auto const &gameLobby) {
                         auto const &accountNames = gameLobby.accountNames;
                         return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                       })
      == gameLobbies.end ())
    {
      if (auto gameLobbyToAddUser = ranges::find_if (gameLobbies, [lobbyType, accountName = user.accountName] (GameLobby const &gameLobby) { return matchingLobby (accountName, gameLobby, lobbyType); }); gameLobbyToAddUser != gameLobbies.end ())
        {
          if (auto error = gameLobbyToAddUser->tryToAddUser (user))
            {
              matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ user.accountName, error.value () }));
            }
          else
            {
              matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueSuccess{}));
              if (gameLobbyToAddUser->accountNames.size () == gameLobbyToAddUser->maxUserCount ())
                {
                  askUsersToJoinGame (gameLobbyToAddUser);
                }
            }
        }
      else
        {
          auto gameLobby = GameLobby{};
          gameLobby.lobbyAdminType = lobbyType;
          if (auto error = gameLobby.tryToAddUser (user))
            {
              matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinGameLobbyError{ user.accountName, error.value () }));
            }
          gameLobbies.emplace_back (gameLobby);
          matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueSuccess{}));
        }
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::JoinMatchMakingQueueError{ "User is allready in gamelobby" }));
    }
}

void
Matchmaking::wantsToJoinAGameWrapper (user_matchmaking::WantsToJoinGame const &wantsToJoinGameEv)
{
  co_spawn (io_context, wantsToJoinGame (wantsToJoinGameEv), boost::asio::detached);
}

boost::asio::awaitable<void>
Matchmaking::wantsToJoinGame (user_matchmaking::WantsToJoinGame const &wantsToJoinGameEv)
{
  if (auto gameLobby = ranges::find_if (gameLobbies,
                                        [accountName = user.accountName] (auto const &gameLobby) {
                                          auto const &accountNames = gameLobby.accountNames;
                                          return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                        });
      gameLobby != gameLobbies.end ())
    {
      if (wantsToJoinGameEv.answer)
        {
          if (ranges::find_if (gameLobby->readyUsers, [accountName = user.accountName] (std::string const &readyUserAccountName) { return readyUserAccountName == accountName; }) == gameLobby->readyUsers.end ())
            {
              gameLobby->readyUsers.push_back (user.accountName);
              if (gameLobby->readyUsers.size () == gameLobby->accountNames.size ())
                {
                  co_await startGame (*gameLobby);
                  gameLobbies.erase (gameLobby);
                }
            }
          else
            {
              matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::WantsToJoinGameError{ "You already accepted to join the game" }));
            }
        }
      else
        {
          gameLobby->cancelTimer ();
          if (gameLobby->lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers)
            {
              matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::GameStartCanceledRemovedFromQueue{}));
              gameLobby->removeUser (user.accountName);
              if (gameLobby->accountNames.empty ())
                {
                  gameLobbies.erase (gameLobby);
                }
            }
        }
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::WantsToJoinGameError{ "No game to join" }));
    }
}

void
Matchmaking::leaveMatchMakingQueue ()
{
  if (auto gameLobby = ranges::find_if (gameLobbies,
                                        [accountName = user.accountName] (auto const &gameLobby) {
                                          auto const &accountNames = gameLobby.accountNames;
                                          return gameLobby.lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers && ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                        });
      gameLobby != gameLobbies.end ())
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveQuickGameQueueSuccess{}));
      gameLobby->removeUser (user.accountName);
      gameLobby->cancelTimer ();
      if (gameLobby->accountNames.empty ())
        {
          gameLobbies.erase (gameLobby);
        }
    }
  else
    {
      matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LeaveQuickGameQueueError{ "User is not in queue" }));
    }
}

void
Matchmaking::loginAsGuest ()
{
  user.accountName = boost::uuids::to_string (boost::uuids::random_generator () ());
}

void
Matchmaking::informUserAlreadyLoggedin ()
{
  matchmakingCallbacks.sendMsgToUser (objectToStringWithObjectName (user_matchmaking::LoginAccountError{ user.accountName, "Account already logged in" }));
}