#
# Copyright (c) 2024 Waldemar Schneider (w-schneider1987 at web dot de)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

add_library(matchmaking_proxy)

add_library(myproject::matchmaking_proxy ALIAS matchmaking_proxy)

set(Logging)
if(LOG_BOOST_ASIO)
  list(APPEND Logging BOOST_ASIO_ENABLE_HANDLER_TRACKING)
endif()

target_compile_definitions(matchmaking_proxy PUBLIC 
${Logging}
)

find_package(confu_soci)

option(WITH_SSL_VERIFICATION "Enable ssl verification" OFF)
if(WITH_SSL_VERIFICATION)
  find_package(certify)
endif()

find_package(libsodium)
find_package(sml)
find_package(login_matchmaking_game_shared)
find_package(confu_json)
find_package(Boost)
find_package(magic_enum)
find_package(my_web_socket)
find_package(confu_algorithm)
target_link_libraries(
  matchmaking_proxy
  PUBLIC confu_soci::confu_soci
         libsodium::libsodium
         sml::sml
         login_matchmaking_game_shared::login_matchmaking_game_shared
         confu_json::confu_json
         Boost::headers
         magic_enum::magic_enum
         my_web_socket::my_web_socket
         confu_algorithm::confu_algorithm
)
if(WITH_SSL_VERIFICATION)
  target_link_libraries(matchmaking_proxy PUBLIC certify::core)
endif()
if(WITH_SSL_VERIFICATION)
  target_compile_definitions(matchmaking_proxy PUBLIC MATCHMAKING_PROXY_WITH_SSL_VERIFICATION)
endif()
target_compile_features(matchmaking_proxy PUBLIC cxx_std_20)

target_include_directories(matchmaking_proxy
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>)

add_subdirectory(database)
add_subdirectory(server)
add_subdirectory(logic)
add_subdirectory(pw_hash)
install(FILES
util.hxx
DESTINATION include/matchmaking_proxy/
)
install(TARGETS matchmaking_proxy DESTINATION lib)