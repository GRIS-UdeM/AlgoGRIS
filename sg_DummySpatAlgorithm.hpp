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

#include "Containers/sg_TaggedAudioBuffer.hpp"
#include "Data/StrongTypes/sg_SourceIndex.hpp"
#include "Data/sg_AudioStructs.hpp"
#include "Data/sg_LogicStrucs.hpp"
#include "Data/sg_Macros.hpp"
#include "Data/sg_Triplet.hpp"
#include "sg_AbstractSpatAlgorithm.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "tl/optional.hpp"

namespace gris
{
//==============================================================================
/** A dummy spatialization algorithm created when the instantiation of another algorithm fails.
 *
 * It holds an error and removes the possibility of having no active algorithms when a problem occurs.  */
class DummySpatAlgorithm final : public AbstractSpatAlgorithm
{
    Error mError;

public:
    //==============================================================================
    explicit DummySpatAlgorithm(Error const error) : mError(error) {}
    ~DummySpatAlgorithm() override = default;
    SG_DELETE_COPY_AND_MOVE(DummySpatAlgorithm)
    //==============================================================================
    void updateSpatData(source_index_t /*sourceIndex*/, SourceData const & /*sourceData*/) noexcept override {}
    void process(AudioConfig const & /*config*/,
                 SourceAudioBuffer & /*sourcesBuffer*/,
                 SpeakerAudioBuffer & /*speakersBuffer*/,
#if USE_FORK_UNION
    #if FU_METHOD == FU_USE_ATOMIC_WRAPPER
                 AtomicSpeakerBuffer & /*atomicSpeakerBuffer*/,
    #elif FU_METHOD == FU_USE_BUFFER_PER_THREAD
                 ThreadSpeakerBuffer & /*threadSpeakerBuffer*/,
    #endif
#endif
                 juce::AudioBuffer<float> & /*stereoBuffer*/,
                 SourcePeaks const & /*sourcePeaks*/,
                 SpeakersAudioConfig const * /*altSpeakerConfig*/) override
    {
    }
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override { return juce::Array<Triplet>{}; }
    [[nodiscard]] bool hasTriplets() const noexcept override { return false; }
    [[nodiscard]] tl::optional<Error> getError() const noexcept override { return mError; }

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(DummySpatAlgorithm)
};

} // namespace gris
