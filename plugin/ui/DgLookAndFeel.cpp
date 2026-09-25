#include "plugin/ui/DgLookAndFeel.h"

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
    const auto centre = bounds.getCentre();
    const float lineW = 3.0f;
    const float arcR = radius - lineW;
    const juce::PathStrokeType stroke(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(colours::outline);
    g.strokePath(track, stroke);

    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar
        ? rotaryStartAngle + static_cast<float>(slider.valueToProportionOfLength(0.0)) * (rotaryEndAngle - rotaryStartAngle)
        : rotaryStartAngle;

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, std::min(from, angle), std::max(from, angle), true);
    g.setColour(slider.isEnabled() ? colours::accent : colours::textDim);
    g.strokePath(value, stroke);

    const juce::Point<float> tip(centre.x + (arcR - 6.0f) * std::sin(angle), centre.y - (arcR - 6.0f) * std::cos(angle));
    g.setColour(colours::text);
    g.drawLine({ centre, tip }, 2.0f);
}

} // namespace dg::ui
