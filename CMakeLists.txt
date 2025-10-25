cmake_minimum_required (VERSION 3.21)
project (multiplayer_clock)

add_library (clock_sync
  src/clock_sync.cpp
  src/clock_offset_calculator.cpp
  src/exponential_average.cpp
)
target_include_directories (clock_sync PUBLIC include)

# add_executable (${PROJECT_NAME} main.cpp)
# target_link_libraries (${PROJECT_NAME} clock_sync)

add_subdirectory (tests)