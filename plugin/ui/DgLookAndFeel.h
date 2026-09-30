#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/Fonts.h"

namespace dg::ui {

namespace colours {
inline const juce::Colour background { 0xff121417 };
inline const juce::Colour panel { 0xff1b1f24 };
inline const juce::Colour outline { 0xff2c323a };
inline const juce::Colour text { 0xffd8dde3 };
inline const juce::Colour textDim { 0xff8a939e };
inline const juce::Colour accent { 0xfff2b134 };
inline const juce::Colour latched { 0xff3aa0ff };
inline const juce::Colour warning { 0xffff8c2a };
inline const juce::Colour danger { 0xffff4d4d };
inline const juce::Colour backgroundTop { 0xff15181b };
inline const juce::Colour backgroundBottom { 0xff0f1113 };
inline const juce::Colour panelTop { 0xff1f2329 };
inline const juce::Colour panelBottom { 0xff181b20 };
inline const juce::Colour highlight { 0x12ffffff }; // Lichtkante: 7 % Weiß
inline const juce::Colour shadow { 0x99000000 };
inline const juce::Colour groove { 0xff0c0e10 };    // vertiefte Linien und Konturen
inline const juce::Colour padBase { 0xff4cd07d };
inline const juce::Colour padTop { 0xff27302f };
inline const juce::Colour padBottom { 0xff161b1b };
inline const juce::Colour padEmpty { 0xff101214 };
inline const juce::Colour padTextLit { 0xff06240f }; // Text auf leuchtendem Pad
} // namespace colours

class DgLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DgLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;

    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;

private:
    juce::SharedResourcePointer<EmbeddedFonts> fonts_; // hält die Typefaces geladen
};

} // namespace dg::ui
