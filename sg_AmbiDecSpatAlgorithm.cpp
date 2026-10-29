/*
 This file is part of SpatGRIS.

 Developer: Gaël Lane Lépine

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

#include "sg_AmbiDecSpatAlgorithm.hpp"
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
AmbiDecSpatAlgorithm::AmbiDecSpatAlgorithm(SpeakerSetup const & speakerSetup,
                                           SpatMode const & /*projectSpatMode*/,
                                           SourcesData const & sources,
                                           double const sampleRate,
                                           int const bufferSize,
                                           BinauralSettings & /*binauralSettings*/)
    : mSpeakerSetup(speakerSetup)
    , mBufferSize(bufferSize)
    , mSampleRate(sampleRate)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mSafHAmb = nullptr;

    for (auto & source : sources) {
        if (source.value->directOut) {
            mDirectOutActiveInputChannels.push_back(source.key);
            mInitialDirectOutPairsFromPlayer.push_back(std::pair(source.key, *source.value->directOut));
        } else {
            mActiveInputChannels.push_back(source.key);
        }
    }
}

//==============================================================================
AmbiDecSpatAlgorithm::~AmbiDecSpatAlgorithm()
{
    stopTimer();
    ambi_dec_destroy(&mSafHAmb);
}

//==============================================================================
void AmbiDecSpatAlgorithm::process(AudioConfig const & config,
                                   SourceAudioBuffer & sourcesBuffer,
                                   SpeakerAudioBuffer & speakersBuffer,
                                   juce::AudioBuffer<float> & /*stereoBuffer*/,
                                   SourcePeaks const & /*sourcePeaks*/,
                                   [[maybe_unused]] SpeakersAudioConfig const * /*altSpeakerConfig*/) noexcept
    NONBLOCKING
{
    /** This algorithm is only used for the ambisonic player.
     * sourcesBuffer and speakersBuffer are used as audio output
     * - sourcesBuffer is used by the Stereo reduction algorithm
     * - speakersBuffer is used by the hrtf (binaural) algorithm or when there is no stereo reduction
     * SG audio spatialization processing should be skipped.
     */
    ASSERT_AUDIO_THREAD;

    jassert(sourcesBuffer.size() == mNumAudioOutSpks + mNumDirectOutInputs);

    if (!mSAFConfigureNeeded.get() && ambi_dec_getCodecStatus(mSafHAmb) == CODEC_STATUS_INITIALISED) {
        mAudioOutBuffer.clear();

        const int numSamples = sourcesBuffer.getNumSamples();

        if (mUsingLowDelay) {
            const int numFrames = numSamples / mFrameSize;
            float * const * bufferData = mAmbAudioInBuffer.getArrayOfWritePointers();
            float * const * firstAmbAudioBufferData = mAudioOutBuffer.getArrayOfWritePointers();

            if (numSamples % mFrameSize == 0) {
                for (int frame = 0; frame < numFrames; ++frame) {
                    const int frameOffset = frame * mFrameSize;
                    for (int ch{}; ch < mNumChannelsForSafHAmb; ++ch) {
                        mPFrameData[ch] = bufferData[ch] + frameOffset;
                    }
                    for (int ch{ 0 }; ch < mNumAudioOutSpks; ++ch) {
                        mFirstOutBuffersPtrs[ch] = firstAmbAudioBufferData[ch] + frameOffset;
                    }
                    ambi_dec_process(mSafHAmb,
                                     mPFrameData.data(),
                                     mFirstOutBuffersPtrs.data(),
                                     mNumChannelsForSafHAmb,
                                     mNumAudioOutSpks,
                                     mFrameSize);
                }
            } else {
                sourcesBuffer.silence();
                speakersBuffer.silence();
                mAmbAudioInBuffer.clear();
                mDirectOutAudioInBuffer.clear();
                mAudioOutBuffer.clear();
                jassertfalse;
            }
        } else {
            // using FIFO buffering
            const float * const * inHost = mAmbAudioInBuffer.getArrayOfWritePointers();
            float * const * outHost = mAudioOutBuffer.getArrayOfWritePointers();

            for (int n = 0; n < numSamples; ++n) {
                for (int ch = 0; ch < mNumChannelsForSafHAmb; ++ch)
                    mFirstInBuffersPtrs[ch][mFirstInPos] = inHost[ch][n];
                ++mFirstInPos;

                if (mFirstInPos == mFrameSize) {
                    ambi_dec_process(mSafHAmb,
                                     mFirstInBuffersPtrs.data(),
                                     mFirstOutBuffersPtrs.data(),
                                     mNumChannelsForSafHAmb,
                                     mNumAudioOutSpks,
                                     mFrameSize);
                    mFirstInPos = 0;
                    mFirstOutPos = 0;
                    mFirstAvailableOut = mFrameSize;
                }

                if (mFirstAvailableOut > 0) {
                    for (int ch = 0; ch < mNumAudioOutSpks; ++ch) {
                        outHost[ch][n] = mFirstOutBuffersPtrs[ch][mFirstOutPos];
                    }
                    ++mFirstOutPos;
                    --mFirstAvailableOut;
                } else {
                    for (int ch = 0; ch < mNumAudioOutSpks; ++ch)
                        outHost[ch][n] = 0.0f;
                }
            }
        }

        // Copy decoded channels and direct outs to sourcesBuffer
        auto outSrcBufferPtrs = sourcesBuffer.getArrayOfWritePointers(mActiveInputChannels);
        auto directOutSrcBufferPtrs = sourcesBuffer.getArrayOfWritePointers(mDirectOutActiveInputChannels);
        for (int ch{}; ch < outSrcBufferPtrs.size(); ++ch) {
            auto * dst{ outSrcBufferPtrs[ch] };
            auto const * src{ mAudioOutBuffer.getReadPointer(ch) };

            std::copy_n(src, sourcesBuffer.getNumSamples(), dst);
        }
        for (int ch{}; ch < directOutSrcBufferPtrs.size(); ++ch) {
            auto * dst{ directOutSrcBufferPtrs[ch] };
            auto const * src{ mDirectOutAudioInBuffer.getReadPointer(ch) };

            std::copy_n(src, sourcesBuffer.getNumSamples(), dst);
        }
        // Copy decoded channels and direct outs to speakersBuffer
        auto outBufferPtrs = speakersBuffer.getArrayOfWritePointers(mActiveOutputChannels);
        auto directOutBufferPtrs = speakersBuffer.getArrayOfWritePointers(mDirectOutActiveOutputChannels);
        auto & directOutPairs{ config.directOutPairs };
        bool atLeastOneDirectOutSourceIsSolo{};
        for (auto & source : config.sourcesAudioConfig) {
            if (!source.value.directOut && source.value.isMuted) {
                atLeastOneDirectOutSourceIsSolo = true;
                break;
            }
        }
        if (!atLeastOneDirectOutSourceIsSolo) {
            for (int ch{}; ch < outBufferPtrs.size(); ++ch) {
                auto * dst{ outBufferPtrs[ch] };
                auto const * src{ mAudioOutBuffer.getReadPointer(ch) };

                std::copy_n(src, speakersBuffer.getNumSamples(), dst);
            }
        }
        for (auto & pair : directOutPairs) {
            auto srcIndex{ pair.first };
            auto it = std::find_if(mInitialDirectOutPairsFromPlayer.begin(),
                                   mInitialDirectOutPairsFromPlayer.end(),
                                   [srcIndex](const auto & initialPair) { return initialPair.first == srcIndex; });
            if (it != mInitialDirectOutPairsFromPlayer.end()) {
                auto ch = std::distance(mInitialDirectOutPairsFromPlayer.begin(), it);
                auto const * src{ mDirectOutAudioInBuffer.getReadPointer(ch) };
                auto spkOutputPatch{ pair.second };

                auto directSpkIt = std::find(mDirectOutActiveOutputChannels.begin(),
                                             mDirectOutActiveOutputChannels.end(),
                                             spkOutputPatch);
                auto spatSpksIt = std::find(mActiveOutputChannels.begin(), mActiveOutputChannels.end(), spkOutputPatch);

                if (directSpkIt != mDirectOutActiveOutputChannels.end()) {
                    auto index = std::distance(mDirectOutActiveOutputChannels.begin(), directSpkIt);
                    auto * dst{ directOutBufferPtrs[static_cast<int>(index)] };

                    std::transform(dst, dst + numSamples, src, dst, std::plus<>{});
                }
                if (spatSpksIt != mActiveOutputChannels.end()) {
                    auto index = std::distance(mActiveOutputChannels.begin(), spatSpksIt);
                    auto * dst{ outBufferPtrs[static_cast<int>(index)] };

                    std::transform(dst, dst + numSamples, src, dst, std::plus<>{});
                }
            }
        }

        for (int ch{}; ch < mNumAudioOutSpks; ++ch) {
            if (std::isnan(mAudioOutBuffer.getRMSLevel(ch, 0, numSamples))) {
                mSAFConfigureNeeded.set(true);
            }
        }
        if (mSAFConfigureNeeded.get()) {
            mAudioOutBuffer.clear();
            sourcesBuffer.silence();
            speakersBuffer.silence();
        }
    }
}

