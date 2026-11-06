#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/SlotParams.h"
#include "plugin/PadMapping.h"
#include "plugin/ui/CpuMeter.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/AdvancedPanel.h"
#include "plugin/ui/LivePanel.h"
#include "plugin/ui/PadGrid.h"
#include "plugin/ui/PerformancePanel.h"
#include "plugin/ui/SettingsDialog.h"
#include "plugin/ui/SlotEditor.h"
#include "plugin/ui/Surfaces.h"

namespace dg {

class DubgefahrenProcessor;

class DubgefahrenEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    const ui::LivePanel& livePanel() const { return live_; }
    const ui::AdvancedPanel& advancedPanel() const { return advanced_; }
    const ui::PerformancePanel& performancePanel() const { return perf_; }
    enum class Layout { EditAdvanced, Edit, Live };
    // Basisgröße (vor der Skalierung) pro Layout.
    static juce::Point<int> baseSize(Layout l)
    {
        switch (l)
        {
            case Layout::Edit: return { 1200, 656 };
            case Layout::Live: return { 880, 460 };
            case Layout::EditAdvanced: break;
        }
        return { 1200, 776 };
    }
    Layout layout() const { return layout_; }
    // Für Tests und die Header-Taster: wechselt die Ansicht bzw. klappt Advanced auf/zu.
    void setLiveView(bool live);
    void setAdvancedOpen(bool open);
    bool slotEditorVisible() const { return slotEditor_.isVisible(); }
    bool advancedPanelVisible() const { return advanced_.isVisible(); }

    explicit DubgefahrenEditor(DubgefahrenProcessor& proc);
    ~DubgefahrenEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void selectSlot(int slot);
    int selectedSlot() const { return selectedSlot_; }

    // Für Tests: pollt den Processor-Zustand, den timerCallback() sonst auf dem Message-Timer tut.
    void pollProcessorState();
    bool followFocusToggleState() const { return followFocus_.getToggleState(); }
    juce::String cpuMeterText() const { return cpuMeter_.getText(); }
    bool slotEditorShowsEmptyHint() const { return slotEditor_.showsEmptyHint(); }
    bool padShowsEmpty(int slot) const { return pads_.isEmpty(slot); }
    bool padShowsSample(int slot) const { return pads_.isSample(slot); }
    bool padShowsMissing(int slot) const { return pads_.isMissing(slot); }
    bool slotEditorShowsSampleControls() const { return slotEditor_.showsSampleControls(); }
    juce::String slotEditorSampleText() const { return slotEditor_.sampleButtonText(); }
    bool slotEditorLatchSelectable() const { return slotEditor_.isLatchSelectable(); }
    juce::String lastMessage() const { return lastMessage_; }
    // Für Tests: der Slot-Editor mit Wellenform und Loop-Controls.
    ui::SlotEditor& slotEditor() { return slotEditor_; }
    const ui::SlotEditor& slotEditor() const { return slotEditor_; }
    // Für Tests: der zuletzt geöffnete, noch offene Dialog (Meldung oder Rename).
    juce::AlertWindow* topDialog() const;
    // Für Tests öffentlich: öffnet das Kontextmenü eines Pads.
    void showPadMenu(int slot);

    // Settings: modaler Dialog mit den Reitern MIDI und Kits. Öffentlich für Tests.
    void showSettings();
    void closeSettings();
    ui::SettingsDialog* settingsDialog() const { return settingsDialog_; }
    bool settingsButtonVisible() const { return settingsButton_.isVisible(); }
    // Position eines Pads im Raster (Koordinaten des Rasters).
    juce::Point<int> padPosition(int slot) { return pads_.pad(slot).getPosition(); }

    // Für Tests öffentlich: bauen die Popup-Menüs von Kit-Button, Pad-Rechtsklick und leerem Pad.
    juce::PopupMenu buildKitMenu(const juce::Array<juce::File>& kitFiles) const;
    static constexpr int kPadColourMenuBase = 100; // + Palettenindex
    juce::PopupMenu buildPadMenu(int slot) const;
    void applyPadMenuResult(int slot, int result);
    ui::PadGrid& padGrid() { return pads_; }
    juce::PopupMenu buildSourceMenu(const juce::StringArray& sampleFiles) const;
    static juce::PopupMenu buildSampleMenu(const juce::StringArray& sampleFiles);
    // Schreibt das aktuelle Kit nach file, kopiert bei neuem Kit die Samples mit.
    bool exportKitTo(const juce::File& file);

private:
    void timerCallback() override;
    void layoutContent();
    void applyLayout();
    void refreshAll();
    void setPanic(bool down);
    void showKitMenu();
    void showSourceMenu(int slot);
    void showSampleMenu(int slot);
    void chooseSource(int slot, int result, const juce::StringArray& files);
    void addSampleFile(int slot);
    void renameSlot(int slot);
    void importKit();
    void exportKit();
    void loadKit(const juce::File& file);
    void showMessage(const juce::String& title, const juce::String& text);
    // Legt einen Dialog im Look des Editors an. Der Editor merkt ihn sich und schließt ihn in seinem
    // Destruktor: Der Dialog benutzt lnf_ und darf den Editor deshalb nicht überleben.
    juce::AlertWindow* createDialog(const juce::String& title, const juce::String& text, juce::MessageBoxIconType icon);
    // Popup-Menüs sind eigene Fenster und erben das Look-and-Feel des Editors nicht von selbst.
    juce::PopupMenu styled(juce::PopupMenu menu);
    void maybeShowConfigWarning();
    // Bit s gesetzt = Slot s hat eine klingende Quelle.
    std::uint32_t soundMask() const;

    DubgefahrenProcessor& proc_;
    ui::DgLookAndFeel lnf_;
    juce::Component content_;
    juce::Label title_;
    ui::CpuMeter cpuMeter_;
    juce::TextButton kitButton_ { juce::String::fromUTF8("KIT ▼") };
    juce::TextButton settingsButton_ { "SETTINGS" };
    juce::TextButton importButton_ { "IMPORT" };
    juce::TextButton exportButton_ { "EXPORT" };
    juce::TextButton panicButton_ { "PANIC" };
    juce::TextButton viewButton_ { "LIVE" };
    juce::TextButton advancedButton_ { juce::String::fromUTF8("ADVANCED ▼") };
    juce::ToggleButton followFocus_ { "EDITOR FOLLOWS FOCUS" };
    ui::PadGrid pads_;
    ui::SlotEditor slotEditor_;
    ui::LivePanel live_;
    ui::AdvancedPanel advanced_;
    ui::PerformancePanel perf_;
    std::unique_ptr<juce::FileChooser> chooser_;
    // Das modale Settings-Fenster benutzt lnf_ und wird im Destruktor vor ihm geschlossen.
    std::unique_ptr<juce::DialogWindow> settingsWindow_;
    ui::SettingsDialog* settingsDialog_ = nullptr;
    struct ClipboardSlot
    {
        SlotParams params;
        juce::String name;
        juce::String sample;
        int colour = 0;
    };
    std::optional<ClipboardSlot> clipboard_;
    juce::String lastMessage_;
    juce::Array<juce::Component::SafePointer<juce::AlertWindow>> dialogs_;
    int selectedSlot_ = 0;
    int lastFocus_ = -1;
    int lastStateGeneration_ = -1;
    std::uint32_t lastSoundMask_ = 0;
    bool configWarningShown_ = false;
    Layout layout_ = Layout::EditAdvanced;
};

} // namespace dg
