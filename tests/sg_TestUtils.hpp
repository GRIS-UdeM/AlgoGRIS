#pragma once
<<<<<<< HEAD
#include "Data/StrongTypes/sg_OutputPatch.hpp"
#include "Data/StrongTypes/sg_SourceIndex.hpp"
#include "Data/sg_constants.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
=======
#include "sg_PinkNoiseGenerator.hpp"
>>>>>>> 5ca1402 (put in my own tests, only the VBAP for now, need to get the others from sg_AbstractSpatAlgorithm.cpp)
#include <Containers/sg_TaggedAudioBuffer.hpp>
#include <Data/sg_AudioStructs.hpp>
#include <cmath>
#include <array>
#include <random>
#include "catch2/catch_message.hpp"
#include "catch2/catch_test_macros.hpp"

#define REQUIRE_MESSAGE(cond, msg)                                                                                     \
    do {                                                                                                               \
        INFO(msg);                                                                                                     \
        REQUIRE(cond);                                                                                                 \
    } while (0)

namespace gris::tests
{

float constexpr static testDurationSeconds{ .1f };
std::array<int, 4> constexpr static bufferSizes{ 1, 512, 1024, SourceAudioBuffer::MAX_NUM_SAMPLES };

inline void initBuffers(const int bufferSize,
                        const size_t numSources,
                        const size_t numSpeakers,
                        SourceAudioBuffer& sourceBuffer,
                        SpeakerAudioBuffer& speakerBuffer,
                        juce::AudioBuffer<float>& stereoBuffer)
{
    // if (sourceBuffer)
    {
        juce::Array<source_index_t> sourcesIndices;
        for (int i = 1; i <= numSources; ++i)
            sourcesIndices.add(source_index_t{ i });

        sourceBuffer.init(sourcesIndices);
        sourceBuffer.setNumSamples(bufferSize);
    }

    // if (speakerBuffer)
     {
        juce::Array<output_patch_t> speakerIndices;
        for (int i = 1; i <= numSpeakers; ++i)
            speakerIndices.add(output_patch_t{ i });

        speakerBuffer.init(speakerIndices);
        speakerBuffer.setNumSamples(bufferSize);
    }

    // if (stereoBuffer)
    {
        stereoBuffer.setSize(2, bufferSize);
        stereoBuffer.clear();
    }
}

/** fill Source Buffers with pink noise, and calculate the peaks */
inline void fillSourceBuffersWithNoise(const size_t numSources,
                                       SourceAudioBuffer& sourceBuffer,
                                       const int bufferSize,
                                       SourcePeaks& sourcePeaks)
{
    sourceBuffer.silence();
    for (int i = 1; i <= numSources; ++i)
    {
        const auto sourceIndex { source_index_t{ i } };
        fillWithPinkNoise(sourceBuffer[sourceIndex].getArrayOfWritePointers(), bufferSize, 1, .5f);
        sourcePeaks[sourceIndex] = sourceBuffer[sourceIndex].getMagnitude(0, bufferSize);
    }
}

inline void checkSpeakerBufferValidity(const SpeakerAudioBuffer & buffer)
{
    for (auto const & speaker : buffer) {
        auto const * speakerBuffer = speaker.value->getReadPointer(0);

        for (int sampleNumber = 0; sampleNumber < buffer.getNumSamples(); ++sampleNumber) {
            const auto sampleValue = speakerBuffer[sampleNumber];

            REQUIRE_MESSAGE(std::isfinite(sampleValue), "Output contains NaN or Inf values!");
            //REQUIRE_MESSAGE(sampleValue >= -1.f && sampleValue <= 1.f, "Output " + juce::String(sampleValue) + " exceeds valid range!");
        }
    }
}

inline void checkSourceBufferValidity(const SourceAudioBuffer & buffer)
{
    for (auto const & source : buffer) {
        auto const * sourceBuffer = source.value->getReadPointer(0);

        for (int sampleNumber = 0; sampleNumber < buffer.getNumSamples(); ++sampleNumber) {
            const auto sampleValue = sourceBuffer[sampleNumber];

            REQUIRE_MESSAGE(std::isfinite(sampleValue), "Output contains NaN or Inf values!");
            // REQUIRE_MESSAGE(sampleValue >= -1.0f && sampleValue <= 1.0f, "Output exceeds valid range!");
        }
    }
}

} // namespace gris::tests
