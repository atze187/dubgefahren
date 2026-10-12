#pragma once
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/Controls.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

class AdvancedPanel final : public juce::Component
{
public:
    explicit AdvancedPanel(DubgefahrenProcessor& proc);
    void paint(juce::Graphics& g) override;
    void resized() override;
    std::vector<juce::Rectangle<int>> controlBounds() const
    {
        std::vector<juce::Rectangle<int>> out;
        for (auto* c : getChildren())
            out.push_back(c->getBounds());
        return out;
    }

private:
    Knob drive_ { u8("Drive") };
    Knob delayFeedback_ { u8("Feedback") };
    Knob delayTone_ { u8("Tone") };
    Knob delayWow_ { u8("Wow") };
    Knob delayMix_ { u8("Mix") };
    Knob phaserRate_ { u8("Rate") };
    Knob phaserDepth_ { u8("Depth") };
    Knob phaserMix_ { u8("Mix") };
    Knob reverbDecay_ { u8("Decay") };
    Knob reverbTone_ { u8("Tone") };
    Knob reverbMix_ { u8("Mix") };
};

} // namespace dg::ui
