 target_sources(matchmaking_proxy PUBLIC
         gameLobby.cxx
         server.cxx
  )


 install(FILES
        gameLobby.hxx
        matchmakingOption.hxx
        server.hxx
        user.hxx
        DESTINATION include/matchmaking_proxy/server
  )