//==============================================================================
void gris::AmbiDecSpatAlgorithm::configure(int numAmbisonicChannels, int numDirectOutChannels)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto ambisonicOrder{ static_cast<int>(std::sqrt(numAmbisonicChannels)) - 1 };
    jassert((ambisonicOrder + 1) * (ambisonicOrder + 1) == numAmbisonicChannels);
    mAmbisonicOrder = ambisonicOrder;
    mNumAmbiInputs = numAmbisonicChannels;
    mNumDirectOutInputs = numDirectOutChannels;
    mNumChannelsForSafHAmb = numAmbisonicChannels;

    mAmbAudioInBuffer.setSize(numAmbisonicChannels, mBufferSize);
    mDirectOutAudioInBuffer.setSize(numDirectOutChannels, mBufferSize);

    configureSAF();
}

//==============================================================================
void gris::AmbiDecSpatAlgorithm::copyAmbPlayerBuffer(juce::AudioBuffer<float> & inputBuffer)
{
    ASSERT_AUDIO_THREAD;

    if (mSAFConfigurationChecksDone) {
        const auto totalNumChan{ mNumAmbiInputs + mNumDirectOutInputs };

        jassert(inputBuffer.getNumChannels() == totalNumChan);
        jassert(inputBuffer.getNumSamples() == mAmbAudioInBuffer.getNumSamples()
                && inputBuffer.getNumSamples() == mDirectOutAudioInBuffer.getNumSamples());

        mAmbAudioInBuffer.clear();
        mDirectOutAudioInBuffer.clear();

        for (int ch = 0; ch < mNumAmbiInputs; ++ch)
            mAmbAudioInBuffer.copyFrom(ch, 0, inputBuffer, ch, 0, inputBuffer.getNumSamples());
        for (int directOutChan = mNumAmbiInputs, ch{}; directOutChan < inputBuffer.getNumChannels();
             ++directOutChan, ++ch)
            mDirectOutAudioInBuffer.copyFrom(ch, 0, inputBuffer, directOutChan, 0, inputBuffer.getNumSamples());
    }
}

