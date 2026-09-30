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
    return juce::Font(font(f.getHeight(), f.isBold()).withKerningFactor(f.getExtraKerningFactor()));
}

} // namespace dg::ui
