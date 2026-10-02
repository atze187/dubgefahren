#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <utility>
#include <vector>
#include "plugin/ParameterLayout.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/SampleFiles.h"
#include "plugin/PadMapping.h"
#include "plugin/ui/PadGrid.h"
#include "PadMappingTestHelpers.h"
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

TEST_CASE("the pad grid lays out pad 1 at the bottom left or the top left", "[editor][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    grid.setSize(360, 392);
    const auto at = [&](int slot) { return grid.pad(slot).getBounds().getPosition(); };

    // Standard: Pad 1 unten links, Pad 5 darüber, Pad 13 in der obersten Reihe.
    CHECK(at(0).x == at(4).x);
    CHECK(at(0).x < at(1).x);
    CHECK(at(0).y == at(1).y);
    CHECK(at(0).y > at(4).y);
    CHECK(at(4).y > at(12).y);

    CHECK(grid.setOrigin(PadOrigin::TopLeft));
    // Oben links: Pad 1 in der obersten Reihe, Pad 13 unten; die Spalten bleiben gleich.
    CHECK(at(0).x == at(4).x);
    CHECK(at(0).x < at(1).x);
    CHECK(at(0).y == at(3).y);
    CHECK(at(0).y < at(4).y);
    CHECK(at(4).y < at(12).y);
    CHECK(at(12).y - at(0).y == 3 * (at(4).y - at(0).y));

    CHECK_FALSE(grid.setOrigin(PadOrigin::TopLeft)); // unverändert
    CHECK(grid.setOrigin(PadOrigin::BottomLeft));
    CHECK(at(0).y > at(12).y);
}

namespace {
// (Text, Häkchen) der Einträge eines Menüs.
std::vector<std::pair<juce::String, bool>> tickedItems(const juce::PopupMenu& m)
{
    std::vector<std::pair<juce::String, bool>> out;
    for (juce::PopupMenu::MenuItemIterator it(m); it.next();)
        if (!it.getItem().isSeparator)
            out.emplace_back(it.getItem().text, it.getItem().isTicked);
    return out;
}
} // namespace

TEST_CASE("the MIDI menu shows the pad 1 note and the origin and changes the origin", "[editor][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    auto& mapping = PadMapping::instance();
    mapping.setFirstNote(36);
    mapping.setOrigin(PadOrigin::BottomLeft);
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    using Items = std::vector<std::pair<juce::String, bool>>;
    CHECK(tickedItems(e->buildMidiMenu()) == Items { { juce::String::fromUTF8("Pad 1 note: 36…"), false },
                                                      { "Pad 1 at top left", false } });

    e->applyMidiMenuResult(DubgefahrenEditor::kMidiOriginTop);
    CHECK(mapping.origin() == PadOrigin::TopLeft);
    CHECK(tickedItems(e->buildMidiMenu())[1].second); // Häkchen gesetzt
    CHECK(e->padPosition(0).y < e->padPosition(4).y); // Pad 1 in der obersten Reihe

    e->applyMidiMenuResult(DubgefahrenEditor::kMidiOriginTop);
    CHECK(mapping.origin() == PadOrigin::BottomLeft);
    CHECK(e->padPosition(0).y > e->padPosition(4).y);
}

TEST_CASE("a second editor follows a change made elsewhere", "[editor][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    PadMapping::instance().setOrigin(PadOrigin::BottomLeft);
    DubgefahrenProcessor a, b;
    std::unique_ptr<juce::AudioProcessorEditor> editorA(a.createEditor());
    std::unique_ptr<juce::AudioProcessorEditor> editorB(b.createEditor());
    auto* ea = static_cast<DubgefahrenEditor*>(editorA.get());
    auto* eb = static_cast<DubgefahrenEditor*>(editorB.get());
    CHECK(eb->padPosition(0).y > eb->padPosition(4).y);

    ea->applyMidiMenuResult(DubgefahrenEditor::kMidiOriginTop);
    CHECK(eb->padPosition(0).y > eb->padPosition(4).y); // noch nicht abgefragt
    eb->pollProcessorState();
    CHECK(eb->padPosition(0).y < eb->padPosition(4).y);
}

TEST_CASE("the first note dialog accepts only valid numbers", "[editor][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    PadMapping::instance().setFirstNote(36);
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    e->showFirstNoteDialog();
    auto* dialog = e->topDialog();
    REQUIRE(dialog != nullptr);
    auto* field = dialog->getTextEditor("note");
    auto* ok = dialog->getButton("OK");
    REQUIRE(field != nullptr);
    REQUIRE(ok != nullptr);
    CHECK(field->getText() == "36");
    CHECK(ok->isEnabled());

    // Der Texteditor meldet Änderungen über die Message-Queue; der Test löst den Rückruf direkt aus.
    for (const char* bad : { "", "113", "999", "abc", "1e2" })
    {
        INFO(bad);
        field->setText(bad, false);
        field->onTextChange();
        CHECK_FALSE(ok->isEnabled());
    }

    // Ein erzwungener Aufruf mit Unsinn ändert nichts.
    for (const char* bad : { "", "abc", "-3", "113", "1e2", "0x20", "36.5", "1234", "3 6" })
    {
        INFO(bad);
        CHECK_FALSE(e->applyFirstNoteText(bad));
        CHECK(PadMapping::instance().firstNote() == 36);
    }
    field->setText(" 32 ", false);
    field->onTextChange();
    CHECK(ok->isEnabled());
    CHECK(e->applyFirstNoteText(field->getText()));
    CHECK(PadMapping::instance().firstNote() == 32);
    CHECK(tickedItems(e->buildMidiMenu())[0].first == juce::String::fromUTF8("Pad 1 note: 32…"));
}
