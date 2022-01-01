target_sources(${TARGET_NAME_LIB} INTERFACE 
    util.hxx
    userMatchmakingSerialization.hxx
    matchmakingGameSerialization.hxx
)

add_subdirectory(database)
add_subdirectory(server)
add_subdirectory(logic)
add_subdirectory(pw_hash)