#include <catch2/catch_all.hpp>
#include "../../Data/sg_LegacySpatFileFormat.hpp"
#include "../../StructGRIS/ValueTreeUtilities.hpp"

void checkSpeakerSetupConversion(const std::string & version, const std::string & speakerSetupName)
{
    SECTION("Converting speaker setup " + speakerSetupName + " with version " + version)
    {
        // get the speaker setup directory. The build hack is because the pipeline uses that as current directory
        auto speakerSetupDir = juce::File::getCurrentWorkingDirectory();
        if (speakerSetupDir.getFileName() == "build")
            speakerSetupDir = speakerSetupDir.getParentDirectory();
        speakerSetupDir = speakerSetupDir.getChildFile("tests/temp");
        REQUIRE(speakerSetupDir.exists());

        // now get the speaker setup file and make sure it also exists
        auto const speakerSetupFile = speakerSetupDir.getChildFile(speakerSetupName);
        REQUIRE(speakerSetupFile.exists());

        // do the conversion and make sure it's valid
        auto const vt = [&version, &speakerSetupFile]() {
            if (version == "0") {
                auto const savedElement{ juce::XmlDocument{ speakerSetupFile }.getDocumentElement() };
                auto speakerSetup{ gris::readLegacySpeakerSetup(*savedElement) };
                return gris::SpeakerSetup::toVt(*speakerSetup);
            } else {
                return gris::convertSpeakerSetup(juce::ValueTree::fromXml(speakerSetupFile.loadFileAsString()));
            }
        }();

        REQUIRE(vt.isValid());
        REQUIRE(vt[gris::SPEAKER_SETUP_VERSION] == gris::CURRENT_SPEAKER_SETUP_VERSION);
    }
}

TEST_CASE("Speaker Setup Conversion", "[core]")
{
    checkSpeakerSetupConversion("3.1.14", "Cube_default_speaker_setup.xml");
    checkSpeakerSetupConversion("3.1.14", "Dome_default_speaker_setup.xml");
    checkSpeakerSetupConversion("3.2.11", "Cube0(0)Subs0.xml");
    checkSpeakerSetupConversion("3.2.11", "Dome0(0)Subs0.xml");
    checkSpeakerSetupConversion("3.2.3", "Cube12(3X4)Subs2 Centres.xml");
    checkSpeakerSetupConversion("3.2.5", "Dome13(9-4)Subs2 Bremen.xml");
    checkSpeakerSetupConversion("3.2.9", "Dome8(4-4)Subs1 ZiMMT Small Studio.xml");
    checkSpeakerSetupConversion("3.3.0", "Cube93(32-32-16-8-4-1)Subs5 Satosphere.xml");
    checkSpeakerSetupConversion("3.3.0", "Dome93(32-32-16-8-4-1)Subs5 Satosphere.xml");
    checkSpeakerSetupConversion("3.3.5", "Cube26(8-8-6-2-2)Subs3 Lisbonne.xml");
    checkSpeakerSetupConversion("3.3.5", "Dome61(29-11-14-7)Subs0 Brahms.xml");
    checkSpeakerSetupConversion("3.3.6", "Cube24(8-8-8)Subs2 Studio PANaroma.xml");
    checkSpeakerSetupConversion("3.3.6", "Dome20(8-6-4-2)Sub4 Lakefield Icosa.xml");
    checkSpeakerSetupConversion("3.3.7", "Dome32(4X8)Subs4 SubMix.xml");
    checkSpeakerSetupConversion("0", "default_speaker_setup.xml");
}
