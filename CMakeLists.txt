cmake_minimum_required(VERSION 3.15.7)
if(NOT DEFINED CMAKE_CXX_STANDARD) # Only set the cxx_standard if it is not set
                                   # by someone else
  set(CMAKE_CXX_STANDARD 23)
endif()
project(
  matchmaking_proxy
  VERSION 0.0.1
  LANGUAGES CXX C)
include(cmake/PreventInSourceBuilds.cmake)
include(ProjectOptions.cmake)
myproject_setup_options()
myproject_local_options()
target_compile_features(myproject_options
                        INTERFACE cxx_std_${CMAKE_CXX_STANDARD})
add_library(myproject::myproject_options ALIAS myproject_options)
add_library(myproject::myproject_warnings ALIAS myproject_warnings)
add_subdirectory(matchmaking_proxy)
option(BUILD_TESTS "enable tests" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif(BUILD_TESTS)
add_executable(run_server main.cxx)
target_link_libraries(run_server matchmaking_proxy)

target_compile_definitions(
  run_server
  PRIVATE # BOOST_ASIO_DISABLE_THREADS
          LOG_CO_SPAWN_PRINT_EXCEPTIONS
          LOG_COMPUTER_CONTROLLED_OPPONENT_MASSAGE_RECEIVED
          LOG_FOR_STATE_MACHINE
          LOG_MY_WEBSOCKET
          BOOST_ASIO_ENABLE_HANDLER_TRACKING
          CURRENT_BINARY_DIR="${CMAKE_BINARY_DIR}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_custom_target(copy_compile_commands_json ALL
                  DEPENDS ${CMAKE_SOURCE_DIR}/compile_commands.json)

add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/compile_commands.json
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/compile_commands.json
          ${CMAKE_SOURCE_DIR}/compile_commands.json
  COMMENT
    "copy compile_commands.json build to compile_commands.json project root so tools can find it more easily"
)
