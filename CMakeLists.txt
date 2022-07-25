cmake_minimum_required(VERSION 3.22)
project(project VERSION 1.0.0
  DESCRIPTION project
  LANGUAGES CXX
)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
include(cmake/Cache.cmake)
set(PROJECT_WARNINGS project_warning)
add_library(${PROJECT_WARNINGS} INTERFACE)
include(cmake/CompilerWarnings.cmake)
set_project_warnings(${PROJECT_WARNINGS})
set(PROJECT_OPTIONS project_options)
add_library(${PROJECT_OPTIONS} INTERFACE)
target_compile_features(${PROJECT_OPTIONS} INTERFACE cxx_std_20)
include(cmake/StaticAnalyzers.cmake)
include(cmake/Sanitizers.cmake)
enable_sanitizers(${PROJECT_OPTIONS})
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(TARGET_NAME_LIB project_lib)
add_library(${TARGET_NAME_LIB} INTERFACE)
add_subdirectory(matchmaking_proxy)
target_link_libraries(${TARGET_NAME_LIB} INTERFACE
  ${PROJECT_WARNINGS}
  ${PROJECT_OPTIONS}
  CONAN_PKG::range-v3
  CONAN_PKG::confu_soci
  CONAN_PKG::confu_json
  CONAN_PKG::libsodium
  CONAN_PKG::certify
  CONAN_PKG::pipes
  CONAN_PKG::sml
  CONAN_PKG::fmt
  CONAN_PKG::corrade
)

# TODO test address sanitizer with -fno-omit-frame-pointer
target_compile_definitions(${TARGET_NAME_LIB} INTERFACE
  $<$<CONFIG:DEBUG>:DEBUG>
  _LIBCPP_ENABLE_CXX20_REMOVED_TYPE_TRAITS

  # BOOST_ASIO_ENABLE_HANDLER_TRACKING
  # LOG_FOR_STATE_MACHINE
  LOG_CO_SPAWN_PRINT_EXCEPTIONS
  LOG_MY_WEBSOCKET
  LOG_MY_WEBSOCKET_READ_END

  # LOG_OBJECT_TO_STRING_WITH_OBJECT_NAME
)

# create executable
OPTION(BUILD_TESTS "enable tests" OFF)

IF(BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
ENDIF(BUILD_TESTS)

set(TARGET_NAME project)
add_executable(${TARGET_NAME} main.cxx)
target_link_libraries(${TARGET_NAME} PRIVATE
  ${TARGET_NAME_LIB}
  ${PROJECT_WARNINGS}
  ${PROJECT_OPTIONS}
  CONAN_PKG::range-v3
  CONAN_PKG::confu_soci
  CONAN_PKG::confu_json
  CONAN_PKG::libsodium
  CONAN_PKG::certify
  CONAN_PKG::pipes
  CONAN_PKG::sml
  CONAN_PKG::fmt
  CONAN_PKG::durak
)

target_compile_definitions(${TARGET_NAME} PRIVATE PATH_TO_USER_DEFINED_GAME_OPTION="userDefinedGameOption.hxx"
)