#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/Controls.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

class PerformancePanel final : public juce::Component
{
public:
    explicit PerformancePanel(DubgefahrenProcessor& proc);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    Knob pitch_ { u8("Pitch") };
    Knob rate_ { u8("Rate") };
    Knob depth_ { u8("Depth") };
    Knob sweep_ { u8("Sweep") };
    Choice target_ { u8("Target") };
    Choice latchStop_ { u8("Latch on Stop") };
};

} // namespace dg::ui
