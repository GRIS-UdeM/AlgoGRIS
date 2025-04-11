cmake_minimum_required(VERSION 3.5)
project(AlgoGRIS VERSION 0.0.1)

# Specify the source test directory
set(SOURCE_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# Set the output directory for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Collect source files
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${SOURCE_TESTS_DIR}/*.cpp"
)

# Define the SharedCode target
target_sources(SharedCode INTERFACE ${SourceFiles})

# Define the AlgoGRIS_test target
add_executable(AlgoGRIS_test ${SOURCE_TESTS_DIR}/tests.cpp)

# Set C++ standard for targets
set_target_properties(AlgoGRIS_test PROPERTIES CXX_STANDARD 11)

# Include directories
target_include_directories(AlgoGRIS_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link dependencies
target_link_libraries(AlgoGRIS_test PRIVATE SharedCode)
