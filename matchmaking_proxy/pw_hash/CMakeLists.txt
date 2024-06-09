 target_sources(matchmaking_proxy PUBLIC
 )

 install(FILES
        matchmaking_proxy/pw_hash/constant.hxx
        matchmaking_proxy/pw_hash/passwordHash.hxx
        DESTINATION include/matchmaking_proxy
        )