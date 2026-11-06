#include "plugin/ui/DgLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace dg::ui {

DgLookAndFeel::DgLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, colours::window);
    setColour(juce::Label::textColourId, colours::text);
    setColour(juce::Slider::textBoxTextColourId, colours::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
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

void DgLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                     float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const float lineW = 3.0f;
    const float arcR = radius - lineW;
    if (arcR < 3.0f)
        return; // zu klein zum Zeichnen (erstes Layout, winzige Zelle)
    const auto centre = bounds.getCentre();
    const juce::PathStrokeType stroke(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(colours::lcd);
    g.strokePath(track, stroke);

    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar
        ? rotaryStartAngle + static_cast<float>(slider.valueToProportionOfLength(0.0)) * (rotaryEndAngle - rotaryStartAngle)
        : rotaryStartAngle;

    if (std::abs(angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, std::min(from, angle), std::max(from, angle), true);
        g.setColour(slider.isEnabled() ? colours::amber : colours::textDim);
        g.strokePath(value, stroke);
    }

    // Flacher Körper mit Kante, darauf der Zeiger.
    const float bodyR = arcR - 6.0f;
    if (bodyR < 4.0f)
        return;
    g.setColour(colours::button);
    g.fillEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour(colours::edgeLight);
    g.drawEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    const juce::Point<float> dir(std::sin(angle), -std::cos(angle));
    g.setColour(slider.isEnabled() ? colours::text : colours::textDim);
    g.drawLine({ centre + dir * (bodyR * 0.35f), centre + dir * (bodyR * 0.85f) }, 2.0f);
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
