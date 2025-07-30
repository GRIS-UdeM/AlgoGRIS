/*
 This file is part of SpatGRIS.

 Developers: Samuel Béland, Olivier Bélanger, Nicolas Masson

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

#include "sg_HybridSpatAlgorithm.hpp"
#include "sg_DummySpatAlgorithm.hpp"

namespace gris
{
//==============================================================================
HybridSpatAlgorithm::HybridSpatAlgorithm(SpeakerSetup const & speakerSetup, std::vector<source_index_t> && sourceIds)
    : mVbap(std::make_unique<VbapSpatAlgorithm>(speakerSetup.speakers, std::move(sourceIds)))
    , mMbap(std::make_unique<MbapSpatAlgorithm>(speakerSetup, std::move(sourceIds)))
{
}

//==============================================================================
void HybridSpatAlgorithm::updateSpatData(source_index_t const sourceIndex, SourceData const & sourceData) noexcept
{
    if (!sourceData.position.has_value()) {
        // resetting a position should reset both algorithms
        mVbap->updateSpatData(sourceIndex, sourceData);
        mMbap->updateSpatData(sourceIndex, sourceData);
        return;
    }

    // Valid position: only send to the right algorithm
    switch (sourceData.hybridSpatMode) {
    case SpatMode::vbap:
        mVbap->updateSpatData(sourceIndex, sourceData);
        return;
    case SpatMode::mbap:
        mMbap->updateSpatData(sourceIndex, sourceData);
        return;
    case SpatMode::hybrid:
    case SpatMode::invalid:
        break;
    }
    jassertfalse;
}

//==============================================================================
void HybridSpatAlgorithm::process(AudioConfig const & config,
                                  SourceAudioBuffer & sourcesBuffer,
                                  SpeakerAudioBuffer & speakersBuffer,
#if SG_USE_FORK_UNION && (SG_FU_METHOD == SG_FU_USE_ARRAY_OF_ATOMICS || SG_FU_METHOD == SG_FU_USE_BUFFER_PER_THREAD)
                                  ForkUnionBuffer & forkUnionBuffer,
#endif
                                  juce::AudioBuffer<float> & stereoBuffer,
                                  SourcePeaks const & sourcePeaks,
                                  SpeakersAudioConfig const * altSpeakerConfig)
{
#if SG_USE_FORK_UNION && (SG_FU_METHOD == SG_FU_USE_ARRAY_OF_ATOMICS || SG_FU_METHOD == SG_FU_USE_BUFFER_PER_THREAD)
    mVbap->process(config, sourcesBuffer, speakersBuffer, forkUnionBuffer, stereoBuffer, sourcePeaks, altSpeakerConfig);
    mMbap->process(config, sourcesBuffer, speakersBuffer, forkUnionBuffer, stereoBuffer, sourcePeaks, altSpeakerConfig);
#else
    mVbap->process(config, sourcesBuffer, speakersBuffer, stereoBuffer, sourcePeaks, altSpeakerConfig);
    mMbap->process(config, sourcesBuffer, speakersBuffer, stereoBuffer, sourcePeaks, altSpeakerConfig);
#endif
}

//==============================================================================
juce::Array<Triplet> HybridSpatAlgorithm::getTriplets() const noexcept
{
    return mVbap->getTriplets();
}

//==============================================================================
bool HybridSpatAlgorithm::hasTriplets() const noexcept
{
    return true;
}

//==============================================================================
tl::optional<AbstractSpatAlgorithm::Error> HybridSpatAlgorithm::getError() const noexcept
{
    // It seems this always return nullopt...
    return mVbap->getError().disjunction(mMbap->getError());
}

//==============================================================================
std::unique_ptr<AbstractSpatAlgorithm> HybridSpatAlgorithm::make(SpeakerSetup const & speakerSetup,
                                                                 std::vector<source_index_t> && sourceIds)
{
    if (speakerSetup.numOfSpatializedSpeakers() < 3) {
        return std::make_unique<DummySpatAlgorithm>(Error::notEnoughDomeSpeakers);
    }
    return std::make_unique<HybridSpatAlgorithm>(speakerSetup, std::move(sourceIds));
}

} // namespace gris
