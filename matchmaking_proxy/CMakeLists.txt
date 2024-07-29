#
# Copyright (c) 2024 Waldemar Schneider (w-schneider1987 at web dot de)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

add_library(matchmaking_proxy)

add_library(myproject::matchmaking_proxy ALIAS matchmaking_proxy)

find_package(confu_soci)
find_package(certify)
find_package(libsodium)
find_package(sml)
find_package(login_matchmaking_game_shared)
find_package(durak)
find_package(confu_json)
find_package(Boost)
find_package(magic_enum)
find_package(range-v3)
find_package(my_web_socket)

target_link_libraries(
  matchmaking_proxy
  PUBLIC confu_soci::confu_soci
         certify::core
         libsodium::libsodium
         sml::sml
         range-v3::range-v3
         login_matchmaking_game_shared::login_matchmaking_game_shared
         durak::durak
         confu_json::confu_json
         boost::boost
         magic_enum::magic_enum
         my_web_socket::my_web_socket
         )


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