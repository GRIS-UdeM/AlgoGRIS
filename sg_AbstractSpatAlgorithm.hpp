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
#include "Data/sg_SpatMode.hpp"
#include "Data/sg_Triplet.hpp"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "tl/optional.hpp"
#include <cstdint>
#include <memory>

#if SG_USE_FORK_UNION
    #if JUCE_WINDOWS
        // this disables an annoying warning about structure alignment
        #pragma warning(disable : 4324)
    #endif
    #include <fork_union.hpp>
#endif

namespace gris
{
//==============================================================================
/** @return true if executed from the OSC thread. */
bool isOscThread();

/** @return true if executed neither from the OSC thread nor from the message thread. */
bool isProbablyAudioThread();

//==============================================================================
#define ASSERT_OSC_THREAD jassert(isOscThread())

// clang-format off
/** Enable the audio thread checks if we aren't running unit tests. */
#if !defined(ALGOGRIS_UNIT_TESTS)
    #define ASSERT_AUDIO_THREAD jassert(isProbablyAudioThread())
    #define ASSERT_NOT_AUDIO_THREAD jassert(!isProbablyAudioThread())
#else
    // The do { } while(0) is necessary enforce that this macro has to be used as an expression, e.g.
    // to disallow the programmer to write ASSERT_AUDIO_THREAD without a semicolon.
    // Otherwise it could work locally and then fail when someone else biulds with !defined(ALGOGRIS_UNIT_TESTS)
    #define ASSERT_AUDIO_THREAD do { } while (0)
    #define ASSERT_NOT_AUDIO_THREAD do { } while (0)
#endif
// clang-format on

//==============================================================================
/** Base class for a spatialization algorithm. */
class AbstractSpatAlgorithm
{
public:
    /** The types of errors that can arise when instantiating a spatialization algorithm. */
    enum class Error : std::uint8_t {
        notEnoughDomeSpeakers,
        notEnoughCubeSpeakers,
        flatDomeSpeakersTooFarApart,
    };
    //==============================================================================
    AbstractSpatAlgorithm();
    virtual ~AbstractSpatAlgorithm() = default;
    SG_DELETE_COPY_AND_MOVE(AbstractSpatAlgorithm)

#if SG_USE_FORK_UNION && (SG_FU_METHOD == SG_FU_USE_ARRAY_OF_ATOMICS || SG_FU_METHOD == SG_FU_USE_BUFFER_PER_THREAD)
    void silenceForkUnionBuffer(ForkUnionBuffer & forkUnionBuffer) noexcept;

    static void copyForkUnionBuffer(const gris::SpeakersAudioConfig & speakersAudioConfig,
                                    gris::SourceAudioBuffer & sourcesBuffer,
                                    gris::SpeakerAudioBuffer & speakersBuffer,
                                    gris::ForkUnionBuffer & forkUnionBuffer);

#endif

    //==============================================================================
    /** Assigns the position of sources in direct out mode to their assigned speakers' positions.
     *
     * Sources that use the "direct out" feature usually don't receive any positional OSC data. This is not a problem in
     * a physical setup, where there is an actual speaker assigned to the direct output. In a stereo reduction, the
     * source's position has to be faked so that it matches the position of the speaker used as a direct out.     */
    void fixDirectOutsIntoPlace(SourcesData const & sources,
                                SpeakerSetup const & speakerSetup,
                                SpatMode const & projectSpatMode) noexcept;
    //==============================================================================
    /** Updates the data of a source (its position, span, etc.).
     *
     * This is a function that is called really often and that does not happen on the audio thread, so be very careful
     * not to do anything here that might slow down the audio thread. */
    virtual void updateSpatData(source_index_t sourceIndex, SourceData const & sourceData) noexcept = 0;
    /** Processes the actual audio spatialization.
     *
     * @param config current audio configuration
     * @param sourcesBuffer audio input buffers
     * @param speakersBuffer audio output buffers
     * @param stereoBuffer audio output buffers used specifically for stereo reduction
     * @param sourcePeaks pre-processed peak values of the sources buffers
     * @param altSpeakerConfig OPTIONAL. The inner speakers audio configuration when encapsulating two algorithms.
     */
    virtual void process(AudioConfig const & config,
                         SourceAudioBuffer & sourcesBuffer,
                         SpeakerAudioBuffer & speakersBuffer,
#if SG_USE_FORK_UNION && (SG_FU_METHOD == SG_FU_USE_ARRAY_OF_ATOMICS || SG_FU_METHOD == SG_FU_USE_BUFFER_PER_THREAD)
                         ForkUnionBuffer & forkUnionBuffer,
#endif
                         juce::AudioBuffer<float> & stereoBuffer,
                         SourcePeaks const & sourcePeaks,
                         SpeakersAudioConfig const * altSpeakerConfig)
        = 0;
    /** @return the speaker triplets. Only works with VBAP-type algorithms. */
    [[nodiscard]] virtual juce::Array<Triplet> getTriplets() const noexcept = 0;
    /** @return true if the current algorithm uses VBAP internally. */
    [[nodiscard]] virtual bool hasTriplets() const noexcept = 0;
    /** @return the error that happened during instantiation or tl::nullopt if none. */
    [[nodiscard]] virtual tl::optional<Error> getError() const noexcept = 0;
    //==============================================================================
    /** Builds a spatialization algorithm. If the instantiation fails, this will hold a DummySpatAlgorithm.
     *
     * @param speakerSetup the current speaker setup
     * @param projectSpatMode the spatialization mode of the current project
     * @param stereoMode the stereo reduction mode, or tl::nullopt if none.
     * @param sources the sources' data.
     * @param sampleRate the expected sample rate
     * @param bufferSize the expected buffer size in samples
     */
    [[nodiscard]] static std::unique_ptr<AbstractSpatAlgorithm> make(SpeakerSetup const & speakerSetup,
                                                                     SpatMode const & projectSpatMode,
                                                                     tl::optional<StereoMode> stereoMode,
                                                                     SourcesData const & sources,
                                                                     double sampleRate,
                                                                     int bufferSize);

protected:
#if SG_USE_FORK_UNION
    ashvardanian::fork_union::thread_pool_t threadPool;
#endif

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(AbstractSpatAlgorithm)
};

} // namespace gris
