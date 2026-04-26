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
#include "sg_AbstractSpatAlgorithm.hpp"
#include "sg_HybridSpatAlgorithm.hpp"
#include "sg_MbapSpatAlgorithm.hpp"
#include "sg_VbapSpatAlgorithm.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "juce_dsp/juce_dsp.h"
#include "juce_events/juce_events.h"
#include <Containers/sg_StaticMap.hpp>
#include <Containers/sg_StrongArray.hpp>
#include <Containers/sg_TaggedAudioBuffer.hpp>
#include <Data/StrongTypes/sg_OutputPatch.hpp>
#include <Data/StrongTypes/sg_SourceIndex.hpp>
#include <Data/sg_AudioStructs.hpp>
#include <Data/sg_LogicStrucs.hpp>
#include <Data/sg_Narrow.hpp>
#include <Data/sg_SpatMode.hpp>
#include <Data/sg_Triplet.hpp>
#include <Data/sg_constants.hpp>
#include <Utilities/ValueTreeUtilities.hpp>
#include <array>
#include <cstddef>
#include <memory>

namespace gris
{
//==============================================================================
HrtfSpatAlgorithm::HrtfSpatAlgorithm(SpeakerSetup const & speakerSetup,
                                     SpatMode const & projectSpatMode,
                                     SourcesData const & sources,
                                     double const sampleRate,
                                     int const bufferSize,
                                     BinauralSettings & binauralSettings)
    : mSpeakerSetup(speakerSetup)
    , mBufferSize(bufferSize)
    , mSofaFile(binauralSettings.lastSofaFile)
    , mNOrder(binauralSettings.ambisonicOrder)
    , mBinauralLowCpuMode(binauralSettings.lowCpuMode)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const displayError = [&](juce::String const & error) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Unable to process binaural stereo reduction.",
                                               error,
                                               "OK",
                                               nullptr,
                                               nullptr);
    };

    mAmbisonicData.speakersAudioConfig = speakerSetup.toAudioConfig(sampleRate);
    auto ambSpeakers = speakerSetup.ordering;
    ambSpeakers.sort();
    mAmbisonicData.speakersBuffer.init(ambSpeakers);

    auto const & binauralSpeakerData{ speakerSetup.speakers };
    switch (projectSpatMode) {
    case SpatMode::vbap:
        mInnerAlgorithm = std::make_unique<VbapSpatAlgorithm>(binauralSpeakerData, sources.getKeys());
        break;
    case SpatMode::mbap:
        mInnerAlgorithm = std::make_unique<MbapSpatAlgorithm>(speakerSetup, sources.getKeys());
        break;
    case SpatMode::hybrid:
        mInnerAlgorithm
            = std::make_unique<HybridSpatAlgorithm<MbapSpatAlgorithm, VbapSpatAlgorithm>>(speakerSetup,
                                                                                          sources.getKeys());
        break;
    case SpatMode::invalid:
        break;
    }

    jassert(mInnerAlgorithm);

    fixDirectOutsIntoPlace(sources, speakerSetup, projectSpatMode);

    mBFormatMain.Configure(mNOrder, true, bufferSize);
    mBFormatMain.Reset();
    // fadeTimeMilliSec of 0ms is OK because the speakers do not move and movement of source sound
    // is handled in the InnerAlgorithm process.
    auto encoderWorks{ mAmbEncoder.Configure(mNOrder, true, sampleRate, 0) };
    jassert(encoderWorks);
    mPosition.azimuth = 0;
    mPosition.elevation = 0;
    mPosition.distance = 1.f;
    mAmbEncoder.SetPosition(mPosition);
    mAmbEncoder.Reset();
    unsigned int tailLength = 0;
    // lowCpuMode : true means symmetric head (half left calculation + inverted phase for right
    // false means full calculation
    mAmbBinauralDecoderConfigured = mAmbDecoderBinaural.Configure(mNOrder,
                                                                  true,
                                                                  sampleRate,
                                                                  bufferSize,
                                                                  tailLength,
                                                                  mSofaFile.getFullPathName().toStdString(),
                                                                  mBinauralLowCpuMode);

    if (!mAmbBinauralDecoderConfigured) {
        if (mSofaFile.getFullPathName().compare("") == 0) {
            displayError("No SOFA file loaded.\nGo to File and Open SOFA file.");
        } else {
            displayError("Something is wrong with the selected file.\nPlease choose a valid SOFA file.");
        }
    }
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
                                juce::AudioBuffer<float> & stereoBuffer,
                                SourcePeaks const & sourcePeaks,
                                [[maybe_unused]] SpeakersAudioConfig const * altSpeakerConfig) noexcept NONBLOCKING
{
    ASSERT_AUDIO_THREAD;

    if (!mAmbBinauralDecoderConfigured) {
        return;
    }

    jassert(!altSpeakerConfig);
    jassert(stereoBuffer.getNumChannels() == 2);

    speakersBuffer.silence();

    auto & ambBuffer{ mAmbisonicData.speakersBuffer };
    ambBuffer.silence();
    mBFormatMain.Reset();

    if (mInnerAlgorithm)
        // we could use nullptr instead of mAmbisonicData.speakersAudioConfig here
        mInnerAlgorithm
            ->process(config, sourcesBuffer, ambBuffer, stereoBuffer, sourcePeaks, &mAmbisonicData.speakersAudioConfig);

    for (auto const & speaker : mSpeakerSetup.speakers) {
        // compute azimuth rotation 90 degrees counterclockwise
        float azi = speaker.value->position.getPolar().azimuth.getAsRadians() - (PI.get() / 2);
        while (azi <= -PI.get())
            azi += TWO_PI.get();
        while (azi > PI.get())
            azi -= TWO_PI.get();

        mPosition.azimuth = azi;
        mPosition.elevation = speaker.value->position.getPolar().elevation.getAsRadians();
        mPosition.distance = speaker.value->position.getPolar().length;
        mAmbEncoder.SetPosition(mPosition);

        gris::output_patch_t speakerId{ speaker.key };
        mAmbEncoder.ProcessAccumul(ambBuffer[speakerId].getWritePointer(0),
                                   sourcesBuffer.getNumSamples(),
                                   &mBFormatMain);
    }
    mAmbDecoderBinaural.Process(&mBFormatMain,
                                const_cast<float **>(stereoBuffer.getArrayOfWritePointers()),
                                mBufferSize);
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
                                                               int const bufferSize,
                                                               BinauralSettings & binauralSettings)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    return std::make_unique<HrtfSpatAlgorithm>(speakerSetup,
                                               projectSpatMode,
                                               sources,
                                               sampleRate,
                                               bufferSize,
                                               binauralSettings);
}

} // namespace gris