//==============================================================================
void gris::AmbiDecSpatAlgorithm::setOrder(int newOrder)
{
    mAmbisonicOrder = newOrder;
    ambi_dec_setMasterDecOrder(mSafHAmb, (SH_ORDERS)mAmbisonicOrder);
    refreshSettings();
}

//==============================================================================
std::vector<std::pair<source_index_t, output_patch_t>> &
    gris::AmbiDecSpatAlgorithm::getInitialDirectOutPairsFromPlayer()
{
    return mInitialDirectOutPairsFromPlayer;
}

//==============================================================================
std::unique_ptr<AbstractSpatAlgorithm> AmbiDecSpatAlgorithm::make(SpeakerSetup const & speakerSetup,
                                                                  SpatMode const & projectSpatMode,
                                                                  SourcesData const & sources,
                                                                  double const sampleRate,
                                                                  int const bufferSize,
                                                                  BinauralSettings & binauralSettings)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    return std::make_unique<AmbiDecSpatAlgorithm>(speakerSetup,
                                                  projectSpatMode,
                                                  sources,
                                                  sampleRate,
                                                  bufferSize,
                                                  binauralSettings);
}

//==============================================================================
void AmbiDecSpatAlgorithm::showErrorMessage(juce::String & error)
{
    JUCE_ASSERT_MESSAGE_THREAD

    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                           "Unable to process ambisonic decoding.",
                                           error,
                                           "OK",
                                           nullptr,
                                           nullptr);
}

