#pragma once
#include <juce_graphics/juce_graphics.h>
#include "engine/Kit.h"

namespace dg::ui {

struct PadColour
{
    const char* name;     // englisch, für das Menü
    juce::Colour base;    // Grundfarbe: Schimmer, Leuchten, Glow, Wellenform
    juce::Colour litText; // Schrift auf leuchtendem Pad
};

// Eintrag zum gespeicherten Index; Werte außerhalb 0 … kNumPadColours-1 liefern die Standardfarbe.
const PadColour& padColour(int index);

// Latch-Anzeige: heller Kern mit dunklem Ring, lesbar auf jeder Pad-Farbe.
inline const juce::Colour latchDotCore { 0xfff4f6f8 };
inline const juce::Colour latchDotRing { 0xff0a0c0e };

// WCAG 2.x: relative Leuchtdichte und Kontrastverhältnis (1 … 21).
float relativeLuminance(juce::Colour c);
float contrastRatio(juce::Colour a, juce::Colour b);

} // namespace dg::ui
