#pragma once
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace dg::ui {

inline juce::String u8(const char* s) { return juce::String::fromUTF8(s); }

class Knob final : public juce::Component
{
public:
    explicit Knob(const juce::String& labelText);
    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void resized() override;

    juce::Slider slider;
    juce::Label label;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

class Choice final : public juce::Component
{
public:
    explicit Choice(const juce::String& labelText);
    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void resized() override;

    juce::ComboBox box;
    juce::Label label;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment_;
};

class Toggle final : public juce::Component
{
public:
    explicit Toggle(const juce::String& text);
    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void resized() override;

    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
};

} // namespace dg::ui
