#pragma once
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace dg::ui {

inline juce::String u8(const char* s) { return juce::String::fromUTF8(s); }

// LCD-Wertefeld rechts neben dem Segment-Slider: Box 52x18, Text 10 px mit je 4 px Rand.
inline constexpr int kLcdBoxWidth = 52;
inline constexpr int kLcdBoxHeight = 18;
inline constexpr float kLcdFontHeight = 10.0f;

// Breite des Textes in der LCD-Schrift.
float lcdTextWidth(const juce::String& text);

// Zelle aus Beschriftung (oben, 12 px) und Segment-Slider mit LCD-Wert darunter.
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

// Schalter mit einem Segment je Eintrag eines AudioParameterChoice; das aktive Segment leuchtet bernsteinfarben.
class Segmented final : public juce::Component
{
public:
    explicit Segmented(const juce::String& labelText);
    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

    int numSegments() const { return items_.size(); }
    int selectedIndex() const { return selected_; }
    juce::Rectangle<int> segmentBounds(int index) const;
    void selectAt(juce::Point<int> p); // wählt das Segment unter p und schreibt den Parameter

    juce::Label label;

private:
    juce::StringArray items_;
    int selected_ = 0;
    juce::Rectangle<int> row_;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
};

} // namespace dg::ui
