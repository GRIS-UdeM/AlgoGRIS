#pragma once
#include "Data/StrongTypes/sg_OutputPatch.hpp"
#include "Data/StrongTypes/sg_SourceIndex.hpp"
#include "Data/sg_constants.hpp"
#include "sg_PinkNoiseGenerator.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_formats/juce_audio_formats.h"
#include "juce_core/juce_core.h"
#include <Containers/sg_TaggedAudioBuffer.hpp>
#include <Data/sg_AudioStructs.hpp>
#include <cmath>
#include <array>
#include <random>
#include "StructGRIS/ValueTreeUtilities.hpp"
#include "catch2/catch_message.hpp"
#include "catch2/catch_test_macros.hpp"

#define ENABLE_TESTS 1
#define ENABLE_BENCHMARKS 1
#define ENABLE_CATCH2_BENCHMARKS 1
#define USE_FIXED_NUM_LOOPS 0
#define USE_ONLY_TWO_BUFFER_SIZES 1
#define WRITE_TEST_OUTPUT 1

#define REQUIRE_MESSAGE(cond, msg)                                                                                     \
    do {                                                                                                               \
        INFO(msg);                                                                                                     \
        REQUIRE(cond);                                                                                                 \
    } while (0)

namespace gris::tests
{
auto constexpr static vbapTestName = "VBAP";
auto constexpr static stereoTestName = "STEREO";
auto constexpr static mbapTestName = "MBAP";
auto constexpr static hrtfTestName = "HRTF";

#if USE_FIXED_NUM_LOOPS
/** Number of loops over the processing call during tests. */
int constexpr static numTestLoops{ 3 };
#else
/** Duration of the audio loop used for the spatialisation tests. */
float constexpr static testDurationSeconds{ 1.f };
#endif

/** A list of buffer sizes used for testing. */
#if USE_ONLY_TWO_BUFFER_SIZES
std::array<int, 2> constexpr static bufferSizes{ 512, 1024 };
#else
std::array<int, 4> constexpr static bufferSizes{ 1, 512, 1024, SourceAudioBuffer::MAX_NUM_SAMPLES };
#endif

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
void initBuffers(const int bufferSize,
                 const size_t numSources,
                 const size_t numSpeakers,
                 SourceAudioBuffer & sourceBuffer,
                 SpeakerAudioBuffer & speakerBuffer,
                 juce::AudioBuffer<float> & stereoBuffer);

/**
 * @brief Fills the source buffers with pink noise and calculates the peak values.
 *
 * @param numSources Number of source channels.
 * @param sourceBuffer Reference to the SourceAudioBuffer to fill.
 * @param bufferSize Number of samples per buffer.
 * @param sourcePeaks Reference to the SourcePeaks to store calculated peak values.
 */
void fillSourceBuffersWithNoise(const size_t numSources,
                                SourceAudioBuffer & sourceBuffer,
                                const int bufferSize,
                                SourcePeaks & sourcePeaks);

void fillSourceBuffersWithSine(const size_t numSources,
                               SourceAudioBuffer & sourceBuffer,
                               const int bufferSize,
                               SourcePeaks & sourcePeaks,
                               float & lastPhase);

/**
 * @brief Checks the validity of the source buffer.
 *
 * Ensures that all values are finite and within the range [-1.0, 1.0].
 *
 * @param buffer The SourceAudioBuffer to check.
 */
void checkSourceBufferValidity(const SourceAudioBuffer & buffer);

struct AudioBufferComparator {
    std::map<int, juce::AudioBuffer<float>> cachedSpeakerBuffers;

    /**
     * @brief Checks the validity of the speaker buffer.
     *
     * Ensures that all values are finite and within the range [-1.0, 1.0].
     *
     * @param buffer The SpeakerAudioBuffer to check.
     */
    // void checkSpeakerBufferValidity(const SpeakerAudioBuffer & buffer);

    static void forAllSpatializedSpeakers(const SpeakersAudioConfig & speakersAudioConfig,
                                          const SpeakerAudioBuffer & newSpeakerBuffers,
                                          int bufferSize,
                                          std::function<void(int, const float * const, int)> func);

    static juce::File getSpeakerWavFile(juce::StringRef testName, int bufferSize, int speakerId);

    static void compareBuffers(const float * const curBuffer, const juce::AudioBuffer<float> & savedBuffer);

    static void makeSureSpeakerBufferMatchesSavedVersion(juce::StringRef testName,
                                                         const SpeakersAudioConfig & speakersAudioConfig,
                                                         const SpeakerAudioBuffer & speakerAudioBuffer,
                                                         int bufferSize,
                                                         int curLoop);

    static void makeSureStereoBufferMatchesSavedVersion(juce::StringRef testName,
                                                        const juce::AudioBuffer<float> & stereoAudioBuffer,
                                                        int bufferSize,
                                                        int curLoop);

    void cacheSpeakerBuffersInMemory(const SpeakersAudioConfig & speakersAudioConfig,
                                     const SpeakerAudioBuffer & newSpeakerBuffers,
                                     int bufferSize);

    void cacheStereoBuffersInMemory(const juce::AudioBuffer<float> & newStereoBuffers, int bufferSize);

    void writeCachedBuffersToDisk(juce::StringRef testName, int bufferSize, double sampleRate = 48000.0);
};
} // namespace gris::tests
