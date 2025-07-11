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
#include <Data/sg_LogicStrucs.hpp>
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
#define WRITE_TEST_OUTPUT_TO_DISK 0

#define REQUIRE_MESSAGE(cond, msg)                                                                                     \
    do {                                                                                                               \
        INFO(msg);                                                                                                     \
        jassert(cond);                                                                                                 \
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
float constexpr static testDurationSeconds{ .5f };
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
                 std::vector<std::vector<AtomicWrapper<float>>> & atomicSpeakerBuffer,
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

/**
 * @brief Fills the source buffers with a 440 Hz sine wave and calculates the peak values.
 *
 * @param numSources Number of source channels.
 * @param sourceBuffer Reference to the SourceAudioBuffer to fill.
 * @param bufferSize Number of samples per buffer.
 * @param sourcePeaks Reference to the SourcePeaks to store calculated peak values.
 * @param lastPhase Reference to the last phase of the sine wave, used for continuity.
 */
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

/**
 * @brief Checks the validity of the speaker buffer.
 *
 * Ensures that all values are finite and within the range [-1.0, 1.0].
 *
 * @param buffer The SpeakerAudioBuffer to check.
 */
void checkSpeakerBufferValidity(const SpeakerAudioBuffer & buffer);

/**
 * @brief Utility struct for comparing and managing audio buffers during tests.
 *
 * AudioBufferComparator provides methods to compare, cache, and write audio buffers
 * for speaker and stereo configurations. It is used to ensure that generated audio
 * matches expected results and to facilitate regression testing by comparing against
 * saved buffer data.
 */
struct AudioBufferComparator {
    /**
     * @brief Cached audio buffers, indexed by speaker ID.
     */
    std::map<int, juce::AudioBuffer<float>> cachedBuffers;

    /**
     * @brief Utility function to easily iterates over all non-direct and non-muted speakers and apply a function to
     * their buffers.
     *
     * @param speakersAudioConfig The configuration of the speakers.
     * @param speakerBuffers The buffer containing new speaker audio data.
     * @param bufferSize The number of samples in each buffer.
     * @param func The function to apply, taking (speakerId, buffer pointer, bufferSize).
     */
    static void forAllSpatializedSpeakers(const SpeakersAudioConfig & speakersAudioConfig,
                                          const SpeakerAudioBuffer & speakerBuffers,
                                          int bufferSize,
                                          std::function<void(int, const float * const, int)> func);

    /**
     * @brief Gets the file path for a saved speaker WAV file for a given test.
     *
     * @param testName The name of the test.
     * @param bufferSize The test buffer size.
     * @param speakerId The ID of the speaker.
     * @return juce::File The file object representing the WAV file.
     */
    static juce::File getSpeakerWavFile(juce::StringRef testName, int bufferSize, int speakerId);

    /**
     * @brief Compares a current buffer with a buffer.
     *
     * @param curBuffer Pointer to the current buffer data.
     * @param savedBuffer The saved buffer to compare against.
     */
    static void compareBuffers(const float * const curBuffer, const juce::AudioBuffer<float> & savedBuffer);

    /**
     * @brief Ensures the speaker buffer matches the saved version for a given test.
     *
     * @param testName The name of the test.
     * @param speakersAudioConfig The configuration of the speakers.
     * @param speakerAudioBuffer The buffer containing speaker audio data.
     * @param bufferSize The buffer size.
     * @param curLoop The current test loop iteration.
     */
    static void makeSureSpeakerBufferMatchesSavedVersion(juce::StringRef testName,
                                                         const SpeakersAudioConfig & speakersAudioConfig,
                                                         const SpeakerAudioBuffer & speakerAudioBuffer,
                                                         int bufferSize,
                                                         int curLoop);

    /**
     * @brief Ensures the stereo buffer matches the saved version for a given test.
     *
     * @param testName The name of the test.
     * @param stereoAudioBuffer The buffer containing stereo audio data.
     * @param bufferSize The buffer size.
     * @param curLoop The current test loop iteration.
     */
    static void makeSureStereoBufferMatchesSavedVersion(juce::StringRef testName,
                                                        const juce::AudioBuffer<float> & stereoAudioBuffer,
                                                        int bufferSize,
                                                        int curLoop);

    /**
     * @brief Caches the current speaker buffers in memory for later comparison or writing.
     *
     * @param speakersAudioConfig The configuration of the speakers.
     * @param newSpeakerBuffers The buffer containing new speaker audio data.
     * @param bufferSize The buffer size.
     */
    void cacheSpeakerBuffersInMemory(const SpeakersAudioConfig & speakersAudioConfig,
                                     const SpeakerAudioBuffer & speakerBuffers,
                                     int bufferSize);

    /**
     * @brief Caches the current stereo buffers in memory for later comparison or writing.
     *
     * @param newStereoBuffers The buffer containing new stereo audio data.
     * @param bufferSize The buffer size.
     */
    void cacheStereoBuffersInMemory(const juce::AudioBuffer<float> & stereoBuffers, int bufferSize);

    /**
     * @brief Writes all cached buffers to disk as WAV files for regression testing.
     *
     * @param testName The name of the test.
     * @param bufferSize The buffer size.
     * @param sampleRate The sample rate to use for the WAV files (default: 48000.0).
     */
    void writeCachedBuffersToDisk(juce::StringRef testName, int bufferSize, double sampleRate = 48000.0);
};
} // namespace gris::tests
