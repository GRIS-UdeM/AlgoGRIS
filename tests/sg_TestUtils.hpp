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
#define USE_ONLY_TWO_BUFFER_SIZES 0

#define REQUIRE_MESSAGE(cond, msg)                                                                                     \
    do {                                                                                                               \
        INFO(msg);                                                                                                     \
        REQUIRE(cond);                                                                                                 \
    } while (0)

namespace gris::tests
{

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

inline void makeSureStereoBufferMatchesSavedVersion(const juce::AudioBuffer<float> & buffer, int bufferSize)
{
}

// inline void saveBufferToFile (const juce::AudioBuffer<float>& buffer, const juce::String& fileName)
//{
//     juce::File file (fileName);
//     if (!file.existsAsFile()) {
//         file.create();
//     }
//     juce::FileOutputStream outputStream (file);
//     if (outputStream.openedOk()) {
//         buffer.writeToStream (outputStream);
//     }
// }
//
// static void saveBufferToFile (const juce::AudioBuffer<float>& buffer, const std::string& path)
//{
//     std::ofstream out (path, std::ios::binary);
//     int numChannels = buffer.getNumChannels ();
//     int numSamples = buffer.getNumSamples ();
//     out.write (reinterpret_cast<const char*>(&numChannels), sizeof (int));
//     out.write (reinterpret_cast<const char*>(&numSamples), sizeof (int));
//     for (int ch = 0; ch < numChannels; ++ch)
//         out.write (reinterpret_cast<const char*> (buffer.getReadPointer (ch)), sizeof (float) * numSamples);
// }
//
// static bool loadBufferFromFile (juce::AudioBuffer<float>& buffer, const std::string& path)
//{
//     std::ifstream in (path, std::ios::binary);
//     if (!in)
//         return false;
//     int numChannels, numSamples;
//     in.read (reinterpret_cast<char*> (&numChannels), sizeof (int));
//     in.read (reinterpret_cast<char*>(&numSamples), sizeof (int));
//     buffer.setSize (numChannels, numSamples);
//     for (int ch = 0; ch < numChannels; ++ch)
//         in.read (reinterpret_cast<char*> (buffer.getWritePointer (ch)), sizeof (float) * numSamples);
//     return true;
// }
//
// static bool buffersApproximatelyEqual (const juce::AudioBuffer<float>& a,
//                                        const juce::AudioBuffer<float>& b,
//                                        float tolerance = 1e-5f)
//{
//     if (a.getNumChannels () != b.getNumChannels () || a.getNumSamples () != b.getNumSamples ())
//         return false;
//     for (int ch = 0; ch < a.getNumChannels (); ++ch) {
//         const float* aData = a.getReadPointer (ch);
//         const float* bData = b.getReadPointer (ch);
//         for (int i = 0; i < a.getNumSamples (); ++i) {
//             if (std::abs (aData[i] - bData[i]) > tolerance)
//                 return false;
//         }
//     }
//     return true;
// }

std::map<int, juce::AudioBuffer<float>> cachedSpeakerBuffers;

inline void cacheSpeakerBuffersInMemory(const SpeakerAudioBuffer & newSpeakerBuffers,
                                        const SpeakersAudioConfig & speakersAudioConfig,
                                        int bufferSize)
{
    // first get buffer data we can read
    juce::Array<output_patch_t> const keys{ speakersAudioConfig.getKeys() };
    juce::Array<float const *> usableNewBuffers = newSpeakerBuffers.getArrayOfReadPointers(keys);

    // then for each spatialized, unmuted speaker
    for (const auto & speaker : speakersAudioConfig) {
        if (speaker.value.isMuted || speaker.value.isDirectOutOnly || speaker.value.gain < SMALL_GAIN)
            continue;

        // get the new data
        const int speakerId = speaker.key.get();
        const float * newIndividualSpeakerBuffer = usableNewBuffers[speakerId];

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
    }
}

inline void writeSpeakerBuffersToWavFiles(juce::StringRef testName, int bufferSize, double sampleRate = 48000.0)
{
    juce::WavAudioFormat wavFormat;
    juce::String const curTestDirName{ "tests/util/buffer_dumps/" + testName + "/" + juce::String(bufferSize) };
    juce::File const outputDir = getValidCurrentDirectory().getChildFile(curTestDirName);

    if (!outputDir.exists())
        outputDir.createDirectory();

    for (const auto & [speakerId, buffer] : cachedSpeakerBuffers) {
        juce::File wavFile = outputDir.getChildFile("speaker_" + juce::String(speakerId) + ".wav");
        std::unique_ptr<juce::FileOutputStream> outputStream(wavFile.createOutputStream());

        if (outputStream) {
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(outputStream.get(), sampleRate, 1, 16, {}, 0));

            if (writer) {
                outputStream.release(); // Writer takes ownership
                writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
            } else {
                // failed to create writer!
                jassertfalse;
            }
        } else {
            // failed to create stream!
            jassertfalse;
        }
    }

