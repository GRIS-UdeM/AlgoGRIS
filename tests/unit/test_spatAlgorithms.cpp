#include <catch2/catch_all.hpp>
#include <tests/sg_TestUtils.hpp>
#include <sg_AbstractSpatAlgorithm.hpp>
#include "../../StructGRIS/ValueTreeUtilities.hpp"

using namespace gris;
using namespace gris::tests;

static void distributeSourcesOnSphere(AbstractSpatAlgorithm * algo, SpatGrisData & data)
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
        auto & source{ data.project.sources[sourceIndex] };

        source.position = PolarVector(radians_t{ curAzimuth }, radians_t{ curRing * elevSteps }, 1.f);
        // DBG ("src " << i << ": " << source.position->getPolar ().toString ());
        curAzimuth += azimSteps;

        algo->updateSpatData(sourceIndex, source);

        if (curRing < numRings && i % numSourcesPerRing == 0) {
            ++curRing;
            curAzimuth = 0;
        }
    }
}

static void incrementAllSourcesAzimuth(AbstractSpatAlgorithm * algo, SpatGrisData & data, radians_t azimuthIncrement)
{
    for (int i = 1; i <= data.project.sources.size(); ++i) {
        const auto sourceIndex{ source_index_t{ i } };
        auto & source{ data.project.sources[sourceIndex] };

        // DBG ("src " << i << " before: " << source.position->getPolar ().toString ());
        auto const curPosition = source.position;
        source.position = curPosition->withAzimuth(curPosition->getPolar().azimuth + azimuthIncrement);
        // DBG ("src " << i << " after: " << source.position->getPolar ().toString ());

        algo->updateSpatData(sourceIndex, source);
    }
}

#if WRITE_TEST_OUTPUT
static void renderProjectOutput(juce::StringRef testName,
                                gris::SpatGrisData & data,
                                SourceAudioBuffer & sourceBuffer,
                                SpeakerAudioBuffer & speakerBuffer,
                                juce::AudioBuffer<float> & stereoBuffer,
                                SourcePeaks & sourcePeaks)
{
    const auto config{ data.toAudioConfig() };
    const auto numSources{ config->sourcesAudioConfig.size() };
    const auto numSpeakers{ config->speakersAudioConfig.size() };
    SpeakerBufferComparator comparator;

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
        distributeSourcesOnSphere(algo.get(), data);

        float lastPhase{ 0.f };

    #if USE_FIXED_NUM_LOOPS
        // now simulate processing an numTestLoops audio loops
        for (int i = 0; i < numTestLoops; ++i) {
    #else
        // now simulate processing an audio loop of testDurationSeconds
        auto const numLoops{ static_cast<int>(DEFAULT_SAMPLE_RATE * testDurationSeconds / bufferSize) };
        for (int i = 0; i < numLoops; ++i) {
    #endif
            // animate the sources and fill them with sine waves
            incrementAllSourcesAzimuth(algo.get(), data, TWO_PI / bufferSize);
            fillSourceBuffersWithSine(numSources, sourceBuffer, bufferSize, sourcePeaks, lastPhase);

            // process the audio
            speakerBuffer.silence();
            stereoBuffer.clear();
            algo->process(*config, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);

            // rite, so this fails because the stereo algorithm fills the stereoBuffer, and not the speakerBuffer
            //  cache the output buffers to memory
            comparator.cacheSpeakerBuffersInMemory(config->speakersAudioConfig, speakerBuffer, bufferSize);
        }

        // and once all loops are done, write the cached buffers to disk
        comparator.writeCachedSpeakerBuffersToDisk(testName, bufferSize);
    }
}
#endif

