 target_sources(matchmaking_proxy PUBLIC
         gameLobby.cxx
         server.cxx
  )


 install(FILES
        matchmaking_proxy/server/gameLobby.hxx
        matchmaking_proxy/server/matchmakingOption.hxx
        matchmaking_proxy/server/myWebsocket.hxx
        matchmaking_proxy/server/server.hxx
        matchmaking_proxy/server/user.hxx
        DESTINATION include/matchmaking_proxy
  )