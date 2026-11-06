#include <catch2/catch_test_macros.hpp>
#include <memory>
#include "plugin/PadMapping.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UserSettings.h"
#include "plugin/ui/SettingsDialog.h"
#include "PadMappingTestHelpers.h"
#include "RenderTestHelpers.h"

using namespace dg;

namespace {
struct EditorFixture
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> editor { proc.createEditor() };
    DubgefahrenEditor& e() { return *static_cast<DubgefahrenEditor*>(editor.get()); }
};
} // namespace

TEST_CASE("the settings button replaces the MIDI button and opens the dialog modally", "[settings][editor]")
{
    EditorFixture f;
    CHECK(f.e().settingsButtonVisible());
    CHECK(f.e().settingsDialog() == nullptr);

    f.e().showSettings();
    auto* dialog = f.e().settingsDialog();
    REQUIRE(dialog != nullptr);
    CHECK(dialog->currentTab() == ui::SettingsDialog::Tab::Midi);
    REQUIRE(dialog->getTopLevelComponent() != nullptr);
    CHECK(dialog->getTopLevelComponent()->isCurrentlyModal());

    f.e().showSettings();
    CHECK(f.e().settingsDialog() == dialog); // kein zweiter Dialog

    f.e().closeSettings();
    CHECK(f.e().settingsDialog() == nullptr);

    f.e().setLiveView(true);
    CHECK_FALSE(f.e().settingsButtonVisible());
    f.e().setLiveView(false);
}

TEST_CASE("the kits tab shows the folder and the warning", "[settings]")
{
    EditorFixture f;
    f.e().showSettings();
    auto* d = f.e().settingsDialog();
    REQUIRE(d != nullptr);
    d->showTab(ui::SettingsDialog::Tab::Kits);
    CHECK(d->currentTab() == ui::SettingsDialog::Tab::Kits);
    CHECK(d->kitFolderText() == f.proc.kitFolder().folder.getFullPathName());
    CHECK(d->kitWarningText().isEmpty());
    f.e().closeSettings();

    // Eine gespeicherte, inzwischen verschwundene Einstellung: Warnung, Rückfall auf den Standardordner.
    UserSettings::instance().setKitFolder(juce::File::createTempFile("dggone").getFullPathName());
    DubgefahrenProcessor other;
    std::unique_ptr<juce::AudioProcessorEditor> editor(other.createEditor());
    auto* e2 = static_cast<DubgefahrenEditor*>(editor.get());
    e2->showSettings();
    e2->settingsDialog()->showTab(ui::SettingsDialog::Tab::Kits);
    CHECK(e2->settingsDialog()->kitWarningText().contains("settings"));
    e2->closeSettings();
    other.useDefaultKitFolder();
}

TEST_CASE("changing the kit folder applies immediately", "[settings]")
{
    EditorFixture f;
    const auto dir = juce::File::createTempFile("dgkitdlg");
    dir.createDirectory();
    f.e().showSettings();
    auto* d = f.e().settingsDialog();
    REQUIRE(d != nullptr);
    d->showTab(ui::SettingsDialog::Tab::Kits);

    d->applyKitFolder(dir);
    CHECK(f.proc.kitFolder().folder == dir);
    CHECK(d->kitFolderText() == dir.getFullPathName());

    d->useDefaultKitFolderClicked();
    CHECK(f.proc.kitFolder().folder == defaultKitFolder());
    CHECK(d->kitFolderText() == defaultKitFolder().getFullPathName());
    f.e().closeSettings();
    dir.deleteRecursively();
}

TEST_CASE("the origin switch moves the pad grid", "[settings][padmapping]")
{
    EditorFixture f;
    dgtest::ScopedPadMapping restore;
    PadMapping::instance().setOrigin(PadOrigin::BottomLeft);
    f.e().pollProcessorState();
    f.e().showSettings();
    auto* d = f.e().settingsDialog();
    REQUIRE(d != nullptr);
    CHECK_FALSE(d->originTopLeft());

    d->setOriginTopLeft(true);
    CHECK(PadMapping::instance().origin() == PadOrigin::TopLeft);
    CHECK(d->originTopLeft());
    f.e().pollProcessorState();
    CHECK(f.e().padPosition(0).y < f.e().padPosition(4).y);

    d->setOriginTopLeft(false);
    f.e().pollProcessorState();
    CHECK(f.e().padPosition(0).y > f.e().padPosition(4).y);
    f.e().closeSettings();
}

TEST_CASE("closing the dialog and destroying the editor with an open dialog is safe", "[settings][editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* e = static_cast<DubgefahrenEditor*>(editor.get());
        e->showSettings();
        e->closeSettings();
        e->showSettings();
        REQUIRE(e->settingsDialog() != nullptr);
    } // Editor mit offenem Dialog zerstören
    SUCCEED();
}

