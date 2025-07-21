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
#if USE_FORK_UNION
    if (!threadPool.try_spawn(std::thread::hardware_concurrency())) {
        std::fprintf(stderr, "Failed to fork the threads\n");
        jassertfalse;
    }
#endif
}

#if USE_FORK_UNION
    #if FU_METHOD == FU_USE_ARRAY_OF_ATOMICS
void AbstractSpatAlgorithm::clearAtomicSpeakerBuffer(AtomicSpeakerBuffer & atomicSpeakerBuffer) noexcept
{
    ashvardanian::fork_union::for_n(threadPool, atomicSpeakerBuffer.size(), [&](std::size_t i) noexcept {
        auto & individualSpeakerBuffer{ atomicSpeakerBuffer[i] };
        for (auto & wrapper : individualSpeakerBuffer)
            wrapper._a.store(0.f, std::memory_order_relaxed);
    });
}
    #elif FU_METHOD == FU_USE_BUFFER_PER_THREAD
void AbstractSpatAlgorithm::silenceThreadSpeakerBuffer(ThreadSpeakerBuffer & threadSpeakerBuffer) noexcept
{
    namespace fu = ashvardanian::fork_union;

    // TODO VB: this is in one of the examples but somehow this function doesn't exist?
    // threadPool.for_threads([&](std::size_t thread_index) noexcept {
    //     std::printf("Hello from thread # %zu (of %zu)\n", thread_index + 1, pool.count_threads());
    // });

    fu::for_n(threadPool, threadSpeakerBuffer.size(), [&](fu::prong_t prong) noexcept {
        jassert(threadPool.is_lock_free());

        // for each thread buffer
        auto & individualThreadBuffer{ threadSpeakerBuffer[prong.task_index] };

        // for each speaker buffer in the thread buffer
        for (auto & speakerBuffer : individualThreadBuffer)
            std::fill(speakerBuffer.begin(), speakerBuffer.end(), 0.f); // silence all speaker samples
    });
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
            return StereoSpatAlgorithm::make(speakerSetup, projectSpatMode, sources);
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
        return MbapSpatAlgorithm::make(speakerSetup);
    case SpatMode::hybrid:
        return HybridSpatAlgorithm::make(speakerSetup, sources.getKeys());
    case SpatMode::invalid:
        break;
    }

    jassertfalse;
    return nullptr;
}
} // namespace gris
