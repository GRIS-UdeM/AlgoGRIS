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

#include "sg_ParallelMbapSpatAlgorithm.hpp"
#include "Containers/sg_StaticMap.hpp"
#include "Containers/sg_StrongArray.hpp"
#include "Containers/sg_TaggedAudioBuffer.hpp"
#include "Data/StrongTypes/sg_SourceIndex.hpp"
#include "Data/sg_AudioStructs.hpp"
#include "Data/sg_LogicStrucs.hpp"
#include "Data/sg_Narrow.hpp"
#include "Data/sg_Triplet.hpp"
#include "Implementations/sg_mbap.hpp"
#include "sg_AbstractSpatAlgorithm.hpp"
#include "sg_DummySpatAlgorithm.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "juce_events/juce_events.h"
#include <cmath>
#include <cassert>
#include <cstdlib>
#include <memory>

namespace gris
{

  ParallelMbapSpatAlgorithm::ParallelMbapSpatAlgorithm(SpeakerSetup const & speakerSetup, std::vector<source_index_t> srcIds):
    sourceIds(srcIds),
    MbapSpatAlgorithm(speakerSetup, std::move(srcIds))
  {}

//==============================================================================
void ParallelMbapSpatAlgorithm::process(AudioConfig const & config,
                                SourceAudioBuffer & sourcesBuffer,
                                SpeakerAudioBuffer & speakersBuffer,
                                [[maybe_unused]] juce::AudioBuffer<float> & stereoBuffer,
                                SourcePeaks const & sourcePeaks,
                                SpeakersAudioConfig const * altSpeakerConfig) [[clang::nonblocking]]
{
    ASSERT_AUDIO_THREAD;

    auto const & speakersAudioConfig{ altSpeakerConfig ? *altSpeakerConfig : config.speakersAudioConfig };

    namespace fu = ashvardanian::fork_union;

    jassert(sourceIds.size() > 0);

    threadPool.for_n(sourceIds.size(), [&](fu::prong_t prong) noexcept {
        processSource(config,
                      sourceIds[prong.task],
                      sourcePeaks,
                      sourcesBuffer,
                      speakersAudioConfig,
                      speakersBuffer);
    });
    // sleep with 1us periodicity
    threadPool.sleep(1);
    std::cout << "cruuuunch" << "\n";

}

inline void ParallelMbapSpatAlgorithm::processSource(const gris::AudioConfig & config,
                                             const gris::source_index_t & sourceId,
                                             const gris::SourcePeaks & sourcePeaks,
                                             gris::SourceAudioBuffer & sourceBuffer,
                                             const gris::SpeakersAudioConfig & speakersAudioConfig,
                                             gris::SpeakerAudioBuffer & speakerBuffers)
{
    auto const & source = config.sourcesAudioConfig[sourceId];
    if (source.isMuted || source.directOut || sourcePeaks[sourceId] < SMALL_GAIN) {
        // speaker silent
        return;
    }

    auto & data{ mData[sourceId] };
    data.dataQueue.getMostRecent(data.currentData);
    if (data.currentData == nullptr) {
        // no spat data
        return;
    }

    auto const numSamples{ sourceBuffer.getNumSamples() };
    auto const & spatData{ data.currentData->get() };
    auto & lastGains{ data.lastGains };
    auto const & targetGains{ spatData.gains };
    auto const & gainInterpolation{ config.spatGainsInterpolation };
    auto const gainFactor{ std::pow(gainInterpolation, 0.1f) * 0.0099f + 0.99f };

    // process attenuation if Player does not exist
    auto * inputSamples{ sourceBuffer[sourceId].getWritePointer(0) };
    if (config.mbapAttenuationConfig.shouldProcess) {
        config.mbapAttenuationConfig.process(inputSamples,
                                             numSamples,
                                             spatData.mbapSourceDistance,
                                             data.attenuationState);
    }

    // Process spatialization
    [[maybe_unused]] size_t i = 0;
    for (auto const & speaker : speakersAudioConfig) {
        if (speaker.value.isMuted || speaker.value.isDirectOutOnly || speaker.value.gain < SMALL_GAIN) {
            // speaker silent
            continue;
        }

        auto & currentGain{ lastGains[speaker.key] };
        auto const & targetGain{ targetGains[speaker.key] };
        auto const gainDiff{ targetGain - currentGain };
        auto const gainSlope{ gainDiff / narrow<float>(numSamples) };

        auto * outputSamples{ speakerBuffers[speaker.key].getWritePointer(0) };
        if (juce::approximatelyEqual(gainSlope, 0.f) || std::abs(gainDiff) < SMALL_GAIN) {
            // no interpolation
            currentGain = targetGain;
            if (currentGain >= SMALL_GAIN) {
                for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex)
                    std::atomic_ref<float>(outputSamples[sampleIndex]).fetch_add(inputSamples[sampleIndex] * currentGain, std::memory_order::relaxed);
            }
            continue;
        }

        // interpolation necessary
        if (juce::approximatelyEqual(gainInterpolation, 0.f)) {
            // linear interpolation over buffer size
            for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
                currentGain += gainSlope;
                std::atomic_ref<float>(outputSamples[sampleIndex]).fetch_add(inputSamples[sampleIndex] * currentGain);
            }
        } else {
            // log interpolation with 1st order filter
            if (targetGain < SMALL_GAIN) {
                // targeting silence
                for (int sampleIndex{}; sampleIndex < numSamples && currentGain >= SMALL_GAIN; ++sampleIndex) {
                    currentGain = targetGain + (currentGain - targetGain) * gainFactor;
                    std::atomic_ref<float>(outputSamples[sampleIndex]).fetch_add(inputSamples[sampleIndex] * currentGain);
                }
                continue;
            }

            // not targeting silence
            for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
                currentGain = (currentGain - targetGain) * gainFactor + targetGain;
                std::atomic_ref<float>(outputSamples[sampleIndex]).fetch_add(inputSamples[sampleIndex] * currentGain);
            }
        }
    }
}

} // namespace gris
