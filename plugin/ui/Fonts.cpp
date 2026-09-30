#include "plugin/ui/Fonts.h"
#include <algorithm>
#include <juce_core/juce_core.h>
#include "DgFonts.h"

namespace dg::ui {

EmbeddedFonts::EmbeddedFonts()
    : regular(juce::Typeface::createSystemTypefaceFor(dg_fonts::InterRegular_ttf, static_cast<std::size_t>(dg_fonts::InterRegular_ttfSize))),
      semiBold(juce::Typeface::createSystemTypefaceFor(dg_fonts::InterSemiBold_ttf, static_cast<std::size_t>(dg_fonts::InterSemiBold_ttfSize)))
{
}

juce::FontOptions font(float height, bool bold)
{
    // Kein statisches Objekt: Die Typefaces sollen vor dem Entladen der DLL freigegeben sein.
    const juce::SharedResourcePointer<EmbeddedFonts> fonts;
    return juce::FontOptions(bold ? fonts->semiBold : fonts->regular).withHeight(std::max(1.0f, height));
}

juce::Font withEmbeddedTypeface(const juce::Font& f)
{
    // isBold() sucht das ganze Wort "Bold" im Stilnamen und erkennt den eingebetteten Schnitt "SemiBold" nicht.
    const bool bold = f.isBold() || f.getTypefaceStyle().containsIgnoreCase("bold");
    return juce::Font(font(f.getHeight(), bold).withKerningFactor(f.getExtraKerningFactor()));
}

} // namespace dg::ui
