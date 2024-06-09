target_sources(matchmaking_proxy PUBLIC
        database.cxx
        )

install(FILES
        matchmaking_proxy/database/constant.hxx
        matchmaking_proxy/database/database.hxx
        DESTINATION include/matchmaking_proxy
)        