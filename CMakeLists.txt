cmake_minimum_required(VERSION 3.30)
project(AlgoGRIS
  VERSION 1.0.0
  DESCRIPTION "Spatialization Algorithms"
  HOMEPAGE_URL "https://github.com/GRIS-UdeM/AlgoGRIS"
  LANGUAGES C CXX
)

# Dependencies
include(CTest)
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

add_subdirectory(submodules/fork_union)
add_subdirectory(submodules/StructGRIS) # JUCE & JuceHeader.h now handled there

# Global CMake configuration
set(CMAKE_INCLUDE_CURRENT_DIR 1)
set(CMAKE_CXX_SCAN_FOR_MODULES 0)
set(CMAKE_CXX_STANDARD 20)

find_package(CURL QUIET)

# Library setup
add_library(AlgoGRIS
    sg_AbstractSpatAlgorithm.cpp
    sg_AbstractSpatAlgorithm.hpp
    sg_DopplerSpatAlgorithm.cpp
    sg_DopplerSpatAlgorithm.hpp
    sg_DummySpatAlgorithm.hpp
    sg_HrtfSpatAlgorithm.cpp
    sg_HrtfSpatAlgorithm.hpp
    sg_HybridSpatAlgorithm.cpp
    sg_HybridSpatAlgorithm.hpp
    sg_MbapSpatAlgorithm.cpp
    sg_MbapSpatAlgorithm.hpp
    sg_PinkNoiseGenerator.cpp
    sg_PinkNoiseGenerator.hpp
    sg_StereoSpatAlgorithm.cpp
    sg_StereoSpatAlgorithm.hpp
    sg_VbapSpatAlgorithm.cpp
    sg_VbapSpatAlgorithm.hpp

    Implementations/sg_mbap.cpp
    Implementations/sg_mbap.hpp
    Implementations/sg_vbap.cpp
    Implementations/sg_vbap.hpp

    tl/optional.hpp
)

target_include_directories(AlgoGRIS
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/submodules/StructGRIS"
)

target_link_libraries(AlgoGRIS
  PUBLIC
    StructGRIS
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_dsp
    fork_union
)

target_compile_definitions(AlgoGRIS
  PUBLIC
    $<$<BOOL:${BUILD_TESTING}>:ALGOGRIS_UNIT_TESTS>
)

# Tests
add_library(MainTest STATIC
  tests/Catch2Main.cpp
  tests/sg_TestUtils.cpp
  tests/sg_TestUtils.hpp
)

target_link_libraries(MainTest PUBLIC
  Catch2::Catch2
  AlgoGRIS
)

if(TARGET CURL::libcurl)
  target_link_libraries(MainTest PUBLIC CURL::libcurl)
endif()

if(TARGET Catch2::Catch2WithMain)
  include(Catch)
  function(algogris_add_test _filename)
    get_filename_component(_testname "${_filename}" NAME_WE)
    add_executable("${_testname}" "${_filename}")
    target_link_libraries("${_testname}" PRIVATE MainTest)
    target_compile_options("${_testname}" PRIVATE -O0)
    catch_discover_tests("${_testname}")
  endfunction()

  algogris_add_test("tests/unit/test_core.cpp")
  algogris_add_test("tests/unit/test_spatAlgorithms.cpp")
  algogris_add_test("tests/unit/test_speaker_setup_conversion.cpp")
endif()
