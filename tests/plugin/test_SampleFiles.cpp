#include <catch2/catch_test_macros.hpp>
#include "engine/Kit.h"
#include "plugin/SampleFiles.h"
#include "SampleTestHelpers.h"

using namespace dg;

namespace {
void writeText(const juce::File& f, const juce::String& text) { REQUIRE(f.replaceWithText(text)); }
} // namespace

TEST_CASE("the sample folder sits next to the kit file and has its name", "[samples]")
{
    dgtest::TempDir tmp;
    const auto kit = tmp.dir.getChildFile("Dub Kit.dgkit");
    CHECK(sampleFolderFor(kit) == tmp.dir.getChildFile("Dub Kit"));
    CHECK(sampleFolderFor(juce::File()) == juce::File());
}

TEST_CASE("only audio files are listed, sorted without regard to case", "[samples]")
{
    dgtest::TempDir tmp;
    for (const char* n : { "b.mp3", "a.WAV", "notes.txt", "C.flac", "d.aif", "e.aiff" })
        writeText(tmp.dir.getChildFile(n), "x");
    CHECK(listSampleFiles(tmp.dir) == juce::StringArray { "a.WAV", "b.mp3", "C.flac", "d.aif", "e.aiff" });
    CHECK(listSampleFiles(tmp.dir.getChildFile("missing")).isEmpty());
    CHECK(listSampleFiles(juce::File()).isEmpty());
    CHECK(sampleFileWildcard() == "*.wav;*.aif;*.aiff;*.mp3;*.flac");
}

TEST_CASE("importing a file copies it into the kit folder without overwriting", "[samples]")
{
    dgtest::TempDir tmp;
    const auto src = tmp.dir.getChildFile("elsewhere").getChildFile("horn.wav");
    src.getParentDirectory().createDirectory();
    writeText(src, "one");
    const auto folder = tmp.dir.getChildFile("Dub");

    juce::String error;
    const auto first = importSampleFile(src, folder, error);
    CHECK(first == folder.getChildFile("horn.wav"));
    CHECK(first.loadFileAsString() == "one");

    CHECK(importSampleFile(src, folder, error) == first); // gleicher Inhalt: wiederverwenden

    writeText(src, "two");
    const auto second = importSampleFile(src, folder, error);
    CHECK(second == folder.getChildFile("horn (2).wav"));
    CHECK(first.loadFileAsString() == "one");

    CHECK(importSampleFile(first, folder, error) == first); // liegt schon im Ordner

    CHECK(importSampleFile(tmp.dir.getChildFile("nope.wav"), folder, error) == juce::File());
    CHECK(error.isNotEmpty());
}

TEST_CASE("exporting copies the used samples and reports missing ones", "[samples]")
{
    dgtest::TempDir tmp;
    const auto from = tmp.dir.getChildFile("Old");
    const auto to = tmp.dir.getChildFile("New");
    from.createDirectory();
    to.createDirectory();
    writeText(from.getChildFile("horn.wav"), "horn");
    writeText(to.getChildFile("horn.wav"), "stale");

    Kit k = makeFactoryKit();
    k.slots[0].source = SourceType::Sample;
    k.samples[0] = "horn.wav";
    k.slots[1].source = SourceType::Sample;
    k.samples[1] = "horn.wav"; // doppelt genutzt: nur einmal kopieren
    k.slots[2].source = SourceType::Sample;
    k.samples[2] = "gone.wav";
    k.samples[3] = "ignored.wav"; // kein Sample-Slot

    const auto problems = copyKitSamples(k, from, to);
    CHECK(to.getChildFile("horn.wav").loadFileAsString() == "horn");
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].startsWith("gone.wav"));
    CHECK_FALSE(to.getChildFile("ignored.wav").exists());

    CHECK(copyKitSamples(k, from, from).isEmpty());
}
