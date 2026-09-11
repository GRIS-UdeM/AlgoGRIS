/*
 This file is part of SpatGRIS.

 Developers: Gaël Lane Lépine

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

#include "sg_AmbiEncSpatAlgorithm.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
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
#include <Data/sg_constants.hpp>
#include <Utilities/ValueTreeUtilities.hpp>
#include <array>
#include <cstddef>
#include <memory>

namespace gris
{
//==============================================================================
AmbiEncSpatAlgorithm::AmbiEncSpatAlgorithm(SpeakerSetup const & speakerSetup,
                                     SpatMode const & /*projectSpatMode*/,
                                     SourcesData const & /*sources*/,
                                     double const sampleRate,
                                     int const bufferSize,
                                     BinauralSettings & /*binauralSettings*/)
    : mSpeakerSetup(speakerSetup)
    , mBufferSize(bufferSize)
    , mSampleRate(sampleRate)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mSafFirstHAmb = nullptr;
    mSafSecondHAmb = nullptr;
}

//==============================================================================
AmbiEncSpatAlgorithm::~AmbiEncSpatAlgorithm()
{
    stopTimer();
    ambi_enc_destroy(&mSafFirstHAmb);
    if (mUseSecondSafAmb) {
        ambi_enc_destroy(&mSafSecondHAmb);
    }
}

