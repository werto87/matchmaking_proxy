target_sources(${TARGET_NAME_LIB} PRIVATE 
${CMAKE_CURRENT_LIST_DIR}/util.hxx
util.cxx
serialization.hxx
)

add_subdirectory(database)
add_subdirectory(game)
add_subdirectory(server)
add_subdirectory(logic)
add_subdirectory(pw_hash)