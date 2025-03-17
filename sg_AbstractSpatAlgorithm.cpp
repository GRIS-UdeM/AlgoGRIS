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

namespace gris
{
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
    return !isOscThread() && !juce::MessageManager::getInstance()->isThisTheMessageThread();
}

//==============================================================================
void AbstractSpatAlgorithm::fixDirectOutsIntoPlace(SourcesData const & sources,
                                                   SpeakerSetup const & speakerSetup,
                                                   SpatMode const & projectSpatMode) noexcept
{
#if JUCE_DEBUG
    juce::UnitTestRunner testRunner;
    testRunner.runAllTests();
#endif

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


// Unit test for NumberRangeInputFilter
class AbstractSpatAlgorithmTest : public juce::UnitTest
{
public:
    AbstractSpatAlgorithmTest() : juce::UnitTest("AbstractSpatAlgorithmTest") {}

    void runTest() override
    {
        auto const defaultProject { ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE)) };
        auto const defaultSpeakerSetup{ SpeakerSetup::fromXml(*parseXML(DEFAULT_SPEAKER_SETUP_FILE)) };
        auto const binauralSpeakerSetup{ SpeakerSetup::fromXml(*parseXML(BINAURAL_SPEAKER_SETUP_FILE)) };

        AudioConfig config;
        SourceAudioBuffer sourceBuffer;
        SpeakerAudioBuffer speakerBuffer;
        juce::AudioBuffer<float> stereoBuffer;
        SourcePeaks sourcePeaks;

        beginTest("VBAP test");
        {
            auto vbapAlgorithm = AbstractSpatAlgorithm::make(*defaultSpeakerSetup,
                                                             SpatMode::vbap,
                                                             {},
                                                             defaultProject->sources,
                                                             DEFAULT_SAMPLE_RATE,
                                                             DEFAULT_BUFFER_SIZE);

            vbapAlgorithm->process(config, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);

            //here we expect things not to crash and be fast lol
            //expect(false);
            //expectEquals(editor.getText().getIntValue(), 12);
        }

        beginTest("Binaural test");
        {
            auto hrtfAlgorithm = AbstractSpatAlgorithm::make(*binauralSpeakerSetup,
                                                             SpatMode::invalid,
                                                             StereoMode::hrtf,
                                                             defaultProject->sources,
                                                             DEFAULT_SAMPLE_RATE,
                                                             DEFAULT_BUFFER_SIZE);

            hrtfAlgorithm->process(config, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);

            //here we expect things not to crash and be fast lol
            //expect(false);
            //expectEquals(editor.getText().getIntValue(), 12);
        }

    }
};

#if JUCE_DEBUG
// This will automatically create an instance of the test class and add it to the list of tests to be run.
static AbstractSpatAlgorithmTest abstractSpatAlgorithmTest;
#endif


} // namespace gris