static void testUsingProjectData(juce::StringRef testName,
                                 gris::SpatGrisData & data,
                                 SourceAudioBuffer & sourceBuffer,
                                 SpeakerAudioBuffer & speakerBuffer,
                                 juce::AudioBuffer<float> & stereoBuffer,
                                 SourcePeaks & sourcePeaks)
{
#if ENABLE_TESTS
    const auto config{ data.toAudioConfig() };
    const auto numSources{ config->sourcesAudioConfig.size() };
    const auto numSpeakers{ config->speakersAudioConfig.size() };

    SpeakerBufferComparator speakerComparator;

    // for every test buffer size
    for (int bufferSize : bufferSizes) {
        std::cout << "\tTesting audio loop with buffer size: " << bufferSize << "...\n";
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
        distributeSourcesOnSphere(algo.get(), data);

        float lastPhase{ 0.f };

    #if USE_FIXED_NUM_LOOPS
        // now simulate processing an numTestLoops audio loops
        for (int i = 0; i < numTestLoops; ++i) {
    #else
        // now simulate processing an audio loop of testDurationSeconds
        auto const numLoops{ static_cast<int>(DEFAULT_SAMPLE_RATE * testDurationSeconds / bufferSize) };
        for (int i = 0; i < numLoops; ++i) {
    #endif

            // animate the sources and fill them with sine waves
            incrementAllSourcesAzimuth(algo.get(), data, TWO_PI / bufferSize);
            fillSourceBuffersWithSine(numSources, sourceBuffer, bufferSize, sourcePeaks, lastPhase);
            checkSourceBufferValidity(sourceBuffer);

            // process the audio
            speakerBuffer.silence();
            stereoBuffer.clear();
            algo->process(*config, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);

            // check that the audio output is valid
            speakerComparator.makeSureSpeakerBufferMatchesSavedVersion(testName,
                                                                       config->speakersAudioConfig,
                                                                       speakerBuffer,
                                                                       bufferSize,
                                                                       i);
            // makeSureStereoBufferMatchesSavedVersion (stereoBuffer);
        }
    }
#endif
}

static void benchmarkUsingProjectData(std::string testName,
                                      gris::SpatGrisData & data,
                                      SourceAudioBuffer & sourceBuffer,
                                      SpeakerAudioBuffer & speakerBuffer,
                                      juce::AudioBuffer<float> & stereoBuffer,
                                      SourcePeaks & sourcePeaks)
{
#if ENABLE_BENCHMARKS
    const auto config{ data.toAudioConfig() };
    const auto numSources{ config->sourcesAudioConfig.size() };
    const auto numSpeakers{ config->speakersAudioConfig.size() };
    const auto bufferSize{ 512 };
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
    distributeSourcesOnSphere(algo.get(), data);

    fillSourceBuffersWithNoise(numSources, sourceBuffer, bufferSize, sourcePeaks);
    checkSourceBufferValidity(sourceBuffer);

    // process the audio
    speakerBuffer.silence();
    stereoBuffer.clear();
    #if ENABLE_CATCH2_BENCHMARKS
    BENCHMARK("processing loop")
    #else
    std::cout << testName << "\n";
    for (int i = 0; i < 1000; ++i)
    #endif
    {
        // catch2 will run this benchmark section in a loop, so we need to clear the output buffers before each run
        speakerBuffer.silence();
        stereoBuffer.clear();
        algo->process(*config, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);
    };
#endif
}

static SpatGrisData getSpatGrisDataFromFiles(const std::string & projectFilename,
                                             const std::string & speakerSetupFilename)
{
    SpatGrisData spatGrisData;

    // hack to get around the pipeline having a different path than other places
    auto utilDir = getValidCurrentDirectory().getChildFile("tests/util");

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

TEST_CASE(vbapTestName, "[spat]")
{
    SpatGrisData vbapData = getSpatGrisDataFromFiles("default_preset.xml", "default_speaker_setup.xml");
    vbapData.project.spatMode = SpatMode::vbap;
    vbapData.appData.stereoMode = {};

    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

    std::cout << "Starting " << vbapTestName << " tests:\n";
#if WRITE_TEST_OUTPUT
    renderProjectOutput(vbapTestName, vbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
#endif
    testUsingProjectData(vbapTestName, vbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
    std::cout << vbapTestName << " tests done.\n";
    benchmarkUsingProjectData("vbap benchmark", vbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
}

// TEST_CASE(stereoTestName, "[spat]")
// {
//     SpatGrisData stereoData = getSpatGrisDataFromFiles("default_preset.xml", "STEREO_SPEAKER_SETUP.xml");
//     stereoData.project.spatMode = SpatMode::vbap;
//     stereoData.appData.stereoMode = StereoMode::stereo;

//     SourceAudioBuffer sourceBuffer;
//     SpeakerAudioBuffer speakerBuffer;
//     juce::AudioBuffer<float> stereoBuffer;
//     SourcePeaks sourcePeaks;

//     std::cout << "Starting " << stereoTestName << " tests:\n";
// #if WRITE_TEST_OUTPUT
//     renderProjectOutput(stereoTestName, stereoData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
// #endif
//     testUsingProjectData(stereoTestName, stereoData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
//     std::cout << stereoTestName << " tests done.\n";

//     benchmarkUsingProjectData("stereo benchmark", stereoData, sourceBuffer, speakerBuffer, stereoBuffer,
//     sourcePeaks);
// }

TEST_CASE(mbapTestName, "[spat]")
{
    SpatGrisData mbapData
        = getSpatGrisDataFromFiles("default_project18(8X2-Subs2).xml", "Cube_default_speaker_setup.xml");

    mbapData.project.spatMode = SpatMode::mbap;
    mbapData.appData.stereoMode = {};

    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

    std::cout << "Starting " << mbapTestName << " tests:\n";
#if WRITE_TEST_OUTPUT
    renderProjectOutput(mbapTestName, mbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
#endif
    testUsingProjectData(mbapTestName, mbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
    std::cout << mbapTestName << " tests done.\n";

    benchmarkUsingProjectData("mbap benchmark", mbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
}

// TEST_CASE(hrtfTestName, "[spat]")
// {
//     SpatGrisData hrtfData = getSpatGrisDataFromFiles("default_preset.xml", "BINAURAL_SPEAKER_SETUP.xml");
//     hrtfData.project.spatMode = SpatMode::vbap;
//     hrtfData.appData.stereoMode = StereoMode::hrtf;

//     SourceAudioBuffer sourceBuffer;
//     SpeakerAudioBuffer speakerBuffer;
//     juce::AudioBuffer<float> stereoBuffer;
//     SourcePeaks sourcePeaks;

//     std::cout << "Starting " << hrtfTestName << " tests:\n";
// #if WRITE_TEST_OUTPUT
//     renderProjectOutput(hrtfTestName, hrtfData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
// #endif
//     testUsingProjectData(hrtfTestName, hrtfData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
//     std::cout << hrtfTestName << " tests done.\n";

//     benchmarkUsingProjectData("hrtf benchmark", hrtfData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
// }
