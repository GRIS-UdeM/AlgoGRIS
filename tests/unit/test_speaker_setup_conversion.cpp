#include <catch2/catch_all.hpp>
#include "../../StructGRIS/ValueTreeUtilities.hpp"

void checkSpeakerSetupConversion(const juce::File & setupDir, std::string && version, std::string && speakerSetupPath)
{
    SECTION("Converting speaker setup " + version)
    {
        auto const speakerSetupFile = setupDir.getChildFile(speakerSetupPath);
        REQUIRE(speakerSetupFile.exists());

        auto const vt = gris::convertSpeakerSetup(juce::ValueTree::fromXml(speakerSetupFile.loadFileAsString()));
        REQUIRE(vt.isValid());
        REQUIRE(vt[gris::VERSION] == gris::CURRENT_SPEAKER_SETUP_VERSION);
    }
}

TEST_CASE("Speaker Setup Conversion", "[core]")
{
    auto const setupDir{ juce::File::getCurrentWorkingDirectory().getChildFile(
        "../../Resources/templates/Speaker setups") };
    // std::cout << "templateDir: " << setupDir.getFullPathName() << "\n";
    REQUIRE(setupDir.exists());

    checkSpeakerSetupConversion(setupDir, "3.1.14", "CUBE/Cube_default_speaker_setup.xml");
    checkSpeakerSetupConversion(setupDir, "3.3.7", "DOME/Dome32(4X8)Subs4 SubMix.xml");
}
