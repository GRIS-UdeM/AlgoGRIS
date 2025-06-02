#pragma once
#include "Data/StrongTypes/sg_OutputPatch.hpp"
#include "Data/StrongTypes/sg_SourceIndex.hpp"
#include "Data/sg_constants.hpp"
#include "sg_PinkNoiseGenerator.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include <Containers/sg_TaggedAudioBuffer.hpp>
#include <Data/sg_AudioStructs.hpp>
#include <cmath>
#include <array>
#include <random>
#include "catch2/catch_message.hpp"
#include "catch2/catch_test_macros.hpp"

#define ENABLE_TESTS 0
#define ENABLE_BENCHMARKS 1

#define REQUIRE_MESSAGE(cond, msg)                                                                                     \
    do {                                                                                                               \
        INFO(msg);                                                                                                     \
        REQUIRE(cond);                                                                                                 \
    } while (0)

namespace gris::tests
{

/** Duration of the audio loop used for the spatialisation tests. */
float constexpr static testDurationSeconds{ .1f };

/** A list of buffer sizes used for testing. */
std::array<int, 4> constexpr static bufferSizes{ 1, 512, 1024, SourceAudioBuffer::MAX_NUM_SAMPLES };

/**
 * @brief Initializes source, speaker, and stereo audio buffers for testing.
 *
 * @param bufferSize Number of samples per buffer.
 * @param numSources Number of source channels.
 * @param numSpeakers Number of speaker channels.
 * @param sourceBuffer Reference to the SourceAudioBuffer to initialize.
 * @param speakerBuffer Reference to the SpeakerAudioBuffer to initialize.
 * @param stereoBuffer Reference to the stereo juce::AudioBuffer<float> to initialize.
 */
inline void initBuffers(const int bufferSize,
                        const size_t numSources,
                        const size_t numSpeakers,
                        SourceAudioBuffer & sourceBuffer,
                        SpeakerAudioBuffer & speakerBuffer,
                        juce::AudioBuffer<float> & stereoBuffer)
{
    juce::Array<source_index_t> sourcesIndices;
    for (int i = 1; i <= numSources; ++i)
        sourcesIndices.add(source_index_t{ i });
    sourceBuffer.init(sourcesIndices);
    sourceBuffer.setNumSamples(bufferSize);

    juce::Array<output_patch_t> speakerIndices;
    for (int i = 1; i <= numSpeakers; ++i)
        speakerIndices.add(output_patch_t{ i });
    speakerBuffer.init(speakerIndices);
    speakerBuffer.setNumSamples(bufferSize);

    stereoBuffer.setSize(2, bufferSize);
    stereoBuffer.clear();
}

/**
 * @brief Fills the source buffers with pink noise and calculates the peak values.
 *
 * @param numSources Number of source channels.
 * @param sourceBuffer Reference to the SourceAudioBuffer to fill.
 * @param bufferSize Number of samples per buffer.
 * @param sourcePeaks Reference to the SourcePeaks to store calculated peak values.
 */
inline void fillSourceBuffersWithNoise(const size_t numSources,
                                       SourceAudioBuffer & sourceBuffer,
                                       const int bufferSize,
                                       SourcePeaks & sourcePeaks)
{
    sourceBuffer.silence();
    for (int i = 1; i <= numSources; ++i) {
        const auto sourceIndex{ source_index_t{ i } };
        fillWithPinkNoise(sourceBuffer[sourceIndex].getArrayOfWritePointers(), bufferSize, 1, .5f);
        sourcePeaks[sourceIndex] = sourceBuffer[sourceIndex].getMagnitude(0, bufferSize);
    }
}

/**
 * @brief Checks the validity of the speaker buffer.
 *
 * Ensures that all values are finite and within the range [-1.0, 1.0].
 *
 * @param buffer The SpeakerAudioBuffer to check.
 */
inline void checkSpeakerBufferValidity(const SpeakerAudioBuffer & buffer)
{
    for (auto const & speaker : buffer) {
        auto const * speakerBuffer = speaker.value->getReadPointer(0);

        for (int sampleNumber = 0; sampleNumber < buffer.getNumSamples(); ++sampleNumber) {
            const auto sampleValue = speakerBuffer[sampleNumber];

            REQUIRE_MESSAGE(std::isfinite(sampleValue), "Output contains NaN or Inf values!");
            REQUIRE_MESSAGE((sampleValue >= -1.f && sampleValue <= 1.f),
                            "Output " + juce::String(sampleValue) + " exceeds valid range!");
        }
    }
}

/**
 * @brief Checks the validity of the source buffer.
 *
 * Ensures that all values are finite and within the range [-1.0, 1.0].
 *
 * @param buffer The SourceAudioBuffer to check.
 */
inline void checkSourceBufferValidity(const SourceAudioBuffer & buffer)
{
    for (auto const & source : buffer) {
        auto const * sourceBuffer = source.value->getReadPointer(0);

        for (int sampleNumber = 0; sampleNumber < buffer.getNumSamples(); ++sampleNumber) {
            const auto sampleValue = sourceBuffer[sampleNumber];

            REQUIRE_MESSAGE(std::isfinite(sampleValue), "Output contains NaN or Inf values!");
            REQUIRE_MESSAGE((sampleValue >= -1.0f && sampleValue <= 1.0f), "Output exceeds valid range!");
        }
    }
}

} // namespace gris::tests
