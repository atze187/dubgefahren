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

private:
    Knob drive_ { u8("Drive") };
    Knob cutoff_ { u8("Cutoff") };
    Knob resonance_ { u8("Reso") };
    Knob filterType_ { u8("LP·BP·HP") };
    Choice delayTime_ { u8("Zeit") };
    Knob delayFeedback_ { u8("Feedback") };
    Knob delayTone_ { u8("Tone") };
    Knob delayWow_ { u8("Wow") };
    Knob delayMix_ { u8("Mix") };
    Knob reverbDecay_ { u8("Decay") };
    Knob reverbTone_ { u8("Tone") };
    Knob reverbMix_ { u8("Mix") };
    Knob master_ { u8("Master") };
};

} // namespace dg::ui
