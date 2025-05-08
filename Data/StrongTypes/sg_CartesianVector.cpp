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

#include "sg_CartesianVector.hpp"
#include "Data/StrongTypes/sg_Radians.hpp"
#include "../sg_PolarVector.hpp"
#include "juce_core/juce_core.h"
#include "tl/optional.hpp"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <memory>

namespace gris
{
juce::String const CartesianVector::XmlTags::POSITION = "POSITION";
juce::String const CartesianVector::XmlTags::X = "X";
juce::String const CartesianVector::XmlTags::Y = "Y";
juce::String const CartesianVector::XmlTags::Z = "Z";

//==============================================================================
CartesianVector::CartesianVector(PolarVector const & polarVector) noexcept
{
    // Mathematically, the azimuth angle should start from the pole and be equal to 90 degrees at the equator.
    // We have to accomodate for a slightly different coordinate system where the azimuth angle starts at the equator
    // and is equal to 90 degrees at the north pole.

    // This is quite dangerous because any trigonometry done outside of this class might get things wrong.

    auto const diffElev = HALF_PI.get() - polarVector.elevation.get();
    auto const inverseElevation{ diffElev == 0.0f ? 0.0000001f : diffElev };

    x = polarVector.length * std::sin(inverseElevation) * std::cos(polarVector.azimuth.get());
    y = polarVector.length * std::sin(inverseElevation) * std::sin(polarVector.azimuth.get());
    z = polarVector.length * std::cos(inverseElevation);
}

//==============================================================================
CartesianVector CartesianVector::crossProduct(CartesianVector const & other) const noexcept
{
    auto const newX{ y * other.z - z * other.y };
    auto const newY{ z * other.x - x * other.z };
    auto const newZ{ x * other.y - y * other.x };
    CartesianVector const unscaledResult{ newX, newY, newZ };

    auto const length = unscaledResult.length();
    auto const result{ unscaledResult / length };

    return result;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> CartesianVector::toXml() const noexcept
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::POSITION) };
    result->setAttribute(XmlTags::X, x);
    result->setAttribute(XmlTags::Y, y);
    result->setAttribute(XmlTags::Z, z);
    return result;
}

juce::String CartesianVector::toString() const noexcept
{
    return "(" + juce::String{ x } + ", " + juce::String{ y } + ", " + juce::String{ z } + ")";
}

tl::optional<CartesianVector> CartesianVector::fromString(const juce::String & str) noexcept
{
    auto trimmed = str.trim();

#if 0

    DBG ("str: " << str);
    int first = str.indexOfChar (',');  // Should be 10
    int second = str.indexOfChar (',', first + 1);  // Should be ~20
    DBG ("first: " << first);
    DBG ("second: " << second);
    DBG ("char at first: " << str[first]); // Should be ','
    DBG ("after first: " << str.substring (first + 1)); // Should start with " 0.707107, ..."

    auto const commaIndex = trimmed.indexOfChar (',');
    auto const secondCommaIndex = trimmed.indexOfChar (',', commaIndex + 1);

    if (commaIndex < 0 || secondCommaIndex < 0) {
        jassertfalse;
        return {};
    }
    auto const xStr = trimmed.substring (1, commaIndex).trim ();
    auto const yStr = trimmed.substring (commaIndex + 1, secondCommaIndex).trim ();
    auto const zStr = trimmed.substring (secondCommaIndex + 1, trimmed.length () - 1).trim ();

    if (xStr.isEmpty () || yStr.isEmpty () || zStr.isEmpty ()) {
        jassertfalse;
        return {};
    }

    return CartesianVector { xStr.getFloatValue (), yStr.getFloatValue (), zStr.getFloatValue () };
#else

    // Remove the parentheses
    trimmed = trimmed.fromFirstOccurrenceOf("(", false, false).upToLastOccurrenceOf(")", false, false);

    // Split the string into tokens
    juce::StringArray tokens;
    tokens.addTokens(trimmed, ",", "\""); // split by commas

    // Convert to floats
    if (tokens.size() == 3) {
        float x = tokens[0].trim().getFloatValue();
        float y = tokens[1].trim().getFloatValue();
        float z = tokens[2].trim().getFloatValue();

        DBG("x = " << x << ", y = " << y << ", z = " << z);
        return CartesianVector{ x, y, z };
    } else {
        DBG("Unexpected number of tokens: " << tokens.size());
        return {};
    }

#endif
}

//==============================================================================
tl::optional<CartesianVector> CartesianVector::fromXml(juce::XmlElement const & xml)
{
    juce::StringArray const requiredTags{ XmlTags::X, XmlTags::Y, XmlTags::Z };

    if (!std::all_of(requiredTags.begin(), requiredTags.end(), [&](juce::String const & tag) {
            return xml.hasAttribute(tag);
        })) {
        return tl::nullopt;
    }

    CartesianVector result;
    result.x = static_cast<float>(xml.getDoubleAttribute(XmlTags::X));
    result.y = static_cast<float>(xml.getDoubleAttribute(XmlTags::Y));
    result.z = static_cast<float>(xml.getDoubleAttribute(XmlTags::Z));

    return result;
}

//==============================================================================
float CartesianVector::angleWith(CartesianVector const & other) const noexcept
{
    auto inner = dotProduct(other) / std::sqrt(length2() * other.length2());
    inner = std::clamp(inner, -1.0f, 1.0f);
    return std::abs(std::acos(inner));
}

//==============================================================================
float CartesianVector::length() const noexcept
{
    return std::sqrt(length2());
}

} // namespace gris
