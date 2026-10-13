#include "plugin/ui/PadColours.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

namespace {
// Funktionslokal, damit die Reihenfolge der statischen Initialisierung keine Rolle spielt.
const std::array<PadColour, kNumPadColours>& palette()
{
    static const std::array<PadColour, kNumPadColours> p { {
        { "Green",     colours::padBase,          colours::padTextLit },
        { "Turquoise", juce::Colour(0xff2fd3c7), juce::Colour(0xff042624) },
        { "Blue",      juce::Colour(0xff4f9dff), juce::Colour(0xff061a33) },
        { "Violet",    juce::Colour(0xffa98bff), juce::Colour(0xff1a0f3a) },
        { "Pink",      juce::Colour(0xffff6fc0), juce::Colour(0xff3a0a24) },
        { "Red",       juce::Colour(0xffff5a52), juce::Colour(0xff3a0806) },
        { "Orange",    juce::Colour(0xffff9a3c), juce::Colour(0xff3a1a03) },
        { "Yellow",    juce::Colour(0xfff5e663), juce::Colour(0xff332c02) },
    } };
    return p;
}

float linearChannel(juce::uint8 v)
{
    const float c = static_cast<float>(v) / 255.0f;
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
} // namespace

const PadColour& padColour(int index) { return palette()[static_cast<std::size_t>(clampPadColour(index))]; }

float relativeLuminance(juce::Colour c)
{
    return 0.2126f * linearChannel(c.getRed()) + 0.7152f * linearChannel(c.getGreen()) + 0.0722f * linearChannel(c.getBlue());
}

float contrastRatio(juce::Colour a, juce::Colour b)
{
    const float la = relativeLuminance(a);
    const float lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f);
}

} // namespace dg::ui
