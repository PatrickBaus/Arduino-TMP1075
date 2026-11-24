cmake_minimum_required(VERSION 3.14)
project(TMP1075Tests CXX)

# The library has to build as C++11, which is what the oldest Arduino cores use.
# GNU extensions stay enabled (gnu++11), exactly like the Arduino toolchains.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.12
  GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(doctest)
if(NOT doctest_POPULATED)
  # Only the single header is needed, so do not add doctest's own build.
  FetchContent_Populate(doctest)
endif()

# Fake Arduino core (Arduino.h, Wire.h), which records the bus traffic.
add_library(arduino_mocks STATIC mocks/Wire.cpp)
target_include_directories(arduino_mocks PUBLIC mocks)

# The library under test, built with strict warnings and coverage instrumentation.
add_library(tmp1075 STATIC ../src/TMP1075.cpp)
target_include_directories(tmp1075 PUBLIC ../src)
target_link_libraries(tmp1075 PUBLIC arduino_mocks)
target_compile_options(tmp1075 PRIVATE -Wall -Wextra -Werror --coverage -O0)

add_executable(tmp1075_tests test_main.cpp test_tmp1075.cpp)
target_include_directories(tmp1075_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR}/doctest)
target_link_libraries(tmp1075_tests PRIVATE tmp1075 --coverage)

enable_testing()
add_test(NAME tmp1075_tests COMMAND tmp1075_tests)
