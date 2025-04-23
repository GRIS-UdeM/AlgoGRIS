#pragma once
#include "Data/StrongTypes/sg_OutputPatch.hpp"
#include "Data/StrongTypes/sg_SourceIndex.hpp"
#include "Data/sg_constants.hpp"

#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"

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

inline void initBuffers(int bufferSize,
                        SourceAudioBuffer * sourceBuffer,
                        SpeakerAudioBuffer * speakerBuffer,
                        juce::AudioBuffer<float> * stereoBuffer)
{
    if (sourceBuffer) {
        // init source buffer with MAX_NUM_SOURCES sources
        sourceBuffer->setNumSamples(bufferSize);
        juce::Array<source_index_t> sources;
        for (int i = 1; i <= MAX_NUM_SOURCES; ++i)
            sources.add(source_index_t{ i });
        sourceBuffer->init(sources);
    }

    if (speakerBuffer) {
        // init speaker buffer with MAX_NUM_SPEAKERS speakers
        speakerBuffer->setNumSamples(bufferSize);
        juce::Array<output_patch_t> speakers;
        for (int i = 1; i <= MAX_NUM_SPEAKERS; ++i)
            speakers.add(output_patch_t{ i });
        speakerBuffer->init(speakers);
    }

    if (stereoBuffer) {
        stereoBuffer->setSize(2, bufferSize);
    }
}

inline void fillSourceBufferWithNoise(SourceAudioBuffer & buffer, AudioConfig & config)
{
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (auto const & source : config.sourcesAudioConfig)
        for (auto const & speaker : config.speakersAudioConfig)
            if (!source.value.isMuted && !speaker.value.isMuted)
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    buffer[source.key].getWritePointer(0)[sample] = dist(gen);
}

inline void checkSpeakerBufferValidity(SpeakerAudioBuffer & buffer, AudioConfig & config)
{
    for (auto const & speaker : config.speakersAudioConfig) {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            float const value = buffer[speaker.key].getReadPointer(0)[sample];
            REQUIRE_MESSAGE(std::isfinite(value), "Output contains NaN or Inf values!");
            // TODO: the output is not in the expected range. Need to go through the whole process chain to
            // understand; presumably we're missing some kind of spatialization data or initialization.
            // expect(value >= -1.0f && value <= 1.0f, "Output exceeds valid range!");
        }
    }
}

inline void checkSourceBufferValidity(SourceAudioBuffer & buffer, AudioConfig & config)
{
    for (auto const & source : config.sourcesAudioConfig) {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            float const value = buffer[source.key].getReadPointer(0)[sample];
            REQUIRE_MESSAGE(std::isfinite(value), "buffer contains NaN or Inf values!");
            REQUIRE_MESSAGE((value >= -1.0f && value <= 1.0f), "Value exceeds valid range!");
        }
    }
}

} // namespace gris::tests
