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

# Global CMake configuration
set(CMAKE_INCLUDE_CURRENT_DIR 1)
set(CMAKE_CXX_SCAN_FOR_MODULES 0)
set(CMAKE_CXX_STANDARD 20)

# 1. Look for a system JUCE if any
if(NOT TARGET juce::juce_core)
  find_package(JUCE QUIET)
endif()

find_package(CURL QUIET)

# 2. If not, look for the one we are shipping
if(NOT TARGET juce::juce_core)
  add_subdirectory(submodules/JUCE)
endif()

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

  Containers/sg_AtomicUpdater.hpp
  Containers/sg_LogBuffer.cpp
  Containers/sg_LogBuffer.hpp
  Containers/sg_OwnedMap.hpp
  Containers/sg_StaticMap.hpp
  Containers/sg_StaticVector.hpp
  Containers/sg_StrongArray.hpp
  Containers/sg_TaggedAudioBuffer.hpp
  Containers/sg_ThreadSafeBuffer.hpp

  Data/StrongTypes/sg_CartesianVector.cpp
  Data/StrongTypes/sg_Dbfs.hpp
  Data/StrongTypes/sg_Degrees.hpp
  Data/StrongTypes/sg_Hz.hpp
  Data/StrongTypes/sg_Meters.hpp
  Data/StrongTypes/sg_OutputPatch.hpp
  Data/StrongTypes/sg_Radians.hpp
  Data/StrongTypes/sg_StrongFloat.hpp
  Data/StrongTypes/sg_StrongIndex.hpp
  Data/StrongTypes/sg_CartesianVector.hpp
  Data/StrongTypes/sg_SourceIndex.hpp

  Data/sg_AudioStructs.cpp
  Data/sg_AudioStructs.hpp
  Data/sg_CommandId.hpp
  Data/sg_constants.cpp
  Data/sg_constants.hpp
  Data/sg_LegacyLbapPosition.cpp
  Data/sg_LegacyLbapPosition.hpp
  Data/sg_LegacySpatFileFormat.cpp
  Data/sg_LegacySpatFileFormat.hpp
  Data/sg_LogicStrucs.cpp
  Data/sg_LogicStrucs.hpp
  Data/sg_Macros.hpp
  Data/sg_Narrow.hpp
  Data/sg_PolarVector.cpp
  Data/sg_PolarVector.hpp
  Data/sg_Position.cpp
  Data/sg_Position.hpp
  Data/sg_SpatMode.cpp
  Data/sg_SpatMode.hpp
  Data/sg_Triplet.hpp

  Implementations/sg_mbap.cpp
  Implementations/sg_mbap.hpp
  Implementations/sg_vbap.cpp
  Implementations/sg_vbap.hpp

  tl/optional.hpp
)

file(GENERATE
    OUTPUT JuceHeader.h
    CONTENT
    [=[
#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#if ! JUCE_DONT_DECLARE_PROJECTINFO
namespace ProjectInfo
{
    const char* const  projectName    = "AlgoGRIS";
    const char* const  companyName    = "Gris / SAT";
    const char* const  versionString  = "1.0.0";
    const int          versionNumber  = 0x100000;
}
#endif
]=]
)

target_link_libraries(AlgoGRIS
  PUBLIC
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_dsp
)

target_compile_definitions(AlgoGRIS
  PUBLIC
    $<$<BOOL:${BUILD_TESTING}>:ALGOGRIS_UNIT_TESTS>
)

# Tests
add_library(MainTest STATIC
  tests/Catch2Main.cpp
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
    target_compile_options("${_testname}" PRIVATE -O0 -g)
    catch_discover_tests("${_testname}")
  endfunction()

  algogris_add_test("tests/unit/test_core.cpp")
  algogris_add_test("tests/unit/test_spatAlgorithms.cpp")
endif()
