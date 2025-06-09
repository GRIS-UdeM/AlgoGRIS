#include <catch2/catch_all.hpp>
#include <tests/sg_TestUtils.hpp>
#include <sg_AbstractSpatAlgorithm.hpp>
#include "../../StructGRIS/ValueTreeUtilities.hpp"

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

static SpatGrisData getSpatGrisDataFromFiles(const std::string & projectFilename,
                                             const std::string & speakerSetupFilename)
{
    SpatGrisData spatGrisData;

    // hack to get around the pipeline having a different path than other places
    auto utilDir = getCurDir().getChildFile("tests/util");

    // make sure project file exists
    const auto projectFile{ utilDir.getChildFile(projectFilename) };
    // std::cout << "full path for projectFile: " << projectFile.getFullPathName() << "\n";
    REQUIRE(projectFile.existsAsFile());

    // make sure project file opens correctly
    const auto project{ parseXML(projectFile) };
    REQUIRE(project);
    if (project)
        spatGrisData.project = *ProjectData::fromXml(*project);

    // make sure speaker setup file exists
    const auto speakerSetupFile{ utilDir.getChildFile(speakerSetupFilename) };
    // std::cout << "full path for speakerSetupFile: " << speakerSetupFile.getFullPathName() << "\n";
    REQUIRE(speakerSetupFile.existsAsFile());

    // make sure speaker setup opens correctly
    const auto speakerSetup{ parseXML(speakerSetupFile) };
    REQUIRE(speakerSetup);
    if (speakerSetup)
        spatGrisData.speakerSetup = *SpeakerSetup::fromXml(*speakerSetup);

    return spatGrisData;
}

TEST_CASE("VBAP test", "[spat]")
{
    SpatGrisData vbapData = getSpatGrisDataFromFiles("default_preset.xml", "default_speaker_setup.xml");
    vbapData.project.spatMode = SpatMode::vbap;
    vbapData.appData.stereoMode = {};

    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;
    testUsingProjectData(vbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
}

TEST_CASE("MBAP test", "[spat]")
{
    SpatGrisData mbapData
        = getSpatGrisDataFromFiles("default_project18(8X2-Subs2).xml", "Cube_default_speaker_setup.xml");

    mbapData.project.spatMode = SpatMode::mbap;
    mbapData.appData.stereoMode = {};

    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;
    testUsingProjectData(mbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
}

TEST_CASE("HRTF test", "[spat]")
{
    SpatGrisData hrtfData = getSpatGrisDataFromFiles("default_preset.xml", "BINAURAL_SPEAKER_SETUP.xml");
    hrtfData.project.spatMode = SpatMode::vbap;
    hrtfData.appData.stereoMode = StereoMode::hrtf;

    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;
    testUsingProjectData(hrtfData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
}

TEST_CASE("Stereo speaker", "[spat]")
{
    SpatGrisData stereoData = getSpatGrisDataFromFiles("default_preset.xml", "STEREO_SPEAKER_SETUP.xml");
    stereoData.project.spatMode = SpatMode::vbap;
    stereoData.appData.stereoMode = StereoMode::stereo;

    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;
    testUsingProjectData(stereoData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
}
