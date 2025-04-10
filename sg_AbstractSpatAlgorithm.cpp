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

//TODO VB: need to move to algogris
#include "../../Source/sg_PinkNoiseGenerator.hpp"
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
class AbstractSpatAlgorithmTest : public juce::UnitTest
{
    float constexpr static testDurationSeconds{ .1f };
    //std::vector<int> const bufferSizes{ /*1,*/ 512, 1024, SourceAudioBuffer::MAX_NUM_SAMPLES };
    std::vector<int> const bufferSizes{ 1024 };

    SourceAudioBuffer sourceBuffers;
    SpeakerAudioBuffer speakerBuffers;
    juce::AudioBuffer<float> stereoBuffer;
    SourcePeaks sourcePeaks;

public:
    bool static isRunning;

    AbstractSpatAlgorithmTest() : juce::UnitTest("AbstractSpatAlgorithmTest") {}

    void initialise() override { isRunning = true; }

    void checkSourceBufferValidity(const SourceAudioBuffer & buffer)
    {
        for (auto const & source : buffer) {
            juce::String output = "Source " + juce::String(source.key.get()) + ": ";
            auto const * sourceBuffer = source.value->getReadPointer(0);

            for (int sampleNumber = 0; sampleNumber < buffer.getNumSamples(); ++sampleNumber) {
                const auto sampleValue = sourceBuffer[sampleNumber];

                output += "Sample " + juce::String(sampleNumber) + ": " + juce::String(sampleValue) + " ";
                expect(std::isfinite(sampleValue), "Output contains NaN or Inf values!");
                expect(sampleValue >= -1.0f && sampleValue <= 1.0f, "Output exceeds valid range!");
            }

            //DBG(output);
        }
    }

    void checkSpeakerBufferValidity(const SpeakerAudioBuffer & buffer)
    {
        for (auto const & speaker : buffer)
        {
            juce::String output = "Speaker " + juce::String(speaker.key.get()) + ": ";
            auto const * speakerBuffer = speaker.value->getReadPointer(0);

            for (int sampleNumber = 0; sampleNumber < buffer.getNumSamples(); ++sampleNumber)
            {
                const auto sampleValue = speakerBuffer[sampleNumber];

                output += "Sample " + juce::String(sampleNumber) + ": " + juce::String(sampleValue) + " ";
                expect(std::isfinite(sampleValue), "Output contains NaN or Inf values!");
                expect(sampleValue >= -MAX_SAMPLE_VALUE && sampleValue <= MAX_SAMPLE_VALUE,  "Output " + juce::String (sampleValue) + " exceeds valid range!");
            }

            //DBG(output);
        }
    }

    void initBuffers(const int bufferSize, const size_t numSources, const size_t numSpeakers)
    {
        //construct the arrays of indices, because why use ints
        juce::Array<source_index_t> sourcesIndices;
        for (int i = 1; i <= numSources; ++i)
            sourcesIndices.add(source_index_t{ i });

        juce::Array<output_patch_t> speakerIndices;
        for (int i = 1; i <= numSpeakers; ++i)
            speakerIndices.add(output_patch_t{ i });

        // init source buffers,  fill 'em with pink noise, and calculate the peaks
        sourceBuffers.init(sourcesIndices);
        sourceBuffers.setNumSamples(bufferSize);

        //TODO VB: this should probably be done on each loop?
        for (auto source : sourcesIndices) {
            fillWithPinkNoise(sourceBuffers[source].getArrayOfWritePointers(), bufferSize, 1, .5f);
            sourcePeaks[source] = sourceBuffers[source].getMagnitude(0, bufferSize);
        }

        // init speaker buffers
        speakerBuffers.init(speakerIndices);
        speakerBuffers.setNumSamples(bufferSize);

        // init stereo buffer
        stereoBuffer.setSize(2, bufferSize);
        stereoBuffer.clear();
    }

    /** this is called once per buffer, to prepare the source data for the processing loop. */
    void updateSourceData(AbstractSpatAlgorithm * algo, SpatGrisData & data)
    {
#if 1
        const auto numSources{ data.project.sources.size() };
        const auto numRings{ 3 };
#else
        const auto numSources{ 1 };
        const auto numRings{ 1 };
#endif

        const auto numSourcesPerRing{ numSources/numRings };
        const auto elevSteps{ HALF_PI.get() / numRings };
        const auto azimSteps{ TWO_PI.get() / numSourcesPerRing };
        auto curRing{ 0 };
        auto curAzimuth{ 0.f };

        const std::array<CartesianVector, 18> positions{ {
            { -0.714628, 0.384052, 0.584647 },
            { -0.662694, 0.436887, 0.608249 },
            { -0.607971, 0.489342, 0.625232 },
            { -0.554899, 0.514906, 0.653421 },
            { -0.508563, 0.538774, 0.67163 },
            { -0.462055, 0.553451, 0.692963 },
            { -0.357304, 0.564436, 0.744141 },
            { -0.267278, 0.57124, 0.776046 },
            { -0.195697, 0.565936, 0.800886 },
            { -0.101748, 0.54087, 0.83493 },
            { -0.0162338, 0.503246, 0.863991 },
            { 0.0491514, 0.453285, 0.890009 },
            { 0.148419, 0.390287, 0.908652 },
            {0.292823, 0.248623, 0.923277 },
            { 0.380084, 0.137711, 0.914643 },
            { 0.432869, 0.0602729, 0.89944 },
            { 0.47356, -0.0163296, 0.88061 },
            { 0.512228, -0.0916619, 0.853944 },
        } }; 

        for (int i = 1; i <= numSources; ++i) {
            const auto sourceIndex{ source_index_t{ i } };
            auto source{ data.project.sources[sourceIndex] };

#if 1
            source.position = positions[i-1];
            //source.position = CartesianVector {1.f, 1.f, 1.f};
            //THIS ONE IS FINE, LOCATION TAKEN DIRECTION FROM IRL RUN
            //computeGains: (-3.08809e-08, 0.706473, 0.70774); polar: PolarVector(azimuth: 1.5708, elevation: 0.786294, length: 1)
            //source.position = CartesianVector{ 0.f, 0.706473f, 0.70774f };
            DBG(source.position->toString());
#else
            source.position = {};
#endif
            //DBG(source.position->toString());
            curAzimuth += azimSteps;

            algo->updateSpatData(sourceIndex, source);

            if (curRing < numRings && i % numSourcesPerRing == 0) {
                ++curRing;
                curAzimuth = 0;
            }
        }
    }