//==============================================================================
void AmbiEncSpatAlgorithm::process(AudioConfig const & /*config*/,
                                SourceAudioBuffer & /*sourcesBuffer*/,
                                SpeakerAudioBuffer & speakersBuffer,
                                juce::AudioBuffer<float> & /*stereoBuffer*/,
                                SourcePeaks const & /*sourcePeaks*/,
                                [[maybe_unused]] SpeakersAudioConfig const * /*altSpeakerConfig*/) noexcept NONBLOCKING
{
    /** This algorithm is only used for ambisonic recording. All SG audio spatialization processing
    * should already be done at this point, and speakersBuffer can be used as audio input to do the
    * processing.
    */
    ASSERT_AUDIO_THREAD;

    if (!mSAFConfigureNeeded.get()) {
        mFirstAmbAudioBuffer.clear();
        mSecondAmbAudioBuffer.clear();
        mAmbAudioOutBuffer.clear();
        mDirectOutAudioOutBuffer.clear();

        const int numSamples = speakersBuffer.getNumSamples();

        if (mUsingLowDelay)
        {
            const int numFrames = numSamples / mFrameSize;
            auto bufferPtrs = speakersBuffer.getArrayOfWritePointers(mActiveChannels);
            float * const * bufferData = bufferPtrs.data();
            float * const * firstAmbAudioBufferData = mFirstAmbAudioBuffer.getArrayOfWritePointers();
            float * const * secondAmbAudioBufferData = nullptr;

            if (mUseSecondSafAmb) {
                secondAmbAudioBufferData = mSecondAmbAudioBuffer.getArrayOfWritePointers();
            }

            if (numSamples % mFrameSize == 0) {
                for (int frame = 0; frame < numFrames; ++frame) {
                    const int frameOffset = frame * mFrameSize;
                    for (int ch{}; ch < mNumSpksForFirstSafHAmb; ++ch) {
                        mPFrameData[ch] = bufferData[ch] + frameOffset;
                    }
                    for (int ch{ 0 }; ch < mAmbiNumOutputs; ++ch) {
                        mFirstOutBuffersPtrs[ch] = firstAmbAudioBufferData[ch] + frameOffset;
                    }
                    ambi_enc_process(mSafFirstHAmb,
                                     mPFrameData.data(),
                                     mFirstOutBuffersPtrs.data(),
                                     mNumSpksForFirstSafHAmb,
                                     mAmbiNumOutputs,
                                     mFrameSize);

                    if (mUseSecondSafAmb) {
                        for (int ch{}; ch < mNumSpksForSecondSafHAmb; ++ch) {
                            mPFrameSecData[ch] = bufferData[ch + mNumSpksForFirstSafHAmb] + frameOffset;
                        }
                        for (int ch{ 0 }; ch < mAmbiNumOutputs; ++ch) {
                            mSecondOutBuffersPtrs[ch] = secondAmbAudioBufferData[ch] + frameOffset;
                        }
                        ambi_enc_process(mSafSecondHAmb,
                                         mPFrameSecData.data(),
                                         mSecondOutBuffersPtrs.data(),
                                         mNumSpksForSecondSafHAmb,
                                         mAmbiNumOutputs,
                                         mFrameSize);
                    }
                }
            } else {
                mFirstAmbAudioBuffer.clear();
                mSecondAmbAudioBuffer.clear();
                mAmbAudioOutBuffer.clear();
                mDirectOutAudioOutBuffer.clear();
                jassertfalse;
            }
        } else {
            // using FIFO buffering
            auto inHostVec = speakersBuffer.getArrayOfWritePointers(mActiveChannels);
            const float * const * inHost = inHostVec.data();
            float * const * firstOutHost = mFirstAmbAudioBuffer.getArrayOfWritePointers();
            float * const * secondOutHost = mUseSecondSafAmb ? mSecondAmbAudioBuffer.getArrayOfWritePointers() : nullptr;

            for (int n = 0; n < numSamples; ++n) {
                for (int ch = 0; ch < mNumSpksForFirstSafHAmb; ++ch)
                    mFirstInBuffersPtrs[ch][mFirstInPos] = inHost[ch][n];
                ++mFirstInPos;

                if (mFirstInPos == mFrameSize) {
                    ambi_enc_process(mSafFirstHAmb,
                                     mFirstInBuffersPtrs.data(),
                                     mFirstOutBuffersPtrs.data(),
                                     mNumSpksForFirstSafHAmb,
                                     mAmbiNumOutputs,
                                     mFrameSize);
                    mFirstInPos = 0;
                    mFirstOutPos = 0;
                    mFirstAvailableOut = mFrameSize;
                }

                if (mFirstAvailableOut > 0) {
                    for (int ch = 0; ch < mAmbiNumOutputs; ++ch) {
                        firstOutHost[ch][n] = mFirstOutBuffersPtrs[ch][mFirstOutPos];
                    }
                    ++mFirstOutPos;
                    --mFirstAvailableOut;
                } else {
                    for (int ch = 0; ch < mAmbiNumOutputs; ++ch)
                        firstOutHost[ch][n] = 0.0f;
                }

                if (mUseSecondSafAmb) {
                    for (int ch = 0; ch < mNumSpksForSecondSafHAmb; ++ch)
                        mSecondInBuffersPtrs[ch][mSecondInPos] = inHost[ch + mNumSpksForFirstSafHAmb][n];
                    ++mSecondInPos;

                    if (mSecondInPos == mFrameSize) {
                        ambi_enc_process(mSafSecondHAmb,
                                         mSecondInBuffersPtrs.data(),
                                         mSecondOutBuffersPtrs.data(),
                                         mNumSpksForSecondSafHAmb,
                                         mAmbiNumOutputs,
                                         mFrameSize);
                        mSecondInPos = 0;
                        mSecondOutPos = 0;
                        mSecondAvailableOut = mFrameSize;
                    }

                    if (mSecondAvailableOut > 0) {
                        for (int ch = 0; ch < mAmbiNumOutputs; ++ch)
                            secondOutHost[ch][n] = mSecondOutBuffersPtrs[ch][mSecondOutPos];
                        ++mSecondOutPos;
                        --mSecondAvailableOut;
                    } else {
                        for (int ch = 0; ch < mAmbiNumOutputs; ++ch)
                            secondOutHost[ch][n] = 0.0f;
                    }
                }
            }
        }

        // Copy direct out
        auto directOutBufferPtrs = speakersBuffer.getArrayOfWritePointers(mDirectOutActiveChannels);
        for (int ch{}; ch < mDirectOutNumOutputs; ++ch) {
            mDirectOutAudioOutBuffer.copyFrom(ch, 0, directOutBufferPtrs[ch], speakersBuffer.getNumSamples());
        }
  
        // Combine first and second buffers to output buffer
        for (int ch{}; ch < mFirstAmbAudioBuffer.getNumChannels(); ++ch) {
            mAmbAudioOutBuffer.copyFrom(ch, 0, mFirstAmbAudioBuffer, ch, 0, mFirstAmbAudioBuffer.getNumSamples());
        }
        if (mUseSecondSafAmb) {
            for (int ch{}; ch < mSecondAmbAudioBuffer.getNumChannels(); ++ch) {
                mAmbAudioOutBuffer.addFrom(ch, 0, mSecondAmbAudioBuffer, ch, 0, mSecondAmbAudioBuffer.getNumSamples());
            }
        }
        //const float gain = std::pow(10.0f, -10.8f / 20.0f);
        //mAmbAudioOutBuffer.applyGain(gain);
        for (int ch{}; ch < mAmbiNumOutputs; ++ch) {
            if (std::isnan(mAmbAudioOutBuffer.getRMSLevel(ch, 0, mAmbAudioOutBuffer.getNumSamples()))) {
                mSAFConfigureNeeded.set(true);
            }
        }
        if (mSAFConfigureNeeded.get()) {
            mAmbAudioOutBuffer.clear();
            mDirectOutAudioOutBuffer.clear();
        }
    }
}

