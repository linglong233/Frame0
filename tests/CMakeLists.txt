include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(ReactionTimerTests
    test_Timer.cpp
    test_Config.cpp
    test_Storage.cpp
    test_TestSession.cpp
)

target_link_libraries(ReactionTimerTests
    PRIVATE reaction_timer_lib gtest_main
)

include(GoogleTest)
gtest_discover_tests(ReactionTimerTests)