//==============================================================================
void AmbiDecSpatAlgorithm::configureSAF()
{
    JUCE_ASSERT_MESSAGE_THREAD

    if (mSAFReconfigureAttempts >= SAF_MAX_RECONFIGURATION_ATTEMPTS) {
        auto msg{ juce::String("Unable to configure ambisonic algorithm.") };
        showErrorMessage(msg);
        return;
    }

    for (auto const & channel : mSpeakerSetup.speakers) {
        if (channel.value->isDirectOutOnly) {
            mDirectOutActiveOutputChannels.push_back(static_cast<output_patch_t>(channel.key.get()));
            continue;
        }
        mActiveOutputChannels.push_back(static_cast<output_patch_t>(channel.key.get()));
    }
    mNumAudioOutSpks = mActiveOutputChannels.size();

    ambi_dec_create(&mSafHAmb);
    ambi_dec_init(mSafHAmb, static_cast<int>(mSampleRate));
    ambi_dec_setMasterDecOrder(mSafHAmb, (SH_ORDERS)mAmbisonicOrder);
    ambi_dec_setNormType(mSafHAmb, NORM_SN3D); // iem = SN3D
    ambi_dec_setChOrder(mSafHAmb, CH_ORDER::CH_ACN);
    ambi_dec_setDecMethod(mSafHAmb, 0, AMBI_DEC_DECODING_METHODS::DECODING_METHOD_ALLRAD);
    ambi_dec_setDecMethod(mSafHAmb, 1, AMBI_DEC_DECODING_METHODS::DECODING_METHOD_ALLRAD);
    ambi_dec_setDecEnableMaxrE(mSafHAmb, 0, 1);
    ambi_dec_setDecEnableMaxrE(mSafHAmb, 1, 1);
    ambi_dec_setDecNormType(mSafHAmb, 0, AMBI_DEC_DIFFUSE_FIELD_EQ_APPROACH::AMPLITUDE_PRESERVING);
    ambi_dec_setDecNormType(mSafHAmb, 1, AMBI_DEC_DIFFUSE_FIELD_EQ_APPROACH::ENERGY_PRESERVING);
    ambi_dec_setTransitionFreq(mSafHAmb, 500.0f);
    ambi_dec_setNumLoudspeakers(mSafHAmb, mNumAudioOutSpks);

    auto numberOfBands{ ambi_dec_getNumberOfBands() };
    mFrameSize = ambi_dec_getFrameSize();
    mHostBlockSize = mBufferSize;
    mUsingLowDelay = (mHostBlockSize % mFrameSize == 0); // in this case we can skip the FIFO buffering, since
    // block size is a multiple of the processing framesize

    jassert(mFrameSize > 0);
    jassert(mNumChannelsForSafHAmb >= 0);
    jassert(mNumAudioOutSpks >= 0);
    jassert(mHostBlockSize >= 0);

    mFirstInBuffers.resize(mNumChannelsForSafHAmb * mFrameSize);
    mFirstInBuffersPtrs.resize(mNumChannelsForSafHAmb);
    for (int ch = 0; ch < mNumChannelsForSafHAmb; ch++)
        mFirstInBuffersPtrs[ch] = &mFirstInBuffers[ch * mFrameSize];
    mFirstOutBuffers.resize(mNumAudioOutSpks * mFrameSize);
    mFirstOutBuffersPtrs.resize(mNumAudioOutSpks);
    for (int ch = 0; ch < mNumAudioOutSpks; ch++)
        mFirstOutBuffersPtrs[ch] = &mFirstOutBuffers[ch * mFrameSize];

    mFirstInPos = mFirstOutPos = mFirstAvailableOut = 0;
    if (!mFirstInBuffers.empty())
        std::fill(mFirstInBuffers.begin(), mFirstInBuffers.end(), 0.0f);
    if (!mFirstOutBuffers.empty())
        std::fill(mFirstOutBuffers.begin(), mFirstOutBuffers.end(), 0.0f);

    mAudioOutBuffer.setSize(mNumAudioOutSpks, mBufferSize);
    mAudioOutBuffer.clear();

    mPFrameData.resize(mNumChannelsForSafHAmb);

    // Set position of speakers for the ambisonic decoder
    int spkChan2SAFIndex{};
    for (auto const & speaker : mSpeakerSetup.speakers) {
        if (!speaker.value->isDirectOutOnly) {
            float azi = speaker.value->position.getPolar().azimuth.getAsRadians() - (PI.get() / 2);
            while (azi <= -PI.get())
                azi += TWO_PI.get();
            while (azi > PI.get())
                azi -= TWO_PI.get();

            azi = radians_t{ azi }.getAsDegrees();
            float elev = speaker.value->position.getPolar().elevation.getAsDegrees();
            ambi_dec_setLoudspeakerAzi_deg(mSafHAmb, spkChan2SAFIndex, azi);
            ambi_dec_setLoudspeakerElev_deg(mSafHAmb, spkChan2SAFIndex, elev);

            spkChan2SAFIndex++;
        }
    }

    ambi_dec_refreshSettings(mSafHAmb);

    startTimer(80);
    mSAFConfigureNeeded.set(false);
}

