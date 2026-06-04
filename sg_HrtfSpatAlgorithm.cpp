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
    , mSampleRate(sampleRate)
    , mSofaFile(binauralSettings.lastSofaFile)
    , mBinauralRenderer(binauralSettings.renderer)
    , mUseDefaultHRIRs(binauralSettings.useDefaultHRIRs)
    , mEnableHRIRsDiffuseEQ(binauralSettings.enableHRIRsDiffuseEQ)
    , mNOrder(binauralSettings.ambisonicOrder)
    , mBinauralLowCpuMode(binauralSettings.lowCpuMode)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    switch (projectSpatMode) {
    case SpatMode::vbap:
        mInnerAlgorithm = VbapSpatAlgorithm::make(speakerSetup, sources.getKeys());
        break;
    case SpatMode::mbap:
        mInnerAlgorithm = MbapSpatAlgorithm::make(speakerSetup, sources.getKeys());
        break;
    case SpatMode::hybrid:
        mInnerAlgorithm
            = HybridSpatAlgorithm<MbapSpatAlgorithm, VbapSpatAlgorithm>::make(speakerSetup, sources.getKeys());
        break;
    case SpatMode::invalid:
        break;
    }

    jassert(mInnerAlgorithm);

    fixDirectOutsIntoPlace(sources, speakerSetup, projectSpatMode);

    mHRTFData.speakersAudioConfig = speakerSetup.toAudioConfig(sampleRate);
    auto hrtfSpeakers = speakerSetup.ordering;
    hrtfSpeakers.sort();
    mHRTFData.speakersBuffer.init(hrtfSpeakers);
    mHRTFData.speakersBuffer.setNumSamples(mBufferSize);

    if (mBinauralRenderer == BinauralRenderer::saf) {
        configureSAF();
    } else {
        configureLibspatialaudio();
    }
}

