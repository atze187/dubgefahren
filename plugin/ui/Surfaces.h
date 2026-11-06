#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

inline constexpr float kPanelCorner = 2.0f;

// Körper eines Panels: flache Fläche (colours::strip) mit 1-px-Kante (colours::edge).
void drawPanelBody(juce::Graphics& g, juce::Rectangle<float> bounds);

// Senkrechter Gruppentrenner als einzelne Linie.
void drawDivider(juce::Graphics& g, float x, float top, float bottom);

// Flacher Fensterhintergrund (colours::window).
void drawWindowBackground(juce::Graphics& g, juce::Rectangle<int> bounds);

} // namespace dg::ui
