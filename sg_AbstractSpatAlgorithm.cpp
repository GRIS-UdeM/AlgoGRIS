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
#if 1
        //init souce buffer
        SourceAudioBuffer sourceBuffer;
        juce::Array<source_index_t> sources;
        for (int i = 1; i <= MAX_NUM_SOURCES; ++i)
            sources.add(source_index_t{ i });
        sourceBuffer.init(sources);

        //init speaker buffer
        SpeakerAudioBuffer speakerBuffer;
        juce::Array<output_patch_t> speakers;
        for (int i = 1; i <= MAX_NUM_SPEAKERS; ++i)
            speakers.add(output_patch_t{ i });
        speakerBuffer.init(speakers);

        //init stereo buffer (for stereo reduction) and source peak level values
        juce::AudioBuffer<float> stereoBuffer{ 2, DEFAULT_BUFFER_SIZE };
        SourcePeaks sourcePeaks;

        beginTest("VBAP test");
        {
            //init project data and audio config
            SpatGrisData vbapData;
            vbapData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(DEFAULT_SPEAKER_SETUP_FILE));
            vbapData.project = *ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE));
            vbapData.project.spatMode = SpatMode::vbap;
            vbapData.appData.stereoMode = {};
            auto const vbapConfig{ vbapData.toAudioConfig() };

            auto vbapAlgorithm{ AbstractSpatAlgorithm::make(vbapData.speakerSetup,
                                                            vbapData.project.spatMode,
                                                            vbapData.appData.stereoMode,
                                                            vbapData.project.sources,
                                                            vbapData.appData.audioSettings.sampleRate,
                                                            vbapData.appData.audioSettings.bufferSize) };

            vbapAlgorithm->process(*vbapConfig, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);

            //here we expect things not to crash and be fast lol
            //expect(false);
            //expectEquals(editor.getText().getIntValue(), 12);
        }

        beginTest("HRTF test");
        {
            SpatGrisData hrtfData;

            hrtfData.speakerSetup = *SpeakerSetup::fromXml(*parseXML(BINAURAL_SPEAKER_SETUP_FILE));
            hrtfData.project = *ProjectData::fromXml(*parseXML(DEFAULT_PROJECT_FILE));
            hrtfData.project.spatMode = SpatMode::vbap;
            hrtfData.appData.stereoMode = StereoMode::hrtf;
            auto const hrtfConfig{ hrtfData.toAudioConfig() };

            auto hrtfAlgorithm{ AbstractSpatAlgorithm::make(hrtfData.speakerSetup,
                                                            hrtfData.project.spatMode,
                                                            hrtfData.appData.stereoMode,
                                                            hrtfData.project.sources,
                                                            hrtfData.appData.audioSettings.sampleRate,
                                                            hrtfData.appData.audioSettings.bufferSize) };

            hrtfAlgorithm->process(*hrtfConfig, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);

            //here we expect things not to crash and be fast lol
            //expect(false);
            //expectEquals(editor.getText().getIntValue(), 12);
        }
#endif
    }
};

#if JUCE_DEBUG
// This will automatically create an instance of the test class and add it to the list of tests to be run.
static AbstractSpatAlgorithmTest abstractSpatAlgorithmTest;
#endif

} // namespace gris
