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

#include "sg_ParallelVbapSpatAlgorithm.hpp"
// needs to be included after ParallelMbapSpatAlgorithm or UNSAFE_SLEEP won't be defined yet.
// some compiler seem to choke on the non-nested version of this.
#if UNSAFE_SLEEP && defined(__has_feature)
    #if defined __has_feature(realtime_sanitizer)
        #include <sanitizer/rtsan_interface.h>
    #endif
#endif

#include "Containers/sg_StaticMap.hpp"
#include "Containers/sg_StrongArray.hpp"
#include "Containers/sg_TaggedAudioBuffer.hpp"
#include "Data/StrongTypes/sg_OutputPatch.hpp"
#include "Data/StrongTypes/sg_Radians.hpp"
#include "Data/StrongTypes/sg_SourceIndex.hpp"
#include "Data/sg_AudioStructs.hpp"
#include "Data/sg_LogicStrucs.hpp"
#include "Data/sg_Narrow.hpp"
#include "Data/sg_Triplet.hpp"
#include "Data/sg_constants.hpp"
#include "Implementations/sg_vbap.hpp"
#include "sg_AbstractSpatAlgorithm.hpp"
#include "sg_DummySpatAlgorithm.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "juce_events/juce_events.h"
#include <cmath>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>

namespace gris
{

//==============================================================================
ParallelVbapSpatAlgorithm::ParallelVbapSpatAlgorithm(SpeakersData const & speakers,
                                                     [[maybe_unused]] std::vector<source_index_t> srcIds,
                                                     unsigned int numberOfThreads)
    : ParallelAlgorithm(numberOfThreads)
    , VbapSpatAlgorithm(speakers, srcIds)
    , sourceIds{ srcIds }
{
}

ParallelVbapSpatAlgorithm::ParallelVbapSpatAlgorithm(SpeakersData const & speakers, std::vector<source_index_t> srcIds)
    : ParallelVbapSpatAlgorithm(speakers, srcIds, std::thread::hardware_concurrency())
{
}

//==============================================================================
void ParallelVbapSpatAlgorithm::process(AudioConfig const & config,
                                        SourceAudioBuffer & sourcesBuffer,
                                        SpeakerAudioBuffer & speakersBuffer,
                                        juce::AudioBuffer<float> & /*stereoBuffer*/,
                                        SourcePeaks const & sourcePeaks,
                                        SpeakersAudioConfig const * altSpeakerConfig) noexcept NONBLOCKING
{
    ASSERT_AUDIO_THREAD;

    auto const & speakersAudioConfig{ altSpeakerConfig ? *altSpeakerConfig : config.speakersAudioConfig };

    namespace fu = ashvardanian::fork_union;

    jassert(sourceIds.size() > 0);
#if THREAD_WAIT_METHOD == SPIN_SLEEP
    SpinSleepWait::resetStates();
#endif
#if UNSAFE_SLEEP && defined(__has_feature)
    #if defined __has_feature(realtime_sanitizer)
    __rtsan::ScopedDisabler disableRealtimeWarnings;
    #endif
#endif
    threadPool.for_n(sourceIds.size(), [&](fu::prong_t prong) noexcept {
        jassert(threadPool.is_lock_free());

        processSource(config, sourceIds[prong.task], sourcePeaks, sourcesBuffer, speakersAudioConfig, speakersBuffer);
    });
#if THREAD_WAIT_METHOD == SLEEP
    threadPool.sleep(1);
#endif
}

inline void ParallelVbapSpatAlgorithm::processSource(const gris::AudioConfig & config,
                                                     const gris::source_index_t & sourceId,
                                                     const gris::SourcePeaks & sourcePeaks,
                                                     gris::SourceAudioBuffer & sourcesBuffer,
                                                     const gris::SpeakersAudioConfig & speakersAudioConfig,
                                                     SpeakerAudioBuffer & speakerBuffers)
{
    auto const & source = config.sourcesAudioConfig[sourceId];
    if (source.isMuted || source.directOut || sourcePeaks[sourceId] < SMALL_GAIN) {
        // source silent
        return;
    }

    auto & data{ mData[sourceId] };
    data.spatDataQueue.getMostRecent(data.currentSpatData);
    if (data.currentSpatData == nullptr) {
        // no spat data
        return;
    }

    auto const numSamples{ sourcesBuffer.getNumSamples() };
    auto const & gains{ data.currentSpatData->get() };
    auto & lastGains{ data.lastGains };
    auto const * inputSamples{ sourcesBuffer[sourceId].getReadPointer(0) };
    auto const & gainInterpolation{ config.spatGainsInterpolation };
    auto const gainFactor{ std::pow(gainInterpolation, 0.1f) * 0.0099f + 0.99f };

    [[maybe_unused]] size_t i = 0;
    for (auto const & speaker : speakersAudioConfig) {
        if (speaker.value.isMuted || speaker.value.isDirectOutOnly || speaker.value.gain < SMALL_GAIN) {
            // speaker silent
            continue;
        }

        auto & currentGain{ lastGains[speaker.key] };
        auto const & targetGain{ gains[speaker.key] };
        auto const gainDiff{ targetGain - currentGain };
        auto const gainSlope{ gainDiff / narrow<float>(numSamples) };

        auto * outputSamples{ speakerBuffers[speaker.key].getWritePointer(0) };

        if (juce::approximatelyEqual(gainSlope, 0.f) || std::abs(gainDiff) < SMALL_GAIN) {
            // no interpolation
            currentGain = targetGain;
            if (currentGain >= SMALL_GAIN) {
                for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex)
                    std::atomic_ref<float>(outputSamples[sampleIndex])
                        .fetch_add(inputSamples[sampleIndex] * currentGain, std::memory_order::relaxed);
            }
            continue;
        }

        // interpolation necessary
        if (juce::approximatelyEqual(gainInterpolation, 0.f)) {
            // linear interpolation over buffer size
            for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
                currentGain += gainSlope;
                std::atomic_ref<float>(outputSamples[sampleIndex])
                    .fetch_add(inputSamples[sampleIndex] * currentGain, std::memory_order::relaxed);
            }
        } else {
            // log interpolation with 1st order filter
            if (targetGain < SMALL_GAIN) {
                // targeting silence
                for (int sampleIndex{}; sampleIndex < numSamples && currentGain >= SMALL_GAIN; ++sampleIndex) {
                    currentGain = targetGain + (currentGain - targetGain) * gainFactor;
                    std::atomic_ref<float>(outputSamples[sampleIndex])
                        .fetch_add(inputSamples[sampleIndex] * currentGain, std::memory_order::relaxed);
                }
                continue;
            }

            // not targeting silence
            for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
                currentGain = targetGain + (currentGain - targetGain) * gainFactor;
                std::atomic_ref<float>(outputSamples[sampleIndex])
                    .fetch_add(inputSamples[sampleIndex] * currentGain, std::memory_order::relaxed);
            }
        }
    }
}

