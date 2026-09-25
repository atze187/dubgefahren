#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace dg::ui {

namespace colours {
inline const juce::Colour background { 0xff121417 };
inline const juce::Colour panel { 0xff1b1f24 };
inline const juce::Colour outline { 0xff2c323a };
inline const juce::Colour text { 0xffd8dde3 };
inline const juce::Colour textDim { 0xff8a939e };
inline const juce::Colour accent { 0xfff2b134 };
inline const juce::Colour playing { 0xff4cd07d };
inline const juce::Colour latched { 0xff3aa0ff };
} // namespace colours

class DgLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DgLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;
};

} // namespace dg::ui
