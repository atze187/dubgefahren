#include "plugin/ui/Controls.h"
#include <algorithm>
#include <cmath>
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"

namespace dg::ui {

namespace {
constexpr int kLabelHeight = 12;
constexpr int kControlHeight = 22;
constexpr float kLcdTextRoom = static_cast<float>(kLcdBoxWidth - 8); // je 4 px Rand

void setupLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text.toUpperCase(), juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centredLeft);
    l.setBorderSize(juce::BorderSize<int>(0, 0, 0, 0));
    l.setColour(juce::Label::textColourId, colours::textDim);
    l.setFont(font(10.0f));
}

// Text mit so vielen Nachkommastellen wie passen (höchstens decimals); nie "-0.00".
juce::String lcdText(double value, int decimals)
{
    for (int d = decimals;; --d)
    {
        // JUCE zeigt bei 0 Stellen alle signifikanten Stellen; ganze Zahlen deshalb selbst runden.
        auto text = d > 0 ? juce::String(value, d) : juce::String(static_cast<juce::int64>(std::llround(value)));
        if (text.startsWith("-") && text.getDoubleValue() == 0.0)
            text = text.substring(1);
        if (d <= 0 || lcdTextWidth(text) <= kLcdTextRoom)
            return text;
    }
}
} // namespace

float lcdTextWidth(const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(juce::Font(font(kLcdFontHeight)), text, 0.0f, 0.0f);
    return glyphs.getBoundingBox(0, -1, false).getWidth();
}

Knob::Knob(const juce::String& labelText)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    // Farben des Wertefelds direkt am Slider setzen: Das Feld wird beim Anlegen gebaut, bevor der
    // Regler unter dem Look-and-Feel des Editors hängt, und übernähme sonst dessen Standardfarben.
    slider.setColour(juce::Slider::textBoxOutlineColourId, colours::edge);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, colours::lcd);
    slider.setColour(juce::Slider::textBoxTextColourId, colours::amber);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, kLcdBoxWidth, kLcdBoxHeight);
    slider.textFromValueFunction = [](double value) { return lcdText(value, 2); };
    setupLabel(label, labelText);
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void Knob::attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    attachment_.reset();
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);
    // Die Attachment setzt die Textfunktion des Parameters (bis zu 7 Nachkommastellen). Angezeigt
    // werden zwei, bei Zeiten (Einheit s) drei, bei großen Werten weniger, damit der Text ins LCD passt;
    // der Wert selbst bleibt unverändert genau.
    const auto* param = apvts.getParameter(paramId);
    const int decimals = param != nullptr && param->getLabel() == "s" ? 3 : 2;
    slider.textFromValueFunction = [decimals](double value) { return lcdText(value, decimals); };
    slider.updateText();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds(r.removeFromTop(kLabelHeight));
    slider.setBounds(r);
}

Choice::Choice(const juce::String& labelText)
{
    setupLabel(label, labelText);
    addAndMakeVisible(box);
    addAndMakeVisible(label);
}

void Choice::attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    attachment_.reset();
    box.clear(juce::dontSendNotification);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId)))
        box.addItemList(choice->choices, 1);
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramId, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds(r.removeFromTop(kLabelHeight));
    box.setBounds(r.removeFromTop(std::min(kControlHeight, r.getHeight())));
}

Toggle::Toggle(const juce::String& text)
{
    button.setButtonText(text);
    addAndMakeVisible(button);
}

void Toggle::attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    attachment_.reset();
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramId, button);
}

void Toggle::resized() { button.setBounds(getLocalBounds().withSizeKeepingCentre(getWidth(), std::min(kControlHeight, getHeight()))); }

Segmented::Segmented(const juce::String& labelText)
{
    setupLabel(label, labelText);
    addAndMakeVisible(label);
}

void Segmented::attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    attachment_.reset();
    items_.clear();
    selected_ = 0;
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId));
    if (choice == nullptr)
    {
        jassertfalse; // nur Choice-Parameter
        repaint();
        return;
    }
    items_ = choice->choices;
    attachment_ = std::make_unique<juce::ParameterAttachment>(
        *choice,
        [this](float v) {
            selected_ = std::clamp(juce::roundToInt(v), 0, std::max(0, items_.size() - 1));
            repaint();
        });
    attachment_->sendInitialUpdate();
    repaint();
}

void Segmented::resized()
{
    auto r = getLocalBounds();
    label.setBounds(r.removeFromTop(kLabelHeight));
    row_ = r.removeFromTop(std::min(kControlHeight, r.getHeight()));
}

juce::Rectangle<int> Segmented::segmentBounds(int index) const
{
    const int n = std::max(1, items_.size());
    // Ganzzahlige Grenzen, die die Breite lückenlos teilen.
    const int x0 = row_.getX() + row_.getWidth() * index / n;
    const int x1 = row_.getX() + row_.getWidth() * (index + 1) / n;
    return { x0, row_.getY(), x1 - x0, row_.getHeight() };
}

void Segmented::paint(juce::Graphics& g)
{
    for (int i = 0; i < items_.size(); ++i)
    {
        const auto r = segmentBounds(i);
        const bool active = i == selected_;
        const auto cell = r.toFloat().reduced(0.5f);
        g.setColour(active ? colours::button : colours::lcd);
        g.fillRoundedRectangle(cell, 2.0f);
        g.setColour(active ? colours::edgeLight : colours::edge);
        g.drawRoundedRectangle(cell, 2.0f, 1.0f);
        g.setColour(active ? colours::amber : colours::textDim);
        g.setFont(juce::Font(font(10.0f, active)));
        g.drawFittedText(items_[i].toUpperCase(), r.reduced(2, 0), juce::Justification::centred, 1, 0.8f);
    }
}

void Segmented::selectAt(juce::Point<int> p)
{
    for (int i = 0; i < items_.size(); ++i)
        if (segmentBounds(i).contains(p))
        {
            if (attachment_ != nullptr)
                attachment_->setValueAsCompleteGesture(static_cast<float>(i));
            selected_ = i;
            repaint();
            return;
        }
}

void Segmented::mouseDown(const juce::MouseEvent& e) { selectAt(e.getPosition()); }

} // namespace dg::ui
