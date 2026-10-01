#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <utility>
#include <vector>
#include "plugin/ParameterLayout.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/SampleFiles.h"
#include "engine/Kit.h"
#include "SampleTestHelpers.h"

using namespace dg;

TEST_CASE("editor opens with the stored scale and follows focus", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.setUiScale(1.5f);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    REQUIRE(dynamic_cast<DubgefahrenEditor*>(editor.get()) != nullptr);
    CHECK(editor->getWidth() == 1500);
    CHECK(editor->getHeight() == 960);

    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    CHECK(e->selectedSlot() == 0);
    e->selectSlot(6);
    CHECK(e->selectedSlot() == 6);
}

TEST_CASE("editor refreshes after the host restores state", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    DubgefahrenProcessor a;
    a.setSlotName(1, "Alpha");
    a.setEditorFollowsFocus(false);
    juce::MemoryBlock state;
    a.getStateInformation(state);

    DubgefahrenProcessor b;
    std::unique_ptr<juce::AudioProcessorEditor> editor(b.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    b.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    e->pollProcessorState();

    CHECK(b.slotName(1) == "Alpha");
    CHECK(e->followFocusToggleState() == false);
}

TEST_CASE("editor shows the cpu meter in the header", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    CHECK(e->cpuMeterText() == "CPU 0 %");
}

namespace {
std::vector<std::pair<juce::String, bool>> menuItems(const juce::PopupMenu& m)
{
    std::vector<std::pair<juce::String, bool>> out;
    for (juce::PopupMenu::MenuItemIterator it(m); it.next();)
    {
        const auto& item = it.getItem();
        if (!item.isSeparator)
            out.emplace_back(item.text, item.isEnabled);
    }
    return out;
}
using Items = std::vector<std::pair<juce::String, bool>>;
} // namespace

TEST_CASE("empty slots show a hint in the slot editor and 'Empty' on the pad", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.clearSlot(2);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    CHECK(e->padShowsEmpty(2));
    CHECK_FALSE(e->padShowsEmpty(0));
    e->selectSlot(2);
    CHECK(e->slotEditorShowsEmptyHint());
    e->selectSlot(0);
    CHECK_FALSE(e->slotEditorShowsEmptyHint());

    p.clearSlot(0); // von außen geändert: Timer-Poll frischt auf
    e->pollProcessorState();
    CHECK(e->padShowsEmpty(0));
    CHECK(e->slotEditorShowsEmptyHint());
}

TEST_CASE("without a kit file the source menu offers synth and a disabled sample hint", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    CHECK(menuItems(e->buildSourceMenu({})) == Items { { "Synth", true }, { "Sample (export the kit first)", false } });
}

TEST_CASE("with a kit file the source menu has a sample submenu with the folder's files", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), tmp.dir.getChildFile("Dub.dgkit"));
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    CHECK(menuItems(e->buildSourceMenu({ "a.wav" })) == Items { { "Synth", true }, { "Sample", true } });
    const auto addFile = juce::String::fromUTF8("Add File…");
    CHECK(menuItems(DubgefahrenEditor::buildSampleMenu({ "a.wav", "b.mp3" }))
          == Items { { "a.wav", true }, { "b.mp3", true }, { addFile, true } });
    CHECK(menuItems(DubgefahrenEditor::buildSampleMenu({}))
          == Items { { "(no samples in kit folder)", false }, { addFile, true } });
}

TEST_CASE("a sample slot shows the sample controls, its file and no latch", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    const auto kitFile = tmp.dir.getChildFile("Dub.dgkit");
    sampleFolderFor(kitFile).createDirectory();
    juce::WavAudioFormat wav;
    dgtest::writeSine(wav, sampleFolderFor(kitFile).getChildFile("horn.wav"), 441.0f, 48000.0, 4800);

    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kitFile);
    p.setSlotSample(2, "horn.wav");
    p.waitForSampleLoads();
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    e->selectSlot(2);
    CHECK(e->slotEditorShowsSampleControls());
    CHECK(e->slotEditorSampleText() == "horn.wav");
    CHECK_FALSE(e->slotEditorLatchSelectable());
    CHECK_FALSE(e->slotEditorShowsEmptyHint());
    CHECK(e->padShowsSample(2));
    CHECK_FALSE(e->padShowsSample(0));

    e->selectSlot(0);
    CHECK_FALSE(e->slotEditorShowsSampleControls());
    CHECK(e->slotEditorLatchSelectable());
}

