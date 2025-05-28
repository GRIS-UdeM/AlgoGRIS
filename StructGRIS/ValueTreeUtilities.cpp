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

#include "ValueTreeUtilities.hpp"
#include "../Data/sg_Position.hpp"

namespace gris
{
bool convertProperties(const juce::ValueTree & source, juce::ValueTree & dest)
{
    juce::String sourceType = source.getType().toString();

    if (sourceType == "SPEAKER_SETUP") {
        if (!source.hasProperty(SPAT_MODE) || !source.hasProperty("DIFFUSION") || !source.hasProperty("GENERAL_MUTE")) {
            jassertfalse;
            return false;
        }

        dest.setProperty(SPAT_MODE, source[SPAT_MODE], nullptr);
        dest.setProperty(SPEAKER_SETUP_VERSION, CURRENT_SPEAKER_SETUP_VERSION, nullptr);
        dest.setProperty("DIFFUSION", source["DIFFUSION"], nullptr);
        dest.setProperty("GENERAL_MUTE", source["GENERAL_MUTE"], nullptr);
        return true;
    }
    if (sourceType.contains("SPEAKER_")) {
        if (!source.hasProperty(STATE) || !source.hasProperty(GAIN) || !source.hasProperty(DIRECT_OUT_ONLY)) {
            jassertfalse;
            return false;
        }

        dest.setProperty(STATE, source[STATE], nullptr);
        dest.setProperty(GAIN, source[GAIN], nullptr);
        dest.setProperty(DIRECT_OUT_ONLY, source[DIRECT_OUT_ONLY], nullptr);
        return true;

    } else if (sourceType == CartesianVector::XmlTags::POSITION) {
        if (!source.hasProperty(CartesianVector::XmlTags::X) || !source.hasProperty(CartesianVector::XmlTags::Y)
            || !source.hasProperty(CartesianVector::XmlTags::Z)) {
            jassertfalse;
            return false;
        }

        auto const x = source[CartesianVector::XmlTags::X];
        auto const y = source[CartesianVector::XmlTags::Y];
        auto const z = source[CartesianVector::XmlTags::Z];

        dest.setProperty(CARTESIAN_POSITION,
                         juce::VariantConverter<Position>::toVar(Position{ CartesianVector{ x, y, z } }),
                         nullptr);

        return true;

    } else if (sourceType == "HIGHPASS") {
        if (!source.hasProperty("FREQ")) {
            jassertfalse;
            return false;
        }

        dest.setProperty(HIGHPASS_FREQ, source["FREQ"], nullptr);
        return true;
    }

    // unsupported type
    jassertfalse;
    return false;

    // for (int i = 0; i < source.getNumProperties(); ++i) {
    //     auto const propertyName = source.getPropertyName(i);
    //     auto const propertyValue = source.getProperty(propertyName);
    //     dest.setProperty(propertyName, propertyValue, nullptr);
    // }
};

juce::ValueTree convertSpeakerSetup(const juce::ValueTree & oldSpeakerSetup)
{
    if (oldSpeakerSetup.getType() != SPEAKER_SETUP) {
        // invalid speaker setup!
        jassertfalse;
        return {};
    }

    // get outta here if the version is already up to date
    if (oldSpeakerSetup[SPEAKER_SETUP_VERSION] == CURRENT_SPEAKER_SETUP_VERSION)
        return oldSpeakerSetup;

    // create new value tree and copy root properties into it
    auto newVt = juce::ValueTree(SPEAKER_SETUP);
    if (!convertProperties(oldSpeakerSetup, newVt))
        return {};

    newVt.setProperty(SPEAKER_SETUP_VERSION, CURRENT_SPEAKER_SETUP_VERSION, nullptr);

    // create and append the main speaker group node
    auto mainSpeakerGroup = juce::ValueTree(SPEAKER_GROUP);
    mainSpeakerGroup.setProperty(SPEAKER_GROUP_NAME, MAIN_SPEAKER_GROUP_NAME, nullptr);
    mainSpeakerGroup.setProperty(CARTESIAN_POSITION, juce::VariantConverter<Position>::toVar(Position{}), nullptr);
    newVt.appendChild(mainSpeakerGroup, nullptr);

    // then add all speakers to the main group
    for (const auto & speaker : oldSpeakerSetup) {
        if (!speaker.getType().toString().contains("SPEAKER_")
            || speaker.getChild(0).getType().toString() != "POSITION") {
            // corrupted file? Speakers must have a type that starts with SPEAKER_ and have a POSITION child
            jassertfalse;
            return {};
        }

        auto newSpeaker = juce::ValueTree{ SPEAKER };
        const auto speakerId = speaker.getType().toString().removeCharacters("SPEAKER_");
        newSpeaker.setProperty(SPEAKER_PATCH_ID, speakerId, nullptr);

        // copy properties for the speaker and its children
        if (!convertProperties(speaker, newSpeaker))
            return {};

        for (const auto child : speaker)
            if (!convertProperties(child, newSpeaker))
                return {};

        mainSpeakerGroup.appendChild(newSpeaker, nullptr);
    }

    return newVt;
}

juce::ValueTree getTopParent(const juce::ValueTree & vt)
{
    auto parent = vt.getParent();
    while (parent.isValid()) {
        if (parent.getParent().isValid())
            parent = parent.getParent();
        else
            break;
    }
    return parent;
}
} // namespace gris
