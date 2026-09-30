#pragma once
#include <juce_graphics/juce_graphics.h>

namespace dg::ui {

// Die eingebetteten Typefaces (Inter). Wer eine juce::SharedResourcePointer<EmbeddedFonts>
// hält, hält sie geladen; DgLookAndFeel tut das für die Lebensdauer des Editors.
struct EmbeddedFonts
{
    EmbeddedFonts();
    juce::Typeface::Ptr regular;
    juce::Typeface::Ptr semiBold;
};

// Eingebettete Schrift in der gegebenen Höhe. bold liefert den Schnitt SemiBold.
juce::FontOptions font(float height, bool bold = false);

// Gleiche Höhe, Fettung und Laufweite wie f, aber in der eingebetteten Schrift.
juce::Font withEmbeddedTypeface(const juce::Font& f);

} // namespace dg::ui
