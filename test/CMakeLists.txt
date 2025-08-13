add_executable(
  _test
  main.cxx
  matchmaking.cxx
  matchmakingGame.cpp
  rating.cxx
  server.cxx
  startGame.cxx
  database.cxx
  networkingUtil.cxx
  )
add_subdirectory(benchmark)
find_package(Catch2)
find_package(modern_durak_game_option)
find_package(modern_durak_game_shared)

target_link_libraries(_test myproject_options myproject_warnings
                      modern_durak_game_option::modern_durak_game_option
                      matchmaking_proxy Catch2::Catch2WithMain
                      modern_durak_game_shared::modern_durak_game_shared
                      )
target_compile_definitions(
  _test
  PUBLIC $<$<CONFIG:DEBUG>:DEBUG>
         # BOOST_ASIO_ENABLE_HANDLER_TRACKING 
        #  MATCHMAKING_PROXY_LOG_FOR_STATE_MACHINE
        #  MATCHMAKING_PROXY_LOG_OBJECT_TO_STRING_WITH_OBJECT_NAME
)

target_include_directories(_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME _test COMMAND _test -r junit)
if (CMAKE_BINARY_DIR)
    target_compile_definitions(_test PRIVATE PATH_TO_BINARY="${CMAKE_BINARY_DIR}")
    target_compile_definitions(_test PRIVATE PATH_TO_SOURCE="${CMAKE_SOURCE_DIR}")
endif()