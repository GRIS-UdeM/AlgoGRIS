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
#include "../../StructGRIS/ValueTreeUtilities.hpp"
#include "catch2/catch_message.hpp"
#include "catch2/catch_test_macros.hpp"

#define ENABLE_TESTS 1
#define ENABLE_BENCHMARKS 1
#define ENABLE_CATCH2_BENCHMARKS 1
#define USE_FIXED_NUM_LOOPS 0
#define USE_ONLY_TWO_BUFFER_SIZES 1
#define WRITE_TEST_OUTPUT 0

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
std::array<int, 2> constexpr static bufferSizes { 512, 1024 };
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
        auto const sourceIndex{ source_index_t{ i } };
        fillWithPinkNoise(sourceBuffer[sourceIndex].getArrayOfWritePointers(), bufferSize, 1, .5f);
        sourcePeaks[sourceIndex] = sourceBuffer[sourceIndex].getMagnitude(0, bufferSize);
    }
}

inline void fillSourceBuffersWithSine(const size_t numSources,
                                      SourceAudioBuffer & sourceBuffer,
                                      const int bufferSize,
                                      SourcePeaks & sourcePeaks,
                                      float & lastPhase)
{
    constexpr float frequency = 440.f;
    constexpr float sampleRate = 48000.f;
    constexpr float phaseIncrement = juce::MathConstants<float>::twoPi * frequency / sampleRate;

    sourceBuffer.silence();

    for (int i = 1; i <= numSources; ++i) {
        auto const sourceIndex{ source_index_t{ i } };
        auto curPhase = lastPhase;

        for (int s = 0; s < bufferSize; ++s) {
            sourceBuffer[sourceIndex].setSample(0, s, std::sin(curPhase) * 0.25f);

            curPhase += phaseIncrement;
            if (curPhase > juce::MathConstants<float>::twoPi)
                curPhase -= juce::MathConstants<float>::twoPi; // wrap phase
        }

        sourcePeaks[sourceIndex] = sourceBuffer[sourceIndex].getMagnitude(0, bufferSize);
    }

    lastPhase += bufferSize * phaseIncrement;
    if (lastPhase > juce::MathConstants<float>::twoPi)
        lastPhase -= juce::MathConstants<float>::twoPi;
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

inline void forAllSpatializedSpeakers(const SpeakersAudioConfig & speakersAudioConfig,
                                      const SpeakerAudioBuffer & newSpeakerBuffers,
                                      int bufferSize,
                                      std::function<void(int, const float * const, int)> func)
{
    juce::Array<output_patch_t> const keys{ speakersAudioConfig.getKeys() };
    juce::Array<float const *> usableNewBuffers = newSpeakerBuffers.getArrayOfReadPointers(keys);

    // then for each spatialized, unmuted speaker
    for (const auto & speaker : speakersAudioConfig) {
        if (speaker.value.isMuted || speaker.value.isDirectOutOnly || speaker.value.gain < SMALL_GAIN)
            continue;

        // get the new data
        const int speakerId = speaker.key.get();
        const float * const newIndividualSpeakerBuffer = usableNewBuffers[speakerId];

        func(speakerId, newIndividualSpeakerBuffer, bufferSize);
    }
}

 // TODO VB: presumably this shouldn't be just in the namespace?
 std::map<int, juce::AudioBuffer<float>> cachedSpeakerBuffers;

 inline void cacheSpeakerBuffersInMemory(const SpeakersAudioConfig & speakersAudioConfig,
                                         const SpeakerAudioBuffer & newSpeakerBuffers,
                                         int bufferSize)
 {
     forAllSpatializedSpeakers(
         speakersAudioConfig,
         newSpeakerBuffers,
         bufferSize,
         [](int speakerId, const float * newIndividualSpeakerBuffer, int bufferSize) {
             // get the cached data
             juce::AudioSampleBuffer & cachedBuffer = cachedSpeakerBuffers[speakerId];

             const int oldSize = cachedBuffer.getNumSamples();
             const int newSize = oldSize + bufferSize;

             // if the cached buffer is empty
             if (cachedBuffer.getNumChannels() == 0)
                 cachedBuffer.setSize(1, newSize, false, true); // resize and wipe it
             else
                 cachedBuffer.setSize(1, newSize, true, true); // otherwise resize and preserve existing

             // finally copy the new data
             cachedBuffer.copyFrom(0, oldSize, newIndividualSpeakerBuffer, bufferSize);
         });
 }

juce::File getSpeakerDumpFile (juce::StringRef testName, int bufferSize, int speakerId)
{
    auto const curTestDirName { "tests/util/buffer_dumps/" + testName + "/" + juce::String (bufferSize) };
    auto const outputDir = getValidCurrentDirectory ().getChildFile (curTestDirName);

    if (!outputDir.exists ())
        outputDir.createDirectory ();

    return outputDir.getChildFile ("speaker_" + juce::String (speakerId) + ".wav");
}

inline void writeCachedSpeakerBuffersToDisk(juce::StringRef testName, int bufferSize, double sampleRate = 48000.0)
{
    juce::WavAudioFormat wavFormat;

    for (const auto & [speakerId, buffer] : cachedSpeakerBuffers) {
        juce::File wavFile = getSpeakerDumpFile(testName, bufferSize, speakerId);
        std::unique_ptr<juce::FileOutputStream> outputStream(wavFile.createOutputStream());

        if (outputStream) {
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(outputStream.get(), sampleRate, 1, 16, {}, 0));

            if (writer) {
                outputStream.release(); // Writer takes ownership
                writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
            } else {
                // failed to create reader!
                jassertfalse;
            }
        } else {
            // failed to create stream!
            jassertfalse;
        }
    }

    cachedSpeakerBuffers.clear();
}