    cachedSpeakerBuffers.clear();
}

inline void saveAllSpeakerBuffersToFile(const SpeakerAudioBuffer & speakerBuffers,
                                        const SpeakersAudioConfig & speakersAudioConfig,
                                        int bufferSize)
{
    // get actual data we can use
    juce::Array<float const *> usableSpeakerBuffers
        = speakerBuffers.getArrayOfReadPointers(speakersAudioConfig.getKeys());

    // for each speaker
    for (auto const & speaker : speakersAudioConfig) {
        // skip it if unused
        if (speaker.value.isMuted || speaker.value.isDirectOutOnly || speaker.value.gain < SMALL_GAIN)
            continue;

        // get the individual speaker buffer
        const int speakerId{ speaker.key.get() };
        const float * const individualSpeakerBuffer = usableSpeakerBuffers[speakerId];

        // create the file if it doesn't exist

        // and append all current samples to the file
        for (int i = 0; i < bufferSize; ++i)
            DBG("speakerId " << juce::String(speakerId) << " sample [" << juce::String(i)
                             << "]: " << individualSpeakerBuffer[i]);
    }
}

inline void saveSpeakerBufferToFile(float const * const speakersBuffer, int bufferSize)
{
    // so this should be printing into a file
    for (int i = 0; i < bufferSize; ++i)
        DBG(speakersBuffer[i]);
}

// inline void makeSureSpeakerBufferMatchesSavedVersion (const SpeakerAudioBuffer* buffer, int bufferSize)
//{
//     const juce::String stereoFile = "reference_output/stereo_" + std::to_string (bufferSize) + ".bin";
//
//     const juce::String speakerFile = "reference_output/speaker_" + std::to_string (bufferSize) + ".bin";
//
//     juce::AudioBuffer<float> refStereo;
//     juce::AudioBuffer<float> refSpeaker;
//
//     if (!loadBufferFromFile (refStereo, stereoFile) || !loadBufferFromFile (refSpeaker, speakerFile)) {
//         std::cout << "Saving reference output...\n";
//         saveBufferToFile (stereoBuffer, stereoFile);
//         saveBufferToFile (speakerBuffer, speakerFile);
//     }
//     else {
//         bool ok1 = buffersApproximatelyEqual (stereoBuffer, refStereo);
//         bool ok2 = buffersApproximatelyEqual (speakerBuffer, refSpeaker);
//         if (!ok1 || !ok2)
//             throw std::runtime_error ("Output buffers don't match saved reference.");
//     }
// }
//
// inline void makeSureBufferMatchesSavedVersion (const SpeakerAudioBuffer* buffer)
//{
//     std::string prefix = "reference_output/";
//     std::string stereoFile = prefix + "stereo_" + std::to_string (bufferSize) + ".bin";
//     std::string speakerFile = prefix + "speaker_" + std::to_string (bufferSize) + ".bin";
//
//     juce::AudioBuffer<float> refStereo;
//     juce::AudioBuffer<float> refSpeaker;
//
//     if (!loadBufferFromFile (refStereo, stereoFile) || !loadBufferFromFile (refSpeaker, speakerFile)) {
//         std::cout << "Saving reference output...\n";
//         saveBufferToFile (stereoBuffer, stereoFile);
//         saveBufferToFile (speakerBuffer, speakerFile);
//     }
//     else {
//         bool ok1 = buffersApproximatelyEqual (stereoBuffer, refStereo);
//         bool ok2 = buffersApproximatelyEqual (speakerBuffer, refSpeaker);
//         if (!ok1 || !ok2)
//             throw std::runtime_error ("Output buffers don't match saved reference.");
//     }
// }

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