//==============================================================================
HrtfSpatAlgorithm::~HrtfSpatAlgorithm()
{
    if (mBinauralRenderer == BinauralRenderer::saf) {
        stopTimer();
        binauraliserNF_destroy(&mSafFirstHBin);
        if (mUseSecondSafHBin) {
            binauraliserNF_destroy(&mSafSecondHBin);
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

    jassert(!altSpeakerConfig);
    jassert(stereoBuffer.getNumChannels() == 2);

    speakersBuffer.silence();

    auto & hrtfBuffer{ mHRTFData.speakersBuffer };
    hrtfBuffer.silence();

    if (mInnerAlgorithm)
        mInnerAlgorithm->process(config,
                                 sourcesBuffer,
                                 /*hrtfBuffer*/ speakersBuffer,
                                 stereoBuffer,
                                 sourcePeaks,
                                 /*&mHRTFData.speakersAudioConfig*/ altSpeakerConfig);

    if (mBinauralRenderer == BinauralRenderer::saf) {
        if (!mSAFConfigureNeeded.get() && binauraliser_getCodecStatus(mSafFirstHBin) == CODEC_STATUS_INITIALISED
            && (mUseSecondSafHBin ? binauraliser_getCodecStatus(mSafSecondHBin) == CODEC_STATUS_INITIALISED : true)) {
            mFirstStereoBuffer.clear();
            mSecondStereoBuffer.clear();

            const int numSamples = /*hrtfBuffer*/ speakersBuffer.getNumSamples();
            jassert(numSamples == sourcesBuffer.getNumSamples());

            if (mUsingLowDelay) {
                const int numFrames = numSamples / mFrameSize;
                auto bufferPtrs = /*hrtfBuffer*/ speakersBuffer.getArrayOfWritePointers(mActiveChannels);
                float * const * bufferData = bufferPtrs.data();
                float * firstStereoL = mFirstStereoBuffer.getWritePointer(0);
                float * firstStereoR = mFirstStereoBuffer.getWritePointer(1);
                float * secondStereoL = nullptr;
                float * secondStereoR = nullptr;

                if (mUseSecondSafHBin) {
                    secondStereoL = mSecondStereoBuffer.getWritePointer(0);
                    secondStereoR = mSecondStereoBuffer.getWritePointer(1);
                }

                if (numSamples % mFrameSize == 0) {
                    for (int frame = 0; frame < numFrames; ++frame) {
                        const int frameOffset = frame * mFrameSize;
                        for (int ch{}; ch < mNumSpksForFirstSafHBin; ++ch) {
                            mPFrameData[ch] = bufferData[ch] + frameOffset;
                        }
                        float * firstOutPtrs[2] = { firstStereoL + frameOffset, firstStereoR + frameOffset };
                        binauraliserNF_process(mSafFirstHBin,
                                               mPFrameData.data(),
                                               firstOutPtrs,
                                               mNumSpksForFirstSafHBin,
                                               mNumOutputs,
                                               mFrameSize);
                        if (mUseSecondSafHBin) {
                            for (int ch{}; ch < mNumSpksForSecondSafHBin; ++ch) {
                                mPFrameSecData[ch] = bufferData[ch + mNumSpksForFirstSafHBin] + frameOffset;
                            }
                            float * secondOutPtrs[2] = { secondStereoL + frameOffset, secondStereoR + frameOffset };
                            binauraliserNF_process(mSafSecondHBin,
                                                   mPFrameSecData.data(),
                                                   secondOutPtrs,
                                                   mNumSpksForSecondSafHBin,
                                                   mNumOutputs,
                                                   mFrameSize);
                        }
                    }
                } else {
                    /*hrtfBuffer*/ speakersBuffer.silence();
                    jassertfalse;
                }
            } else {
                // using FIFO buffering
                auto inHostVec = /*hrtfBuffer*/ speakersBuffer.getArrayOfWritePointers(mActiveChannels);
                const float * const * inHost = inHostVec.data();
                float * const * firstOutHost = mFirstStereoBuffer.getArrayOfWritePointers();
                float * const * secondOutHost
                    = mUseSecondSafHBin ? mSecondStereoBuffer.getArrayOfWritePointers() : nullptr;

                for (int n = 0; n < numSamples; ++n) {
                    for (int ch = 0; ch < mNumSpksForFirstSafHBin; ++ch)
                        mFirstInBuffersPtrs[ch][mFirstInPos] = inHost[ch][n];
                    ++mFirstInPos;

                    if (mFirstInPos == mFrameSize) {
                        binauraliserNF_process(mSafFirstHBin,
                                               mFirstInBuffersPtrs.data(),
                                               mFirstOutBuffersPtrs.data(),
                                               mNumSpksForFirstSafHBin,
                                               mNumOutputs,
                                               mFrameSize);
                        mFirstInPos = 0;
                        mFirstOutPos = 0;
                        mFirstAvailableOut = mFrameSize;
                    }

                    if (mFirstAvailableOut > 0) {
                        for (int ch = 0; ch < mNumOutputs; ++ch) {
                            firstOutHost[ch][n] = mFirstOutBuffersPtrs[ch][mFirstOutPos];
                        }
                        ++mFirstOutPos;
                        --mFirstAvailableOut;
                    } else {
                        for (int ch = 0; ch < mNumOutputs; ++ch)
                            firstOutHost[ch][n] = 0.0f;
                    }

                    if (mUseSecondSafHBin) {
                        for (int ch = 0; ch < mNumSpksForSecondSafHBin; ++ch)
                            mSecondInBuffersPtrs[ch][mSecondInPos] = inHost[ch + mNumSpksForFirstSafHBin][n];
                        ++mSecondInPos;

                        if (mSecondInPos == mFrameSize) {
                            binauraliserNF_process(mSafSecondHBin,
                                                   mSecondInBuffersPtrs.data(),
                                                   mSecondOutBuffersPtrs.data(),
                                                   mNumSpksForSecondSafHBin,
                                                   mNumOutputs,
                                                   mFrameSize);
                            mSecondInPos = 0;
                            mSecondOutPos = 0;
                            mSecondAvailableOut = mFrameSize;
                        }

                        if (mSecondAvailableOut > 0) {
                            for (int ch = 0; ch < mNumOutputs; ++ch)
                                secondOutHost[ch][n] = mSecondOutBuffersPtrs[ch][mSecondOutPos];
                            ++mSecondOutPos;
                            --mSecondAvailableOut;
                        } else {
                            for (int ch = 0; ch < mNumOutputs; ++ch)
                                secondOutHost[ch][n] = 0.0f;
                        }
                    }
                }
            }
            // Combine the first and second stereo buffers to stereo out buffer
            for (int ch{}; ch < mFirstStereoBuffer.getNumChannels(); ++ch) {
                stereoBuffer.copyFrom(ch, 0, mFirstStereoBuffer, ch, 0, mFirstStereoBuffer.getNumSamples());
            }
            if (mUseSecondSafHBin) {
                for (int ch{}; ch < mSecondStereoBuffer.getNumChannels(); ++ch) {
                    stereoBuffer.addFrom(ch, 0, mSecondStereoBuffer, ch, 0, mSecondStereoBuffer.getNumSamples());
                }
            }
            if (std::isnan(stereoBuffer.getRMSLevel(0, 0, stereoBuffer.getNumSamples()))
                || std::isnan(stereoBuffer.getRMSLevel(1, 0, stereoBuffer.getNumSamples()))) {
                stereoBuffer.clear();
                mSAFConfigureNeeded.set(true);
            }
        }
    } else {
        // libspatialaudio
        if (!mAmbBinauralDecoderConfigured) {
            return;
        }
        mBFormatMain.Reset();

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
            mAmbEncoder.Reset();
            mAmbEncoder.Refresh();

            gris::output_patch_t speakerId{ speaker.key };
            mAmbEncoder.ProcessAccumul(/*hrtfBuffer*/ speakersBuffer[speakerId].getWritePointer(0),
                                       sourcesBuffer.getNumSamples(),
                                       &mBFormatMain);
        }
        mAmbDecoderBinaural.Process(&mBFormatMain,
                                    const_cast<float **>(stereoBuffer.getArrayOfWritePointers()),
                                    mBufferSize);
    }
}

//==============================================================================
juce::Array<Triplet> HrtfSpatAlgorithm::getTriplets() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassert(hasTriplets());
    return mInnerAlgorithm->getTriplets();
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

//==============================================================================
void HrtfSpatAlgorithm::showErrorMessage(juce::String & error)
{
    JUCE_ASSERT_MESSAGE_THREAD

    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                           "Unable to process binaural stereo reduction.",
                                           error,
                                           "OK",
                                           nullptr,
                                           nullptr);
}