TEST_CASE("the settings dialog paints", "[settings][look]")
{
    EditorFixture f;
    f.e().showSettings();
    auto* d = f.e().settingsDialog();
    REQUIRE(d != nullptr);
    for (const auto tab : { ui::SettingsDialog::Tab::Midi, ui::SettingsDialog::Tab::Kits })
    {
        d->showTab(tab);
        const auto img = dgtest::snapshot(*d);
        REQUIRE(img.isValid());
        dgtest::savePng(img, tab == ui::SettingsDialog::Tab::Midi ? "settings-midi" : "settings-kits");
    }
    f.e().closeSettings();
}

namespace {
ui::SettingsDialog::Host hostWith(std::function<void(const juce::String&, std::function<void(const juce::String&)>)> prompt,
                                  std::function<void(std::function<void()>)> confirm)
{
    ui::SettingsDialog::Host h;
    h.promptText = [prompt](const juce::String&, const juce::String&, std::function<void(const juce::String&)> ok) { prompt("", ok); };
    h.confirm = [confirm](const juce::String&, const juce::String&, std::function<void()> yes) { confirm(yes); };
    h.close = [] {};
    return h;
}
} // namespace

TEST_CASE("the profile list shows the profiles and the active one", "[settings][profiles]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    DubgefahrenProcessor proc;
    auto& mapping = PadMapping::instance();
    mapping.createProfile();
    mapping.createProfile();

    ui::SettingsDialog d(proc, {});
    CHECK(d.profileNames() == mapping.profileNames());
    CHECK(d.profileNames().size() == 3);
    CHECK(d.selectedProfile() == mapping.activeProfile());
}

TEST_CASE("a profile file copied into the folder shows up when the list is read again", "[settings][profiles]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    DubgefahrenProcessor proc;
    ui::SettingsDialog d(proc, {});
    CHECK_FALSE(d.profileNames().contains("Friend"));

    const auto file = PadMapping::instance().profilesFolder().getChildFile("Friend.json");
    file.replaceWithText(padProfileToJson(makeConsecutiveProfile("Friend", 60, PadOrigin::TopLeft)));
    CHECK(d.profileNames().contains("Friend"));

    d.selectProfile("Friend");
    CHECK(d.selectedProfile() == "Friend");
    CHECK(PadMapping::instance().noteMap().slotForNote(60) == 0);
    CHECK(PadMapping::instance().origin() == PadOrigin::TopLeft);
    file.deleteFile(); // der Testordner gehört dem Prozess, aber die Datei soll andere Tests nicht stören
}

TEST_CASE("new, rename and delete update the list and the selection", "[settings][profiles]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    DubgefahrenProcessor proc;
    juce::String answer = "Launchpad";
    bool confirmed = true;
    ui::SettingsDialog d(proc, hostWith([&](const juce::String&, auto ok) { ok(answer); },
                                        [&](auto yes) { if (confirmed) yes(); }));

    d.newProfileClicked();
    const auto created = d.selectedProfile();
    CHECK(created == PadMapping::instance().activeProfile());
    CHECK(d.profileNames().contains(created));
    CHECK(d.renameEnabled());
    CHECK(d.deleteEnabled());

    d.renameProfileClicked();
    CHECK(d.selectedProfile() == "Launchpad");
    CHECK(d.profileNames().contains("Launchpad"));
    CHECK_FALSE(d.profileNames().contains(created));
    CHECK(d.hintText().isEmpty());

    answer = "default";
    d.renameProfileClicked();
    CHECK(d.selectedProfile() == "Launchpad");
    CHECK(d.hintText() == "Name not allowed or already taken");

    confirmed = false;
    d.deleteProfileClicked();
    CHECK(d.selectedProfile() == "Launchpad");

    confirmed = true;
    d.deleteProfileClicked();
    CHECK(d.selectedProfile() == "Default");
    CHECK_FALSE(d.profileNames().contains("Launchpad"));
}

TEST_CASE("Default cannot be renamed or deleted", "[settings][profiles]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    DubgefahrenProcessor proc;
    ui::SettingsDialog d(proc, {});
    d.selectProfile("Default");
    CHECK_FALSE(d.renameEnabled());
    CHECK_FALSE(d.deleteEnabled());
}

