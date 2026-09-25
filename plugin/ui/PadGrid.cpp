#include "plugin/ui/PadGrid.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/Controls.h"
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

class PadGrid::Pad final : public juce::Component
{
public:
    Pad(PadGrid& owner, int slot) : owner_(owner), slot_(slot) {}

    ~Pad() override
    {
        if (held_)
            owner_.proc_.previewRelease(slot_);
    }

    void setState(bool active, bool latched, bool focus, bool selected)
    {
        if (active == active_ && latched == latched_ && focus == focus_ && selected == selected_)
            return;
        active_ = active;
        latched_ = latched;
        focus_ = focus;
        selected_ = selected;
        repaint();
    }

    void setName(const juce::String& n)
    {
        name_ = n;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced(3.0f);
        g.setColour(active_ ? colours::playing.withAlpha(0.35f) : colours::panel);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(focus_ ? colours::accent : colours::outline);
        g.drawRoundedRectangle(r, 6.0f, selected_ ? 3.0f : 1.5f);

        g.setColour(colours::textDim);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(juce::String(slot_ + 1), r.reduced(6.0f), juce::Justification::topLeft);
        g.setColour(colours::text);
        g.setFont(juce::FontOptions(13.0f));
        g.drawFittedText(name_, r.reduced(6.0f).toNearestInt(), juce::Justification::centred, 2);

        if (latched_)
        {
            g.setColour(colours::latched);
            g.fillEllipse(r.getRight() - 14.0f, r.getY() + 6.0f, 8.0f, 8.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (held_)
            {
                owner_.proc_.previewRelease(slot_);
                held_ = false;
            }
            if (owner_.onContextMenu)
                owner_.onContextMenu(slot_);
            return;
        }
        held_ = true;
        owner_.proc_.previewPress(slot_);
        if (owner_.onSelect)
            owner_.onSelect(slot_);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (held_)
            owner_.proc_.previewRelease(slot_);
        held_ = false;
    }

private:
    PadGrid& owner_;
    const int slot_;
    juce::String name_;
    bool active_ = false, latched_ = false, focus_ = false, selected_ = false, held_ = false;
};

PadGrid::PadGrid(DubgefahrenProcessor& proc) : proc_(proc)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        pads_[static_cast<std::size_t>(s)] = std::make_unique<Pad>(*this, s);
        addAndMakeVisible(*pads_[static_cast<std::size_t>(s)]);
    }
    refreshNames();
}

PadGrid::~PadGrid() = default;

void PadGrid::setPadStates(std::uint32_t active, std::uint32_t latched, int focus)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& pad = *pads_[static_cast<std::size_t>(s)];
        const bool selected = pad.getProperties()["selected"];
        pad.setState((active >> s) & 1u, (latched >> s) & 1u, s == focus, selected);
    }
}

void PadGrid::setSelected(int slot)
{
    for (int s = 0; s < kNumSlots; ++s)
        pads_[static_cast<std::size_t>(s)]->getProperties().set("selected", s == slot);
    setPadStates(proc_.activeMask(), proc_.latchedMask(), proc_.focusSlot());
    for (auto& p : pads_)
        p->repaint();
}

void PadGrid::refreshNames()
{
    for (int s = 0; s < kNumSlots; ++s)
        pads_[static_cast<std::size_t>(s)]->setName(proc_.slotName(s));
}

void PadGrid::paint(juce::Graphics& g)
{
    auto legend = getLocalBounds().removeFromBottom(20).toFloat();
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(colours::playing);
    g.drawText(u8("● spielt"), legend.removeFromLeft(80.0f), juce::Justification::centredLeft);
    g.setColour(colours::accent);
    g.drawText(u8("◆ Fokus"), legend.removeFromLeft(80.0f), juce::Justification::centredLeft);
    g.setColour(colours::latched);
    g.drawText(u8("● gelatcht"), legend.removeFromLeft(90.0f), juce::Justification::centredLeft);
}

void PadGrid::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(24);
    const int w = area.getWidth() / 4;
    const int h = area.getHeight() / 4;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int col = s % 4;
        const int rowFromBottom = s / 4; // Pad 1 unten links wie beim BU16
        pads_[static_cast<std::size_t>(s)]->setBounds(area.getX() + col * w, area.getY() + (3 - rowFromBottom) * h, w, h);
    }
}

} // namespace dg::ui
