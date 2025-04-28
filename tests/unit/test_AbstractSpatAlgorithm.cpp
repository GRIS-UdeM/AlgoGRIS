#include <catch2/catch_all.hpp>
#include <tests/sg_TestUtils.hpp>
#include <sg_AbstractSpatAlgorithm.hpp>

using namespace gris;
using namespace gris::tests;

static void updateSourcePeaks(AudioConfig & config, const SourceAudioBuffer & sourceBuffer, SourcePeaks & sourcePeaks)
{
    for (auto const & source : config.sourcesAudioConfig) {
        auto const peak{ sourceBuffer[source.key].getMagnitude(0, sourceBuffer.getNumSamples()) };
        sourcePeaks[source.key] = peak;
    }
}
static void updateSourceData(AbstractSpatAlgorithm * algo, SpatGrisData & data)
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

TEST_CASE("VBAP test", "[spat]")
{
    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

    GIVEN("VBAP data sourced from XML")
    {
        // init project data and audio config
        SpatGrisData vbapData;
        vbapData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(DEFAULT_SPEAKER_SETUP_FILE));
        vbapData.project = *ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE));
        vbapData.project.spatMode = SpatMode::vbap;
        vbapData.appData.stereoMode = {};

        // the default project and speaker setups have 18 sources and 18 speakers
        auto const vbapConfig{ vbapData.toAudioConfig() };
        // DBG("number of sources: " << vbapConfig->sourcesAudioConfig.size());
        // DBG("number of speakers: " << vbapConfig->speakersAudioConfig.size());
        THEN("The VBAP algo executes correctly")
        {
            for (int bufferSize : bufferSizes) {
                vbapData.appData.audioSettings.bufferSize = bufferSize;
                initBuffers(bufferSize, &sourceBuffer, &speakerBuffer, &stereoBuffer);

                auto vbapAlgo{ AbstractSpatAlgorithm::make(vbapData.speakerSetup,
                                                           vbapData.project.spatMode,
                                                           vbapData.appData.stereoMode,
                                                           vbapData.project.sources,
                                                           vbapData.appData.audioSettings.sampleRate,
                                                           vbapData.appData.audioSettings.bufferSize) };

                updateSourceData(vbapAlgo.get(), vbapData);

                auto const numLoops{ static_cast<int>(DEFAULT_SAMPLE_RATE * testDurationSeconds / bufferSize) };
                for (int i = 0; i < numLoops; ++i) {
                    fillSourceBufferWithNoise(sourceBuffer, *vbapConfig);
                    updateSourcePeaks(*vbapConfig, sourceBuffer, sourcePeaks);

                    checkSourceBufferValidity(sourceBuffer, *vbapConfig);
                    vbapAlgo->process(*vbapConfig, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);
                    checkSpeakerBufferValidity(speakerBuffer, *vbapConfig);
                }
            }
        }
    }
}

TEST_CASE("HRTF test", "[spat]")
{
    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

    GIVEN("HRTF data sourced from XML")
    {
        SpatGrisData hrtfData;
        hrtfData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(BINAURAL_SPEAKER_SETUP_FILE));
        hrtfData.project = *ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE));
        hrtfData.project.spatMode = SpatMode::vbap;
        hrtfData.appData.stereoMode = StereoMode::hrtf;
        auto const hrtfConfig{ hrtfData.toAudioConfig() };

        for (int bufferSize : bufferSizes) {
            hrtfData.appData.audioSettings.bufferSize = bufferSize;
            initBuffers(bufferSize, &sourceBuffer, &speakerBuffer, &stereoBuffer);

            auto hrtfAlgo{ AbstractSpatAlgorithm::make(hrtfData.speakerSetup,
                                                       hrtfData.project.spatMode,
                                                       hrtfData.appData.stereoMode,
                                                       hrtfData.project.sources,
                                                       hrtfData.appData.audioSettings.sampleRate,
                                                       hrtfData.appData.audioSettings.bufferSize) };
            updateSourceData(hrtfAlgo.get(), hrtfData);

            auto const numLoops{ static_cast<int>(DEFAULT_SAMPLE_RATE * testDurationSeconds / bufferSize) };
            for (int i = 0; i < numLoops; ++i) {
                fillSourceBufferWithNoise(sourceBuffer, *hrtfConfig);
                updateSourcePeaks(*hrtfConfig, sourceBuffer, sourcePeaks);

                checkSourceBufferValidity(sourceBuffer, *hrtfConfig);
                hrtfAlgo->process(*hrtfConfig, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);
                checkSpeakerBufferValidity(speakerBuffer, *hrtfConfig);
            }
        }
    }
}
