target_sources(matchmaking_proxy PUBLIC
        database.cxx
        )

install(FILES
        constant.hxx
        database.hxx
        DESTINATION include/matchmaking_proxy/database
)        