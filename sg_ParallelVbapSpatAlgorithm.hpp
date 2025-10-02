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
#include "Implementations/sg_vbap.hpp"
#include "sg_AbstractSpatAlgorithm.hpp"
#include "sg_VbapSpatAlgorithm.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "tl/optional.hpp"
#include <memory>

namespace gris
{

//==============================================================================
/**
 * Vbap spatialization algorithm parallelized with fork_union.
 */
class ParallelVbapSpatAlgorithm final
    : public ParallelAlgorithm
    , public VbapSpatAlgorithm
{
public:
    //==============================================================================
    ParallelVbapSpatAlgorithm() = delete;
    ~ParallelVbapSpatAlgorithm() override = default;
    ParallelVbapSpatAlgorithm(SpeakersData const & speakers,
                              std::vector<source_index_t> theSourceIds,
                              unsigned int numberOfThreads);
    /**
     * instanciate without numberOfThreads gets half the hardware thread. This is
     * a hack so that hybrid can instanciate without knowing the type...
     */
    ParallelVbapSpatAlgorithm(SpeakersData const & speakers, std::vector<source_index_t> sid);
    // SG_DELETE_COPY_AND_MOVE(ParallelVbapSpatAlgorithm)
    //==============================================================================
    void process(AudioConfig const & config,
                 SourceAudioBuffer & sourcesBuffer,
                 SpeakerAudioBuffer & speakersBuffer,
                 juce::AudioBuffer<float> & stereoBuffer,
                 SourcePeaks const & sourcePeaks,
                 SpeakersAudioConfig const * altSpeakerConfig) override;
    //==============================================================================
    static std::unique_ptr<AbstractSpatAlgorithm>
        make(SpeakerSetup const & speakerSetup, std::vector<source_index_t> theSourceIds, unsigned int numberOfThreads);

private:
    void processSource(const gris::AudioConfig & config,
                       const gris::source_index_t & sourceId,
                       const gris::SourcePeaks & sourcePeaks,
                       gris::SourceAudioBuffer & sourcesBuffer,
                       const gris::SpeakersAudioConfig & speakersAudioConfig,
                       SpeakerAudioBuffer & speakersBuffer);

    std::vector<source_index_t> sourceIds;

    JUCE_LEAK_DETECTOR(ParallelVbapSpatAlgorithm)
};

} // namespace gris
