#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/Controls.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

class FxPanel final : public juce::Component
{
public:
    explicit FxPanel(DubgefahrenProcessor& proc);
    void paint(juce::Graphics& g) override;
    void resized() override;

    // true, wenn die Rate-Zelle die Teilung statt der Frequenz zeigt (Sync an).
    bool showsSyncedRate() const { return lfoDiv_.isVisible(); }

private:
    void updateRateCell();

    Knob drive_ { u8("Drive") };
    Choice delayTime_ { u8("Time") };
    Knob delayFeedback_ { u8("Feedback") };
    Knob delayWow_ { u8("Wow") };
    Knob delayMix_ { u8("Mix") };
    Choice fltType_ { u8("Type") };
    Knob fltCutoff_ { u8("Cutoff") };
    Knob fltRes_ { u8("Reso") };
    Choice lfoShape_ { u8("Shape") };
    Knob lfoRate_ { u8("Rate") };
    Choice lfoDiv_ { u8("Rate") };
    Toggle lfoSync_ { u8("Sync") };
    Knob lfoCutDepth_ { u8("Cut Amt") };
    Knob lfoResDepth_ { u8("Res Amt") };
    Knob reverbDecay_ { u8("Decay") };
    Knob reverbTone_ { u8("Tone") };
    Knob reverbMix_ { u8("Mix") };
    Knob master_ { u8("Master") };
};

} // namespace dg::ui
