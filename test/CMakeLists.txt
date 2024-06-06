add_executable(
  _test
  gameOption.cxx
  main.cxx
  matchmaking.cxx
  matchmakingGame.cpp
  rating.cxx
  server.cxx
  startGame.cxx
  useAsLib.cxx
  util.cxx)
find_package(Catch2)
find_package(modern_durak_game_option)

target_link_libraries(_test myproject_options myproject_warnings
                      modern_durak_game_option::modern_durak_game_option
                      matchmaking_proxy Catch2::Catch2WithMain)
target_compile_definitions(
  _test
  PUBLIC $<$<CONFIG:DEBUG>:DEBUG>
         # BOOST_ASIO_ENABLE_HANDLER_TRACKING LOG_FOR_STATE_MACHINE
         # LOG_CO_SPAWN_PRINT_EXCEPTIONS LOG_MY_WEBSOCKET
         # LOG_MY_WEBSOCKET_READ_END LOG_OBJECT_TO_STRING_WITH_OBJECT_NAME
)

target_include_directories(_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME _test COMMAND _test -r junit)
