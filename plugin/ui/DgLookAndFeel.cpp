#include "plugin/ui/DgLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace dg::ui {

DgLookAndFeel::DgLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, colours::background);
    setColour(juce::Label::textColourId, colours::text);
    setColour(juce::Slider::textBoxTextColourId, colours::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, colours::panel);
    setColour(juce::ComboBox::outlineColourId, colours::outline);
    setColour(juce::ComboBox::textColourId, colours::text);
    setColour(juce::ComboBox::arrowColourId, colours::textDim);
    setColour(juce::TextButton::buttonColourId, colours::panel);
    setColour(juce::TextButton::buttonOnColourId, colours::accent);
    setColour(juce::TextButton::textColourOffId, colours::text);
    setColour(juce::TextButton::textColourOnId, colours::background);
    setColour(juce::ToggleButton::textColourId, colours::text);
    setColour(juce::ToggleButton::tickColourId, colours::accent);
    setColour(juce::ToggleButton::tickDisabledColourId, colours::textDim);
    setColour(juce::PopupMenu::backgroundColourId, colours::panel);
    setColour(juce::PopupMenu::textColourId, colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour(juce::PopupMenu::highlightedTextColourId, colours::background);
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
    const juce::PathStrokeType stroke(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    // Spur: vertieft und dunkel.
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(colours::groove);
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
        if (slider.isEnabled())
        {
            // Schwacher Glow: zwei breitere, fast durchsichtige Striche. Bewusst ohne Blur, weil sich
            // der Bogen mit jedem Wert ändert und ein Blur deshalb nie aus dem Cache käme.
            g.setColour(colours::accent.withAlpha(0.08f));
            g.strokePath(value, juce::PathStrokeType(lineW + 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(colours::accent.withAlpha(0.16f));
            g.strokePath(value, juce::PathStrokeType(lineW + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour(slider.isEnabled() ? colours::accent : colours::textDim);
        g.strokePath(value, stroke);
    }

    // Körper mit Verlauf und kleinem Schatten, darauf der Zeiger.
    const float bodyR = arcR - 6.0f;
    if (bodyR < 4.0f)
        return;
    juce::Path body;
    body.addEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    knobShadow(juce::roundToInt(bodyR * 2.0f)).render(g, body);
    g.setGradientFill(juce::ColourGradient(colours::knobTop, centre.x, centre.y - bodyR * 0.6f, colours::knobBottom, centre.x,
                                           centre.y + bodyR, true));
    g.fillPath(body);
    g.setColour(colours::groove);
    g.strokePath(body, juce::PathStrokeType(1.0f));

    const juce::Point<float> dir(std::sin(angle), -std::cos(angle));
    g.setColour(slider.isEnabled() ? colours::text : colours::textDim);
    g.drawLine({ centre + dir * (bodyR * 0.35f), centre + dir * (bodyR * 0.85f) }, 2.0f);
}

melatonin::DropShadow& DgLookAndFeel::knobShadow(int diameter)
{
    auto& slot = knobShadows_[diameter];
    if (slot == nullptr)
        slot = std::make_unique<melatonin::DropShadow>(colours::shadow, 3, juce::Point<int> { 0, 2 });
    return *slot;
}

void DgLookAndFeel::drawRaisedBody(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour base, bool down, bool highlighted,
                                   bool enabled)
{
    constexpr float corner = 5.0f;
    auto r = bounds.reduced(0.5f);
    r.removeFromBottom(1.0f); // Platz für die Schattenlinie
    if (r.getWidth() < 2.0f || r.getHeight() < 2.0f)
        return;
    const float alpha = enabled ? 1.0f : 0.5f;

    if (!down)
    {
        g.setColour(colours::shadow.withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(r.translated(0.0f, 1.0f), corner);
    }
    auto top = base.brighter(0.14f);
    auto bottom = base.darker(0.14f);
    if (down)
        std::swap(top, bottom);
    else if (highlighted)
    {
        top = top.brighter(0.06f);
        bottom = bottom.brighter(0.06f);
    }
    g.setGradientFill(juce::ColourGradient(top.withMultipliedAlpha(alpha), 0.0f, r.getY(), bottom.withMultipliedAlpha(alpha), 0.0f,
                                           r.getBottom(), false));
    g.fillRoundedRectangle(r, corner);
    g.setColour(colours::groove.withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(r, corner, 1.0f);
    if (!down && r.getWidth() > 2.0f * corner)
    {
        g.setColour(colours::highlight.withMultipliedAlpha(alpha));
        g.fillRect(juce::Rectangle<float>(r.getX() + corner, r.getY() + 1.0f, r.getWidth() - 2.0f * corner, 1.0f));
    }
}

void DgLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    drawRaisedBody(g, button.getLocalBounds().toFloat(), backgroundColour, shouldDrawButtonAsDown, shouldDrawButtonAsHighlighted,
                   button.isEnabled());
}

void DgLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int, int, int, int, juce::ComboBox& box)
{
    if (width < 4 || height < 4)
        return;
    drawRaisedBody(g, juce::Rectangle<int>(0, 0, width, height).toFloat(), box.findColour(juce::ComboBox::backgroundColourId),
                   isButtonDown, box.isMouseOver(true), box.isEnabled());
    if (width < 30)
        return;
    const float cx = static_cast<float>(width) - 14.0f;
    const float cy = static_cast<float>(height) * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
    arrow.lineTo(cx, cy + 2.5f);
    arrow.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId).withAlpha(box.isEnabled() ? 0.9f : 0.3f));
    g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void DgLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool, bool)
{
    const auto area = button.getLocalBounds();
    if (area.getHeight() < 6 || area.getWidth() < 6)
        return;
    // Zustand als kleine Leuchte statt als Häkchen.
    constexpr float lampSize = 10.0f;
    const juce::Rectangle<float> lampBounds(4.0f, (static_cast<float>(area.getHeight()) - lampSize) * 0.5f, lampSize, lampSize);
    juce::Path lamp;
    lamp.addEllipse(lampBounds);
    if (button.getToggleState())
    {
        if (button.isEnabled())
            lampGlow_.render(g, lamp);
        g.setColour(button.findColour(button.isEnabled() ? juce::ToggleButton::tickColourId : juce::ToggleButton::tickDisabledColourId));
        g.fillPath(lamp);
    }
    else
    {
        g.setColour(colours::groove);
        g.fillPath(lamp);
        lampInset_.render(g, lamp);
    }
    g.setColour(colours::outline);
    g.strokePath(lamp, juce::PathStrokeType(1.0f));

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
