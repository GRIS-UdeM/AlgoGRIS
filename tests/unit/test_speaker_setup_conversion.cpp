#include <catch2/catch_all.hpp>
#include "../../StructGRIS/ValueTreeUtilities.hpp"

void checkSpeakerSetupConversion(const juce::File & setupDir, std::string && version, std::string && speakerSetupPath)
{
    SECTION("Converting speaker setup " + speakerSetupPath + " with version " + version)
    {
        auto const speakerSetupFile = setupDir.getChildFile(speakerSetupPath);
        REQUIRE(speakerSetupFile.exists());

        // std::cout << "converting " << speakerSetupFile.getFileName() << "\n";
        auto const vt = gris::convertSpeakerSetup(juce::ValueTree::fromXml(speakerSetupFile.loadFileAsString()));
        REQUIRE(vt.isValid());
        REQUIRE(vt[gris::VERSION] == gris::CURRENT_SPEAKER_SETUP_VERSION);
    }
}

TEST_CASE("Speaker Setup Conversion", "[core]")
{
    auto const speakerSetupDir{ juce::File::getCurrentWorkingDirectory().getChildFile("tests/temp") };
    REQUIRE(speakerSetupDir.exists());

    checkSpeakerSetupConversion(speakerSetupDir, "3.1.14", "Cube_default_speaker_setup.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.1.14", "Dome_default_speaker_setup.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.2.11", "Cube0(0)Subs0.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.2.11", "Dome0(0)Subs0.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.2.3", "Cube12(3X4)Subs2 Centres.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.2.5", "Dome13(9-4)Subs2 Bremen.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.2.9", "Dome8(4-4)Subs1 ZiMMT Small Studio.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.3.0", "Cube93(32-32-16-8-4-1)Subs5 Satosphere.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.3.0", "Dome93(32-32-16-8-4-1)Subs5 Satosphere.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.3.5", "Cube26(8-8-6-2-2)Subs3 Lisbonne.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.3.5", "Dome61(29-11-14-7)Subs0 Brahms.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.3.6", "Cube24(8-8-8)Subs2 Studio PANaroma.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.3.6", "Dome20(8-6-4-2)Sub4 Lakefield Icosa.xml");
    checkSpeakerSetupConversion(speakerSetupDir, "3.3.7", "Dome32(4X8)Subs4 SubMix.xml");
}
