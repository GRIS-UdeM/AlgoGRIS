#include <catch2/catch_all.hpp>
#include "../../StructGRIS/ValueTreeUtilities.hpp"

TEST_CASE("Speaker Setup Conversion", "[core]")
{
    auto const algogrisDir{ juce::File::getCurrentWorkingDirectory() };
    std::cout << "algogrisDir: " << algogrisDir.getFullPathName() << "\n";
    REQUIRE(algogrisDir.exists());

    auto const setupDir{ algogrisDir.getChildFile("../../Resources/templates/Speaker setups") };
    std::cout << "templateDir: " << setupDir.getFullPathName() << "\n";
    REQUIRE(setupDir.exists());

    GIVEN("Speaker Setup Version 3.1.14")
    {
        auto const speakerSetupFile = setupDir.getChildFile("CUBE/Cube_default_speaker_setup.xml");
        std::cout << "speakerSetupFile: " << speakerSetupFile.getFullPathName() << "\n";
        REQUIRE(speakerSetupFile.exists());

        THEN("Check Conversion")
        {
            auto const vt = gris::convertSpeakerSetup(juce::ValueTree::fromXml(speakerSetupFile.loadFileAsString()));
            REQUIRE(vt.isValid());
            REQUIRE(vt["VERSION"].toString() == "4.0.0");
        }
    }

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
