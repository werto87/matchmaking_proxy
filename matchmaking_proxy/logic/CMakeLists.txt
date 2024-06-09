 target_sources(matchmaking_proxy PUBLIC
         matchmaking.cxx
         matchmakingGame.cxx
         rating.cxx
 )

 install(FILES
        matchmaking_proxy/logic/matchmaking.hxx
        matchmaking_proxy/logic/matchmakingAllowedTypes.hxx
        matchmaking_proxy/logic/matchmakingData.hxx
        matchmaking_proxy/logic/matchmakingGame.hxx
        matchmaking_proxy/logic/matchmakingGameAllowedTypes.hxx
        matchmaking_proxy/logic/rating.hxx
        DESTINATION include/matchmaking_proxy
        )
