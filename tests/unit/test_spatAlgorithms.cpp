#include <Utilities/ValueTreeUtilities.hpp>
#include <catch2/catch_all.hpp>
#include <tests/sg_TestUtils.hpp>
#include <sg_AbstractSpatAlgorithm.hpp>

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
                                               data.appData.audioSettings.bufferSize,
                                               data.project.useMulticoreDSP) };

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
        initBuffers(bufferSize, numSources, numSpeakers, sourceBuffer, speakerBuffer, stereoBuffer);

        // create our spatialization algorithm
        auto algo{ AbstractSpatAlgorithm::make(data.speakerSetup,
                                               data.project.spatMode,
                                               data.appData.stereoMode,
                                               data.project.sources,
                                               data.appData.audioSettings.sampleRate,
                                               data.appData.audioSettings.bufferSize,
                                               data.project.useMulticoreDSP) };

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
                                           data.appData.audioSettings.bufferSize,
                                           data.project.useMulticoreDSP) };

    // position the sound sources
    distributeSourcesOnSphere(algo.get(), data);

    fillSourceBuffersWithNoise(numSources, sourceBuffer, bufferSize, sourcePeaks);
    checkSourceBufferValidity(sourceBuffer);

    // process the audio
    BENCHMARK("processing loop")
    {
        for (int i = 0; i < 6; ++i) {
            algo->process(*config, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);
            // make the sources move
            incrementAllSourcesAzimuth(algo.get(), data, TWO_PI / bufferSize);
        }
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
    REQUIRE(projectFile.existsAsFile());

    // make sure project file opens correctly
    const auto project{ parseXML(projectFile) };
    REQUIRE(project);
    if (project)
        spatGrisData.project = *ProjectData::fromXml(*project);

    // make sure speaker setup file exists
    const auto speakerSetupFile{ utilDir.getChildFile(speakerSetupFilename) };
    REQUIRE(speakerSetupFile.existsAsFile());

    // make sure speaker setup opens correctly
    const auto speakerSetup{ parseXML(speakerSetupFile) };
    REQUIRE(speakerSetup);
    if (speakerSetup)
        spatGrisData.speakerSetup = *SpeakerSetup::fromXml(*speakerSetup);

    return spatGrisData;
}

void spatTest(std::string testName,
              // some tests, like parallel vbap and mbap should use the same validation files
              // as their non parallel counterpart
              std::string validationFileTestName,
              std::string testProjectFile,
              std::string testSpeakerSetupFile,
              std::string benchmarkProjectFile,
              std::string benchmarkSpeakerSetupFile,
              SpatMode spatMode,
              tl::optional<StereoMode> stereoMode,
              bool multicoreDSP)
{
    SECTION(testName)
    {
        // 1. init needed structures
        SpatGrisData sgData = getSpatGrisDataFromFiles(testProjectFile, testSpeakerSetupFile);
        sgData.project.spatMode = spatMode;
        sgData.project.useMulticoreDSP = multicoreDSP;
        sgData.appData.stereoMode = stereoMode;

        SourceAudioBuffer sourceBuffer;
        SpeakerAudioBuffer speakerBuffer;
        juce::AudioBuffer<float> stereoBuffer;
        SourcePeaks sourcePeaks;

        // 2. tests
        std::cout << "Starting " << testName << " tests:" << std::endl;
#if WRITE_TEST_OUTPUT_TO_DISK
        renderProjectOutput(validationFileTestName, sgData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
#endif
        testUsingProjectData(validationFileTestName, sgData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
        std::cout << testName << " tests done." << std::endl;

        // 3. benchmarks, using more sources
        sgData = getSpatGrisDataFromFiles(benchmarkProjectFile, benchmarkSpeakerSetupFile);
        sgData.project.spatMode = spatMode;
        sgData.project.useMulticoreDSP = multicoreDSP;
        sgData.appData.stereoMode = stereoMode;

        benchmarkUsingProjectData(sgData, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks);
    }
}

TEST_CASE("Spatialization tests", "[spat]")
{
    spatTest(vbapTestName,                // test name
             vbapTestName,                // test name used to load validation files
             "default_preset.xml",        // project file used for tests
             "default_speaker_setup.xml", // speaker setup used for tests
             "default_preset_256.xml",    // project file used for benchmarks
             "default_speaker_setup.xml", // speaker setup used for benchmarks
             SpatMode::vbap,              // spatialisation algorithm flavour
             tl::nullopt,                 // stereo reduction
             false);                      // parallelize DSP computations

    spatTest("Parallel Vbap test",
             vbapTestName,
             "default_preset.xml",
             "default_speaker_setup.xml",
             "default_preset_256.xml",
             "default_speaker_setup.xml",
             SpatMode::vbap,
             tl::nullopt,
             true);

    spatTest(mbapTestName,
             mbapTestName,
             "default_project18(8X2-Subs2).xml",
             "Cube_default_speaker_setup.xml",
             "default_preset_256.xml",
             "Cube_default_speaker_setup.xml",
             SpatMode::mbap,
             tl::nullopt,
             false);

    spatTest("parallel mbap",
             mbapTestName,
             "default_project18(8X2-Subs2).xml",
             "Cube_default_speaker_setup.xml",
             "default_preset_256.xml",
             "Cube_default_speaker_setup.xml",
             SpatMode::mbap,
             tl::nullopt,
             true);

    spatTest(hybridTestName,
             hybridTestName,
             "default_project18(8X2-Subs2).xml",
             "default_speaker_setup.xml",
             "hybrid_256.xml",
             "default_speaker_setup.xml",
             SpatMode::hybrid,
             tl::nullopt,
             false);

    /*     spatTest("parallel hybrid",
                 hybridTestName,
                 "default_project18(8X2-Subs2).xml",
                 "default_speaker_setup.xml",
                 "hybrid_256.xml",
                 "default_speaker_setup.xml",
                 SpatMode::hybrid,
                 tl::nullopt,
                 true); */

    spatTest(hrtfTestName,
             hrtfTestName,
             "default_preset.xml",
             "BINAURAL_SPEAKER_SETUP.xml",
             "default_preset_256.xml",
             "BINAURAL_SPEAKER_SETUP.xml",
             SpatMode::vbap,
             StereoMode::hrtf,
             false);

    spatTest(stereoTestName,
             stereoTestName,
             "default_preset.xml",
             "STEREO_SPEAKER_SETUP.xml",
             "default_preset_256.xml",
             "STEREO_SPEAKER_SETUP.xml",
             SpatMode::vbap,
             StereoMode::stereo,
             false);
}
