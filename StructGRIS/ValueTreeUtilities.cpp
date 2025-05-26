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
/** This simply copies properties from one valuetree to another, nothing else. */
void copyProperties(const juce::ValueTree & source, juce::ValueTree & dest)
{
    // these are the only types of sources we're expecting here
    jassert(source.getType().toString().contains("SPEAKER_")
            || source.getType().toString() == CartesianVector::XmlTags::POSITION
            || source.getType().toString() == "HIGHPASS");

    if (source.getType().toString() == CartesianVector::XmlTags::POSITION) {
        auto const x = source[CartesianVector::XmlTags::X];
        auto const y = source[CartesianVector::XmlTags::Y];
        auto const z = source[CartesianVector::XmlTags::Z];

        dest.setProperty(CARTESIAN_POSITION,
                         juce::VariantConverter<Position>::toVar(Position{ CartesianVector{ x, y, z } }),
                         nullptr);

        return;
    }

    for (int i = 0; i < source.getNumProperties(); ++i) {
        auto const propertyName = source.getPropertyName(i);
        auto const propertyValue = source.getProperty(propertyName);
        dest.setProperty(propertyName, propertyValue, nullptr);
    }
};

juce::ValueTree convertSpeakerSetup(const juce::ValueTree & oldSpeakerSetup)
{
    if (oldSpeakerSetup.getType() != SPEAKER_SETUP) {
        // invalid speaker setup!
        jassertfalse;
        return {};
    }

    // get outta here if the version is already up to date
    if (oldSpeakerSetup[VERSION] == CURRENT_SPEAKER_SETUP_VERSION)
        return oldSpeakerSetup;

    // create new value tree and copy root properties into it
    auto newVt = juce::ValueTree(SPEAKER_SETUP);
    copyProperties(oldSpeakerSetup, newVt);
    newVt.setProperty(VERSION, CURRENT_SPEAKER_SETUP_VERSION, nullptr);

    // create and append the main speaker group node
    auto mainSpeakerGroup = juce::ValueTree(SPEAKER_GROUP);
    mainSpeakerGroup.setProperty(ID, MAIN_SPEAKER_GROUP, nullptr);
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
        newSpeaker.setProperty("ID", speakerId, nullptr);

        // copy properties for the speaker and its children
        copyProperties(speaker, newSpeaker);
        for (const auto child : speaker)
            copyProperties(child, newSpeaker);

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