//==============================================================================
void HrtfSpatAlgorithm::configureSAF()
{
    JUCE_ASSERT_MESSAGE_THREAD

    if (mSAFReconfigureAttempts >= 5) {
        auto msg{ juce::String(juce::String("Unable to configure ") + mSofaFile.getFileName())
                  + juce::String(".\nPlease try another file.") };
        showErrorMessage(msg);
        return;
    }

    mNumSpksToConvert = mSpeakerSetup.speakers.size();
    mNumSpksForFirstSafHBin = mNumSpksToConvert < SAF_MAX_NUM_CHANNELS ? mNumSpksToConvert : SAF_MAX_NUM_CHANNELS;
    mNumSpksForSecondSafHBin = mNumSpksToConvert > SAF_MAX_NUM_CHANNELS ? mNumSpksToConvert - SAF_MAX_NUM_CHANNELS : 0;
    mUseSecondSafHBin = mNumSpksForSecondSafHBin > 0;

    binauraliserNF_create(&mSafFirstHBin);
    binauraliser_setSofaFilePath(mSafFirstHBin, mSofaFile.getFullPathName().toStdString().c_str());
    binauraliser_setUseDefaultHRIRsflag(mSafFirstHBin, 0);
    binauraliser_setNumSources(mSafFirstHBin, mNumSpksForFirstSafHBin);
    binauraliserNF_init(mSafFirstHBin, mSampleRate);

    if (mUseSecondSafHBin) {
        binauraliserNF_create(&mSafSecondHBin);
        binauraliser_setSofaFilePath(mSafSecondHBin, mSofaFile.getFullPathName().toStdString().c_str());
        binauraliser_setUseDefaultHRIRsflag(mSafSecondHBin, 0);
        binauraliser_setNumSources(mSafSecondHBin, mNumSpksForSecondSafHBin);
        binauraliserNF_init(mSafSecondHBin, mSampleRate);
    }

    mFrameSize = binauraliser_getFrameSize();
    mNumOutputs = 2;
    mHostBlockSize = mBufferSize;
    mUsingLowDelay = (mHostBlockSize % mFrameSize == 0); // in this case we can skip the FIFO buffering, since
    // block size is a multiple of the processing framesize

    jassert(mFrameSize > 0);
    jassert(mNumSpksForFirstSafHBin >= 0);
    jassert(mNumOutputs >= 0);
    jassert(mHostBlockSize >= 0);

    for (auto const & channel : mSpeakerSetup.speakers) {
        mActiveChannels.push_back(channel.key);
    }

    mFirstInBuffers.resize(mNumSpksForFirstSafHBin * mFrameSize);
    mFirstInBuffersPtrs.resize(mNumSpksForFirstSafHBin);
    for (int ch = 0; ch < mNumSpksForFirstSafHBin; ch++)
        mFirstInBuffersPtrs[ch] = &mFirstInBuffers[ch * mFrameSize];
    mFirstOutBuffers.resize(mNumOutputs * mFrameSize);
    mFirstOutBuffersPtrs.resize(mNumOutputs);
    for (int ch = 0; ch < mNumOutputs; ch++)
        mFirstOutBuffersPtrs[ch] = &mFirstOutBuffers[ch * mFrameSize];

    mFirstInPos = mFirstOutPos = mFirstAvailableOut = 0;
    if (!mFirstInBuffers.empty())
        std::fill(mFirstInBuffers.begin(), mFirstInBuffers.end(), 0.0f);
    if (!mFirstOutBuffers.empty())
        std::fill(mFirstOutBuffers.begin(), mFirstOutBuffers.end(), 0.0f);

    mFirstStereoBuffer.setSize(mNumOutputs, mBufferSize);
    mFirstStereoBuffer.clear();

    mPFrameData.resize(mNumSpksForFirstSafHBin);
    mPFrameSecData.resize(mUseSecondSafHBin ? mNumSpksForSecondSafHBin : 0);

    if (mUseSecondSafHBin) {
        mSecondInBuffers.resize(mNumSpksForSecondSafHBin * mFrameSize);
        mSecondInBuffersPtrs.resize(mNumSpksForSecondSafHBin);
        for (int ch = 0; ch < mNumSpksForSecondSafHBin; ch++)
            mSecondInBuffersPtrs[ch] = &mSecondInBuffers[ch * mFrameSize];
        mSecondOutBuffers.resize(mNumOutputs * mFrameSize);
        mSecondOutBuffersPtrs.resize(mNumOutputs);
        for (int ch = 0; ch < mNumOutputs; ch++)
            mSecondOutBuffersPtrs[ch] = &mSecondOutBuffers[ch * mFrameSize];

        mSecondInPos = mSecondOutPos = mSecondAvailableOut = 0;
        if (!mSecondInBuffers.empty())
            std::fill(mSecondInBuffers.begin(), mSecondInBuffers.end(), 0.0f);
        if (!mSecondOutBuffers.empty())
            std::fill(mSecondOutBuffers.begin(), mSecondOutBuffers.end(), 0.0f);

        mSecondStereoBuffer.setSize(mNumOutputs, mBufferSize);
        mSecondStereoBuffer.clear();
    }

    // Set position of speakers for binaural rendering
    auto gainAdjust{ std::sqrt(mNumSpksToConvert) };
    gainAdjust = gainAdjust == 0 ? 1.0f : gainAdjust;
    int spkChan2SAFIndex{};
    for (auto const & speaker : mSpeakerSetup.speakers) {
        float azi = speaker.value->position.getPolar().azimuth.getAsRadians() - (PI.get() / 2);
        while (azi <= -PI.get())
            azi += TWO_PI.get();
        while (azi > PI.get())
            azi -= TWO_PI.get();

        azi = radians_t{ azi }.getAsDegrees();
        float elev = speaker.value->position.getPolar().elevation.getAsDegrees();
        float distance = speaker.value->position.getPolar().length;

        if (spkChan2SAFIndex < mNumSpksForFirstSafHBin) {
            binauraliser_setSourceAzi_deg(mSafFirstHBin, spkChan2SAFIndex, azi);
            binauraliser_setSourceElev_deg(mSafFirstHBin, spkChan2SAFIndex, elev);
            binauraliserNF_setSourceDist_m(mSafFirstHBin, spkChan2SAFIndex, distance);
            binauraliser_setSourceGain(mSafFirstHBin, spkChan2SAFIndex, gainAdjust);
        } else {
            auto const index{ spkChan2SAFIndex - mNumSpksForFirstSafHBin };
            binauraliser_setSourceAzi_deg(mSafSecondHBin, index, azi);
            binauraliser_setSourceElev_deg(mSafSecondHBin, index, elev);
            binauraliserNF_setSourceDist_m(mSafSecondHBin, index, distance);
            binauraliser_setSourceGain(mSafSecondHBin, index, gainAdjust);
        }
        spkChan2SAFIndex++;
    }

    binauraliser_setUseDefaultHRIRsflag(mSafFirstHBin, mUseDefaultHRIRs ? 1 : 0);
    binauraliser_setEnableHRIRsDiffuseEQ(mSafFirstHBin, mEnableHRIRsDiffuseEQ ? 1 : 0);

    if (mUseSecondSafHBin) {
        binauraliser_setUseDefaultHRIRsflag(mSafSecondHBin, mUseDefaultHRIRs ? 1 : 0);
        binauraliser_setEnableHRIRsDiffuseEQ(mSafSecondHBin, mEnableHRIRsDiffuseEQ ? 1 : 0);

        binauraliser_refreshSettings(mSafFirstHBin);
        binauraliser_refreshSettings(mSafSecondHBin);
    }
    startTimer(80);
    mSAFConfigureNeeded.set(false);
}