TEST_CASE("sample load problems are shown once as a message", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), tmp.dir.getChildFile("Dub.dgkit"));
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    p.setSlotSample(1, "missing.wav");
    p.waitForSampleLoads();
    e->pollProcessorState();
    CHECK(e->lastMessage().contains("Some samples could not be loaded"));
    CHECK(e->lastMessage().contains("missing.wav"));
    CHECK(p.takeSampleProblems().isEmpty());
}


TEST_CASE("several failing samples produce one message listing all of them", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), tmp.dir.getChildFile("Dub.dgkit"));
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    p.setSlotSample(1, "missing1.wav");
    p.setSlotSample(3, "missing3.wav");
    e->pollProcessorState(); // Aufträge laufen noch: keine (Teil-)Meldung
    CHECK(e->lastMessage().isEmpty());
    p.waitForSampleLoads();
    e->pollProcessorState();
    CHECK(e->lastMessage().contains("Slot 2"));
    CHECK(e->lastMessage().contains("Slot 4"));
}

TEST_CASE("exporting to a new kit copies the samples and switches the kit file", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    const auto oldKit = tmp.dir.getChildFile("Old.dgkit");
    sampleFolderFor(oldKit).createDirectory();
    juce::WavAudioFormat wav;
    dgtest::writeSine(wav, sampleFolderFor(oldKit).getChildFile("horn.wav"), 441.0f, 48000.0, 4800);

    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), oldKit);
    p.setSlotSample(2, "horn.wav");
    p.waitForSampleLoads();
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    const auto newKit = tmp.dir.getChildFile("New.dgkit");
    CHECK(e->exportKitTo(newKit));
    CHECK(newKit.existsAsFile());
    CHECK(sampleFolderFor(newKit).getChildFile("horn.wav").existsAsFile());
    CHECK(p.kitFile() == newKit);
    CHECK(p.isSampleLoaded(2));
}

TEST_CASE("pad menu disables rename and clear on empty slots", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.clearSlot(1);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    CHECK(menuItems(e->buildPadMenu(0)) == Items { { "Copy", true }, { "Paste", false }, { "Reset to Factory Default", true },
                                                   { "Rename", true }, { "Clear Slot", true } });
    CHECK(menuItems(e->buildPadMenu(1)) == Items { { "Copy", true }, { "Paste", false }, { "Reset to Factory Default", true },
                                                   { "Rename", false }, { "Clear Slot", false } });
}

TEST_CASE("kit menu offers a new empty kit", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    const auto items = menuItems(e->buildKitMenu({}));
    REQUIRE(items.size() >= 2);
    CHECK(items[0] == std::make_pair(juce::String("Load Factory Kit"), true));
    CHECK(items[1] == std::make_pair(juce::String("New Empty Kit"), true));
}

TEST_CASE("editor refreshes when the host changes a slot source directly", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    e->selectSlot(3);

    // z. B. generischer Host-Editor oder Host-Undo: kein setSlot, keine stateGeneration-Änderung
    auto* source = p.state().getParameter("s04_source");
    source->setValueNotifyingHost(source->convertTo0to1(static_cast<float>(SourceType::Empty)));
    e->pollProcessorState();
    CHECK(e->padShowsEmpty(3));
    CHECK(e->slotEditorShowsEmptyHint());

    source->setValueNotifyingHost(source->convertTo0to1(static_cast<float>(SourceType::Synth)));
    e->pollProcessorState();
    CHECK_FALSE(e->padShowsEmpty(3));
    CHECK_FALSE(e->slotEditorShowsEmptyHint());
}

TEST_CASE("a sample slot whose file is missing is marked on the pad and in the slot editor", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), tmp.dir.getChildFile("Dub.dgkit"));
    p.setSlotSample(2, "missing.wav");
    p.waitForSampleLoads();
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    CHECK(e->padShowsSample(2));
    CHECK(e->padShowsMissing(2));
    CHECK_FALSE(e->padShowsMissing(0));
    e->selectSlot(2);
    CHECK(e->slotEditorShowsSampleControls());
    CHECK(e->slotEditorSampleText() == "missing.wav (missing)");
}

