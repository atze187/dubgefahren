#pragma once
#include <juce_graphics/juce_graphics.h>

namespace dg::ui {

// Monospace-Schrift des Systems in der gegebenen Höhe. bold liefert den fetten Schnitt.
juce::FontOptions font(float height, bool bold = false);

// Gleiche Höhe, Fettung und Laufweite wie f, aber in der Monospace-Schrift.
// Der Name bleibt aus der Zeit der eingebetteten Schrift erhalten, damit sich Aufrufer nicht ändern.
juce::Font withEmbeddedTypeface(const juce::Font& f);

} // namespace dg::ui
