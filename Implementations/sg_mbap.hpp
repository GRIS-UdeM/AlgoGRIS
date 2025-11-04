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

/**
 * Matrix-Based Amplitude Panning framework.
 *
 * MBAP (Matrix-Based Amplitude Panning) is a framework
 * to do 3-D sound spatialization.
 *
 * author : Gaël Lane Lépine, 2022
 * based on lbap from Olivier Belanger
 *
 */

#pragma once

#include <Data/StrongTypes/sg_OutputPatch.hpp>
#include <Data/sg_AudioStructs.hpp>
#include <Data/sg_LogicStrucs.hpp>
#include <Data/sg_Position.hpp>
#include <array>
#include <cstddef>
#include <vector>

namespace gris
{
struct SpeakerData;

// This used to be the size of a precomputed matrix. Now that we use a linear
// lookup table, this is kept as a constant to keep gain factors equivalent to what they
// used to be.
static auto constexpr MBAP_SIZE_CONSTANT = 64;
// Size of the distance -> gain factor table. 256 is enough to get < 0.01% error.
auto constexpr LOOKUP_SIZE = 256;
// Maximum distance a source can get from a speaker considering we clamp each position to MBAP_SIZE_CONSTANT -1
static const float MAX_DISTANCE = std::ceil(std::sqrt(
    std::pow(MBAP_SIZE_CONSTANT, 2.0f) + std::pow(MBAP_SIZE_CONSTANT, 2.0f) + std::pow(MBAP_SIZE_CONSTANT, 2.0f)));

static auto const DISTANCE_INCREMENT = MAX_DISTANCE / static_cast<float>(LOOKUP_SIZE);

static const float db_root_power_ratio = std::pow(10.0f, 1.0f / 20);

struct MbapSpeaker {
    Position position{};
    output_patch_t outputPatch{};
};

//==============================================================================
struct MbapField {
    std::vector<output_patch_t> outputOrder; /**< Physical output order. */
    float fieldExponent;                     /**< Speaker gain exponent speakers. */
    std::vector<Position> speakerPositions;  /**< Array of speakers. */
    // lookup table of the gain factor indexed by distance.
    // minimum distance is 0 and max is MAX_DISTANCE.
    std::array<float, LOOKUP_SIZE> distanceLookupTable;
    //==============================================================================
    [[nodiscard]] size_t getNumSpeakers() const;
    void reset();
};

/**
 * Creates the amplitude field according to the position of speakers.
 */
MbapField mbapInit(SpeakersData const & speakers);

/** \brief Calculates the gain of the outputs for a source's position.
 *
 * This function uses the position `pos` to retrieve the gain for every
 * output from the field and fill the array of float `gains`.
 * The user must provide the array of float and is responsible of its
 * memory. This array can be passed to the audio processing function
 * to control the gain of the signal outputs.
 */
void mbap(SourceData const & source, SpeakersSpatGains & gains, MbapField const & field);

} // namespace gris
