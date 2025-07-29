 target_sources(matchmaking_proxy PUBLIC
         matchmaking.cxx
         matchmakingGame.cxx
         rating.cxx
         matchmakingData.cxx
 )

 install(FILES
        matchmaking.hxx
        matchmakingAllowedTypes.hxx
        matchmakingData.hxx
        matchmakingGame.hxx
        matchmakingGameData.hxx
        matchmakingGameAllowedTypes.hxx
        rating.hxx
        DESTINATION include/matchmaking_proxy/logic
        )
