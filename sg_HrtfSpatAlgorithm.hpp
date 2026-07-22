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

#pragma once

#include "sg_AbstractSpatAlgorithm.hpp"
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
#if SAF_USE_OPEN_BLAS_AND_LAPACKE
    #include <complex>
    #define lapack_complex_float std::complex<float>
    #define lapack_complex_double std::complex<double>
#endif
#include "binauraliser_nf.h"
#include "saf.h"
#include "saf_externals.h"

namespace gris
{
//==============================================================================
constexpr auto SAF_MAX_NUM_CHANNELS = MAX_NUM_CHANNELS;
constexpr int SAF_MAX_RECONFIGURATION_ATTEMPTS = 5;
//==============================================================================
/** A head-related-transfer-function based stereo reduction algorithm.
 *
 * This uses either libspatialaudio or Spatial_Audio_Framework internally to process SOFA files, converting
 * the audio output from SpatGRIS's spatialization algorithms into binaural
 * https://github.com/videolan/libspatialaudio
 * https://github.com/leomccormack/Spatial_Audio_Framework
 */
class HrtfSpatAlgorithm final
    : public AbstractSpatAlgorithm
    , juce::Timer
{
    std::unique_ptr<AbstractSpatAlgorithm> mInnerAlgorithm{};
    const SpeakerSetup & mSpeakerSetup;
    int mBufferSize;
    double mSampleRate;
    juce::File mSofaFile{};

    // Spatial_Audio_Framework
    juce::Atomic<bool> mSAFConfigureNeeded{ true };
    int mSAFReconfigureAttempts{};
    bool mSAFConfigurationChecksDone{ false };

    bool mUseDefaultHRIRs{};
    bool mEnableHRIRsDiffuseEQ{ true };
    int mNumSpksToConvert{ 0 };
    int mNumSpksForFirstSafHBin{ 0 };
    int mNumSpksForSecondSafHBin{ 0 };
    bool mUseSecondSafHBin{};

    // binauraliser handles. Each of them can process up to 128 channels
    void * mSafFirstHBin;
    void * mSafSecondHBin;

    int mFrameSize{ 0 };
    int mNumOutputs{ 2 };
    int mHostBlockSize{ 0 };
    bool mUsingLowDelay{ false };

    StaticVector<output_patch_t, MAX_NUM_SPEAKERS> mActiveChannels;
    std::vector<float *> mPFrameData;
    std::vector<float *> mPFrameSecData;

    int mFirstInPos = 0;
    int mFirstOutPos = 0;
    int mFirstAvailableOut = 0;
    int mSecondInPos = 0;
    int mSecondOutPos = 0;
    int mSecondAvailableOut = 0;

    std::vector<float> mFirstInBuffers;
    std::vector<float> mFirstOutBuffers;
    std::vector<float *> mFirstInBuffersPtrs;
    std::vector<float *> mFirstOutBuffersPtrs;
    std::vector<float> mSecondInBuffers;
    std::vector<float> mSecondOutBuffers;
    std::vector<float *> mSecondInBuffersPtrs;
    std::vector<float *> mSecondOutBuffersPtrs;

    juce::AudioBuffer<float> mFirstStereoBuffer;
    juce::AudioBuffer<float> mSecondStereoBuffer;

public:
    //==============================================================================
    /** Note: You should never use this function directly. Use HrtfSpatAlgorithm::make() instead. */
    HrtfSpatAlgorithm(SpeakerSetup const & speakerSetup,
                      SpatMode const & projectSpatMode,
                      SourcesData const & sources,
                      double sampleRate,
                      int bufferSize,
                      BinauralSettings & binauralSettings);
    //==============================================================================
    HrtfSpatAlgorithm() = delete;
    ~HrtfSpatAlgorithm() override;
    SG_DELETE_COPY_AND_MOVE(HrtfSpatAlgorithm)
    //==============================================================================
    void updateSpatData(source_index_t sourceIndex, SourceData const & sourceData) noexcept override;
    void process(AudioConfig const & config,
                 SourceAudioBuffer & sourcesBuffer,
                 SpeakerAudioBuffer & speakersBuffer,
                 juce::AudioBuffer<float> & stereoBuffer,
                 SourcePeaks const & sourcePeaks,
                 SpeakersAudioConfig const * altSpeakerConfig) noexcept override;
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override;
    [[nodiscard]] bool hasTriplets() const noexcept override { return mInnerAlgorithm->hasTriplets(); }
    [[nodiscard]] tl::optional<Error> getError() const noexcept override { return tl::nullopt; }
    //==============================================================================
    /** Instantiates an HRTF algorithm. This should never fail. */
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
    void timerCallback() override;

    //==============================================================================
    JUCE_LEAK_DETECTOR(HrtfSpatAlgorithm)
};

} // namespace gris
