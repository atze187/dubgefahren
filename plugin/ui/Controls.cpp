#include "plugin/ui/Controls.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"

namespace dg::ui {

namespace {
void setupLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, colours::textDim);
    l.setFont(font(12.0f));
}
} // namespace

Knob::Knob(const juce::String& labelText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    // Farben des Wertefelds direkt am Slider setzen: Das Feld wird beim Anlegen gebaut, bevor der
    // Regler unter dem Look-and-Feel des Editors hängt, und übernähme sonst dessen Standardfarben.
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxTextColourId, colours::text);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    setupLabel(label, labelText);
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void Knob::attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    attachment_.reset();
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);
    // Die Attachment setzt die Textfunktion des Parameters (bis zu 7 Nachkommastellen). Angezeigt
    // werden zwei; der Wert selbst bleibt unverändert genau.
    slider.textFromValueFunction = [](double value) {
        const auto text = juce::String(value, 2);
        return text == "-0.00" ? juce::String("0.00") : text;
    };
    slider.updateText();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds(r.removeFromTop(14));
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
    label.setBounds(r.removeFromTop(14));
    box.setBounds(r.withSizeKeepingCentre(r.getWidth(), std::min(24, r.getHeight())));
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

void Toggle::resized() { button.setBounds(getLocalBounds().withSizeKeepingCentre(getWidth(), 24)); }

} // namespace dg::ui
