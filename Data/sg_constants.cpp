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

#include "sg_constants.hpp"
#include "Data/StrongTypes/sg_Dbfs.hpp"
#include "Data/StrongTypes/sg_Hz.hpp"
#include "Data/StrongTypes/sg_StrongFloat.hpp"
#include "Data/sg_Narrow.hpp"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "tl/optional.hpp"
#include <algorithm>

namespace gris
{
SpatGrisVersion const SPAT_GRIS_VERSION{ SpatGrisVersion::fromString(JUCE_STRINGIFY(JUCE_APP_VERSION)) };

juce::Colour const DEFAULT_SOURCE_COLOR{ narrow<juce::uint8>(255), 0, 0 };

//==============================================================================
juce::StringArray const RECORDING_FORMAT_STRINGS{ "WAV",
                                                  "AIFF"
#ifdef __APPLE__
                                                  ,
                                                  "CAF"
#endif
};
juce::StringArray const RECORDING_FILE_TYPE_STRINGS{ "Mono Files", "Interleaved" };
juce::Array<int> const ATTENUATION_DB_VALUES{ 0, -12, -24, -36, -48, -60, -72 };
juce::StringArray const ATTENUATION_DB_STRINGS{ "0", "-12", "-24", "-36", "-48", "-60", "-72" };
juce::Array<int> const ATTENUATION_FREQUENCY_VALUES{ 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
juce::StringArray const ATTENUATION_FREQUENCY_STRINGS{ "125", "250", "500", "1000", "2000", "4000", "8000", "16000 " };

//==============================================================================
template<typename T>
static juce::Array<T> stringToStronglyTypedFloat(juce::StringArray const & strings)
{
    static_assert(std::is_base_of_v<StrongFloatBase, T>);

    juce::Array<T> result{};
    result.ensureStorageAllocated(strings.size());
    for (auto const & string : strings) {
        result.add(T{ string.getFloatValue() });
    }
    return result;
}

//==============================================================================
tl::optional<int> attenuationDbToComboBoxIndex(dbfs_t const attenuation)
{
    static auto const ALLOWED_VALUES = stringToStronglyTypedFloat<dbfs_t>(ATTENUATION_DB_STRINGS);

    auto const index{ ALLOWED_VALUES.indexOf(attenuation) };
    if (index < 0) {
        return tl::nullopt;
    }
    return index + 1;
}

//==============================================================================
tl::optional<int> attenuationFreqToComboBoxIndex(hz_t const freq)
{
    static auto const ALLOWED_VALUES = stringToStronglyTypedFloat<hz_t>(ATTENUATION_FREQUENCY_STRINGS);

    auto const index{ ALLOWED_VALUES.indexOf(freq) };
    if (index < 0) {
        return tl::nullopt;
    }
    return index + 1;
}

} // namespace gris
