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
    // Linksklick auf ein leeres Pad (statt Vorschau): Auswahl der Klangquelle öffnen.
    std::function<void(int)> onEmptyClick;

    juce::Component& pad(int slot);
    bool isEmpty(int slot) const;
    bool isSample(int slot) const;
    bool isMissing(int slot) const;

    // So weit ragt der Glow höchstens über ein Pad hinaus (px im Basis-Layout).
    static constexpr int kGlowReach = 24;

    // Glow aller Pads in den Koordinaten dieses Rasters. Ragt über die Pads und das Raster
    // hinaus und wird deshalb vom Editor unter den Komponenten gezeichnet.
    void paintGlows(juce::Graphics& g);
    // Ein Glow hat sich geändert: Bereich (Raster-Koordinaten) neu zeichnen.
    std::function<void(juce::Rectangle<int>)> onGlowChanged;

    float padBrightness(int slot) const;
    // Grundfarbe eines Pads: ruhend gedämpft, spielend hell. Vorerst für alle gleich (siehe #14).
    void setPadBaseColour(int slot, juce::Colour colour);
    juce::Colour padBaseColour(int slot) const;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class Pad;
    void glowChanged(int slot);
    double lastTickMs_ = 0.0;
    DubgefahrenProcessor& proc_;
    std::array<std::unique_ptr<Pad>, kNumSlots> pads_;
};

} // namespace dg::ui
