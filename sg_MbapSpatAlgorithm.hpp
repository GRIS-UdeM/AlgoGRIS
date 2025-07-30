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

#include "Containers/sg_AtomicUpdater.hpp"
#include "Containers/sg_StrongArray.hpp"
#include "Containers/sg_TaggedAudioBuffer.hpp"
#include "Data/StrongTypes/sg_SourceIndex.hpp"
#include "Data/sg_AudioStructs.hpp"
#include "Data/sg_LogicStrucs.hpp"
#include "Data/sg_Macros.hpp"
#include "Data/sg_Triplet.hpp"
#include "Data/sg_constants.hpp"
#include "Implementations/sg_mbap.hpp"
#include "sg_AbstractSpatAlgorithm.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "tl/optional.hpp"
#include <memory>

namespace gris
{
struct MbapSpatData {
    SpeakersSpatGains gains{};
    float mbapSourceDistance{};
};

using MbapSpatDataQueue = AtomicUpdater<MbapSpatData>;

struct MbapSourceData {
    MbapSpatDataQueue dataQueue{};
    MbapSpatDataQueue::Token * currentData{};
    MbapSourceAttenuationState attenuationState{};
    SpeakersSpatGains lastGains{};
};

//==============================================================================
class MbapSpatAlgorithm final : public AbstractSpatAlgorithm
{
    MbapField mField{};
    StrongArray<source_index_t, MbapSourceData, MAX_NUM_SOURCES> mData{};

public:
    //==============================================================================
    MbapSpatAlgorithm() = delete;
    ~MbapSpatAlgorithm() override = default;
    SG_DELETE_COPY_AND_MOVE(MbapSpatAlgorithm)
    //==============================================================================
    explicit MbapSpatAlgorithm(SpeakerSetup const & speakerSetup, std::vector<source_index_t> && sourceIds);
    //==============================================================================
    void updateSpatData(source_index_t sourceIndex, SourceData const & sourceData) noexcept override;
    void process(AudioConfig const & config,
                 SourceAudioBuffer & sourceBuffer,
                 SpeakerAudioBuffer & speakersBuffer,
#if USE_FORK_UNION && (FU_METHOD == FU_USE_ARRAY_OF_ATOMICS || FU_METHOD == FU_USE_BUFFER_PER_THREAD)
                 ForkUnionBuffer & forkUnionBuffer,
#endif
                 juce::AudioBuffer<float> & stereoBuffer,
                 SourcePeaks const & sourcesPeaks,
                 SpeakersAudioConfig const * altSpeakerConfig) override;
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override;
    [[nodiscard]] bool hasTriplets() const noexcept override { return false; }
    [[nodiscard]] tl::optional<Error> getError() const noexcept override { return tl::nullopt; }
    //==============================================================================
    static std::unique_ptr<AbstractSpatAlgorithm> make(SpeakerSetup const & speakerSetup,
                                                       std::vector<source_index_t> && sourceIds);

private:
    void processSource(const gris::AudioConfig & config,
                       const gris::source_index_t & sourceId,
                       const gris::SourcePeaks & sourcePeaks,
                       gris::SourceAudioBuffer & sourcesBuffer,
                       const gris::SpeakersAudioConfig & speakersAudioConfig,
#if USE_FORK_UNION
    #if FU_METHOD == FU_USE_ARRAY_OF_ATOMICS
                       ForkUnionBuffer & forkUnionBuffer,
    #elif FU_METHOD == FU_USE_BUFFER_PER_THREAD
                       std::vector<std::vector<float>> & speakerBuffer,
    #endif
#endif
                       gris::SpeakerAudioBuffer & speakerBuffers);

#if USE_FORK_UNION
    std::vector<source_index_t> sourceIds;
#endif

    JUCE_LEAK_DETECTOR(MbapSpatAlgorithm)
};

void CopyForkUnionBuffer(const gris::SpeakersAudioConfig & speakersAudioConfig,
                         gris::SourceAudioBuffer & sourcesBuffer,
                         gris::SpeakerAudioBuffer & speakersBuffer,
                         gris::ForkUnionBuffer & forkUnionBuffer);

} // namespace gris
