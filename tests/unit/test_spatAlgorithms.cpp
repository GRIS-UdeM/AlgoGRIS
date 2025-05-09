#include <catch2/catch_all.hpp>
#include <tests/sg_TestUtils.hpp>
#include <sg_AbstractSpatAlgorithm.hpp>

using namespace gris;
using namespace gris::tests;

static void positionSources(AbstractSpatAlgorithm * algo, SpatGrisData & data)
{
    const auto numSources{ data.project.sources.size() };
    const auto numRings{ 3 };
    const auto numSourcesPerRing{ numSources / numRings };
    const auto elevSteps{ HALF_PI.get() / numRings };
    const auto azimSteps{ TWO_PI.get() / numSourcesPerRing };
    auto curRing{ 0 };
    auto curAzimuth{ 0.f };

    for (int i = 1; i <= numSources; ++i) {
        const auto sourceIndex{ source_index_t{ i } };
        auto source{ data.project.sources[sourceIndex] };

        source.position = PolarVector(radians_t{ curAzimuth }, radians_t{ curRing * elevSteps }, 1.f);
        curAzimuth += azimSteps;

        algo->updateSpatData(sourceIndex, source);

        if (curRing < numRings && i % numSourcesPerRing == 0) {
            ++curRing;
            curAzimuth = 0;
        }
    }
}

static void testUsingProjectData(gris::SpatGrisData & data,
                                 SourceAudioBuffer & sourceBuffer,
                                 SpeakerAudioBuffer & speakerBuffer,
                                 juce::AudioBuffer<float> & stereoBuffer,
                                 SourcePeaks & sourcePeaks)
{
    const auto config{ data.toAudioConfig() };
    const auto numSources{ config->sourcesAudioConfig.size() };
    const auto numSpeakers{ config->speakersAudioConfig.size() };

    // for every test buffer size
    for (int bufferSize : bufferSizes) {
        data.appData.audioSettings.bufferSize = bufferSize;

        // init our buffers
        initBuffers(bufferSize, numSources, numSpeakers, sourceBuffer, speakerBuffer, stereoBuffer);

        // create our spatialization algorithm
        auto algo{ AbstractSpatAlgorithm::make(data.speakerSetup,
                                               data.project.spatMode,
                                               data.appData.stereoMode,
                                               data.project.sources,
                                               data.appData.audioSettings.sampleRate,
                                               data.appData.audioSettings.bufferSize) };

        // position the sound sources
        positionSources(algo.get(), data);

        // now simulate processing an audio loop of testDurationSeconds
        auto const numLoops{ static_cast<int>(DEFAULT_SAMPLE_RATE * testDurationSeconds / bufferSize) };
        for (int i = 0; i < numLoops; ++i) {
            // fill the source buffers with pink noise
            fillSourceBuffersWithNoise(numSources, sourceBuffer, bufferSize, sourcePeaks);
            checkSourceBufferValidity(sourceBuffer);

            // process the audio
            speakerBuffer.silence();
            stereoBuffer.clear();
            algo->process(*config, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);

            // check that the audio output is valid
            checkSpeakerBufferValidity(speakerBuffer);
        }
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
        SpatGrisData vbapData;

        const auto speakerSetupFile{ juce::File::getCurrentWorkingDirectory().getChildFile(
            "tests/util/default_speaker_setup.xml") };
        if (const auto speakerSetup{ parseXML(speakerSetupFile) })
            vbapData.speakerSetup = *SpeakerSetup::fromXml(*speakerSetup);

        const auto projectFile{ juce::File::getCurrentWorkingDirectory().getChildFile(
            "tests/util/default_preset.xml") };
        if (const auto project{ parseXML(projectFile) })
            vbapData.project = *ProjectData::fromXml(*project);

        vbapData.project.spatMode = SpatMode::vbap;
        vbapData.appData.stereoMode = {};

        THEN("The VBAP algo executes correctly")
        {
            testUsingProjectData(vbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
        }
    }
}

TEST_CASE("HRTF test", "[spat]")
{
    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

    GIVEN("VBAP data sourced from XML")
    {
        SpatGrisData hrtfData;

        const auto hrtfSpeakerSetupFile{ juce::File::getCurrentWorkingDirectory().getChildFile(
            "tests/util/BINAURAL_SPEAKER_SETUP.xml") };
        if (const auto hrtfSpeakerSetup{ parseXML(hrtfSpeakerSetupFile) })
            hrtfData.speakerSetup = *SpeakerSetup::fromXml(*hrtfSpeakerSetup);

        const auto hrtfProjectFile{ juce::File::getCurrentWorkingDirectory().getChildFile(
            "tests/util/default_preset.xml") };
        if (const auto hrtfProject{ parseXML(hrtfProjectFile) })
            hrtfData.project = *ProjectData::fromXml(*hrtfProject);

        hrtfData.project.spatMode = SpatMode::vbap;
        hrtfData.appData.stereoMode = StereoMode::hrtf;

        THEN("The HRTF algo executes correctly")
        {
            testUsingProjectData(hrtfData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
        }
    }
}

TEST_CASE("MBAP test", "[spat]")
{
    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

    GIVEN("MBAP data sourced from XML")
    {
        SpatGrisData mbapData;

        const auto mbapSpeakerSetupFile{ juce::File::getCurrentWorkingDirectory().getChildFile(
            "tests/util/Cube_default_speaker_setup.xml") };
        if (const auto mbapSpeakerSetup{ parseXML(mbapSpeakerSetupFile) })
            mbapData.speakerSetup = *SpeakerSetup::fromXml(*mbapSpeakerSetup);

        const auto mbapProjectFile{ juce::File::getCurrentWorkingDirectory().getChildFile(
            "tests/util/default_project18(8X2-Subs2).xml") };
        if (const auto project{ parseXML(mbapProjectFile) })
            mbapData.project = *ProjectData::fromXml(*project);

        mbapData.project.spatMode = SpatMode::mbap;
        mbapData.appData.stereoMode = {};

        THEN("The MBAP algo executes correctly")
        {
            testUsingProjectData(mbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
        }
    }
}
