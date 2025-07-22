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

#include "sg_StereoSpatAlgorithm.hpp"
#include "Containers/sg_StaticMap.hpp"
#include "Containers/sg_TaggedAudioBuffer.hpp"
#include "Data/StrongTypes/sg_Radians.hpp"
#include "Data/StrongTypes/sg_SourceIndex.hpp"
#include "Data/sg_AudioStructs.hpp"
#include "Data/sg_LogicStrucs.hpp"
#include "Data/sg_Narrow.hpp"
#include "Data/sg_SpatMode.hpp"
#include "Data/sg_Triplet.hpp"
#include "sg_AbstractSpatAlgorithm.hpp"
#include "sg_HybridSpatAlgorithm.hpp"
#include "sg_MbapSpatAlgorithm.hpp"
#include "sg_VbapSpatAlgorithm.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "juce_events/juce_events.h"
#include <cmath>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <memory>

namespace gris
{
//==============================================================================
void StereoSpatAlgorithm::updateSpatData(source_index_t const sourceIndex, SourceData const & sourceData) noexcept
{
    ASSERT_NOT_AUDIO_THREAD;

    if (sourceData.directOut) {
        return;
    }

    mInnerAlgorithm->updateSpatData(sourceIndex, sourceData);

    // using fast = juce::dsp::FastMathApproximations;

    auto & queue{ mData[sourceIndex].gainsUpdater };
    auto * ticket{ queue.acquire() };
    assert(ticket);
    auto & gains{ ticket->get() };

    if (sourceData.position) {
        auto const x{ std::clamp(sourceData.position->getCartesian().x, -1.0f, 1.0f)
                      * (1.0f - sourceData.azimuthSpan) };

        auto const fromZeroToPi{ (x + 1.0f) * HALF_PI.get() };

        auto const leftGain{ std::cos(fromZeroToPi) * 0.5f + 0.5f };
        auto const rightGain{ std::cos(PI.get() - fromZeroToPi) * 0.5f + 0.5f };

        gains[0] = std::pow(leftGain, 0.5f);
        gains[1] = std::pow(rightGain, 0.5f);
    } else {
        gains[0] = 0.0f;
        gains[1] = 0.0f;
    }

    queue.setMostRecent(ticket);
}

//==============================================================================
void StereoSpatAlgorithm::process(AudioConfig const & config,
                                  SourceAudioBuffer & sourcesBuffer,
                                  SpeakerAudioBuffer & speakersBuffer,
#if USE_FORK_UNION
    #if FU_METHOD == FU_USE_ARRAY_OF_ATOMICS
                                  AtomicSpeakerBuffer & atomicSpeakerBuffer,
    #elif FU_METHOD == FU_USE_BUFFER_PER_THREAD
                                  ThreadSpeakerBuffer & threadSpeakerBuffer,
    #endif
#endif
                                  juce::AudioBuffer<float> & stereoBuffer,
                                  SourcePeaks const & sourcePeaks,
                                  [[maybe_unused]] SpeakersAudioConfig const * altSpeakerConfig)
{
    ASSERT_AUDIO_THREAD;
    jassert(!altSpeakerConfig);
    jassert(stereoBuffer.getNumChannels() == 2);

#if USE_FORK_UNION
    #if FU_METHOD == FU_USE_ARRAY_OF_ATOMICS
    mInnerAlgorithm->process(config,
                             sourcesBuffer,
                             speakersBuffer,
                             atomicSpeakerBuffer,
                             stereoBuffer,
                             sourcePeaks,
                             altSpeakerConfig);
    #elif FU_METHOD == FU_USE_BUFFER_PER_THREAD
    mInnerAlgorithm->process(config,
                             sourcesBuffer,
                             speakersBuffer,
                             threadSpeakerBuffer,
                             stereoBuffer,
                             sourcePeaks,
                             altSpeakerConfig);
    #else
    mInnerAlgorithm->process(config, sourcesBuffer, speakersBuffer, stereoBuffer, sourcePeaks, altSpeakerConfig);
    #endif
#else
    mInnerAlgorithm->process(config, sourcesBuffer, speakersBuffer, stereoBuffer, sourcePeaks, altSpeakerConfig);
#endif

#if USE_FORK_UNION
    auto const sourceIds{ config.sourcesAudioConfig.getKeyVector() };
    ashvardanian::fork_union::for_n(threadPool, sourceIds.size(), [&](std::size_t i) noexcept {
        processSource(config, sourceIds[(int)i], sourcePeaks, sourcesBuffer, stereoBuffer);
    });
#else
    for (auto const & source : config.sourcesAudioConfig)
        processSource(config, source.key, sourcePeaks, sourcesBuffer, stereoBuffer);
#endif
    // Apply gain compensation.
    auto const compensation{ std::pow(10.0f, (narrow<float>(config.sourcesAudioConfig.size()) - 1.0f) * -0.005f) };
    stereoBuffer.applyGain(0, sourcesBuffer.getNumSamples(), compensation);
}

//==============================================================================
inline void StereoSpatAlgorithm::processSource(const gris::AudioConfig & config,
                                               const gris::source_index_t & sourceId,
                                               const gris::SourcePeaks & sourcePeaks,
                                               gris::SourceAudioBuffer & sourcesBuffer,
                                               juce::AudioBuffer<float> & stereoBuffer)
{
    auto const & source = config.sourcesAudioConfig[sourceId];
    if (source.isMuted || source.directOut || sourcePeaks[sourceId] < SMALL_GAIN) {
        return;
    }

    auto & data{ mData[sourceId] };

    data.gainsUpdater.getMostRecent(data.currentGains);
    if (data.currentGains == nullptr) {
        return;
    }

    auto & lastGains{ data.lastGains };
    auto const & gains{ data.currentGains->get() };
    auto const & gainInterpolation{ config.spatGainsInterpolation };
    auto const numSamples{ sourcesBuffer.getNumSamples() };
    auto const * inputSamples{ sourcesBuffer[sourceId].getReadPointer(0) };
    auto const gainFactor{ std::pow(gainInterpolation, 0.1f) * 0.0099f + 0.99f };

    static constexpr std::array<size_t, 2> SPEAKERS{ 0, 1 };
    auto * const * const buffers{ stereoBuffer.getArrayOfWritePointers() };

    for (auto const & speaker : SPEAKERS) {
        auto & currentGain{ lastGains[speaker] };
        auto const & targetGain{ gains[speaker] };
        auto * outputSamples{ buffers[speaker] };
        if (gainInterpolation == 0.0f) {
            // linear interpolation over buffer size
            auto const gainSlope = (targetGain - currentGain) / narrow<float>(numSamples);
            if (targetGain < SMALL_GAIN && currentGain < SMALL_GAIN) {
                // this is not going to produce any more sounds!
                continue;
            }
            for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
                currentGain += gainSlope;
                outputSamples[sampleIndex] += inputSamples[sampleIndex] * currentGain;
            }
        } else {
            // log interpolation with 1st order filter
            for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
                currentGain = targetGain + (currentGain - targetGain) * gainFactor;
                if (currentGain < SMALL_GAIN && targetGain < SMALL_GAIN) {
                    // If the gain is near zero and the target gain is also near zero, this means that
                    // currentGain will no ever increase over this buffer
                    break;
                }
                outputSamples[sampleIndex] += inputSamples[sampleIndex] * currentGain;
            }
        }
    }
}