namespace {
void prepareProc(DubgefahrenProcessor& p)
{
    p.setPlayConfigDetails(0, 2, 48000.0, 512);
    p.prepareToPlay(48000.0, 512);
}

void playNote(DubgefahrenProcessor& p, int note, bool on, int blocks = 1)
{
    juce::AudioBuffer<float> buf(2, 512);
    for (int i = 0; i < blocks; ++i)
    {
        buf.clear();
        juce::MidiBuffer midi;
        if (i == 0)
            midi.addEvent(on ? juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)) : juce::MidiMessage::noteOff(1, note), 0);
        p.processBlock(buf, midi);
    }
}
} // namespace

TEST_CASE("a learning pad shows press and takes the next note", "[settings][learn]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    DubgefahrenProcessor proc;
    prepareProc(proc);
    ui::SettingsDialog d(proc, {});

    d.startLearning(3);
    CHECK(d.learningSlot() == 3);
    CHECK(proc.midiLearnArmed());
    CHECK(d.padCellText(3).contains("press"));

    playNote(proc, 55, true);
    d.pollLearning();
    CHECK(PadMapping::instance().noteMap().noteForSlot(3) == 55);
    CHECK(d.learningSlot() == -1); // kein Weiterspringen zum nächsten Pad
    CHECK(d.padCellText(3).contains("55"));
    CHECK_FALSE(proc.midiLearnArmed());
}

TEST_CASE("a note that another pad owns moves to the learning pad", "[settings][learn]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    DubgefahrenProcessor proc;
    prepareProc(proc);
    ui::SettingsDialog d(proc, {});
    const int note = PadMapping::instance().noteMap().noteForSlot(0);
    REQUIRE(note >= 0);

    d.startLearning(5);
    playNote(proc, note, true);
    d.pollLearning();
    CHECK(PadMapping::instance().noteMap().noteForSlot(5) == note);
    CHECK(d.padCellText(0).endsWith("-"));
}

TEST_CASE("learning stops on a second click, another pad, Esc, a tab switch, closing and destruction", "[settings][learn]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor proc;
    {
        ui::SettingsDialog d(proc, {});
        d.startLearning(2);
        d.startLearning(2); // zweiter Klick auf das lernende Pad
        CHECK(d.learningSlot() == -1);
        CHECK_FALSE(proc.midiLearnArmed());

        d.startLearning(2);
        d.startLearning(7); // anderes Pad: das erste wird abgebrochen
        CHECK(d.learningSlot() == 7);
        CHECK(proc.midiLearnArmed());

        CHECK(d.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)));
        CHECK(d.learningSlot() == -1);
        CHECK_FALSE(proc.midiLearnArmed());
        CHECK_FALSE(d.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey))); // nichts zu beenden: der Dialog darf schließen

        d.startLearning(1);
        d.showTab(ui::SettingsDialog::Tab::Kits);
        CHECK(d.learningSlot() == -1);
        CHECK_FALSE(proc.midiLearnArmed());

        d.showTab(ui::SettingsDialog::Tab::Midi);
        d.startLearning(1);
    } // Zerstörung mitten im Lernen
    CHECK_FALSE(proc.midiLearnArmed());

    EditorFixture f;
    f.e().showSettings();
    f.e().settingsDialog()->startLearning(4);
    f.e().closeSettings();
    CHECK_FALSE(f.proc.midiLearnArmed());
}

TEST_CASE("the grid follows the origin", "[settings][learn][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    DubgefahrenProcessor proc;
    ui::SettingsDialog d(proc, {});

    d.setOriginTopLeft(false);
    CHECK(d.padCellPosition(0).y > d.padCellPosition(4).y);
    CHECK(d.padCellPosition(0).x == d.padCellPosition(4).x);
    CHECK(d.padCellPosition(1).x > d.padCellPosition(0).x);
    d.setOriginTopLeft(true);
    CHECK(d.padCellPosition(0).y < d.padCellPosition(4).y);
}

TEST_CASE("the dialog shows changes made elsewhere", "[settings][learn]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    DubgefahrenProcessor proc;
    ui::SettingsDialog d(proc, {});

    PadMapping::instance().setPadNote(7, 70); // ein anderer Editor
    d.refreshFromMapping();
    CHECK(d.padCellText(7).contains("70"));

    PadMapping::instance().createProfile();
    d.refreshFromMapping();
    CHECK(d.selectedProfile() == PadMapping::instance().activeProfile());
}

TEST_CASE("the note off of a learned note does not leave a voice stuck", "[settings][learn]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    DubgefahrenProcessor proc;
    prepareProc(proc);
    ui::SettingsDialog d(proc, {});

    d.startLearning(3);
    playNote(proc, 55, true);
    d.pollLearning();
    playNote(proc, 55, false);
    CHECK(proc.activeMask() == 0u);

    playNote(proc, 55, true); // jetzt löst die Note Pad 4 aus
    CHECK(proc.activeMask() == (1u << 3));
}