//==============================================================================
void HrtfSpatAlgorithm::reconfigureSAF()
{
    JUCE_ASSERT_MESSAGE_THREAD

    stopTimer();

    mActiveChannels.clear();
    mPFrameData.clear();
    mPFrameSecData.clear();

    configureSAF();
    mSAFReconfigureAttempts++;
}

//==============================================================================
void HrtfSpatAlgorithm::configureLibspatialaudio()
{
    JUCE_ASSERT_MESSAGE_THREAD

    mBFormatMain.Configure(mNOrder, true, mBufferSize);
    mBFormatMain.Reset();
    // fadeTimeMilliSec of 0ms is OK because the speakers do not move and movement of source sound
    // is handled in the InnerAlgorithm process.
    [[maybe_unused]] auto encoderWorks{ mAmbEncoder.Configure(mNOrder, true, mSampleRate) };
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
                                                                  mSampleRate,
                                                                  mBufferSize,
                                                                  tailLength,
                                                                  mSofaFile.getFullPathName().toStdString(),
                                                                  mBinauralLowCpuMode);

    if (!mAmbBinauralDecoderConfigured) {
        if (mSofaFile.getFullPathName().compare("") == 0) {
            auto msg{ juce::String("No SOFA file loaded.\nGo to File and Open SOFA file.") };
            showErrorMessage(msg);
        } else {
            auto msg{ juce::String("Something is wrong with the selected file.\nPlease choose a valid SOFA file.") };
            showErrorMessage(msg);
        }
    }
}

//==============================================================================
void HrtfSpatAlgorithm::timerCallback()
{
    /* reinitialise codec if needed. Thread safe (binauraliser_nf.h) */
    if (binauraliser_getCodecStatus(mSafFirstHBin) == CODEC_STATUS_NOT_INITIALISED) {
        try {
            std::thread threadInit(binauraliserNF_initCodec, mSafFirstHBin);
            threadInit.detach();
        } catch (const std::exception & exception) {
            jassertfalse;
        }
    }
    if (mUseSecondSafHBin && binauraliser_getCodecStatus(mSafSecondHBin) == CODEC_STATUS_NOT_INITIALISED) {
        try {
            std::thread threadInit(binauraliserNF_initCodec, mSafSecondHBin);
            threadInit.detach();
        } catch (const std::exception & exception) {
            jassertfalse;
        }
    }
    if (mSAFConfigureNeeded.get()) {
        reconfigureSAF();
    }
}

} // namespace gris
