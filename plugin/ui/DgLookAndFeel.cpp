#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Controls.h"
#include <algorithm>
#include <cmath>

namespace dg::ui {

DgLookAndFeel::DgLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, colours::window);
    setColour(juce::Label::textColourId, colours::text);
    setColour(juce::Slider::textBoxTextColourId, colours::amber);
    setColour(juce::Slider::textBoxOutlineColourId, colours::edge);
    setColour(juce::Slider::textBoxBackgroundColourId, colours::lcd);
    setColour(juce::ComboBox::backgroundColourId, colours::lcd);
    setColour(juce::ComboBox::outlineColourId, colours::edge);
    setColour(juce::ComboBox::textColourId, colours::text);
    setColour(juce::ComboBox::arrowColourId, colours::textDim);
    setColour(juce::TextButton::buttonColourId, colours::button);
    setColour(juce::TextButton::buttonOnColourId, colours::amber);
    setColour(juce::TextButton::textColourOffId, colours::text);
    setColour(juce::TextButton::textColourOnId, colours::window);
    setColour(juce::ToggleButton::textColourId, colours::text);
    setColour(juce::ToggleButton::tickColourId, colours::amber);
    setColour(juce::ToggleButton::tickDisabledColourId, colours::textDim);
    setColour(juce::PopupMenu::backgroundColourId, colours::strip);
    setColour(juce::PopupMenu::textColourId, colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::amber);
    setColour(juce::PopupMenu::highlightedTextColourId, colours::window);
}

void DgLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                     float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                                     juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }
    constexpr float kSeg = 5.0f;
    constexpr float kGap = 1.0f;
    constexpr float kBar = 8.0f;
    const float left = static_cast<float>(x);
    const float right = left + static_cast<float>(width);
    const float top = static_cast<float>(y) + std::floor((static_cast<float>(height) - kBar) * 0.5f);
    const float filled = left + static_cast<float>(slider.valueToProportionOfLength(slider.getValue())) * (right - left);
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float centre = left + static_cast<float>(slider.valueToProportionOfLength(0.0)) * (right - left);
    const float litFrom = bipolar ? std::min(centre, filled) : left;
    const float litTo = bipolar ? std::max(centre, filled) : filled;
    const bool enabled = slider.isEnabled();
    const auto on = enabled ? colours::accent : colours::textDim;
    for (float sx = left; sx + kSeg <= right + 0.01f; sx += kSeg + kGap)
    {
        const float mid = sx + kSeg * 0.5f;
        g.setColour(mid > litFrom && mid < litTo ? on : colours::edge);
        g.fillRect(sx, top, kSeg, kBar);
    }
    if (bipolar)
    {
        // Mittelmarke über die volle Balkenhöhe.
        g.setColour(colours::edgeLight);
        g.fillRect(std::round(centre) - 0.5f, top, 1.0f, kBar);
    }
}

int DgLookAndFeel::getSliderThumbRadius(juce::Slider&) { return 0; }

juce::Label* DgLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    // LCD-Wertefeld: kleine Schrift, 4 px Rand, mittig; Farben kommen aus den Slider-Farben.
    auto* l = LookAndFeel_V4::createSliderTextBox(slider);
    l->setFont(font(kLcdFontHeight));
    l->setBorderSize(juce::BorderSize<int>(0, 4, 0, 4));
    l->setJustificationType(juce::Justification::centred);
    return l;
}
void DgLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& background,
                                         bool highlighted, bool down)
{
    auto fill = background;
    if (!b.isEnabled())
        fill = fill.withAlpha(0.5f);
    else if (down)
        fill = fill.darker(0.3f);
    else if (highlighted)
        fill = fill.brighter(0.15f);
    const auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    if (r.getWidth() < 2.0f || r.getHeight() < 2.0f)
        return;
    g.setColour(fill);
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(colours::edgeLight.withMultipliedAlpha(b.isEnabled() ? 1.0f : 0.5f));
    g.drawRoundedRectangle(r, 2.0f, 1.0f);
}

void DgLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    if (width < 4 || height < 4)
        return;
    const float alpha = box.isEnabled() ? 1.0f : 0.5f;
    const auto r = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId).withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(r, 2.0f, 1.0f);
    if (width < 30)
        return;
    const float cx = static_cast<float>(width) - 12.0f;
    const float cy = static_cast<float>(height) * 0.5f;
    juce::Path arrow;
    arrow.addTriangle(cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId).withAlpha(box.isEnabled() ? 0.9f : 0.3f));
    g.fillPath(arrow);
}

void DgLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool, bool)
{
    const auto area = button.getLocalBounds();
    if (area.getHeight() < 6 || area.getWidth() < 6)
        return;
    // Zustand als kleine flache Lampe statt als Häkchen.
    constexpr float lampSize = 10.0f;
    const juce::Rectangle<float> lampBounds(4.0f, std::round((static_cast<float>(area.getHeight()) - lampSize) * 0.5f), lampSize, lampSize);
    g.setColour(button.getToggleState()
                    ? button.findColour(button.isEnabled() ? juce::ToggleButton::tickColourId : juce::ToggleButton::tickDisabledColourId)
                    : colours::lcd);
    g.fillRect(lampBounds);
    g.setColour(colours::edgeLight);
    g.drawRect(lampBounds, 1.0f);

    g.setColour(button.findColour(juce::ToggleButton::textColourId).withAlpha(button.isEnabled() ? 1.0f : 0.5f));
    g.setFont(font(std::min(15.0f, static_cast<float>(area.getHeight()) * 0.75f)));
    g.drawFittedText(button.getButtonText(), area.withTrimmedLeft(static_cast<int>(lampSize) + 10).withTrimmedRight(2),
                     juce::Justification::centredLeft, 10);
}

void DgLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    // Wie LookAndFeel_V4, nur dass das LCD-Feld eines deaktivierten Sliders in textDim (ohne weitere Abblendung) statt Bernstein steht.
    g.fillAll(label.findColour(juce::Label::backgroundColourId));
    if (!label.isBeingEdited())
    {
        const auto* parentSlider = dynamic_cast<juce::Slider*>(label.getParentComponent());
        const bool dimmed = parentSlider != nullptr && !parentSlider->isEnabled();
        const float alpha = label.isEnabled() || dimmed ? 1.0f : 0.5f;
        const auto font = getLabelFont(label);
        g.setColour((dimmed ? colours::textDim : label.findColour(juce::Label::textColourId)).withMultipliedAlpha(alpha));
        g.setFont(font);
        const auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / font.getHeight())),
                         label.getMinimumHorizontalScale());
        g.setColour(label.findColour(juce::Label::outlineColourId).withMultipliedAlpha(alpha));
    }
    else if (label.isEnabled())
    {
        g.setColour(label.findColour(juce::Label::outlineColourId));
    }
    g.drawRect(label.getLocalBounds());
}

juce::Font DgLookAndFeel::getLabelFont(juce::Label& label) { return withEmbeddedTypeface(label.getFont()); }

juce::Font DgLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(font(std::min(15.0f, static_cast<float>(buttonHeight) * 0.6f)));
}

juce::Font DgLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return juce::Font(font(std::min(15.0f, static_cast<float>(box.getHeight()) * 0.85f)));
}

juce::Font DgLookAndFeel::getPopupMenuFont() { return juce::Font(font(15.0f)); }
juce::Font DgLookAndFeel::getAlertWindowTitleFont() { return juce::Font(font(17.0f, true)); }
juce::Font DgLookAndFeel::getAlertWindowMessageFont() { return juce::Font(font(15.0f)); }
juce::Font DgLookAndFeel::getAlertWindowFont() { return juce::Font(font(14.0f)); }

} // namespace dg::ui
