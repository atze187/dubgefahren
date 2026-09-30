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

TEST_CASE("a missing sample keeps the slot and its reference, mutes it and reports it once", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(2, "missing.wav");
    CHECK_FALSE(p.isSampleMissing(2)); // lädt noch
    p.waitForSampleLoads();
    CHECK(p.slotSource(2) == SourceType::Sample);
    CHECK(p.slotName(2) == "missing");
    CHECK(p.slotSample(2) == "missing.wav");
    CHECK_FALSE(p.isSampleLoaded(2));
    CHECK(p.isSampleMissing(2));
    CHECK_FALSE(p.isSampleMissing(0)); // Synth-Slot
    CHECK(p.currentKit().samples[2] == "missing.wav");

    prepare(p);
    CHECK(processNote(p, 38) == 0.0f); // stumm
    CHECK((p.activeMask() & (1u << 2)) == 0u);

    const auto problems = p.takeSampleProblems();
    REQUIRE(problems.size() == 1);
    CHECK(problems[0] == juce::String::fromUTF8("Slot 3: missing.wav – file not found"));
    CHECK(p.takeSampleProblems().isEmpty());
}

TEST_CASE("without a kit file a sample slot stays but is muted; an unsafe name empties it", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.setSlotSample(0, "horn.wav"); // Factory-Zustand: keine Kit-Datei
    p.waitForSampleLoads();
    CHECK(p.slotSource(0) == SourceType::Sample);
    CHECK(p.slotSample(0) == "horn.wav");
    CHECK(p.isSampleMissing(0));

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

TEST_CASE("a set restored while the kit folder is gone keeps its samples and loads them once it is back", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    juce::MemoryBlock mb;
    {
        DubgefahrenProcessor a;
        a.applyKit(makeFactoryKit(), kit.kitFile);
        a.setSlotSample(4, "horn.wav");
        a.setSlotSample(6, "horn.wav");
        a.waitForSampleLoads();
        a.getStateInformation(mb);
    }
    const auto away = kit.tmp.dir.getChildFile("unplugged");
    REQUIRE(kit.folder.moveFileTo(away)); // Sample-Laufwerk abgesteckt

    DubgefahrenProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    b.waitForSampleLoads();
    CHECK(b.slotSource(4) == SourceType::Sample);
    CHECK(b.slotSource(6) == SourceType::Sample);
    CHECK(b.slotName(4) == "horn");
    CHECK(b.isSampleMissing(4));
    CHECK(b.isSampleMissing(6));
    CHECK(b.takeSampleProblems().size() == 2);

    // Speichern im stummen Zustand verliert nichts.
    juce::MemoryBlock saved;
    b.getStateInformation(saved);

    REQUIRE(away.moveFileTo(kit.folder)); // wieder angesteckt
    DubgefahrenProcessor c;
    c.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    c.waitForSampleLoads();
    CHECK(c.isSampleLoaded(4));
    CHECK(c.isSampleLoaded(6));
    CHECK(c.takeSampleProblems().isEmpty());

    b.setSlotSample(4, "horn.wav"); // erneut wählen lädt nach
    b.waitForSampleLoads();
    CHECK(b.isSampleLoaded(4));
    CHECK_FALSE(b.isSampleMissing(4));
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

TEST_CASE("pending sample loads are reported until the results are handled", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    CHECK_FALSE(p.hasPendingSampleLoads());

    p.setSlotSample(1, "missing1.wav");
    p.setSlotSample(2, "horn.wav");
    p.setSlotSample(3, "missing3.wav");
    CHECK(p.hasPendingSampleLoads()); // bleibt wahr, bis die Ergebnisse übernommen sind
    p.waitForSampleLoads();
    CHECK_FALSE(p.hasPendingSampleLoads());
    CHECK(p.isSampleLoaded(2));

    // Ein veralteter Auftrag zählt nicht als offen: Slot sofort wieder leeren.
    p.setSlotSample(2, "horn.wav");
    CHECK(p.hasPendingSampleLoads());
    p.clearSlot(2);
    CHECK_FALSE(p.hasPendingSampleLoads());
    p.waitForSampleLoads();
    CHECK_FALSE(p.hasPendingSampleLoads());
}

TEST_CASE("choosing another sample resets the region fields, choosing the same one keeps them", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    juce::WavAudioFormat wav;
    dgtest::writeSine(wav, kit.folder.getChildFile("bell.wav"), 880.0f, 48000.0, 24000);
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(3, "horn.wav");

    SlotParams sp = readSlotFromParameters(p.state(), 3);
    CHECK(sp.sampleStart == 0.0f); // frisch gewählt: Standardwerte
    CHECK(sp.sampleEnd == 1.0f);
    sp.sampleStart = 0.25f;
    sp.loopStart = 0.5f;
    sp.sampleEnd = 0.75f;
    sp.loop = true;
    sp.reverse = true;
    sp.loopXfadePct = 20.0f;
    sp.tuneSemis = 3.0f;
    p.setSlot(3, sp, "horn", "horn.wav");

    p.setSlotSample(3, "horn.wav"); // dasselbe Sample: alles bleibt
    sp = readSlotFromParameters(p.state(), 3);
    CHECK_THAT(sp.sampleStart, WithinAbs(0.25, 1e-4));
    CHECK(sp.loop);

    p.setSlotSample(3, "bell.wav"); // anderes Sample: Bereich zurück, der Rest bleibt
    sp = readSlotFromParameters(p.state(), 3);
    CHECK(sp.sampleStart == 0.0f);
    CHECK(sp.loopStart == 0.0f);
    CHECK(sp.sampleEnd == 1.0f);
    CHECK_FALSE(sp.loop);
    CHECK_FALSE(sp.reverse);
    CHECK_THAT(sp.loopXfadePct, WithinAbs(5.0, 1e-4));
    CHECK_THAT(sp.tuneSemis, WithinAbs(3.0, 1e-4));
}

TEST_CASE("the processor hands out the loaded sample data of a slot", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    CHECK(p.slotSampleData(3) == nullptr);
    p.setSlotSample(3, "horn.wav");
    p.waitForSampleLoads();
    const auto data = p.slotSampleData(3);
    REQUIRE(data != nullptr);
    CHECK(data->samples.size() == 48000u);
    CHECK(p.slotSampleData(0) == nullptr); // Synth-Slot
    p.resetSlotToFactory(3);
    CHECK(p.slotSampleData(3) == nullptr);
    CHECK(data->samples.size() == 48000u); // die herausgegebenen Daten bleiben gültig
}

TEST_CASE("a host state without the region parameters restores their defaults", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    juce::MemoryBlock block;
    a.getStateInformation(block);

    // Den Zustand so zurechtschneiden, wie ihn Version 0.4.0 geschrieben hat: ohne die neuen Parameter.
    auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(xml != nullptr);
    int removed = 0;
    for (const char* key : { "smpStart", "loopStart", "smpEnd", "loop", "reverse", "xfade" })
        for (int s = 0; s < kNumSlots; ++s)
        {
            const auto id = juce::String::formatted("s%02d_", s + 1) + key;
            for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
                if (child->getStringAttribute("id") == id)
                {
                    xml->removeChildElement(child, true);
                    ++removed;
                    break;
                }
        }
    REQUIRE(removed == 6 * kNumSlots);
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml, old);

    DubgefahrenProcessor b;
    SlotParams sp = readSlotFromParameters(b.state(), 0);
    sp.sampleEnd = 0.5f;
    sp.loop = true;
    sp.loopXfadePct = 30.0f;
    b.setSlot(0, sp, "x");
    b.setStateInformation(old.getData(), static_cast<int>(old.getSize()));

    sp = readSlotFromParameters(b.state(), 0);
    CHECK(sp.sampleStart == 0.0f);
    CHECK(sp.sampleEnd == 1.0f);
    CHECK_FALSE(sp.loop);
    CHECK_THAT(sp.loopXfadePct, WithinAbs(5.0, 1e-4));
}
