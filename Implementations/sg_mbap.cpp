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

#include "sg_mbap.hpp"
#include "juce_core/system/juce_PlatformDefs.h"
#include <Data/sg_AudioStructs.hpp>
#include <Data/sg_LogicStrucs.hpp>
#include <Data/sg_Narrow.hpp>
#include <Data/sg_Position.hpp>
#include <Data/sg_constants.hpp>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

namespace gris
{
namespace
{

float linearInterpolation(const MbapField & field,
                          float const source_x,
                          float const source_y,
                          float const source_z,
                          float const speaker_x,
                          float const speaker_y,
                          float const speaker_z)
{
    static constexpr auto H_SIZE = MBAP_SIZE_CONSTANT / 2.0f;

    auto sk_x = speaker_x * (H_SIZE) + H_SIZE;
    auto sk_y = speaker_y * (H_SIZE) + H_SIZE;
    auto sk_z = speaker_z * (H_SIZE) + H_SIZE;

    auto dist = std::sqrt(std::pow(source_x - sk_x, 2.0f) + std::pow(source_y - sk_y, 2.0f)
                          + std::pow(source_z - sk_z, 2.0f));

    auto table_idx = dist / DISTANCE_INCREMENT;
    auto table_idx_floor = static_cast<int>(table_idx);
    auto table_idx_ceil = table_idx_floor + 1;
    auto fractional_part = table_idx - static_cast<float>(table_idx_floor);
    auto val1 = field.distanceLookupTable[table_idx_floor];
    auto val2 = field.distanceLookupTable[table_idx_ceil];
    return val1 + fractional_part * (val2 - val1);
}

//==============================================================================
/* Returns a vector mbap_pos created from an array of mbap_speaker.
 */
static std::vector<Position> mbapPositionsFromSpeakers(MbapSpeaker const * speakers, size_t const num)
{
    std::vector<Position> positions{};
    positions.reserve(num);
    std::transform(speakers, speakers + num, std::back_inserter(positions), [](MbapSpeaker const & speaker) {
        return speaker.position;
    });
    return positions;
}

//==============================================================================
/* Initialize a newly created field for `num` speakers. */
static MbapField initField(std::vector<Position> speakers)
{
    MbapField field{};

    field.speakerPositions = std::move(speakers);

    return field;
}

/**
 * instead of computing one matrix for every speaker, just compute a lookup table of all the domain of possible value
 * and linear interpolate over that instead.
 */
static void computeLookup(MbapField & field)
{
    // This is the max value that is going to ever be looked up in this table so we need to generate the table for
    // entries from 0.0 to MAX_DISTANCE.

    for (int i = 0; i < LOOKUP_SIZE; i++) {
        float dist_val = i * (MAX_DISTANCE / static_cast<float>(LOOKUP_SIZE));

        dist_val = std::pow(db_root_power_ratio, dist_val);

        field.distanceLookupTable[i] = 1.0f / std::sqrt(dist_val);
    }
}

//==============================================================================
/* Create the field */
static MbapField createField(std::vector<Position> speakers)
{
    auto result{ initField(std::move(speakers)) };
    computeLookup(result);
    return result;
}

//==============================================================================
/* Compute the gain of field of speakers, for the given position, and store the result in the `gains` array.*/
static void computeGains(MbapField const & field, SourceData const & source, float * gains)
{
    static constexpr auto H_SIZE = MBAP_SIZE_CONSTANT / 2.0f;
    static constexpr auto SIZE_MINUS_ONE = MBAP_SIZE_CONSTANT - 1.0f;

    auto constexpr EXPONENT_MIN_IN{ 0.0f };
    auto constexpr EXPONENT_MAX_IN{ 1.0f };
    auto constexpr EXPONENT_MIN_OUT{ 4.0f };
    auto constexpr EXPONENT_MAX_OUT{ 8.0f };

    jassert(source.position);

    auto x = source.position->getCartesian().x * (H_SIZE - 1.0f) + H_SIZE;
    auto y = source.position->getCartesian().y * (H_SIZE - 1.0f) + H_SIZE;
    auto z = source.position->getCartesian().z * (H_SIZE - 1.0f) + H_SIZE;
    x = std::clamp(x, 0.0f, SIZE_MINUS_ONE);
    y = std::clamp(y, 0.0f, SIZE_MINUS_ONE);
    z = std::clamp(z, 0.0f, SIZE_MINUS_ONE);

    auto const sourceAzimuthSpan{ source.azimuthSpan };
    auto const sourceElevationSpan{ source.zenithSpan };
    auto const sumAziElevSpans{ 1.0f - sourceAzimuthSpan + 1.0f - sourceElevationSpan };

    float distFromSource{};
    float distXYPlane{};
    float distZ{};

    auto const finalElevSpanExponent{ ((sourceElevationSpan - EXPONENT_MIN_IN) * (EXPONENT_MAX_OUT - EXPONENT_MIN_OUT)
                                       / (EXPONENT_MAX_IN - EXPONENT_MIN_IN))
                                      + EXPONENT_MIN_OUT };

    auto const finalAzimuthSpanExponent{ ((sourceAzimuthSpan - EXPONENT_MIN_IN) * (EXPONENT_MAX_OUT - EXPONENT_MIN_OUT)
                                          / (EXPONENT_MAX_IN - EXPONENT_MIN_IN))
                                         + EXPONENT_MIN_OUT };

    for (size_t i{}; i < static_cast<size_t>(field.speakerPositions.size()); ++i) {
        auto squaredDistX = field.speakerPositions[i].getCartesian().x - source.position->getCartesian().x;
        squaredDistX *= squaredDistX;
        auto squaredDistY = field.speakerPositions[i].getCartesian().y - source.position->getCartesian().y;
        squaredDistY *= squaredDistY;
        distZ = std::abs(field.speakerPositions[i].getCartesian().z - source.position->getCartesian().z);
        auto const squaredDistZ = distZ * distZ;

        distFromSource = std::sqrt(squaredDistX + squaredDistY + squaredDistZ);

        distXYPlane = std::sqrt(squaredDistX + squaredDistY);

        auto const gain{ linearInterpolation(field,
                                             x,
                                             y,
                                             z,
                                             field.speakerPositions[i].getCartesian().x,
                                             field.speakerPositions[i].getCartesian().y,
                                             field.speakerPositions[i].getCartesian().z) };

        auto const gainNoSpan{ std::pow(gain, field.fieldExponent) };
        auto const gainFullElevSpan{ std::pow(gain, field.fieldExponent * distXYPlane) };

        auto const gainFullAzimuthSpan{ std::pow(gain, field.fieldExponent * distZ) };

        auto const azimuthElevationGain{ std::pow(gain, field.fieldExponent * sumAziElevSpans * distFromSource) };

        const float finalElevationGain{ (gainFullElevSpan - gainNoSpan)
                                            * std::pow(sourceElevationSpan, finalElevSpanExponent * distFromSource)
                                        + gainNoSpan };

        const float finalAzimuthGain{ (gainFullAzimuthSpan - gainNoSpan)
                                          * std::pow(sourceAzimuthSpan, finalAzimuthSpanExponent * distFromSource)
                                      + gainNoSpan };

        gains[i] = sumAziElevSpans * finalAzimuthGain + sumAziElevSpans * finalElevationGain + azimuthElevationGain;
    }

    auto const sum{ std::reduce(gains, gains + field.speakerPositions.size(), 0.0f, std::plus()) };

    if (sum > 0.0f) {
        // (pow(2.0, (1.0 - rad))) for energy spreading when moving toward the center.
        auto const radius{ std::sqrt(std::pow(source.position->getCartesian().x, 2.0f)
                                     + std::pow(source.position->getCartesian().y, 2.0f)
                                     + std::pow(source.position->getCartesian().z, 2.0f)) };
        auto const comp = radius < 1.0f ? std::pow(2.0f, 1.0f - radius) : 1.0f;
        // normalization (1.0 / sum) and compensation
        auto const norm = 1.0f / sum * comp;
        std::transform(gains, gains + field.speakerPositions.size(), gains, [norm](float & gain) {
            return gain * norm;
        });
    }
}
} // namespace

//==============================================================================
size_t MbapField::getNumSpeakers() const
{
    return speakerPositions.size();
}

//==============================================================================
void MbapField::reset()
{
    outputOrder.clear();
}

//==============================================================================
MbapField mbapInit(SpeakersData const & speakers)
{
    std::vector<Position> tempSpeakerPositions;
    tempSpeakerPositions.reserve(narrow<std::size_t>(speakers.size()));

    std::vector<MbapSpeaker> MbapSpeakers{};
    MbapSpeakers.reserve(narrow<std::size_t>(speakers.size()));

    for (auto const & speaker : speakers) {
        if (speaker.value->isDirectOutOnly) {
            continue;
        }

        MbapSpeaker const newSpeaker{ speaker.value->position, speaker.key };
        MbapSpeakers.push_back(newSpeaker);
    }

    auto const spk{ mbapPositionsFromSpeakers(&MbapSpeakers[0], MbapSpeakers.size()) };
    for (auto speaker : spk) {
        tempSpeakerPositions.emplace_back(speaker);
    }

    auto field{ createField(tempSpeakerPositions) };

    std::transform(MbapSpeakers.cbegin(),
                   MbapSpeakers.cend(),
                   std::back_inserter(field.outputOrder),
                   [](MbapSpeaker const & speaker) { return speaker.outputPatch; });

    return field;
}

//==============================================================================
void mbap(SourceData const & source, SpeakersSpatGains & gains, MbapField const & field)
{
    jassert(source.position);

    std::array<float, MAX_NUM_SPEAKERS> tempGains{};

    computeGains(field, source, tempGains.data());

    for (size_t i{}; i < field.getNumSpeakers(); ++i) {
        auto const & outputPatch{ field.outputOrder[i] };
        gains[outputPatch] = tempGains[i];
    }
}

} // namespace gris
