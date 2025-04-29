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
#include "Data/sg_LogicStrucs.hpp"
#include "Data/sg_SpatMode.hpp"
#include "sg_PinkNoiseGenerator.hpp"
#include "sg_HrtfSpatAlgorithm.hpp"
#include "sg_HybridSpatAlgorithm.hpp"
#include "sg_MbapSpatAlgorithm.hpp"
#include "sg_StereoSpatAlgorithm.hpp"
#include "sg_VbapSpatAlgorithm.hpp"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "juce_events/juce_events.h"
#include "tl/optional.hpp"
#include <memory>

#ifdef USE_DOPPLER
    #include "sg_DopplerSpatAlgorithm.hpp"
#endif

namespace gris
{
//<<<<<<< HEAD
//=======
//class AbstractSpatAlgorithmTest : public juce::UnitTest
//{
//    float constexpr static testDurationSeconds{ .1f };
//    std::vector<int> const bufferSizes{ 1, 512, 1024, SourceAudioBuffer::MAX_NUM_SAMPLES };
//
//    SourceAudioBuffer sourceBuffers;
//    SpeakerAudioBuffer speakerBuffers;
//    juce::AudioBuffer<float> stereoBuffer;
//    SourcePeaks sourcePeaks;
//
//public:
//    bool static isRunning;
//
//    AbstractSpatAlgorithmTest() : juce::UnitTest("AbstractSpatAlgorithmTest") {}
//
//    void initialise() override { isRunning = true; }
//
//    void checkSourceBufferValidity(const SourceAudioBuffer & buffer)
//    {
//        for (auto const & source : buffer) {
//            juce::String output = "Source " + juce::String(source.key.get()) + ": ";
//            auto const * sourceBuffer = source.value->getReadPointer(0);
//
//            for (int sampleNumber = 0; sampleNumber < buffer.getNumSamples(); ++sampleNumber) {
//                const auto sampleValue = sourceBuffer[sampleNumber];
//
//                output += "Sample " + juce::String(sampleNumber) + ": " + juce::String(sampleValue) + " ";
//                expect(std::isfinite(sampleValue), "Output contains NaN or Inf values!");
//                expect(sampleValue >= -1.0f && sampleValue <= 1.0f, "Output exceeds valid range!");
//            }
//
//            //DBG(output);
//        }
//    }
//
//    void checkSpeakerBufferValidity(const SpeakerAudioBuffer & buffer)
//    {
//        for (auto const & speaker : buffer)
//        {
//            juce::String output = "Speaker " + juce::String(speaker.key.get()) + ": ";
//            auto const * speakerBuffer = speaker.value->getReadPointer(0);
//
//            for (int sampleNumber = 0; sampleNumber < buffer.getNumSamples(); ++sampleNumber)
//            {
//                const auto sampleValue = speakerBuffer[sampleNumber];
//
//                output += "Sample " + juce::String(sampleNumber) + ": " + juce::String(sampleValue) + " ";
//                expect(std::isfinite(sampleValue), "Output contains NaN or Inf values!");
//                expect(sampleValue >= -1.f && sampleValue <= 1.f,  "Output " + juce::String (sampleValue) + " exceeds valid range!");
//            }
//
//            //DBG(output);
//        }
//    }
//
//    void initBuffers(const int bufferSize, const size_t numSources, const size_t numSpeakers)
//    {
//        //construct the arrays of indices
//        juce::Array<source_index_t> sourcesIndices;
//        for (int i = 1; i <= numSources; ++i)
//            sourcesIndices.add(source_index_t{ i });
//
//        juce::Array<output_patch_t> speakerIndices;
//        for (int i = 1; i <= numSpeakers; ++i)
//            speakerIndices.add(output_patch_t{ i });
//
//        // init source buffers
//        sourceBuffers.init(sourcesIndices);
//        sourceBuffers.setNumSamples(bufferSize);
//
//        // init speaker buffers
//        speakerBuffers.init(speakerIndices);
//        speakerBuffers.setNumSamples(bufferSize);
//
//        // init stereo buffer
//        stereoBuffer.setSize(2, bufferSize);
//        stereoBuffer.clear();
//    }
//
//    /** fill Source Buffers with pink noise, and calculate the peaks */
//    void fillSourceBuffersWithNoise (const size_t numSources, const int bufferSize)
//    {
//        sourceBuffers.silence();
//        for (int i = 1; i <= numSources; ++i)
//        {
//            const auto sourceIndex { source_index_t{ i } };
//            fillWithPinkNoise (sourceBuffers[sourceIndex].getArrayOfWritePointers (), bufferSize, 1, .5f);
//            sourcePeaks[sourceIndex] = sourceBuffers[sourceIndex].getMagnitude (0, bufferSize);
//        }
//    }
//
//    void positionSources(AbstractSpatAlgorithm * algo, SpatGrisData & data)
//    {
//        const auto numSources{ data.project.sources.size() };
//        const auto numRings{ 3 };
//        const auto numSourcesPerRing{ numSources/numRings };
//        const auto elevSteps{ HALF_PI.get() / numRings };
//        const auto azimSteps{ TWO_PI.get() / numSourcesPerRing };
//        auto curRing{ 0 };
//        auto curAzimuth{ 0.f };
//
//        for (int i = 1; i <= numSources; ++i) {
//            const auto sourceIndex{ source_index_t{ i } };
//            auto source{ data.project.sources[sourceIndex] };
//
//            source.position = PolarVector(radians_t {curAzimuth}, radians_t { curRing * elevSteps}, 1.f);
//            curAzimuth += azimSteps;
//
//            algo->updateSpatData(sourceIndex, source);
//
//            if (curRing < numRings && i % numSourcesPerRing == 0) {
//                ++curRing;
//                curAzimuth = 0;
//            }
//        }
//    }
//
//    void testUsingProjectData (gris::SpatGrisData& data)
//    {
//        const auto config { data.toAudioConfig () };
//        const auto numSources { config->sourcesAudioConfig.size () };
//        const auto numSpeakers { config->speakersAudioConfig.size () };
//
//        //for every test buffer size
//        for (int bufferSize : bufferSizes) {
//            data.appData.audioSettings.bufferSize = bufferSize;
//
//            //init our buffers
//            initBuffers (bufferSize, numSources, numSpeakers);
//
//            //create our spatialization algorithm
//            auto algo{ AbstractSpatAlgorithm::make(data.speakerSetup,
//                                                   data.project.spatMode,
//                                                   data.appData.stereoMode,
//                                                   data.project.sources,
//                                                   data.appData.audioSettings.sampleRate,
//                                                   data.appData.audioSettings.bufferSize) };
//
//            //position the sound sources
//            positionSources (algo.get (), data);
//
//            //now simulate processing an audio loop of testDurationSeconds
//            auto const numLoops { static_cast<int>(DEFAULT_SAMPLE_RATE * testDurationSeconds / bufferSize) };
//            for (int i = 0; i < numLoops; ++i) {
//                // fill the source buffers with pink noise
//                fillSourceBuffersWithNoise (numSources, bufferSize);
//                checkSourceBufferValidity (sourceBuffers);
//
//                //process the audio
//                speakerBuffers.silence ();
//                stereoBuffer.clear ();
//                algo->process (*config, sourceBuffers, speakerBuffers, stereoBuffer, sourcePeaks, nullptr);
//
//                //check that the audio output is valid
//                checkSpeakerBufferValidity (speakerBuffers);
//            }
//        }
//    }
//
//
//    void runTest() override
//    {
//        beginTest("VBAP test");
//        {
//            // init project data and audio config
//            SpatGrisData vbapData;
//            vbapData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(DEFAULT_SPEAKER_SETUP_FILE));
//            vbapData.project = *ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE));
//            vbapData.project.spatMode = SpatMode::vbap;
//            vbapData.appData.stereoMode = {};
//
//            testUsingProjectData (vbapData);
//        }
//
//        beginTest("HRTF test");
//        {
//            SpatGrisData hrtfData;
//            hrtfData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(BINAURAL_SPEAKER_SETUP_FILE));
//            hrtfData.project = *ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE));
//            hrtfData.project.spatMode = SpatMode::vbap;
//            hrtfData.appData.stereoMode = StereoMode::hrtf;
//
//            testUsingProjectData (hrtfData);
//        }
//
//        beginTest ("MBAP test");
//        {
//            SpatGrisData mbapData;
//            mbapData.project = *ProjectData::fromXml (*juce::parseXML (DEFAULT_CUBE_PROJECT));
//            mbapData.speakerSetup = *SpeakerSetup::fromXml (*parseXML (DEFAULT_CUBE_SPEAKER_SETUP));
//            mbapData.project.spatMode = SpatMode::mbap;
//            mbapData.appData.stereoMode = {};
//
//            testUsingProjectData (mbapData);
//        }
//    }
//
//    void shutdown() override { isRunning = false; }
//};
//
//bool AbstractSpatAlgorithmTest::isRunning = false;
//
//>>>>>>> 9b3f40a (Add proper audio unit testing)
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
    return (!isOscThread() && !juce::MessageManager::getInstance()->isThisTheMessageThread());
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
} // namespace gris