//==============================================================================
void gris::AmbiEncSpatAlgorithm::configure(int ambisonicOrder)
{
    mAmbisonicOrder = ambisonicOrder;
    configureSAF();
}

//==============================================================================
void gris::AmbiEncSpatAlgorithm::copyAmbProcessedBuffer(juce::AudioBuffer<float> & destBuffer)
{
    ASSERT_AUDIO_THREAD;

    const auto numDirectOuts{ mDirectOutActiveChannels.size() };
    const auto totalNumChan{ mAmbiNumOutputs + numDirectOuts };

    jassert(destBuffer.getNumChannels() == totalNumChan);
    jassert(destBuffer.getNumSamples() == mAmbAudioOutBuffer.getNumSamples());

    for (int ch = 0; ch < mAmbiNumOutputs; ++ch)
        destBuffer.copyFrom(ch, 0, mAmbAudioOutBuffer, ch, 0, mAmbAudioOutBuffer.getNumSamples());
    for (int ambiChan = mAmbiNumOutputs, ch{}; ambiChan < totalNumChan; ++ambiChan, ++ch)
        destBuffer.copyFrom(ambiChan, 0, mDirectOutAudioOutBuffer, ch, 0, mDirectOutAudioOutBuffer.getNumSamples());
}

//==============================================================================
std::unique_ptr<AbstractSpatAlgorithm> AmbiEncSpatAlgorithm::make(SpeakerSetup const & speakerSetup,
                                                               SpatMode const & projectSpatMode,
                                                               SourcesData const & sources,
                                                               double const sampleRate,
                                                               int const bufferSize,
                                                               BinauralSettings & binauralSettings)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    return std::make_unique<AmbiEncSpatAlgorithm>(speakerSetup,
                                               projectSpatMode,
                                               sources,
                                               sampleRate,
                                               bufferSize,
                                               binauralSettings);
}

//==============================================================================
void AmbiEncSpatAlgorithm::showErrorMessage(juce::String & error)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                           "Unable to process ambisonic encoding.",
                                           error,
                                           "OK",
                                           nullptr,
                                           nullptr);
}

