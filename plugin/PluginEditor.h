#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/SlotParams.h"
#include "plugin/ui/CpuMeter.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/FxPanel.h"
#include "plugin/ui/PadGrid.h"
#include "plugin/ui/PerformancePanel.h"
#include "plugin/ui/SlotEditor.h"

namespace dg {

class DubgefahrenProcessor;

class DubgefahrenEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kBaseWidth = 1000;
    static constexpr int kBaseHeight = 640;

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

    // Für Tests öffentlich: bauen die Popup-Menüs von Kit-Button, Pad-Rechtsklick und leerem Pad.
    juce::PopupMenu buildKitMenu(const juce::Array<juce::File>& kitFiles) const;
    juce::PopupMenu buildPadMenu(int slot) const;
    static juce::PopupMenu buildSourceMenu();

private:
    void timerCallback() override;
    void layoutContent();
    void refreshAll();
    void setPanic(bool down);
    void showKitMenu();
    void showPadMenu(int slot);
    void showSourceMenu(int slot);
    void renameSlot(int slot);
    void importKit();
    void exportKit();
    void loadKit(const juce::File& file);
    void showMessage(const juce::String& title, const juce::String& text);
    void maybeShowConfigWarning();
    // Bit s gesetzt = Slot s hat eine klingende Quelle.
    std::uint32_t soundMask() const;

    DubgefahrenProcessor& proc_;
    ui::DgLookAndFeel lnf_;
    juce::Component content_;
    juce::Label title_;
    ui::CpuMeter cpuMeter_;
    juce::TextButton kitButton_ { juce::String::fromUTF8("Kit ▾") };
    juce::TextButton importButton_ { "Import" };
    juce::TextButton exportButton_ { "Export" };
    juce::TextButton panicButton_ { "PANIC" };
    juce::ToggleButton followFocus_ { "Editor follows focus" };
    ui::PadGrid pads_;
    ui::SlotEditor slotEditor_;
    ui::FxPanel fx_;
    ui::PerformancePanel perf_;
    std::unique_ptr<juce::FileChooser> chooser_;
    std::optional<std::pair<SlotParams, juce::String>> clipboard_;
    int selectedSlot_ = 0;
    int lastFocus_ = -1;
    int lastStateGeneration_ = -1;
    std::uint32_t lastSoundMask_ = 0;
    bool configWarningShown_ = false;
};

} // namespace dg
