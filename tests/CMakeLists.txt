# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


add_executable (${PROJECT_NAME}_test_time_window test_time_window.cpp)
target_link_libraries (${PROJECT_NAME}_test_time_window GTest::gtest_main)
target_include_directories (${PROJECT_NAME}_test_time_window PRIVATE ../include)

add_executable (${PROJECT_NAME}_test_clock_sync test_clock_sync.cpp)
target_link_libraries (${PROJECT_NAME}_test_clock_sync GTest::gtest_main clock_sync)
target_include_directories (${PROJECT_NAME}_test_clock_sync PRIVATE ../include)
target_compile_options (${PROJECT_NAME}_test_clock_sync PRIVATE -Wall -Werror -Wpedantic)
include (GoogleTest)
gtest_discover_tests (
  ${PROJECT_NAME}_test_time_window
  ${PROJECT_NAME}_test_clock_sync
)