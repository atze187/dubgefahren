#pragma once
#include <array>
#include <functional>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/SlotParams.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

class PadGrid final : public juce::Component
{
public:
    explicit PadGrid(DubgefahrenProcessor& proc);
    ~PadGrid() override;

    void setPadStates(std::uint32_t active, std::uint32_t latched, int focus);
    void setSelected(int slot);
    void refreshNames();

    std::function<void(int)> onSelect;
    std::function<void(int)> onContextMenu;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class Pad;
    DubgefahrenProcessor& proc_;
    std::array<std::unique_ptr<Pad>, kNumSlots> pads_;
};

} // namespace dg::ui