//==============================================================================
void AmbiEncSpatAlgorithm::configureSAF()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    for (auto const & channel : mSpeakerSetup.speakers) {
        if (channel.value->isDirectOutOnly) {
            mDirectOutActiveChannels.push_back(channel.key);
            continue;
        }
        mActiveChannels.push_back(channel.key);
    }

    mNumSpksToConvert = static_cast<int>(mActiveChannels.size());//mSpeakerSetup.speakers.size();
    mNumDirectOutSpks = static_cast<int>(mDirectOutActiveChannels.size());
    mNumSpksForFirstSafHAmb = mNumSpksToConvert < SAF_MAX_NUM_CHANNELS ? mNumSpksToConvert : SAF_MAX_NUM_CHANNELS;
    mNumSpksForSecondSafHAmb = mNumSpksToConvert > SAF_MAX_NUM_CHANNELS ? mNumSpksToConvert - SAF_MAX_NUM_CHANNELS : 0;
    mUseSecondSafAmb = mNumSpksForSecondSafHAmb > 0;

    ambi_enc_create(&mSafFirstHAmb);
    ambi_enc_init(mSafFirstHAmb, static_cast<int>(mSampleRate));
    ambi_enc_setOutputOrder(mSafFirstHAmb, (SH_ORDERS)mAmbisonicOrder);
    ambi_enc_setNormType(mSafFirstHAmb, NORM_N3D);
    //ambi_enc_setEnablePostScaling(mSafFirstHAmb, 0);
    ambi_enc_setChOrder(mSafFirstHAmb, CH_ORDER::CH_ACN);
    ambi_enc_setNumSources(mSafFirstHAmb, mNumSpksForFirstSafHAmb);

    if (mUseSecondSafAmb) {
        ambi_enc_create(&mSafSecondHAmb);
        ambi_enc_init(mSafSecondHAmb, static_cast<int>(mSampleRate));
        ambi_enc_setOutputOrder(mSafSecondHAmb, (SH_ORDERS)mAmbisonicOrder);
        ambi_enc_setNormType(mSafSecondHAmb, NORM_N3D);
        ambi_enc_setChOrder(mSafSecondHAmb, CH_ORDER::CH_ACN);
        ambi_enc_setNumSources(mSafSecondHAmb, mNumSpksForSecondSafHAmb);
    }

    mFrameSize = ambi_enc_getFrameSize();
    mAmbiNumOutputs = ambi_enc_getNSHrequired(mSafFirstHAmb);
    mDirectOutNumOutputs = static_cast<int>(mDirectOutActiveChannels.size());
    mHostBlockSize = mBufferSize;
    mUsingLowDelay = (mHostBlockSize % mFrameSize == 0); // in this case we can skip the FIFO buffering, since
    // block size is a multiple of the processing framesize

    jassert(mFrameSize > 0);
    jassert(mNumSpksForFirstSafHAmb >= 0);
    jassert(mAmbiNumOutputs >= 0);
    jassert(mHostBlockSize >= 0);

    mFirstInBuffers.resize(mNumSpksForFirstSafHAmb * mFrameSize);
    mFirstInBuffersPtrs.resize(mNumSpksForFirstSafHAmb);
    for (int ch = 0; ch < mNumSpksForFirstSafHAmb; ch++)
        mFirstInBuffersPtrs[ch] = &mFirstInBuffers[ch * mFrameSize];
    mFirstOutBuffers.resize(mAmbiNumOutputs * mFrameSize);
    mFirstOutBuffersPtrs.resize(mAmbiNumOutputs);
    for (int ch = 0; ch < mAmbiNumOutputs; ch++)
        mFirstOutBuffersPtrs[ch] = &mFirstOutBuffers[ch * mFrameSize];

    mFirstInPos = mFirstOutPos = mFirstAvailableOut = 0;
    if (!mFirstInBuffers.empty())
        std::fill(mFirstInBuffers.begin(), mFirstInBuffers.end(), 0.0f);
    if (!mFirstOutBuffers.empty())
        std::fill(mFirstOutBuffers.begin(), mFirstOutBuffers.end(), 0.0f);

    mFirstAmbAudioBuffer.setSize(mAmbiNumOutputs, mBufferSize);
    mFirstAmbAudioBuffer.clear();
    mAmbAudioOutBuffer.setSize(mAmbiNumOutputs, mBufferSize);
    mAmbAudioOutBuffer.clear();
    mDirectOutAudioOutBuffer.setSize(mNumDirectOutSpks, mBufferSize);
    mDirectOutAudioOutBuffer.clear();

    mPFrameData.resize(mNumSpksForFirstSafHAmb);
    mPFrameSecData.resize(mUseSecondSafAmb ? mNumSpksForSecondSafHAmb : 0);

    if (mUseSecondSafAmb) {
        mSecondInBuffers.resize(mNumSpksForSecondSafHAmb * mFrameSize);
        mSecondInBuffersPtrs.resize(mNumSpksForSecondSafHAmb);
        for (int ch = 0; ch < mNumSpksForSecondSafHAmb; ch++)
            mSecondInBuffersPtrs[ch] = &mSecondInBuffers[ch * mFrameSize];
        mSecondOutBuffers.resize(mAmbiNumOutputs * mFrameSize);
        mSecondOutBuffersPtrs.resize(mAmbiNumOutputs);
        for (int ch = 0; ch < mAmbiNumOutputs; ch++)
            mSecondOutBuffersPtrs[ch] = &mSecondOutBuffers[ch * mFrameSize];

        mSecondInPos = mSecondOutPos = mSecondAvailableOut = 0;
        if (!mSecondInBuffers.empty())
            std::fill(mSecondInBuffers.begin(), mSecondInBuffers.end(), 0.0f);
        if (!mSecondOutBuffers.empty())
            std::fill(mSecondOutBuffers.begin(), mSecondOutBuffers.end(), 0.0f);

        mSecondAmbAudioBuffer.setSize(mAmbiNumOutputs, mBufferSize);
        mSecondAmbAudioBuffer.clear();
    }

    // Set position of speakers for ambisonic encoding
    // auto gainAdjust{ std::sqrt(mNumSpksToConvert) };
    // gainAdjust = gainAdjust == 0 ? 1.0f : gainAdjust;
    int spkChan2SAFIndex{};
    for (auto const & speaker : mSpeakerSetup.speakers) {
        if (speaker.value->isDirectOutOnly) {
            continue;
        }

        float azi = speaker.value->position.getPolar().azimuth.getAsRadians() - (PI.get() / 2);
        while (azi <= -PI.get())
            azi += TWO_PI.get();
        while (azi > PI.get())
            azi -= TWO_PI.get();

        azi = radians_t{ azi }.getAsDegrees();
        float elev = speaker.value->position.getPolar().elevation.getAsDegrees();

        if (spkChan2SAFIndex < mNumSpksForFirstSafHAmb) {
            ambi_enc_setSourceAzi_deg(mSafFirstHAmb, spkChan2SAFIndex, azi);
            ambi_enc_setSourceElev_deg(mSafFirstHAmb, spkChan2SAFIndex, elev);
            //ambi_enc_setSourceGain(mSafFirstHAmb, spkChan2SAFIndex, static_cast<float>(gainAdjust));
        }
        else {
            auto const index{ spkChan2SAFIndex - mNumSpksForFirstSafHAmb };
            ambi_enc_setSourceAzi_deg(mSafSecondHAmb, index, azi);
            ambi_enc_setSourceElev_deg(mSafSecondHAmb, index, elev);
            //ambi_enc_setSourceGain(mSafSecondHAmb, index, static_cast<float>(gainAdjust));
        }
        spkChan2SAFIndex++;
    }
 
    startTimer(80);
    mSAFConfigureNeeded.set(false);
}

//==============================================================================
void gris::AmbiEncSpatAlgorithm::timerCallback()
{
    if (mSAFConfigureNeeded.get()) {
        auto msg{ juce::String(juce::String("Unable to configure the ambisonic encoder")) };
        showErrorMessage(msg);
        stopTimer();
    }
}

} // namespace gris
