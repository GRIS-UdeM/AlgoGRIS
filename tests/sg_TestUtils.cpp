#include "sg_TestUtils.hpp"

namespace gris::tests
{

void initBuffers(const int bufferSize,
                 const size_t numSources,
                 const size_t numSpeakers,
                 SourceAudioBuffer & sourceBuffer,
                 SpeakerAudioBuffer & speakerBuffer,
#if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                 ForkUnionBuffer & forkUnionBuffer,
#endif
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

#if USE_FORK_UNION
    #if FU_METHOD == FU_USE_ARRAY_OF_ATOMICS
    forkUnionBuffer.resize(numSpeakers);
    for (int i = 0; i < numSpeakers; ++i) {
        forkUnionBuffer[i].clear();
        for (int j = 0; j < bufferSize; ++j)
            forkUnionBuffer[i].emplace_back(0.0f);
    }
    #elif FU_METHOD == FU_USE_BUFFER_PER_THREAD
    // so we have a buffer for each hardware thread
    auto const numThreads = std::thread::hardware_concurrency();
    forkUnionBuffer.resize(numThreads);

    for (auto & curThreadSpeakerBuffer : forkUnionBuffer) {
        // then within each thread we need a buffer for each speaker
        curThreadSpeakerBuffer.resize(numSpeakers);

        // and each speaker buffer contains bufferSize samples
        for (auto & curSpeakerBuffer : curThreadSpeakerBuffer)
            curSpeakerBuffer.assign(bufferSize, 0.f);
    }
    #endif
#endif

    stereoBuffer.setSize(2, bufferSize);
    stereoBuffer.clear();
}

void fillSourceBuffersWithNoise(const size_t numSources,
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

void fillSourceBuffersWithSine(const size_t numSources,
                               SourceAudioBuffer & sourceBuffer,
                               const int bufferSize,
                               SourcePeaks & sourcePeaks,
                               float & lastPhase)
{
    static constexpr float frequency = 440.f;
    static constexpr float sampleRate = 48000.f;
    static constexpr float phaseIncrement = juce::MathConstants<float>::twoPi * frequency / sampleRate;

    sourceBuffer.silence();

    for (int i = 1; i <= numSources; ++i) {
        auto const sourceIndex{ source_index_t{ i } };
        auto curPhase = lastPhase;

        for (int s = 0; s < bufferSize; ++s) {
            sourceBuffer[sourceIndex].setSample(0, s, std::sin(curPhase) * 0.05f);

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

void checkSourceBufferValidity(const SourceAudioBuffer & buffer)
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

void checkSpeakerBufferValidity(const SpeakerAudioBuffer & buffer)
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

void AudioBufferComparator::forAllSpatializedSpeakers(const SpeakersAudioConfig & speakersAudioConfig,
                                                      const SpeakerAudioBuffer & speakerBuffers,
                                                      int bufferSize,
                                                      std::function<void(int, const float * const, int)> func)
{
    juce::Array<output_patch_t> const keys(speakersAudioConfig.getKeys());
    juce::Array<float const *> floatSpeakerBuffers = speakerBuffers.getArrayOfReadPointers(keys);

    // then for each spatialized, unmuted speaker
    for (const auto & speaker : speakersAudioConfig) {
        // get the new data
        const int speakerId = speaker.key.get();
        const float * const individualFloatSpeakerBuffer = floatSpeakerBuffers[speakerId];

        // skip this speaker if it's not spatialized or muted
        if (speaker.value.isMuted || speaker.value.isDirectOutOnly || speaker.value.gain < SMALL_GAIN
            || individualFloatSpeakerBuffer == nullptr)
            continue;

        func(speakerId, individualFloatSpeakerBuffer, bufferSize);
    }
}

void AudioBufferComparator::cacheSpeakerBuffersInMemory(const SpeakersAudioConfig & speakersAudioConfig,
                                                        const SpeakerAudioBuffer & speakerBuffers,
                                                        int bufferSize)
{
    forAllSpatializedSpeakers(
        speakersAudioConfig,
        speakerBuffers,
        bufferSize,
        [this](int speakerId, const float * newIndividualSpeakerBuffer, int bufferSize) {
            jassert(newIndividualSpeakerBuffer != nullptr);

            // get the cached data
            juce::AudioSampleBuffer & cachedBuffer = cachedBuffers[speakerId];

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

void AudioBufferComparator::cacheStereoBuffersInMemory(const juce::AudioBuffer<float> & stereoBuffers, int bufferSize)
{
    jassert(stereoBuffers.getNumChannels() == 2);
    for (int curChannel = 0; curChannel < 2; ++curChannel) {
        // get the cached data
        juce::AudioSampleBuffer & cachedBuffer = cachedBuffers[curChannel];

        const int oldSize = cachedBuffer.getNumSamples();
        const int newSize = oldSize + bufferSize;

        // if the cached buffer is empty
        if (cachedBuffer.getNumChannels() == 0)
            cachedBuffer.setSize(1, newSize, false, true); // resize and wipe it
        else
            cachedBuffer.setSize(1, newSize, true, true); // otherwise resize and preserve existing

        // finally copy the new data
        cachedBuffer.copyFrom(0, oldSize, stereoBuffers.getReadPointer(curChannel), bufferSize);
    }
}

juce::File AudioBufferComparator::AudioBufferComparator::getSpeakerWavFile(juce::StringRef testName,
                                                                           int bufferSize,
                                                                           int speakerId)
{
    auto const curTestDirName{ "tests/util/buffer_wav_files/" + testName + "/" + juce::String(bufferSize) };
    auto const outputDir = getValidCurrentDirectory().getChildFile(curTestDirName);

    if (!outputDir.exists())
        outputDir.createDirectory();

    return outputDir.getChildFile("speaker_" + juce::String(speakerId) + ".wav");
}

void AudioBufferComparator::writeCachedBuffersToDisk(juce::StringRef testName,
                                                     int bufferSize,
                                                     double sampleRate /*= 48000.0*/)
{
    juce::WavAudioFormat wavFormat;

    for (const auto & [speakerId, buffer] : cachedBuffers) {
        juce::File wavFile = getSpeakerWavFile(testName, bufferSize, speakerId);
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

    cachedBuffers.clear();
}

#define PRINT_BUFFERS 0

void AudioBufferComparator::compareBuffers(const float * const curBuffer, const juce::AudioBuffer<float> & savedBuffer)
{
    REQUIRE_MESSAGE(curBuffer != nullptr, "Current buffer is null!");
    REQUIRE_MESSAGE(savedBuffer.getNumSamples() > 0, "Saved buffer has no samples!");
    for (int i = 0; i < savedBuffer.getNumSamples(); ++i) {
        const auto curSample = curBuffer[i];
        const auto savedSample = savedBuffer.getSample(0, i);

#if PRINT_BUFFERS
        if (std::abs(curSample - savedSample) >= .001f) {
            jassertfalse;
            DBG("curBuffer:");
            for (int i = 0; i < savedBuffer.getNumSamples(); ++i)
                DBG(curBuffer[i]);

            DBG("savedBuffer:");
            for (int i = 0; i < savedBuffer.getNumSamples(); ++i)
                DBG(savedBuffer.getSample(0, i));

            DBG("done");
            return;
        }
#endif

        REQUIRE_MESSAGE(std::abs(curSample - savedSample) < .001f,
                        "Buffers do not match at sample " + juce::String(i) + ": " + juce::String(curSample) + " vs "
                            + juce::String(savedSample));
    }
}

void AudioBufferComparator::makeSureSpeakerBufferMatchesSavedVersion(juce::StringRef testName,
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
            const auto speakerWavFile = getSpeakerWavFile(testName, bufferSize, speakerId);

            // this whole thing needs to be a function
            juce::AudioBuffer<float> wavBuffer(1, bufferSize);
            {
                juce::AudioFormatManager formatManager;
                formatManager.registerFormat(new juce::WavAudioFormat(), true);
                std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(speakerWavFile));

                REQUIRE_MESSAGE(reader != nullptr,
                                "Failed to create reader for file: " << speakerWavFile.getFullPathName());

                REQUIRE_MESSAGE(reader->numChannels == 1,
                                "File is not mono! Number of channels: " << reader->numChannels);
                // read the stored wave data into wavBuffer
                const auto readOk = reader->read(&wavBuffer, 0, bufferSize, curLoop * bufferSize, true, false);
                REQUIRE_MESSAGE(readOk, "Could not read from file: " << speakerWavFile.getFullPathName());
            }

            // compare the current and saved buffers
            compareBuffers(individualSpeakerBuffer, wavBuffer);
        });
}

void AudioBufferComparator::makeSureStereoBufferMatchesSavedVersion(juce::StringRef testName,
                                                                    const juce::AudioBuffer<float> & stereoAudioBuffer,
                                                                    int bufferSize,
                                                                    int curLoop)
{
    jassert(stereoAudioBuffer.getNumChannels() == 2);
    for (int curChannel = 0; curChannel < 2; ++curChannel) {
        const auto speakerWavFile = getSpeakerWavFile(testName, bufferSize, curChannel);

        // this whole thing needs to be a function
        juce::AudioBuffer<float> wavBuffer(1, bufferSize);
        {
            juce::AudioFormatManager formatManager;
            formatManager.registerFormat(new juce::WavAudioFormat(), true);
            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(speakerWavFile));

            REQUIRE_MESSAGE(reader != nullptr,
                            "Failed to create reader for file: " << speakerWavFile.getFullPathName());

            REQUIRE_MESSAGE(reader->numChannels == 1, "File is not mono! Number of channels: " << reader->numChannels);
            // read the stored wave data into wavBuffer
            const auto readOk = reader->read(&wavBuffer, 0, bufferSize, curLoop * bufferSize, true, false);
            REQUIRE_MESSAGE(readOk, "Could not read from file: " << speakerWavFile.getFullPathName());
        }

        // compare the current and saved buffers
        compareBuffers(stereoAudioBuffer.getReadPointer(curChannel), wavBuffer);
    }
}

} // namespace gris::tests
