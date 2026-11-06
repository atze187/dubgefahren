#pragma once
#include <juce_graphics/juce_graphics.h>

namespace dg::ui {

// Monospace-Schrift des Systems in der gegebenen Höhe. bold liefert den fetten Schnitt.
juce::FontOptions font(float height, bool bold = false);

// Gleiche Höhe, Fettung und Laufweite wie f, aber in der Monospace-Schrift.
juce::Font withUiTypeface(const juce::Font& f);

} // namespace dg::ui