inline void compareBuffers (const float* const curBuffer, const juce::AudioBuffer<float>& savedBuffer)
{
#if 1
    DBG ("starting");
    for (int i = 0; i < savedBuffer.getNumSamples (); ++i)
        DBG (curBuffer[i]);

    for (int i = 0; i < savedBuffer.getNumSamples (); ++i)
        DBG (savedBuffer.getSample(0, i));

    DBG ("done");

#else
    REQUIRE_MESSAGE(curBuffer != nullptr, "Current buffer is null!");
    REQUIRE_MESSAGE(savedBuffer.getNumSamples() > 0, "Saved buffer has no samples!");
    for (int i = 0; i < savedBuffer.getNumSamples(); ++i) {
        const auto curSample = curBuffer[i];
        const auto savedSample = savedBuffer.getSample(0, i);
        REQUIRE_MESSAGE(std::abs(curSample - savedSample) < 1e-5f,
                        "Buffers do not match at sample " + juce::String(i) + ": " +
                        juce::String(curSample) + " vs " + juce::String(savedSample));
    }
#endif
}

inline void makeSureSpeakerBufferMatchesSavedVersion(juce::StringRef testName,
                                                     const SpeakersAudioConfig & speakersAudioConfig,
                                                     const SpeakerAudioBuffer & speakerAudioBuffer,
                                                     int bufferSize,
                                                     int curLoop)
{
    forAllSpatializedSpeakers(
        speakersAudioConfig,
        speakerAudioBuffer,
        bufferSize,
        [testName, curLoop](int speakerId, float const * const individualSpeakerBuffer, int bufferSize) {
            juce::WavAudioFormat wavFormat;
            const auto speakerDumpFile = getSpeakerDumpFile(testName, bufferSize, speakerId);

            if (auto inputStream{ speakerDumpFile.createInputStream() }) {
                if (auto reader{ wavFormat.createReaderFor(inputStream.get(), true) }) {
                    inputStream.release(); // reader takes ownership

                    // read the stored wave data into wavBuffer
                    juce::AudioBuffer<float> wavBuffer(1, bufferSize);
                    const auto res = reader->read(&wavBuffer, 0, curLoop * bufferSize, bufferSize, true, true);
                    jassert (res);

                    //and compare the 2 buffers
                    compareBuffers(individualSpeakerBuffer, wavBuffer);
                } else {
                    // failed to create reader!
                    jassertfalse;
                }
            } else {
                // failed to create stream!
                jassertfalse;
            }
        });
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
