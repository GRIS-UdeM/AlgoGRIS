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
        auto const curPosition = source.position;
        source.position = curPosition->withAzimuth(curPosition->getPolar().azimuth + azimuthIncrement);

        algo->updateSpatData(sourceIndex, source);
    }
}

#if WRITE_TEST_OUTPUT_TO_DISK
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
    AudioBufferComparator speakerBuffercomparator;
    AudioBufferComparator stereoBuffercomparator;

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

            //  cache the output buffers to memory
            speakerBuffercomparator.cacheSpeakerBuffersInMemory(config->speakersAudioConfig, speakerBuffer, bufferSize);
            stereoBuffercomparator.cacheStereoBuffersInMemory(stereoBuffer, bufferSize);
        }

        // and once all loops are done, write the cached buffers to disk
        speakerBuffercomparator.writeCachedBuffersToDisk(testName + "/speaker", bufferSize);
        stereoBuffercomparator.writeCachedBuffersToDisk(testName + "/stereo", bufferSize);
    }
}
#endif

static void testUsingProjectData(juce::StringRef testName,
                                 gris::SpatGrisData & data,
                                 SourceAudioBuffer & sourceBuffer,
                                 SpeakerAudioBuffer & speakerBuffer,
#if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                                 ForkUnionBuffer & forkUnionBuffer,
#endif
                                 juce::AudioBuffer<float> & stereoBuffer,
                                 SourcePeaks & sourcePeaks)
{
#if ENABLE_TESTS
    const auto config{ data.toAudioConfig() };
    const auto numSources{ config->sourcesAudioConfig.size() };
    const auto numSpeakers{ config->speakersAudioConfig.size() };

    AudioBufferComparator speakerBufferComparator;
    AudioBufferComparator stereoBufferComparator;

    // for every test buffer size
    for (int bufferSize : bufferSizes) {
        std::cout << "\tTesting audio loop with buffer size: " << bufferSize << "...\n";
        data.appData.audioSettings.bufferSize = bufferSize;

        // init our buffers
        initBuffers(bufferSize,
                    numSources,
                    numSpeakers,
                    sourceBuffer,
                    speakerBuffer,
    #if USE_FORK_UNION
        #if FU_METHOD == FU_USE_ARRAY_OF_ATOMICS
                    forkUnionBuffer,
        #elif FU_METHOD == FU_USE_BUFFER_PER_THREAD
                    forkUnionBuffer,
        #endif
    #endif
                    stereoBuffer);

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
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
            algo->silenceForkUnionBuffer(forkUnionBuffer);
    #endif
            algo->process(*config,
                          sourceBuffer,
                          speakerBuffer,
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                          forkUnionBuffer,
    #endif
                          stereoBuffer,
                          sourcePeaks,
                          nullptr);

            checkSpeakerBufferValidity(speakerBuffer);

            // check that the audio output is valid
            speakerBufferComparator.makeSureSpeakerBufferMatchesSavedVersion(testName + "/speaker",
                                                                             config->speakersAudioConfig,
                                                                             speakerBuffer,
                                                                             bufferSize,
                                                                             i);

            stereoBufferComparator.makeSureStereoBufferMatchesSavedVersion(testName + "/stereo",
                                                                           stereoBuffer,
                                                                           bufferSize,
                                                                           i);
        }
    }
#endif
}

static void benchmarkUsingProjectData(gris::SpatGrisData & data,
                                      SourceAudioBuffer & sourceBuffer,
                                      SpeakerAudioBuffer & speakerBuffer,
#if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                                      ForkUnionBuffer & forkUnionBuffer,
#endif
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
    initBuffers(bufferSize,
                numSources,
                numSpeakers,
                sourceBuffer,
                speakerBuffer,
    #if USE_FORK_UNION
        #if FU_METHOD == FU_USE_ARRAY_OF_ATOMICS
                forkUnionBuffer,
        #elif FU_METHOD == FU_USE_BUFFER_PER_THREAD
                forkUnionBuffer,
        #endif
    #endif
                stereoBuffer);

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
    BENCHMARK("processing loop")
    {
        // catch2 will run this benchmark section in a loop, so we need to clear the output buffers before each run
        speakerBuffer.silence();
        stereoBuffer.clear();

    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
        algo->silenceForkUnionBuffer(forkUnionBuffer);
    #endif

        algo->process(*config,
                      sourceBuffer,
                      speakerBuffer,
    #if USE_FORK_UNION
        #if FU_METHOD == FU_USE_ARRAY_OF_ATOMICS
                      forkUnionBuffer,
        #elif FU_METHOD == FU_USE_BUFFER_PER_THREAD
                      forkUnionBuffer,
        #endif
    #endif
                      stereoBuffer,
                      sourcePeaks,
                      nullptr);
    };
#endif
};

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

#if 1 // ONLY_TEST_VBAP

