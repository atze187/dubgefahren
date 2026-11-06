#pragma once
#include <array>
#include <functional>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/SlotParams.h"
#include "plugin/PadMapping.h"

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
    // Pad 1 unten links (Standard) oder oben links, wie das BU16 seine Tasten sendet. Legt das Raster
    // neu an; liefert true, wenn sich der Ursprung geändert hat.
    bool setOrigin(PadOrigin origin);

    std::function<void(int)> onSelect;
    std::function<void(int)> onContextMenu;
    // Linksklick auf ein leeres Pad (statt Vorschau): Auswahl der Klangquelle öffnen.
    std::function<void(int)> onEmptyClick;

    juce::Component& pad(int slot);
    bool isEmpty(int slot) const;
    bool isSample(int slot) const;
    bool isMissing(int slot) const;

    // Abstand der Kachel zum Rand ihrer Zelle (px im Basis-Layout): die Lücke zwischen den Kacheln.
    static constexpr int kTileInset = 3;

    // Die Kachel eines Pads hat sich geändert (Helligkeit, Fokus, Farbe): Bereich (Raster-Koordinaten) neu zeichnen.
    std::function<void(juce::Rectangle<int>)> onGlowChanged;

    float padBrightness(int slot) const;
    // Grundfarbe eines Pads: ruhend gedämpft, spielend hell. refreshNames() setzt sie aus der Slot-Farbe.
    void setPadBaseColour(int slot, juce::Colour colour);
    juce::Colour padBaseColour(int slot) const;
    juce::Colour padLitTextColour(int slot) const;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class Pad;
    void glowChanged(int slot);
    PadOrigin origin_ = PadOrigin::BottomLeft;
    double lastTickMs_ = 0.0;
    DubgefahrenProcessor& proc_;
    std::array<std::unique_ptr<Pad>, kNumSlots> pads_;
};

} // namespace dg::ui
