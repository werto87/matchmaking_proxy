add_executable(_test
        gameOption.cxx
        main.cxx
        matchmaking.cxx
        matchmakingGame.cpp
        rating.cxx
        server.cxx
        startGame.cxx
        useAsLib.cxx
        util.cxx
        )
find_package(Catch2)
target_link_libraries(_test
        myproject_options
        myproject_warnings
        matchmaking_proxy
        Catch2::Catch2WithMain
        )

# TODO test address sanitizer with -fno-omit-frame-pointer
target_compile_definitions(_test INTERFACE
        $<$<CONFIG:DEBUG>:DEBUG>
        _LIBCPP_ENABLE_CXX20_REMOVED_TYPE_TRAITS

        # BOOST_ASIO_ENABLE_HANDLER_TRACKING
        # LOG_FOR_STATE_MACHINE
        LOG_CO_SPAWN_PRINT_EXCEPTIONS
        LOG_MY_WEBSOCKET
        #  LOG_MY_WEBSOCKET_READ_END

        # LOG_OBJECT_TO_STRING_WITH_OBJECT_NAME
        )
target_compile_definitions(_test PUBLIC
        PATH_TO_USER_DEFINED_GAME_OPTION="modern_durak_game_option/userDefinedGameOption.hxx"
        )
add_custom_command(
        TARGET _test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/database/combination.db
        ${CMAKE_CURRENT_BINARY_DIR}/test_database/combination.db
)

target_include_directories(_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
add_test(NAME _test
        COMMAND _test -r junit)



