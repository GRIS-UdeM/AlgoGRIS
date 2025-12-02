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

#include "sg_PinkNoiseGenerator.hpp"
#include "Data/StrongTypes/sg_Dbfs.hpp"
#include "Data/sg_Narrow.hpp"
#include <random>

namespace
{
float pinkNoiseC0{};
float pinkNoiseC1{};
float pinkNoiseC2{};
float pinkNoiseC3{};
float pinkNoiseC4{};
float pinkNoiseC5{};
float pinkNoiseC6{};

std::random_device rd;
std::mt19937_64 gen(rd());
std::uniform_real_distribution<float> dist(-1.f, 1.f);
} // namespace

namespace gris
{
//==============================================================================
void fillWithPinkNoise(float * const * samples,
                       int const numSamples,
                       int const numChannels,
                       float const gain,
                       bool isPulsing,
                       PulsedNoiseParams & params)
{
    static constexpr dbfs_t CORRECTION_DB{ -18.2f };
    static auto const CORRECTION{ CORRECTION_DB.toGain() };

    if (isPulsing)
        params.elapsedTime += static_cast<float>(numSamples) / params.sampleRate;

    for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
        auto const rnd{ dist(gen) };
        pinkNoiseC0 = pinkNoiseC0 * 0.99886f + rnd * 0.0555179f;
        pinkNoiseC1 = pinkNoiseC1 * 0.99332f + rnd * 0.0750759f;
        pinkNoiseC2 = pinkNoiseC2 * 0.96900f + rnd * 0.1538520f;
        pinkNoiseC3 = pinkNoiseC3 * 0.86650f + rnd * 0.3104856f;
        pinkNoiseC4 = pinkNoiseC4 * 0.55000f + rnd * 0.5329522f;
        pinkNoiseC5 = pinkNoiseC5 * -0.7616f - rnd * 0.0168980f;
        auto sampleValue{ pinkNoiseC0 + pinkNoiseC1 + pinkNoiseC2 + pinkNoiseC3 + pinkNoiseC4 + pinkNoiseC5
                          + pinkNoiseC6 + rnd * 0.5362f };
        sampleValue *= gain * CORRECTION;
        pinkNoiseC6 = rnd * 0.115926f;

        for (int channelIndex{}; channelIndex < numChannels; channelIndex++) {
            samples[channelIndex][sampleIndex] += sampleValue;

            if (isPulsing) {
                if (params.elapsedTime < params.silenceDuration) {
                    // silence
                    samples[channelIndex][sampleIndex] = 0.0f;
                } else {
                    samples[channelIndex][sampleIndex]
                        *= juce::Decibels::gainToDecibels(params.currentPhase) * CORRECTION;
                }

                params.currentPhase += params.phaseIncrement;

                if (params.currentPhase >= 1.0f) {
                    params.currentPhase -= 1.0f;
                    if (params.elapsedTime >= params.silenceDuration) {
                        params.elapsedTime = 0.0f;
                    }
                }
            }
        }
    }
}

} // namespace gris