    void runTest() override
    {
        beginTest("VBAP test");
        {
            // init project data and audio config
            SpatGrisData vbapData;
#if 0
            vbapData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(DEFAULT_SPEAKER_SETUP_FILE));
#else
            //vbapData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(juce::File("C:/Users/barth/Documents/git/sat/GRIS/SpatGRIS/Resources/templates/Speaker setups/DOME/Dome_default_speaker_setup.xml")));
            vbapData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(juce::File("C:/Users/barth/Documents/git/sat/GRIS/SpatGRIS/Resources/templates/Speaker setups/DOME/Dome4(4)Subs1 Quad.xml")));
            
#endif
            vbapData.project = *ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE));
            vbapData.project.spatMode = SpatMode::vbap;
            vbapData.appData.stereoMode = {};

            // the default project and speaker setups have 18 sources and 18 speakers, so we should only be using these
            auto vbapConfig{ vbapData.toAudioConfig() };
            vbapConfig->spatGainsInterpolation = 0.f;
            const auto numSources{ vbapConfig->sourcesAudioConfig.size() };
            const auto numSpeakers{ vbapConfig->speakersAudioConfig.size() };

            for (int bufferSize : bufferSizes) {
                vbapData.appData.audioSettings.bufferSize = bufferSize;

                initBuffers(bufferSize, numSources, numSpeakers);

                auto vbapAlgo{ AbstractSpatAlgorithm::make(vbapData.speakerSetup,
                                                           vbapData.project.spatMode,
                                                           vbapData.appData.stereoMode,
                                                           vbapData.project.sources,
                                                           vbapData.appData.audioSettings.sampleRate,
                                                           vbapData.appData.audioSettings.bufferSize) };

                updateSourceData(vbapAlgo.get(), vbapData);

                auto const numLoops{ static_cast<int>(DEFAULT_SAMPLE_RATE * testDurationSeconds / bufferSize) };
                for (int i = 0; i < numLoops; ++i) {
                    //DBG("loop: " + juce::String(i));
                    checkSourceBufferValidity(sourceBuffers);

                    speakerBuffers.silence();
                    vbapAlgo->process(*vbapConfig, sourceBuffers, speakerBuffers, stereoBuffer, sourcePeaks, nullptr);

                    checkSpeakerBufferValidity(speakerBuffers);
                }
            }
        }

        //beginTest("HRTF test");
        //{
        //    SpatGrisData hrtfData;
        //    hrtfData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(BINAURAL_SPEAKER_SETUP_FILE));
        //    hrtfData.project = *ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE));
        //    hrtfData.project.spatMode = SpatMode::vbap;
        //    hrtfData.appData.stereoMode = StereoMode::hrtf;
        //    auto const hrtfConfig{ hrtfData.toAudioConfig() };
        //    const auto numSources{ hrtfConfig->sourcesAudioConfig.size() };
        //    const auto numSpeakers{ hrtfConfig->speakersAudioConfig.size() };

        //    for (int bufferSize : bufferSizes) {
        //        hrtfData.appData.audioSettings.bufferSize = bufferSize;
        //        initBuffers(bufferSize, numSources, numSpeakers);

        //        auto hrtfAlgo{ AbstractSpatAlgorithm::make(hrtfData.speakerSetup,
        //                                                   hrtfData.project.spatMode,
        //                                                   hrtfData.appData.stereoMode,
        //                                                   hrtfData.project.sources,
        //                                                   hrtfData.appData.audioSettings.sampleRate,
        //                                                   hrtfData.appData.audioSettings.bufferSize) };
        //        updateSourceData(hrtfAlgo.get(), hrtfData);

        //        auto const numLoops{ static_cast<int>(DEFAULT_SAMPLE_RATE * testDurationSeconds / bufferSize) };
        //        for (int i = 0; i < numLoops; ++i) {
        //            checkSourceBufferValidity(sourceBuffers);
        //            hrtfAlgo->process(*hrtfConfig, sourceBuffers, speakerBuffers, stereoBuffer, sourcePeaks, nullptr);
        //            checkSpeakerBufferValidity(speakerBuffers);
        //        }
        //    }
        //}
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
