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

#include "sg_HrtfSpatAlgorithm.hpp"
#include "Containers/sg_StaticMap.hpp"
#include "Containers/sg_StrongArray.hpp"
#include "Containers/sg_TaggedAudioBuffer.hpp"
#include "Data/StrongTypes/sg_OutputPatch.hpp"
#include "Data/StrongTypes/sg_SourceIndex.hpp"
#include "Data/sg_AudioStructs.hpp"
#include "Data/sg_LogicStrucs.hpp"
#include "Data/sg_Narrow.hpp"
#include "Data/sg_SpatMode.hpp"
#include "Data/sg_Triplet.hpp"
#include "Data/sg_constants.hpp"
#include "sg_AbstractSpatAlgorithm.hpp"
#include "sg_HybridSpatAlgorithm.hpp"
#include "sg_MbapSpatAlgorithm.hpp"
#include "sg_VbapSpatAlgorithm.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "juce_dsp/juce_dsp.h"
#include "juce_events/juce_events.h"
#include <array>
#include <cstddef>
#include <memory>
#include "StructGRIS/ValueTreeUtilities.hpp"

namespace gris
{
//==============================================================================
HrtfSpatAlgorithm::HrtfSpatAlgorithm(SpeakerSetup const & speakerSetup,
                                     SpatMode const & projectSpatMode,
                                     SourcesData const & sources,
                                     double const sampleRate,
                                     int const bufferSize)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    static auto const hrtfDir{ getHrtfDirectory() };
    if (!hrtfDir.exists()) {
        jassertfalse;
        return;
    }
    static auto const HRTF_FOLDER_0{ hrtfDir.getChildFile("elev0") };
    static auto const HRTF_FOLDER_40{ hrtfDir.getChildFile("elev40") };
    static auto const HRTF_FOLDER_80{ hrtfDir.getChildFile("elev80") };

    static juce::StringArray const NAMES{ "H0e025a.wav",  "H0e020a.wav",  "H0e065a.wav",  "H0e110a.wav",
                                          "H0e155a.wav",  "H0e160a.wav",  "H0e115a.wav",  "H0e070a.wav",
                                          "H40e032a.wav", "H40e026a.wav", "H40e084a.wav", "H40e148a.wav",
                                          "H40e154a.wav", "H40e090a.wav", "H80e090a.wav", "H80e090a.wav" };

    static auto const GET_HRTF_IR_FILE = [](int const speaker) {
        jassert(juce::isPositiveAndBelow(speaker, NAMES.size()));

        auto const & name{ NAMES[speaker] };
        if (speaker < 8) {
            return HRTF_FOLDER_0.getChildFile(name);
        }

        if (speaker < 14) {
            return HRTF_FOLDER_40.getChildFile(name);
        }

        return HRTF_FOLDER_80.getChildFile(name);
    };

    static auto const GET_HRTF_IR_FILES = []() {
        juce::Array<juce::File> files{};
        for (int i{}; i < NAMES.size(); ++i) {
            files.add(GET_HRTF_IR_FILE(i));
        }

        return files;
    };

    static auto const FILES = GET_HRTF_IR_FILES();

    // Init inner spat algorithm
    auto const hrtfSpeakerSetupFile{ getHrtfDirectory().getSiblingFile("tests/util/BINAURAL_SPEAKER_SETUP.xml") };
    if (!hrtfSpeakerSetupFile.existsAsFile()) {
        jassertfalse;
        return;
    }

    auto const binauralXml{ juce::XmlDocument{ hrtfSpeakerSetupFile }.getDocumentElement() };
    if (!binauralXml) {
        jassertfalse;
        return;
    }

    auto const binauralSpeakerSetup{ SpeakerSetup::fromXml(*binauralXml) };
    if (!binauralSpeakerSetup) {
        jassertfalse;
        return;
    }

    mHrtfData.speakersAudioConfig
        = binauralSpeakerSetup->toAudioConfig(44100.0); // TODO: find a way to update this number!
    auto speakers = binauralSpeakerSetup->ordering;

    speakers.sort();
    mHrtfData.speakersBuffer.init(speakers);

#if USE_FORK_UNION
    speakerIds = mHrtfData.speakersAudioConfig.getKeyVector();
#endif

    auto const & binauralSpeakerData{ binauralSpeakerSetup->speakers };

    switch (projectSpatMode) {
    case SpatMode::vbap:
        mInnerAlgorithm = std::make_unique<VbapSpatAlgorithm>(binauralSpeakerData, sources.getKeys());
        break;
    case SpatMode::mbap:
        mInnerAlgorithm = std::make_unique<MbapSpatAlgorithm>(*binauralSpeakerSetup, sources.getKeys());
        break;
    case SpatMode::hybrid:
        mInnerAlgorithm = std::make_unique<HybridSpatAlgorithm>(*binauralSpeakerSetup, sources.getKeys());
        break;
    case SpatMode::invalid:
        break;
    }

    jassert(mInnerAlgorithm);

    // load IRs
    for (int i{}; i < 16; ++i) {
        mConvolutions[narrow<std::size_t>(i)].loadImpulseResponse(FILES[i],
                                                                  juce::dsp::Convolution::Stereo::yes,
                                                                  juce::dsp::Convolution::Trim::no,
                                                                  0,
                                                                  juce::dsp::Convolution::Normalise::no);
    }

