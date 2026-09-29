#pragma once
#include <functional>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/Controls.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

class SlotEditor final : public juce::Component
{
public:
    explicit SlotEditor(DubgefahrenProcessor& proc);
    void setSlot(int slot);
    int slot() const { return slot_; }
    void refresh();
    bool showsEmptyHint() const { return emptyHint_.isVisible(); }

    std::function<void()> onRename;
    std::function<void()> onChooseSample;
    bool showsSampleControls() const { return sampleButton_.isVisible(); }
    juce::String sampleButtonText() const { return sampleButton_.getButtonText(); }
    bool isLatchSelectable() const { return trigMode_.box.isItemEnabled(kLatchItemId); }
    juce::Component& sampleButton() { return sampleButton_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum class Mode { Empty, Synth, Sample };
    static constexpr int kLatchItemId = 2; // ComboBox-IDs 1..3 = Gate, Latch, One-Shot
    Mode mode_ = Mode::Synth;
    DubgefahrenProcessor& proc_;
    int slot_ = -1;
    juce::Label header_;
    juce::TextButton renameButton_ { u8("Rename") };

    Choice wave_ { u8("Wave") };
    Knob pitch_ { u8("Pitch") };
    Knob pw_ { u8("Pulse Width") };
    Choice lfoShape_ { u8("Shape") };
    Knob lfoRate_ { u8("Rate") };
    Toggle lfoSync_ { u8("Sync") };
    Choice lfoSyncDiv_ { u8("Sync Rate") };
    Knob lfoDepth_ { u8("Depth") };
    Knob sweepAmt_ { u8("Amount") };
    Knob sweepTime_ { u8("Time") };
    Knob attack_ { u8("Attack") };
    Knob release_ { u8("Release") };
    Choice trigMode_ { u8("Mode") };
    Knob oneShot_ { u8("Length") };
    Choice choke_ { u8("Choke") };
    Knob vol_ { u8("Volume") };
    Knob pan_ { u8("Pan") };
    Knob send_ { u8("FX-Send") };

    Knob tune_ { u8("Tune") };
    juce::TextButton sampleButton_;
    std::vector<juce::Component*> sampleControls_;

    juce::Label emptyHint_;
    std::vector<juce::Component*> controls_;
};

} // namespace dg::ui
