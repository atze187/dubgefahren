#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "melatonin_blur/melatonin_blur.h"
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

inline constexpr float kPanelCorner = 8.0f;

// Körper eines Panels: Verlauf und Lichtkante oben. Der Schatten kommt von PanelShadow.
void drawPanelBody(juce::Graphics& g, juce::Rectangle<float> bounds);

// Weicher Schlagschatten eines Panels. Ragt über bounds hinaus und wird deshalb vom
// Elternteil unter dem Panel gezeichnet. Als Member halten: das Objekt cacht den Schatten.
class PanelShadow
{
public:
    void render(juce::Graphics& g, juce::Rectangle<float> bounds);

private:
    melatonin::DropShadow shadow_ { { colours::shadow, 10, { 0, 3 } } };
};

// Senkrechter Gruppentrenner als Doppellinie (dunkel, daneben hell).
void drawDivider(juce::Graphics& g, float x, float top, float bottom);

void drawWindowBackground(juce::Graphics& g, juce::Rectangle<int> bounds);

} // namespace dg::ui