TEST_CASE(vbapTestName, "[spat]")
{
    // 1. init needed structures
    SpatGrisData vbapData = getSpatGrisDataFromFiles("default_preset.xml", "default_speaker_setup.xml");
    vbapData.project.spatMode = SpatMode::vbap;
    vbapData.appData.stereoMode = {};

    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
    ForkUnionBuffer forkUnionBuffer;
    #endif
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

    // 2. tests
    std::cout << "Starting " << vbapTestName << " tests:" << std::endl;
    #if WRITE_TEST_OUTPUT_TO_DISK
    renderProjectOutput(vbapTestName, vbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
    #endif
    testUsingProjectData(vbapTestName,
                         vbapData,
                         sourceBuffer,
                         speakerBuffer,
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                         forkUnionBuffer,
    #endif
                         stereoBuffer,
                         sourcePeaks);
    std::cout << vbapTestName << " tests done." << std::endl;

    // 3. benchmarks, using more sources
    vbapData = getSpatGrisDataFromFiles("default_preset_256.xml", "default_speaker_setup.xml");
    benchmarkUsingProjectData(vbapData,
                              sourceBuffer,
                              speakerBuffer,
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                              forkUnionBuffer,
    #endif
                              stereoBuffer,
                              sourcePeaks);
}

TEST_CASE(stereoTestName, "[spat]")
{
    SpatGrisData stereoData = getSpatGrisDataFromFiles("default_preset.xml", "STEREO_SPEAKER_SETUP.xml");
    stereoData.project.spatMode = SpatMode::vbap;
    stereoData.appData.stereoMode = StereoMode::stereo;

    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
    ForkUnionBuffer forkUnionBuffer;
    #endif
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

    std::cout << "Starting " << stereoTestName << " tests:" << std::endl;
    #if WRITE_TEST_OUTPUT_TO_DISK
    renderProjectOutput(stereoTestName, stereoData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
    #endif
    testUsingProjectData(stereoTestName,
                         stereoData,
                         sourceBuffer,
                         speakerBuffer,
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                         forkUnionBuffer,
    #endif
                         stereoBuffer,
                         sourcePeaks);
    std::cout << stereoTestName << " tests done." << std::endl;

    benchmarkUsingProjectData(stereoData,
                              sourceBuffer,
                              speakerBuffer,
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                              forkUnionBuffer,
    #endif
                              stereoBuffer,
                              sourcePeaks);
}

TEST_CASE(mbapTestName, "[spat]")
{
    SpatGrisData mbapData
        = getSpatGrisDataFromFiles("default_project18(8X2-Subs2).xml", "Cube_default_speaker_setup.xml");

    mbapData.project.spatMode = SpatMode::mbap;
    mbapData.appData.stereoMode = {};

    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
    ForkUnionBuffer forkUnionBuffer;
    #endif
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

    std::cout << "Starting " << mbapTestName << " tests:" << std::endl;
    #if WRITE_TEST_OUTPUT_TO_DISK
    renderProjectOutput(mbapTestName, mbapData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
    #endif
    testUsingProjectData(mbapTestName,
                         mbapData,
                         sourceBuffer,
                         speakerBuffer,
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                         forkUnionBuffer,
    #endif
                         stereoBuffer,
                         sourcePeaks);
    std::cout << mbapTestName << " tests done." << std::endl;

    benchmarkUsingProjectData(mbapData,
                              sourceBuffer,
                              speakerBuffer,
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                              forkUnionBuffer,
    #endif
                              stereoBuffer,
                              sourcePeaks);
}

// #else

TEST_CASE(hrtfTestName, "[spat]")
{
    SpatGrisData hrtfData = getSpatGrisDataFromFiles("default_preset.xml", "BINAURAL_SPEAKER_SETUP.xml");
    hrtfData.project.spatMode = SpatMode::vbap;
    hrtfData.appData.stereoMode = StereoMode::hrtf;

    SourceAudioBuffer sourceBuffer;
    SpeakerAudioBuffer speakerBuffer;
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
    ForkUnionBuffer forkUnionBuffer;
    #endif
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

    std::cout << "Starting " << hrtfTestName << " tests:" << std::endl;
    #if WRITE_TEST_OUTPUT_TO_DISK
    renderProjectOutput(hrtfTestName, hrtfData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
    #endif
    testUsingProjectData(hrtfTestName,
                         hrtfData,
                         sourceBuffer,
                         speakerBuffer,
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                         forkUnionBuffer,
    #endif
                         stereoBuffer,
                         sourcePeaks);
    std::cout << hrtfTestName << " tests done." << std::endl;

    benchmarkUsingProjectData(hrtfData,
                              sourceBuffer,
                              speakerBuffer,
    #if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                              forkUnionBuffer,
    #endif
                              stereoBuffer,
                              sourcePeaks);
}

#endif
