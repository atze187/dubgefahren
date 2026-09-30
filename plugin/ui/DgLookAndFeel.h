#pragma once
#include <map>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include "melatonin_blur/melatonin_blur.h"
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
inline const juce::Colour knobTop { 0xff3a4048 };
inline const juce::Colour knobBottom { 0xff1c2025 };
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

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW,
                      int buttonH, juce::ComboBox& box) override;
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

private:
    // Erhabene Fläche für Buttons und Auswahlboxen: Verlauf, Kontur, Lichtkante; gedrückt eingelassen.
    static void drawRaisedBody(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour base, bool down, bool highlighted, bool enabled);
    melatonin::DropShadow& knobShadow(int diameter);

    // Ein Schatten pro Körper-Durchmesser, damit der Cache von melatonin bei gemischten Reglergrößen hält.
    std::map<int, std::unique_ptr<melatonin::DropShadow>> knobShadows_;
    melatonin::DropShadow lampGlow_ { { colours::accent.withAlpha(0.8f), 4 } };
    melatonin::InnerShadow lampInset_ { { juce::Colours::black.withAlpha(0.7f), 2, { 0, 1 } } };
    juce::SharedResourcePointer<EmbeddedFonts> fonts_; // hält die Typefaces geladen
};

} // namespace dg::ui
