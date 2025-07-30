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

#include "sg_AbstractSpatAlgorithm.hpp"
#include "Data/sg_LogicStrucs.hpp"
#include "Data/sg_SpatMode.hpp"
#include "sg_HrtfSpatAlgorithm.hpp"
#include "sg_HybridSpatAlgorithm.hpp"
#include "sg_MbapSpatAlgorithm.hpp"
#include "sg_PinkNoiseGenerator.hpp"
#include "sg_StereoSpatAlgorithm.hpp"
#include "sg_VbapSpatAlgorithm.hpp"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "juce_events/juce_events.h"
#include "tl/optional.hpp"
#include <memory>

#ifdef USE_DOPPLER
    #include "sg_DopplerSpatAlgorithm.hpp"
#endif

namespace gris
{
bool isOscThread()
{
    auto * currentThread{ juce::Thread::getCurrentThread() };
    if (!currentThread) {
        return false;
    }
    return currentThread->getThreadName() == "JUCE OSC server";
}

//==============================================================================
bool isProbablyAudioThread()
{
    return (!isOscThread() && !juce::MessageManager::getInstance()->isThisTheMessageThread());
}

//==============================================================================
AbstractSpatAlgorithm::AbstractSpatAlgorithm()
{
#if SG_USE_FORK_UNION
    if (!threadPool.try_spawn(std::thread::hardware_concurrency())) {
        std::fprintf(stderr, "Failed to fork the threads\n");
        jassertfalse;
    }
#endif
}

#if SG_USE_FORK_UNION
namespace fu = ashvardanian::fork_union;
    #if SG_FU_METHOD == SG_FU_USE_ARRAY_OF_ATOMICS
void AbstractSpatAlgorithm::silenceForkUnionBuffer(ForkUnionBuffer & forkUnionBuffer) noexcept
{
    fu::for_n(threadPool, forkUnionBuffer.size(), [&](std::size_t i) noexcept {
        auto & individualSpeakerBuffer{ forkUnionBuffer[i] };
        for (auto & wrapper : individualSpeakerBuffer)
            wrapper._a.store(0.f, std::memory_order_relaxed);
    });
}

void AbstractSpatAlgorithm::copyForkUnionBuffer(const gris::SpeakersAudioConfig & speakersAudioConfig,
                                                gris::SourceAudioBuffer & sourcesBuffer,
                                                gris::SpeakerAudioBuffer & speakersBuffer,
                                                gris::ForkUnionBuffer & forkUnionBuffer)
{
    // Copy ForkUnionBuffer into speakersBuffer
    size_t i = 0;
    for (auto const & speaker : speakersAudioConfig) {
        // skip silent speaker
        if (speaker.value.isMuted || speaker.value.isDirectOutOnly || speaker.value.gain < SMALL_GAIN)
            continue;

        auto const numSamples{ sourcesBuffer.getNumSamples() };
        auto * outputSamples{ speakersBuffer[speaker.key].getWritePointer(0) };
        auto & inputSamples{ forkUnionBuffer[i++] };

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
            outputSamples[sampleIdx] = inputSamples[sampleIdx]._a;
    }
}
    #elif SG_FU_METHOD == SG_FU_USE_BUFFER_PER_THREAD
void AbstractSpatAlgorithm::silenceForkUnionBuffer(ForkUnionBuffer & forkUnionBuffer) noexcept
{
    fu::for_n(threadPool, forkUnionBuffer.size(), [&](fu::prong_t prong) noexcept {
        jassert(threadPool.is_lock_free());

        // for each thread buffer
        auto & individualThreadBuffer{ forkUnionBuffer[prong.task_index] };

        // for each speaker buffer in the thread buffer
        for (auto & speakerBuffer : individualThreadBuffer)
            std::fill(speakerBuffer.begin(), speakerBuffer.end(), 0.f); // silence all speaker samples
    });
}

void AbstractSpatAlgorithm::copyForkUnionBuffer(const gris::SpeakersAudioConfig & speakersAudioConfig,
                                                gris::SourceAudioBuffer & sourcesBuffer,
                                                gris::SpeakerAudioBuffer & speakersBuffer,
                                                gris::ForkUnionBuffer & forkUnionBuffer)
{
    // Copy forkUnionBuffer into speakersBuffer
    for (auto const & threadBuffers : forkUnionBuffer) {
        size_t curSpeakerNumber = 0;
        for (auto const & speaker : speakersAudioConfig) {
            // skip silent speaker
            if (speaker.value.isMuted || speaker.value.isDirectOutOnly || speaker.value.gain < SMALL_GAIN)
                continue;

            auto const numSamples{ sourcesBuffer.getNumSamples() };
            auto * mainOutputSamples{ speakersBuffer[speaker.key].getWritePointer(0) };
            auto & threadOutputSamples{ threadBuffers[curSpeakerNumber++] };

            for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
                mainOutputSamples[sampleIdx] += threadOutputSamples[sampleIdx];
        }
    }
}
    #endif
#endif

//==============================================================================
void AbstractSpatAlgorithm::fixDirectOutsIntoPlace(SourcesData const & sources,
                                                   SpeakerSetup const & speakerSetup,
                                                   SpatMode const & projectSpatMode) noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const getFakeSourceData = [&](SourceData const & source, SpeakerData const & speaker) -> SourceData {
        auto fakeSourceData{ source };
        fakeSourceData.directOut.reset();
        switch (projectSpatMode) {
        case SpatMode::vbap:
        case SpatMode::hybrid:
            fakeSourceData.position = speaker.position.getPolar().normalized();
            return fakeSourceData;
        case SpatMode::mbap:
            fakeSourceData.position = speaker.position;
            return fakeSourceData;
        case SpatMode::invalid:
            break;
        }
        jassertfalse;
        return fakeSourceData;
    };

    for (auto const & source : sources) {
        auto const & directOut{ source.value->directOut };
        if (!directOut) {
            continue;
        }

        if (!speakerSetup.speakers.contains(*directOut)) {
            continue;
        }

        auto const & speaker{ speakerSetup.speakers[*directOut] };

        updateSpatData(source.key, getFakeSourceData(*source.value, speaker));
    }
}

//==============================================================================
std::unique_ptr<AbstractSpatAlgorithm> AbstractSpatAlgorithm::make(SpeakerSetup const & speakerSetup,
                                                                   SpatMode const & projectSpatMode,
                                                                   tl::optional<StereoMode> stereoMode,
                                                                   SourcesData const & sources,
                                                                   double const sampleRate,
                                                                   int const bufferSize)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (stereoMode) {
        switch (*stereoMode) {
        case StereoMode::hrtf:
            return HrtfSpatAlgorithm::make(speakerSetup, projectSpatMode, sources, sampleRate, bufferSize);
        case StereoMode::stereo:
            return StereoSpatAlgorithm::make(speakerSetup, projectSpatMode, sources, sources.getKeys());
#ifdef USE_DOPPLER
        case StereoMode::doppler:
            return DopplerSpatAlgorithm::make(sampleRate, bufferSize);
#endif
        }
        jassertfalse;
    }

    switch (projectSpatMode) {
    case SpatMode::vbap:
        return VbapSpatAlgorithm::make(speakerSetup, sources.getKeys());
    case SpatMode::mbap:
        return MbapSpatAlgorithm::make(speakerSetup, sources.getKeys());
    case SpatMode::hybrid:
        return HybridSpatAlgorithm::make(speakerSetup, sources.getKeys());
    case SpatMode::invalid:
        break;
    }

    jassertfalse;
    return nullptr;
}
} // namespace gris
