target_sources(${TARGET_NAME_LIB} INTERFACE 
util.cxx
)

add_subdirectory(database)
add_subdirectory(server)
add_subdirectory(logic)
add_subdirectory(pw_hash)