/*
 This file is part of SpatGRIS.

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
#include <JuceHeader.h>

namespace gris
{
/** Converts properties between versions of Speaker Setups.
 * Returns true if the conversion was successful, false if something unexpected happened.
 */
[[nodiscard]] bool convertProperties(const juce::ValueTree & source, juce::ValueTree & dest);

/** Converts an old speaker setup ValueTree to the CURRENT_SPEAKER_SETUP_VERSION.
 *
 * @param oldSpeakerSetup The ValueTree representing the old speaker setup.
 * @return A new ValueTree representing the converted speaker setup.
 */
juce::ValueTree convertSpeakerSetup(const juce::ValueTree & oldSpeakerSetup);

juce::ValueTree getTopParent(const juce::ValueTree & vt);

// Speaker setup identifiers
const juce::Identifier SPEAKER_SETUP{ "SPEAKER_SETUP" };
// TODO VB: we should be using SPAT_GRIS_VERSION instead of SPEAKER_SETUP_VERSION
const juce::Identifier SPEAKER_SETUP_VERSION{ "SPEAKER_SETUP_VERSION" };
const auto CURRENT_SPEAKER_SETUP_VERSION = "4.0.0";
const juce::Identifier SPAT_MODE{ "SPAT_MODE" };
const juce::Identifier UUID{ "UUID" };

// TODO VB: rename
const juce::Identifier DIFFUSION{ "DIFFUSION" };
const juce::Identifier GENERAL_MUTE{ "GENERAL_MUTE" };

const juce::Identifier SPEAKER_GROUP{ "SPEAKER_GROUP" };
const juce::Identifier SPEAKER_GROUP_NAME{ "SPEAKER_GROUP_NAME" };
const auto MAIN_SPEAKER_GROUP_NAME{ "MAIN_SPEAKER_GROUP_NAME" };

const juce::Identifier SPEAKER{ "SPEAKER" };
const juce::Identifier SPEAKER_PATCH_ID{ "SPEAKER_PATCH_ID" };
const juce::Identifier NEXT_SPEAKER_PATCH_ID{ "NEXT_SPEAKER_PATCH_ID" };

const juce::Identifier IO_STATE{ "IO_STATE" };
const juce::Identifier CARTESIAN_POSITION{ "CARTESIAN_POSITION" };
const juce::Identifier GAIN{ "GAIN" };
const juce::Identifier HIGHPASS_FREQ{ "HIGHPASS_FREQ" };
const juce::Identifier DIRECT_OUT_ONLY{ "DIRECT_OUT_ONLY" };
} // namespace gris
