cmake_minimum_required(VERSION 3.16)
project(miniflow)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-g")

if (APPLE)
    # needed for jthread
    set(CMAKE_CXX_FLAGS "-fexperimental-library")
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    add_definitions(-DNDEBUG)
    set(BUILD_TESTS OFF)
else()
    set(BUILD_TESTS ON)
endif()

include_directories("includes")

set(SOURCES
    "src/flow.cpp"
)

set(TEST_SOURCES
    "src/test_dag.cpp"
    "src/test_ring.cpp"
)

add_library(flow ${SOURCES})
add_executable(example "examples/example.cpp")
target_link_libraries(example PRIVATE flow)

if (BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(test ${TEST_SOURCES})
    target_link_libraries(test PRIVATE Catch2::Catch2WithMain)
endif()
