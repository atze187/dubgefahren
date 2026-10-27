#include <catch2/catch_test_macros.hpp>
#include <memory>
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
