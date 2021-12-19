target_sources(${TARGET_NAME_LIB} PRIVATE 
    util.hxx
    serialization.hxx
)

add_subdirectory(database)
add_subdirectory(server)
add_subdirectory(logic)
add_subdirectory(pw_hash)