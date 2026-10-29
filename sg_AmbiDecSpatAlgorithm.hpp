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

#pragma once

#include "sg_AbstractSpatAlgorithm.hpp"
#include "sg_LapackComplexTypes.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "juce_dsp/juce_dsp.h"
#include <Containers/sg_StrongArray.hpp>
#include <Containers/sg_TaggedAudioBuffer.hpp>
#include <Data/StrongTypes/sg_OutputPatch.hpp>
#include <Data/StrongTypes/sg_SourceIndex.hpp>
#include <Data/sg_AudioStructs.hpp>
#include <Data/sg_LogicStrucs.hpp>
#include <Data/sg_Macros.hpp>
#include <Data/sg_SpatMode.hpp>
#include <Data/sg_Triplet.hpp>
#include <Data/sg_constants.hpp>
#include <tl/optional.hpp>
#include <array>
#include <memory>
#include "ambi_dec.h"
#include "saf.h"
#include "saf_externals.h"

namespace gris
{
//==============================================================================
/** An ambisonic algorithm
 *
 * This uses Spatial_Audio_Framework internally to provide ambisonic decoding
 * capabilities to SpatGRIS.
 * https://github.com/leomccormack/Spatial_Audio_Framework
 */
class AmbiDecSpatAlgorithm final
    : public AbstractSpatAlgorithm
    , juce::Timer
{
    const SpeakerSetup & mSpeakerSetup;
    int mBufferSize;
    double mSampleRate;

    // Spatial_Audio_Framework
    juce::Atomic<bool> mSAFConfigureNeeded{ true };
    int mSAFReconfigureAttempts{};
    bool mSAFConfigurationChecksDone{ false };

    int mAmbisonicOrder{ 1 };
    int mNumAmbiInputs{ 0 };
    int mNumDirectOutInputs{ 0 };
    int mNumAudioOutSpks{ 0 };
    int mNumChannelsForSafHAmb{ 0 };

    // Ambisonic handle. It can process up to 128 channels.
    void * mSafHAmb;

    int mFrameSize{ 0 };
    int mHostBlockSize{ 0 };
    bool mUsingLowDelay{ false };

    std::vector<std::pair<source_index_t, output_patch_t>> mInitialDirectOutPairsFromPlayer{};
    StaticVector<source_index_t, MAX_NUM_SPEAKERS> mActiveInputChannels;
    StaticVector<source_index_t, MAX_NUM_SPEAKERS> mDirectOutActiveInputChannels;
    StaticVector<output_patch_t, MAX_NUM_SPEAKERS> mActiveOutputChannels;
    StaticVector<output_patch_t, MAX_NUM_SPEAKERS> mDirectOutActiveOutputChannels;
    std::vector<float *> mPFrameData;

    int mFirstInPos = 0;
    int mFirstOutPos = 0;
    int mFirstAvailableOut = 0;

    std::vector<float> mFirstInBuffers;
    std::vector<float> mFirstOutBuffers;
    std::vector<float *> mFirstInBuffersPtrs;
    std::vector<float *> mFirstOutBuffersPtrs;

    juce::AudioBuffer<float> mAmbAudioInBuffer;
    juce::AudioBuffer<float> mDirectOutAudioInBuffer;
    juce::AudioBuffer<float> mAudioOutBuffer;

public:
    //==============================================================================
    /** Note: You should never use this function directly. Use AmbiDecSpatAlgorithm::make() instead. */
    AmbiDecSpatAlgorithm(SpeakerSetup const & speakerSetup,
                         SpatMode const & projectSpatMode,
                         SourcesData const & sources,
                         double sampleRate,
                         int bufferSize,
                         BinauralSettings & binauralSettings);
    //==============================================================================
    AmbiDecSpatAlgorithm() = delete;
    ~AmbiDecSpatAlgorithm() override;
    SG_DELETE_COPY_AND_MOVE(AmbiDecSpatAlgorithm)
    //==============================================================================
    void updateSpatData(source_index_t /*sourceIndex*/, SourceData const & /*sourceData*/) noexcept override
    {
        jassertfalse;
    }
    /** This algorithm is only used for the ambisonic player.
     * sourcesBuffer and speakersBuffer are used as audio output
     * - sourcesBuffer is used by the Stereo reduction algorithm
     * - speakersBuffer is used by the hrtf (binaural) algorithm or when there is no stereo reduction
     * SG audio spatialization processing should be skipped.
     */
    void process(AudioConfig const & config,
                 SourceAudioBuffer & sourcesBuffer,
                 SpeakerAudioBuffer & speakersBuffer,
                 juce::AudioBuffer<float> & stereoBuffer,
                 SourcePeaks const & sourcePeaks,
                 SpeakersAudioConfig const * altSpeakerConfig) noexcept override;
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override { return juce::Array<Triplet>(); };
    [[nodiscard]] bool hasTriplets() const noexcept override { return false; }
    [[nodiscard]] tl::optional<Error> getError() const noexcept override { return tl::nullopt; }
    //==============================================================================
    void configure(int numAmbisonicChannels, int numDirectOutChannels);
    void copyAmbPlayerBuffer(juce::AudioBuffer<float> & inputBuffer);
    void setOrder(int newOrder);
    std::vector<std::pair<source_index_t, output_patch_t>> & getInitialDirectOutPairsFromPlayer();
    //==============================================================================
    /** Instantiates an ambisonic algorithm. This should never fail. */
    static std::unique_ptr<AbstractSpatAlgorithm> make(SpeakerSetup const & speakerSetup,
                                                       SpatMode const & projectSpatMode,
                                                       SourcesData const & sources,
                                                       double sampleRate,
                                                       int bufferSize,
                                                       BinauralSettings & binauralSettings);

private:
    //==============================================================================
    void showErrorMessage(juce::String & error);
    void configureSAF();
    void reconfigureSAF();
    void refreshSettings();
    void timerCallback() override;

    //==============================================================================
    JUCE_LEAK_DETECTOR(AmbiDecSpatAlgorithm)
};

} // namespace gris
