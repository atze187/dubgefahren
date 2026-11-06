#include "plugin/ui/Surfaces.h"

namespace dg::ui {

void drawPanelBody(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (bounds.isEmpty())
        return;
    g.setColour(colours::edge);
    g.fillRoundedRectangle(bounds, kPanelCorner);
    g.setColour(colours::strip);
    g.fillRoundedRectangle(bounds.reduced(1.0f), kPanelCorner - 1.0f);
}

void drawDivider(juce::Graphics& g, float x, float top, float bottom)
{
    g.setColour(colours::edge);
    g.fillRect(juce::Rectangle<float>(x, top, 1.0f, bottom - top));
}

void drawWindowBackground(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(colours::window);
    g.fillRect(bounds);
}

} // namespace dg::ui