//==============================================================================
juce::Array<Triplet> StereoSpatAlgorithm::getTriplets() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassertfalse;
    return juce::Array<Triplet>{};
}

//==============================================================================
std::unique_ptr<AbstractSpatAlgorithm> StereoSpatAlgorithm::make(SpeakerSetup const & speakerSetup,
                                                                 SpatMode const & projectSpatMode,
                                                                 SourcesData const & sources)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    return std::make_unique<StereoSpatAlgorithm>(speakerSetup, projectSpatMode, sources);
}

//==============================================================================
StereoSpatAlgorithm::StereoSpatAlgorithm(SpeakerSetup const & speakerSetup,
                                         SpatMode const & projectSpatMode,
                                         SourcesData const & sources)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    switch (projectSpatMode) {
    case SpatMode::vbap:
        mInnerAlgorithm = VbapSpatAlgorithm::make(speakerSetup, sources.getKeys());
        break;
    case SpatMode::mbap:
        mInnerAlgorithm = MbapSpatAlgorithm::make(speakerSetup);
        break;
    case SpatMode::hybrid:
        mInnerAlgorithm = HybridSpatAlgorithm::make(speakerSetup, sources.getKeys());
        break;
    case SpatMode::invalid:
        break;
    }

    jassert(mInnerAlgorithm);

    fixDirectOutsIntoPlace(sources, speakerSetup, projectSpatMode);
}

} // namespace gris
