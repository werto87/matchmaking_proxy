set(TEST_TARGET_NAME _test)
add_executable(${TEST_TARGET_NAME}
        main.cxx
        util.cxx
        rating.cxx
        matchmaking.cxx
        useAsLib.cxx
        startGame.cxx
        server.cxx
        gameOption.cxx
        matchmakingGame.cpp)

target_link_libraries(${TEST_TARGET_NAME}
        ${TARGET_NAME_LIB}
        CONAN_PKG::catch2
        CONAN_PKG::durak
        CONAN_PKG::modern_durak_game_option
        )
add_test(NAME ${TEST_TARGET_NAME}
        COMMAND ${TEST_TARGET_NAME} -r junit
        )

target_compile_definitions(${TEST_TARGET_NAME} PRIVATE
        PATH_TO_USER_DEFINED_GAME_OPTION="modern_durak_game_option/gameOption.hxx"
        )