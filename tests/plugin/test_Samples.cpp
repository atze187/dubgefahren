#include <thread>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "engine/Kit.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "SampleTestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
struct SampleKit
{
    dgtest::TempDir tmp;
    juce::File kitFile = tmp.dir.getChildFile("Dub.dgkit");
    juce::File folder = tmp.dir.getChildFile("Dub");
    SampleKit()
    {
        folder.createDirectory();
        juce::WavAudioFormat wav;
        dgtest::writeSine(wav, folder.getChildFile("horn.wav"), 441.0f, 48000.0, 48000);
    }
};

void prepare(DubgefahrenProcessor& p)
{
    p.setPlayConfigDetails(0, 2, 48000.0, 512);
    p.prepareToPlay(48000.0, 512);
}

float processNote(DubgefahrenProcessor& p, int note)
{
    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)), 0);
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    p.processBlock(buf, on);
    return buf.getMagnitude(0, 0, 512);
}
} // namespace

TEST_CASE("choosing a sample sets sample defaults, loads it and plays it", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    CHECK(p.kitFile() == kit.kitFile);
    CHECK(p.sampleFolder() == kit.folder);

    p.setSlotSample(3, "horn.wav");
    CHECK(p.slotSource(3) == SourceType::Sample);
    CHECK(p.slotSample(3) == "horn.wav");
    CHECK(p.slotName(3) == "horn");
    const auto sp = readSlotFromParameters(p.state(), 3);
    CHECK(sp.tuneSemis == 0.0f);
    CHECK(sp.attackS == 0.0f);
    CHECK_THAT(sp.releaseS, WithinAbs(0.05, 1e-4));
    CHECK(sp.trigMode == TriggerMode::OneShot);
    CHECK_THAT(sp.volumeDb, WithinAbs(makeFactoryKit().slots[3].volumeDb, 1e-3)); // bleibt

    p.waitForSampleLoads();
    CHECK(p.isSampleLoaded(3));
    prepare(p);
    CHECK(processNote(p, 39) > 0.0f);
    CHECK((p.activeMask() & (1u << 3)) != 0u);
}

TEST_CASE("a missing sample empties the slot, keeps the name and reports it once", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(2, "missing.wav");
    p.waitForSampleLoads();
    CHECK(p.slotSource(2) == SourceType::Empty);
    CHECK(p.slotName(2) == "missing");
    CHECK(p.slotSample(2).isEmpty());
    const auto problems = p.takeSampleProblems();
    REQUIRE(problems.size() == 1);
    CHECK(problems[0] == juce::String::fromUTF8("Slot 3: missing.wav – file not found"));
    CHECK(p.takeSampleProblems().isEmpty());
}

TEST_CASE("without a kit file or with an unsafe name a sample slot fails", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.setSlotSample(0, "horn.wav"); // Factory-Zustand: keine Kit-Datei
    p.waitForSampleLoads();
    CHECK(p.slotSource(0) == SourceType::Empty);

    p.applyKit(makeFactoryKit(), kit.kitFile);
    SlotParams sp = readSlotFromParameters(p.state(), 1);
    sp.source = SourceType::Sample;
    p.setSlot(1, sp, "evil", "..\\Dub\\horn.wav");
    p.waitForSampleLoads();
    CHECK(p.slotSource(1) == SourceType::Empty);

    const auto problems = p.takeSampleProblems();
    REQUIRE(problems.size() == 2);
    CHECK(problems[0].endsWith("no kit file"));
    CHECK(problems[1].endsWith("invalid file name"));
}

TEST_CASE("state round-trip keeps the kit file and sample slots", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor a;
    a.applyKit(makeFactoryKit(), kit.kitFile);
    a.setSlotSample(4, "horn.wav");
    a.waitForSampleLoads();
    juce::MemoryBlock mb;
    a.getStateInformation(mb);

    DubgefahrenProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    b.waitForSampleLoads();
    CHECK(b.kitFile() == kit.kitFile);
    CHECK(b.slotSource(4) == SourceType::Sample);
    CHECK(b.slotSample(4) == "horn.wav");
    CHECK(b.isSampleLoaded(4));
    CHECK(b.takeSampleProblems().isEmpty());
}

