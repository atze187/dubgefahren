#pragma once
#include <memory>
#include <optional>
#include <utility>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/SlotParams.h"
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

private:
    void timerCallback() override;
    void layoutContent();
    void refreshAll();
    void setPanic(bool down);
    void showKitMenu();
    void showPadMenu(int slot);
    void renameSlot(int slot);
    void importKit();
    void exportKit();
    void loadKit(const juce::File& file);
    void showMessage(const juce::String& title, const juce::String& text);
    void maybeShowConfigWarning();

    DubgefahrenProcessor& proc_;
    ui::DgLookAndFeel lnf_;
    juce::Component content_;
    juce::Label title_;
    juce::TextButton kitButton_ { juce::String::fromUTF8("Kit ▾") };
    juce::TextButton importButton_ { "Import" };
    juce::TextButton exportButton_ { "Export" };
    juce::TextButton panicButton_ { "PANIC" };
    juce::ToggleButton followFocus_ { "Editor folgt Fokus" };
    ui::PadGrid pads_;
    ui::SlotEditor slotEditor_;
    ui::FxPanel fx_;
    ui::PerformancePanel perf_;
    std::unique_ptr<juce::FileChooser> chooser_;
    std::optional<std::pair<SlotParams, juce::String>> clipboard_;
    int selectedSlot_ = 0;
    int lastFocus_ = -1;
    int lastStateGeneration_ = -1;
    bool configWarningShown_ = false;
};

} // namespace dg
