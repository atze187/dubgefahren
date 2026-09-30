#include "plugin/ui/Surfaces.h"

namespace dg::ui {

void drawPanelBody(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (bounds.isEmpty())
        return;
    g.setGradientFill(juce::ColourGradient(colours::panelTop, 0.0f, bounds.getY(), colours::panelBottom, 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, kPanelCorner);
    g.setColour(colours::highlight);
    g.fillRect(juce::Rectangle<float>(bounds.getX() + kPanelCorner, bounds.getY(), bounds.getWidth() - 2.0f * kPanelCorner, 1.0f));
}

void PanelShadow::render(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (bounds.isEmpty())
        return;
    juce::Path p;
    p.addRoundedRectangle(bounds, kPanelCorner);
    shadow_.render(g, p);
}

void drawDivider(juce::Graphics& g, float x, float top, float bottom)
{
    g.setColour(colours::groove);
    g.fillRect(juce::Rectangle<float>(x, top, 1.0f, bottom - top));
    g.setColour(colours::highlight);
    g.fillRect(juce::Rectangle<float>(x + 1.0f, top, 1.0f, bottom - top));
}

void drawWindowBackground(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setGradientFill(juce::ColourGradient(colours::backgroundTop, 0.0f, static_cast<float>(bounds.getY()), colours::backgroundBottom, 0.0f,
                                           static_cast<float>(bounds.getBottom()), false));
    g.fillRect(bounds);
}

} // namespace dg::ui
