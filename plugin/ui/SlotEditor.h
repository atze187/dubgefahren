#pragma once
#include <functional>
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
    void refreshName();

    std::function<void()> onRename;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    DubgefahrenProcessor& proc_;
    int slot_ = -1;
    juce::Label header_;
    juce::TextButton renameButton_ { u8("Umbenennen") };

    Choice wave_ { u8("Welle") };
    Knob pitch_ { u8("Tonhöhe") };
    Knob pw_ { u8("Pulsbreite") };
    Choice lfoShape_ { u8("Form") };
    Knob lfoRate_ { u8("Rate") };
    Toggle lfoSync_ { u8("Sync") };
    Choice lfoSyncDiv_ { u8("Sync-Rate") };
    Knob lfoDepth_ { u8("Tiefe") };
    Knob sweepAmt_ { u8("Betrag") };
    Knob sweepTime_ { u8("Zeit") };
    Knob attack_ { u8("Attack") };
    Knob release_ { u8("Release") };
    Choice trigMode_ { u8("Modus") };
    Knob oneShot_ { u8("Länge") };
    Choice choke_ { u8("Choke") };
    Knob vol_ { u8("Lautstärke") };
    Knob pan_ { u8("Pan") };
    Knob send_ { u8("FX-Send") };
};

} // namespace dg::ui
