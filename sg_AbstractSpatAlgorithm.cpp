/*
 This file is part of SpatGRIS.

 Developers: Gaël Lane Lépine, Samuel Béland, Olivier Bélanger, Nicolas Masson

 SpatGRIS is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 SpatGRIS is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with SpatGRIS.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sg_AbstractSpatAlgorithm.hpp"

#include "sg_HrtfSpatAlgorithm.hpp"
#include "sg_HybridSpatAlgorithm.hpp"
#include "sg_MbapSpatAlgorithm.hpp"
#include "sg_StereoSpatAlgorithm.hpp"
#include "sg_VbapSpatAlgorithm.hpp"

#ifdef USE_DOPPLER
    #include "sg_DopplerSpatAlgorithm.hpp"
#endif
#include <random>

namespace gris
{
void fillSourceBufferWithNoise(SourceAudioBuffer & buffer, AudioConfig& config)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (auto const & source : config.sourcesAudioConfig)
        for (auto const & speaker : config.speakersAudioConfig)
            if (!source.value.isMuted && !speaker.value.isMuted)
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    buffer[source.key].getWritePointer(0)[sample] = dist(gen);
}

class AbstractSpatAlgorithmTest : public juce::UnitTest
{
    float constexpr static testDurationSeconds{ .1f };
    std::vector<int> const bufferSizes{ 1, 512, 1024, SourceAudioBuffer::MAX_NUM_SAMPLES };

    SourceAudioBuffer sourceBuffers;
    SpeakerAudioBuffer speakerBuffers;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

public:
    bool static isRunning;

    AbstractSpatAlgorithmTest() : juce::UnitTest("AbstractSpatAlgorithmTest") {}

    void checkSpeakerBufferValidity(SpeakerAudioBuffer & buffer, AudioConfig & config)
    {
        for (auto const & speaker : config.speakersAudioConfig) {
            const auto speakerBuffer { buffer[speaker.key].getReadPointer(0) };
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                const auto speakerSample = speakerBuffer[sample];
                expect(std::isfinite(speakerSample), "Output contains NaN or Inf values!");
                expect(speakerSample >= -1.0f && speakerSample <= 1.0f, "Output exceeds valid range!");
            }
        }
    }

    void checkSourceBufferValidity(SourceAudioBuffer & buffer, AudioConfig & config)
    {
        for (auto const & source : config.sourcesAudioConfig) {
            const auto sourceBuffer { buffer[source.key].getReadPointer(0) };
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                const auto sourceSample = sourceBuffer[sample];
                expect(std::isfinite(sourceSample), "buffer contains NaN or Inf values!");
                expect(sourceSample >= -1.0f && sourceSample <= 1.0f, "Value exceeds valid range!");
            }
        }
    }

    void initBuffers (int bufferSize, const SpatGrisData& data)
    {
        // init source buffer with MAX_NUM_SOURCES sources
        juce::Array<source_index_t> sources;
        for (int i = 1; i <= MAX_NUM_SOURCES; ++i)
            sources.add(source_index_t{ i });
        sourceBuffers.init(sources);

        // init speaker buffer with MAX_NUM_SPEAKERS speakers
        juce::Array<output_patch_t> speakers;
        for (int i = 1; i <= MAX_NUM_SPEAKERS; ++i)
            speakers.add(output_patch_t{ i });
        speakerBuffers.init(speakers);

        //set proper buffer sizes
        sourceBuffers.setNumSamples(bufferSize);
        speakerBuffers.setNumSamples(bufferSize);
        stereoBuffer.setSize(2, bufferSize);
        stereoBuffer.clear();
#if 0
        //fill source buffers with pink noise
        StaticVector<output_patch_t, MAX_NUM_SPEAKERS> activeChannels{};
        for (int i = 1; i < 19; ++i)
            activeChannels.push_back({i});
        auto temp { sourceBuffers.getArrayOfWritePointers(activeChannels) };

        fillWithPinkNoise(data.data(), numSamples, narrow<int>(data.size()), *mAudioData.config->pinkNoiseGain);

        //update peaks
//        processInputPeaks(sourceBuffers, sourcePeaks);

        //from AudioProcessor::processInputPeaks(SourceAudioBuffer & inputBuffer, SourcePeaks & peaks) const noexcept
            for (auto const channel : sourceBuffers) {
                auto const & config{ mAudioData.config->sourcesAudioConfig[channel.key] };
                auto const & buffer{ *channel.value };
                auto const peak{ config.isMuted ? 0.0f : buffer.getMagnitude(0, inputBuffer.getNumSamples()) };

                sourcePeaks[channel.key] = peak;
            }
#endif
    }

    void initialise() override { isRunning = true; }

    void updateSourcePeaks(AudioConfig& config)
    {
        for (auto const & source : config.sourcesAudioConfig) {
            auto const peak{ sourceBuffers[source.key].getMagnitude(0, sourceBuffers.getNumSamples()) };
            sourcePeaks[source.key] = peak;
        }
    }

    void updateSourceData(AbstractSpatAlgorithm * algo, SpatGrisData & data)
    {
        for (auto & speaker : SpeakerSetup::fromXml(*parseXML(DEFAULT_SPEAKER_SETUP_FILE))->speakers) {
            // update sources data from speakers position of speaker setup
            source_index_t const sourceIndex{ speaker.key.get() };
            auto source = data.project.sources.getNode(sourceIndex);
            source.value->position = speaker.value->position;
            source.value->azimuthSpan = 0.0f;
            source.value->zenithSpan = 0.0f;

            algo->updateSpatData(source.key, *source.value);
        }
    }

    void runTest() override
    {
        beginTest("VBAP test");
        {
            // init project data and audio config
            SpatGrisData vbapData;
            vbapData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(DEFAULT_SPEAKER_SETUP_FILE));
            vbapData.project = *ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE));
            vbapData.project.spatMode = SpatMode::vbap;
            vbapData.appData.stereoMode = {};

            // the default project and speaker setups have 18 sources and 18 speakers
            auto const vbapConfig{ vbapData.toAudioConfig() };
            //DBG("number of sources: " << vbapConfig->sourcesAudioConfig.size());
            //DBG("number of speakers: " << vbapConfig->speakersAudioConfig.size());

            for (int bufferSize : bufferSizes) {
                vbapData.appData.audioSettings.bufferSize = bufferSize;
                initBuffers(bufferSize, vbapData);

                auto vbapAlgo{ AbstractSpatAlgorithm::make(vbapData.speakerSetup,
                                                           vbapData.project.spatMode,
                                                           vbapData.appData.stereoMode,
                                                           vbapData.project.sources,
                                                           vbapData.appData.audioSettings.sampleRate,
                                                           vbapData.appData.audioSettings.bufferSize) };

                updateSourceData(vbapAlgo.get(), vbapData);

                auto const numLoops{ static_cast<int>(DEFAULT_SAMPLE_RATE * testDurationSeconds / bufferSize) };
                for (int i = 0; i < numLoops; ++i) {
                    fillSourceBufferWithNoise(sourceBuffers, *vbapConfig);
                    updateSourcePeaks(*vbapConfig);

                    checkSourceBufferValidity(sourceBuffers, *vbapConfig);
                    vbapAlgo->process(*vbapConfig, sourceBuffers, speakerBuffers, stereoBuffer, sourcePeaks, nullptr);
                    checkSpeakerBufferValidity(speakerBuffers, *vbapConfig);
                }
            }
        }

        beginTest("HRTF test");
        {
            SpatGrisData hrtfData;
            hrtfData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(BINAURAL_SPEAKER_SETUP_FILE));
            hrtfData.project = *ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE));
            hrtfData.project.spatMode = SpatMode::vbap;
            hrtfData.appData.stereoMode = StereoMode::hrtf;
            auto const hrtfConfig{ hrtfData.toAudioConfig() };

            for (int bufferSize : bufferSizes) {
                hrtfData.appData.audioSettings.bufferSize = bufferSize;
                initBuffers(bufferSize, hrtfData);

                auto hrtfAlgo{ AbstractSpatAlgorithm::make(hrtfData.speakerSetup,
                                                           hrtfData.project.spatMode,
                                                           hrtfData.appData.stereoMode,
                                                           hrtfData.project.sources,
                                                           hrtfData.appData.audioSettings.sampleRate,
                                                           hrtfData.appData.audioSettings.bufferSize) };
                updateSourceData(hrtfAlgo.get(), hrtfData);

                auto const numLoops{ static_cast<int>(DEFAULT_SAMPLE_RATE * testDurationSeconds / bufferSize) };
                for (int i = 0; i < numLoops; ++i) {
                    fillSourceBufferWithNoise(sourceBuffers, *hrtfConfig);
                    updateSourcePeaks(*hrtfConfig);

                    checkSourceBufferValidity(sourceBuffers, *hrtfConfig);
                    hrtfAlgo->process(*hrtfConfig, sourceBuffers, speakerBuffers, stereoBuffer, sourcePeaks, nullptr);
                    checkSpeakerBufferValidity(speakerBuffers, *hrtfConfig);
                }
            }
        }
    }

    void shutdown() override { isRunning = false; }
};

bool AbstractSpatAlgorithmTest::isRunning = false;

//==============================================================================
bool isOscThread()
{
    auto * currentThread{ juce::Thread::getCurrentThread() };
    if (!currentThread) {
        return false;
    }
    return currentThread->getThreadName() == "JUCE OSC server";
}

//==============================================================================
bool isProbablyAudioThread()
{
    return (! isOscThread() && !juce::MessageManager::getInstance()->isThisTheMessageThread());
}

bool areUnitTestsRunning()
{
    return AbstractSpatAlgorithmTest::isRunning;
}

//==============================================================================
void AbstractSpatAlgorithm::fixDirectOutsIntoPlace(SourcesData const & sources,
                                                   SpeakerSetup const & speakerSetup,
                                                   SpatMode const & projectSpatMode) noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const getFakeSourceData = [&](SourceData const & source, SpeakerData const & speaker) -> SourceData {
        auto fakeSourceData{ source };
        fakeSourceData.directOut.reset();
        switch (projectSpatMode) {
        case SpatMode::vbap:
        case SpatMode::hybrid:
            fakeSourceData.position = speaker.position.getPolar().normalized();
            return fakeSourceData;
        case SpatMode::mbap:
            fakeSourceData.position = speaker.position;
            return fakeSourceData;
        case SpatMode::invalid:
            break;
        }
        jassertfalse;
        return fakeSourceData;
    };

    for (auto const & source : sources) {
        auto const & directOut{ source.value->directOut };
        if (!directOut) {
            continue;
        }

        if (!speakerSetup.speakers.contains(*directOut)) {
            continue;
        }

        auto const & speaker{ speakerSetup.speakers[*directOut] };

        updateSpatData(source.key, getFakeSourceData(*source.value, speaker));
    }
}

//==============================================================================
std::unique_ptr<AbstractSpatAlgorithm> AbstractSpatAlgorithm::make(SpeakerSetup const & speakerSetup,
                                                                   SpatMode const & projectSpatMode,
                                                                   tl::optional<StereoMode> stereoMode,
                                                                   SourcesData const & sources,
                                                                   double const sampleRate,
                                                                   int const bufferSize)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (stereoMode) {
        switch (*stereoMode) {
        case StereoMode::hrtf:
            return HrtfSpatAlgorithm::make(speakerSetup, projectSpatMode, sources, sampleRate, bufferSize);
        case StereoMode::stereo:
            return StereoSpatAlgorithm::make(speakerSetup, projectSpatMode, sources);
#ifdef USE_DOPPLER
        case StereoMode::doppler:
            return DopplerSpatAlgorithm::make(sampleRate, bufferSize);
#endif
        }
        jassertfalse;
    }

    switch (projectSpatMode) {
    case SpatMode::vbap:
        return VbapSpatAlgorithm::make(speakerSetup);
    case SpatMode::mbap:
        return MbapSpatAlgorithm::make(speakerSetup);
    case SpatMode::hybrid:
        return HybridSpatAlgorithm::make(speakerSetup);
    case SpatMode::invalid:
        break;
    }

    jassertfalse;
    return nullptr;
}

#if JUCE_DEBUG && RUN_UNIT_TEST
// This will automatically create an instance of the test class and add it to the list of tests to be run.
static AbstractSpatAlgorithmTest abstractSpatAlgorithmTest;
#endif

} // namespace gris
