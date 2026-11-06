#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/Fonts.h"

namespace dg::ui {

// Flache LCD-Bedienfeld-Palette: keine Verläufe, keine Lichtkanten, keine Schlagschatten.
namespace colours {
inline const juce::Colour window { 0xff14161a };    // Fensterhintergrund
inline const juce::Colour strip { 0xff191c21 };     // Panelfläche
inline const juce::Colour lcd { 0xff0b0d0f };       // vertiefte Anzeige
inline const juce::Colour edge { 0xff2a2e35 };      // Kante von Panels und Anzeigen
inline const juce::Colour edgeLight { 0xff3a3f47 }; // Kante von Buttons
inline const juce::Colour button { 0xff23272e };
inline const juce::Colour text { 0xffd7dbe0 };
inline const juce::Colour textDim { 0xff7b828d };
inline const juce::Colour amber { 0xffffb347 };     // aktiver Zustand, Werte
inline const juce::Colour accent { 0xff5aa9e6 };    // Blau
inline const juce::Colour warning { 0xffff8c2a };
inline const juce::Colour danger { 0xffff4d4d };

inline const juce::Colour padBase { 0xff4cd07d };
inline const juce::Colour padTextLit { 0xff06240f }; // Text auf leuchtendem Pad
} // namespace colours

// LCD-Anzeige für ein Label: Mono 11, 6 px Rand links und rechts, Hintergrund lcd, Kante edge.
void styleLcd(juce::Label& label, juce::Colour text, juce::Justification justification = juce::Justification::centred);

class DgLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DgLookAndFeel();
    // Segment-Slider: 5 px Segmente, 1 px Lücke, 8 px Balken; bipolare Slider füllen von der Mitte.
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle style, juce::Slider& slider) override;
    int getSliderThumbRadius(juce::Slider& slider) override;
    juce::Label* createSliderTextBox(juce::Slider& slider) override;
    juce::Font getLabelFont(juce::Label& label) override;
    void drawLabel(juce::Graphics& g, juce::Label& label) override;
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
};

} // namespace dg::ui