//==============================================================================
void AmbiDecSpatAlgorithm::reconfigureSAF()
{
    JUCE_ASSERT_MESSAGE_THREAD

    stopTimer();

    mActiveOutputChannels.clear();
    mDirectOutActiveOutputChannels.clear();
    mPFrameData.clear();

    configureSAF();
    mSAFReconfigureAttempts++;
}

//==============================================================================
void gris::AmbiDecSpatAlgorithm::refreshSettings()
{
    ambi_dec_refreshSettings(mSafHAmb);
    mSAFConfigurationChecksDone = false;
}

//==============================================================================
void AmbiDecSpatAlgorithm::timerCallback()
{
    /* reinitialise codec if needed. Thread-safe and safe to call multiple times (ambi_dec.h) */
    if (ambi_dec_getCodecStatus(mSafHAmb) == CODEC_STATUS_NOT_INITIALISED) {
        try {
            std::thread threadInit(ambi_dec_initCodec, mSafHAmb);
            threadInit.detach();
        } catch ([[maybe_unused]] const std::exception & exception) {
            jassertfalse;
        }
    }
    if (!mSAFConfigurationChecksDone) {
        if (ambi_dec_getCodecStatus(mSafHAmb) == CODEC_STATUS_INITIALISED) {
            mSAFConfigurationChecksDone = true;
            invokeCallback(0); // success
        }
    }
    if (mSAFConfigureNeeded.get()) {
        reconfigureSAF();
    }
}

} // namespace gris
