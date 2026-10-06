#pragma once
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/Controls.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

class LivePanel final : public juce::Component
{
public:
    explicit LivePanel(DubgefahrenProcessor& proc);
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
    Knob space_ { u8("Space") };
    Knob grit_ { u8("Grit") };
    Knob throw_ { u8("Throw") };
    Knob cutoff_ { u8("Cutoff") };
    Knob resonance_ { u8("Reso") };
    Knob filterType_ { u8("LP·BP·HP") };
    Choice delayTime_ { u8("Time") };
    Knob master_ { u8("Master") };
};

} // namespace dg::ui
