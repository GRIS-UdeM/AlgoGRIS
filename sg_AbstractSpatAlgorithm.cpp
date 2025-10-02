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
#include "sg_HrtfSpatAlgorithm.hpp"
#include "sg_HybridSpatAlgorithm.hpp"
#include "sg_MbapSpatAlgorithm.hpp"
#include "sg_ParallelMbapSpatAlgorithm.hpp"
#include "sg_ParallelVbapSpatAlgorithm.hpp"
#include "sg_PinkNoiseGenerator.hpp"
#include "sg_StereoSpatAlgorithm.hpp"
#include "sg_VbapSpatAlgorithm.hpp"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "juce_events/juce_events.h"
#include "tl/optional.hpp"
#include <Data/sg_LogicStrucs.hpp>
#include <Data/sg_SpatMode.hpp>
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
}

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
                                                                   int const bufferSize,
                                                                   // defaulted to false
                                                                   bool const useMulticoreDSP)
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

    auto hardwareConcurrency = std::thread::hardware_concurrency();
    switch (projectSpatMode) {
    case SpatMode::vbap:
        if (useMulticoreDSP) {
            return ParallelVbapSpatAlgorithm::make(speakerSetup, sources.getKeys(), hardwareConcurrency);
        } else {
            return VbapSpatAlgorithm::make(speakerSetup, sources.getKeys());
        }
    case SpatMode::mbap:
        if (useMulticoreDSP) {
            return ParallelMbapSpatAlgorithm::make(speakerSetup, sources.getKeys(), hardwareConcurrency);
        } else {
            return MbapSpatAlgorithm::make(speakerSetup, sources.getKeys());
        }
    case SpatMode::hybrid:
        if (useMulticoreDSP) {
            return HybridSpatAlgorithm<ParallelMbapSpatAlgorithm, ParallelVbapSpatAlgorithm>::make(speakerSetup,
                                                                                                   sources.getKeys());
        } else {
            return HybridSpatAlgorithm<MbapSpatAlgorithm, VbapSpatAlgorithm>::make(speakerSetup, sources.getKeys());
        }
    case SpatMode::invalid:
        break;
    }

    jassertfalse;
    return nullptr;
}

//==============================================================================
ParallelAlgorithm::ParallelAlgorithm(unsigned int numberOfThreads)
{
    isValid = threadPool.try_spawn(numberOfThreads);
    if (!isValid) {
        std::fprintf(stderr, "Failed to spawn the threadpool\n");
        jassertfalse;
    }
}

} // namespace gris