    juce::dsp::ProcessSpec const spec{ sampleRate, narrow<juce::uint32>(bufferSize), 2 };
    for (auto & convolution : mConvolutions) {
        convolution.prepare(spec);
        convolution.reset();
    }

    convolutionBuffer.setSize(2, bufferSize);

    fixDirectOutsIntoPlace(sources, speakerSetup, projectSpatMode);
}

//==============================================================================
void HrtfSpatAlgorithm::updateSpatData(source_index_t const sourceIndex, SourceData const & sourceData) noexcept
{
    ASSERT_NOT_AUDIO_THREAD;

    if (sourceData.directOut) {
        return;
    }

    if (mInnerAlgorithm)
        mInnerAlgorithm->updateSpatData(sourceIndex, sourceData);
}

//==============================================================================
void HrtfSpatAlgorithm::process(AudioConfig const & config,
                                SourceAudioBuffer & sourcesBuffer,
                                SpeakerAudioBuffer & speakersBuffer,
#if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                                ForkUnionBuffer & forkUnionBuffer,
#endif
                                juce::AudioBuffer<float> & stereoBuffer,
                                SourcePeaks const & sourcePeaks,
                                [[maybe_unused]] SpeakersAudioConfig const * altSpeakerConfig)
{
    ASSERT_AUDIO_THREAD;
    jassert(!altSpeakerConfig);
    jassert(stereoBuffer.getNumChannels() == 2);

    speakersBuffer.silence();

    auto & hrtfBuffer{ mHrtfData.speakersBuffer };
    jassert(hrtfBuffer.size() == 16);
    hrtfBuffer.silence();

    if (mInnerAlgorithm)
        mInnerAlgorithm->process(config,
                                 sourcesBuffer,
                                 hrtfBuffer,
#if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                                 forkUnionBuffer,
#endif
                                 stereoBuffer,
                                 sourcePeaks,
                                 &mHrtfData.speakersAudioConfig);

    convolutionBuffer.clear();

// TODO: these 2 modes need to be implemented here. Using these modes will work, but will be slower than ideal
#if 0 // USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
    jassert(speakerIds.size() > 0);

    ashvardanian::fork_union::for_n(threadPool, speakerIds.size(), [&](std::size_t i) noexcept {
        processSpeaker((int)i, speakerIds[(int)i], sourcesBuffer, stereoBuffer);
    });
#else
    int i = 0;
    for (auto const & speaker : mHrtfData.speakersAudioConfig) {
        processSpeaker(i++, speaker.key, sourcesBuffer, stereoBuffer);
    }
#endif
}

//==============================================================================
inline void HrtfSpatAlgorithm::processSpeaker(int speakerIndex,
                                              const gris::output_patch_t & speakerId,
                                              gris::SourceAudioBuffer & sourcesBuffer,
                                              juce::AudioBuffer<float> & stereoBuffer)
{
    auto const numSamples{ sourcesBuffer.getNumSamples() };
    auto & hrtfBuffer{ mHrtfData.speakersBuffer };
    auto const magnitude{ hrtfBuffer[speakerId].getMagnitude(0, numSamples) };
    auto & hadSoundLastBlock{ mHrtfData.hadSoundLastBlock[speakerId] };

    // We can skip the speaker if the gain is small enough, but we have to perform one last block so that the
    // convolution's inner state stays coherent.
    if (magnitude <= SMALL_GAIN) {
        if (!hadSoundLastBlock) {
            return;
        }
        hadSoundLastBlock = false;
    } else {
        hadSoundLastBlock = true;
    }

    jassert(convolutionBuffer.getNumSamples() == numSamples);
    convolutionBuffer.copyFrom(0, 0, hrtfBuffer[speakerId], 0, 0, numSamples);
    convolutionBuffer.copyFrom(1, 0, hrtfBuffer[speakerId], 0, 0, numSamples);
    juce::dsp::AudioBlock<float> block{ convolutionBuffer };
    juce::dsp::ProcessContextReplacing<float> const context{ block };
    mConvolutions[speakerIndex].process(context);

    static constexpr std::array<bool, 16> REVERSE{ true, false, false, false, false, true, true, true,
                                                   true, false, false, false, true,  true, true, false };

    if (!REVERSE[speakerIndex]) {
        stereoBuffer.addFrom(0, 0, convolutionBuffer, 0, 0, numSamples);
        stereoBuffer.addFrom(1, 0, convolutionBuffer, 1, 0, numSamples);
    } else {
        stereoBuffer.addFrom(0, 0, convolutionBuffer, 1, 0, numSamples);
        stereoBuffer.addFrom(1, 0, convolutionBuffer, 0, 0, numSamples);
    }
}

//==============================================================================
juce::Array<Triplet> HrtfSpatAlgorithm::getTriplets() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassertfalse;
    return juce::Array<Triplet>{};
}

//==============================================================================
std::unique_ptr<AbstractSpatAlgorithm> HrtfSpatAlgorithm::make(SpeakerSetup const & speakerSetup,
                                                               SpatMode const & projectSpatMode,
                                                               SourcesData const & sources,
                                                               double const sampleRate,
                                                               int const bufferSize)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    return std::make_unique<HrtfSpatAlgorithm>(speakerSetup, projectSpatMode, sources, sampleRate, bufferSize);
}

} // namespace gris
