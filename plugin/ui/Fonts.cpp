#include "plugin/ui/Fonts.h"
#include <algorithm>

namespace dg::ui {

juce::FontOptions font(float height, bool bold)
{
    return juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), std::max(1.0f, height),
                             bold ? juce::Font::bold : juce::Font::plain);
}

juce::Font withEmbeddedTypeface(const juce::Font& f)
{
    const bool bold = f.isBold() || f.getTypefaceStyle().containsIgnoreCase("bold");
    return juce::Font(font(f.getHeight(), bold).withKerningFactor(f.getExtraKerningFactor()));
}

} // namespace dg::ui