TEST_CASE("restoring a state whose kit folder is gone empties the sample slots and reports them", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::MemoryBlock mb;
    {
        SampleKit kit;
        DubgefahrenProcessor a;
        a.applyKit(makeFactoryKit(), kit.kitFile);
        a.setSlotSample(4, "horn.wav");
        a.setSlotSample(6, "horn.wav");
        a.waitForSampleLoads();
        a.getStateInformation(mb);
    } // Kit-Ordner gelöscht

    DubgefahrenProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    b.waitForSampleLoads();
    CHECK(b.slotSource(4) == SourceType::Empty);
    CHECK(b.slotSource(6) == SourceType::Empty);
    CHECK(b.slotName(4) == "horn");
    CHECK(b.takeSampleProblems().size() == 2);
}

TEST_CASE("loading the factory kit clears the kit file and sample slots", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(4, "horn.wav");
    p.waitForSampleLoads();
    p.applyKit(makeFactoryKit(), {});
    p.waitForSampleLoads();
    CHECK(p.kitFile() == juce::File());
    CHECK(p.slotSource(4) == SourceType::Synth);
    CHECK(p.slotSample(4).isEmpty());
    CHECK_FALSE(p.isSampleLoaded(4));
}

TEST_CASE("currentKit and applyKit carry sample references", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor a;
    a.applyKit(makeFactoryKit(), kit.kitFile);
    a.setSlotSample(4, "horn.wav");
    const Kit k = a.currentKit();
    CHECK(k.samples[4] == "horn.wav");
    CHECK(k.samples[0].empty());

    DubgefahrenProcessor b;
    b.applyKit(k, kit.kitFile);
    b.waitForSampleLoads();
    CHECK(b.isSampleLoaded(4));
}

TEST_CASE("replaced sample data is freed only after an audio block", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    prepare(p);
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(4, "horn.wav");
    p.waitForSampleLoads();
    REQUIRE(p.isSampleLoaded(4));

    p.setSlotSample(4, "horn.wav"); // neu laden: alte Daten in die Garbage
    p.waitForSampleLoads();
    CHECK(p.pendingSampleGarbage() == 1);
    processNote(p, 0);
    p.waitForSampleLoads(); // räumt auf
    CHECK(p.pendingSampleGarbage() == 0);

    DubgefahrenProcessor idle; // ohne prepareToPlay: sofort frei
    idle.applyKit(makeFactoryKit(), kit.kitFile);
    idle.setSlotSample(4, "horn.wav");
    idle.waitForSampleLoads();
    idle.clearSlot(4);
    idle.waitForSampleLoads();
    CHECK(idle.pendingSampleGarbage() == 0);
}

TEST_CASE("a stale load result is ignored", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(4, "horn.wav");
    p.resetSlotToFactory(4); // bevor das Ergebnis übernommen wurde
    p.waitForSampleLoads();
    CHECK(p.slotSource(4) == SourceType::Synth);
    CHECK_FALSE(p.isSampleLoaded(4));
    CHECK(p.takeSampleProblems().isEmpty());
}

TEST_CASE("setStateInformation from another thread defers the sample reload", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    juce::MemoryBlock mb;
    {
        DubgefahrenProcessor a;
        a.applyKit(makeFactoryKit(), kit.kitFile);
        a.setSlotSample(4, "horn.wav");
        a.waitForSampleLoads();
        a.getStateInformation(mb);
    }

    DubgefahrenProcessor b;
    std::thread t([&] { b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize())); });
    t.join();
    b.waitForSampleLoads();
    CHECK(b.slotSource(4) == SourceType::Sample);
    CHECK(b.isSampleLoaded(4));
}