namespace {
// Processor mit Kit-Ordner und horn.wav in Slot 3 (Index 2).
struct SampleSlotFixture
{
    dgtest::TempDir tmp;
    juce::File kitFile = tmp.dir.getChildFile("Dub.dgkit");
    DubgefahrenProcessor p;
    SampleSlotFixture()
    {
        sampleFolderFor(kitFile).createDirectory();
        juce::WavAudioFormat wav;
        dgtest::writeSine(wav, sampleFolderFor(kitFile).getChildFile("horn.wav"), 441.0f, 48000.0, 4800);
        p.applyKit(makeFactoryKit(), kitFile);
        p.setSlotSample(2, "horn.wav");
        p.waitForSampleLoads();
    }
    void set(SlotField f, float v)
    {
        auto* param = p.state().getParameter(slotParamId(2, f));
        param->setValueNotifyingHost(param->convertTo0to1(v));
    }
};
} // namespace

TEST_CASE("a sample slot shows the waveform and the loop controls", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleSlotFixture f;
    std::unique_ptr<juce::AudioProcessorEditor> editor(f.p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    e->selectSlot(2);
    auto& slotEditor = e->slotEditor();
    CHECK(slotEditor.waveform().isVisible());
    CHECK(slotEditor.waveform().hasData());
    CHECK(slotEditor.showsLoopControls());
    // Streifen über die volle Breite, eine Reihe hoch, unter der Kopfzeile.
    const auto w = slotEditor.waveform().getBounds();
    CHECK(w.getX() == 12);
    CHECK(w.getWidth() == slotEditor.getWidth() - 24);
    CHECK(w.getY() >= 40);
    CHECK(w.getBottom() <= 40 + 84);

    e->selectSlot(0); // Synth-Slot: unverändert
    CHECK_FALSE(slotEditor.waveform().isVisible());
    CHECK_FALSE(slotEditor.showsLoopControls());

    f.p.clearSlot(2);
    e->pollProcessorState();
    e->selectSlot(2); // leerer Slot
    CHECK_FALSE(slotEditor.waveform().isVisible());
    CHECK_FALSE(slotEditor.showsLoopControls());
}

TEST_CASE("latch becomes selectable for a sample slot when loop is on", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleSlotFixture f;
    std::unique_ptr<juce::AudioProcessorEditor> editor(f.p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    e->selectSlot(2);
    CHECK_FALSE(e->slotEditorLatchSelectable());

    f.set(SlotField::Loop, 1.0f); // wie Automation oder ein Klick auf den Schalter
    CHECK(e->slotEditorLatchSelectable());
    CHECK(e->slotEditor().waveform().isMarkerVisible(ui::WaveformView::Marker::Loop));

    f.set(SlotField::Loop, 0.0f);
    CHECK_FALSE(e->slotEditorLatchSelectable());

    // Ein anderer Sample-Slot mit Loop an: der Zustand folgt dem Slot.
    f.p.setSlotSample(4, "horn.wav");
    auto* loop5 = f.p.state().getParameter(slotParamId(4, SlotField::Loop));
    loop5->setValueNotifyingHost(1.0f);
    e->pollProcessorState();
    e->selectSlot(4);
    CHECK(e->slotEditorLatchSelectable());
    e->selectSlot(2);
    CHECK_FALSE(e->slotEditorLatchSelectable());
    e->selectSlot(0); // Synth: Latch immer wählbar
    CHECK(e->slotEditorLatchSelectable());
}

TEST_CASE("the waveform follows a sample that finishes loading or goes missing", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleSlotFixture f;
    std::unique_ptr<juce::AudioProcessorEditor> editor(f.p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    e->selectSlot(2);
    REQUIRE(e->slotEditor().waveform().hasData());

    f.p.setSlotSample(2, "gone.wav");
    f.p.waitForSampleLoads();
    e->pollProcessorState();
    CHECK_FALSE(e->slotEditor().waveform().hasData());
    CHECK(e->slotEditor().waveform().hintText() == "Sample missing");

    f.p.setSlotSample(2, "horn.wav");
    f.p.waitForSampleLoads();
    e->pollProcessorState();
    CHECK(e->slotEditor().waveform().hasData());
}
