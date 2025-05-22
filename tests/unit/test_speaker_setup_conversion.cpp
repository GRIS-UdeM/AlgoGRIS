#include <catch2/catch_all.hpp>
#include "../../StructGRIS/ValueTreeUtilities.hpp"

void checkSpeakerSetupConversion(juce::File setupDir, std::string version, std::string speakerSetupPath)
{
    SECTION(std::string("Converting speaker setup ") + version)
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
    auto const algogrisDir{ juce::File::getCurrentWorkingDirectory() };
    // std::cout << "algogrisDir: " << algogrisDir.getFullPathName() << "\n";
    REQUIRE(algogrisDir.exists());

    auto const setupDir{ algogrisDir.getChildFile("../../Resources/templates/Speaker setups") };
    // std::cout << "templateDir: " << setupDir.getFullPathName() << "\n";
    REQUIRE(setupDir.exists());

    const auto version = "3.1.14";
    const auto speakerSetupPath = "CUBE/Cube_default_speaker_setup.xml";
    checkSpeakerSetupConversion(setupDir, version, speakerSetupPath);

    // GIVEN("Speaker Setup Version 3.3.7")
    // {
    //     auto const speakerSetupFile = setupDir.getChildFile("DOME/Dome32(4X8)Subs4 SubMix.xml");
    //     std::cout << "speakerSetupFile: " << speakerSetupFile.getFullPathName() << "\n";
    //     REQUIRE(speakerSetupFile.exists());

    //     THEN("Check Conversion")
    //     {
    //         REQUIRE(gris::convertSpeakerSetup(juce::ValueTree::fromXml(speakerSetupFile.loadFileAsString())).isValid());
    //     }
    // }
}