//==============================================================================

// This is an awkward copy paste of sg_VbapSpatAlgorithm's make. we should find a way
// to deduplicate this (and a looooot of other spatialization algorithm code...)
std::unique_ptr<AbstractSpatAlgorithm> ParallelVbapSpatAlgorithm::make(SpeakerSetup const & speakerSetup,
                                                                       std::vector<source_index_t> srcIds,
                                                                       unsigned int numberOfThreads)
{
    auto const getVbap = [srcIds, &speakerSetup, &numberOfThreads]() {
        return std::make_unique<ParallelVbapSpatAlgorithm>(speakerSetup.speakers, srcIds, numberOfThreads);
    };

    if (speakerSetup.numOfSpatializedSpeakers() < 3) {
        return std::make_unique<DummySpatAlgorithm>(Error::notEnoughDomeSpeakers);
    }

    auto const dimensions{ getVbapType(speakerSetup.speakers) };

    if (dimensions == VbapType::threeD) {
        auto vbap = getVbap();
        if (!vbap->isValid) {
            return std::make_unique<DummySpatAlgorithm>(Error::failedToSpawnThreadpool);
        } else {
            return vbap;
        }
    }

    // Verify that the speakers are not too far apart

    juce::Array<radians_t> angles{};
    angles.ensureStorageAllocated(speakerSetup.speakers.size());
    for (auto const & speaker : speakerSetup.speakers) {
        if (speaker.value->isDirectOutOnly) {
            continue;
        }
        angles.add(speaker.value->position.getPolar().azimuth.balanced());
    }

    angles.sort();

    static constexpr radians_t MAX_ANGLE_DIFF{ degrees_t{ 170.0f } };

    auto const * invalidSpeaker{ std::adjacent_find(
        angles.begin(),
        angles.end(),
        [](radians_t const a, radians_t const b) { return b - a > MAX_ANGLE_DIFF; }) };

    auto const innerAreValid{ invalidSpeaker == angles.end() };
    auto const firstAndLastAreValid{ angles.getFirst() + TWO_PI - angles.getLast() <= MAX_ANGLE_DIFF };

    if (innerAreValid && firstAndLastAreValid) {
        auto vbap = getVbap();
        if (!vbap->isValid) {
            return std::make_unique<DummySpatAlgorithm>(Error::failedToSpawnThreadpool);
        } else {
            return vbap;
        }
    }

    return std::make_unique<DummySpatAlgorithm>(Error::flatDomeSpeakersTooFarApart);
}

} // namespace gris
