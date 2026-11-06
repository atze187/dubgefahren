#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/Controls.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "RenderTestHelpers.h"

using namespace dg;

namespace {
// Knob in einer 200x34-Zelle: 12 px Beschriftung, darunter der Slider (22 px) mit der Balkenmitte bei y = 23.
struct KnobFixture
{
    ui::DgLookAndFeel lnf;
    ui::Knob knob { "Test" };

    KnobFixture(double lo, double hi)
    {
        knob.setLookAndFeel(&lnf);
        knob.slider.setRange(lo, hi);
        knob.setSize(200, 34);
    }
    ~KnobFixture() { knob.setLookAndFeel(nullptr); }

    juce::Rectangle<int> track()
    {
        return lnf.getSliderLayout(knob.slider).sliderBounds + knob.slider.getPosition();
    }
    juce::Image render(double value)
    {
        knob.slider.setValue(value, juce::dontSendNotification);
        return dgtest::snapshot(knob);
    }
};

constexpr int kBarY = 23;

int countSegments(const juce::Image& img, int y = kBarY)
{
    int n = 0;
    bool inside = false;
    for (int x = 0; x < img.getWidth(); ++x)
    {
        const bool on = img.getPixelAt(x, y) == ui::colours::accent;
        n += (on && !inside) ? 1 : 0;
        inside = on;
    }
    return n;
}

// Kleinstes und größtes x einer Akzentfläche auf der Balkenmitte; -1, wenn keine da ist.
std::pair<int, int> accentSpan(const juce::Image& img)
{
    int lo = -1, hi = -1;
    for (int x = 0; x < img.getWidth(); ++x)
        if (img.getPixelAt(x, kBarY) == ui::colours::accent)
        {
            lo = lo < 0 ? x : lo;
            hi = x;
        }
    return { lo, hi };
}
} // namespace

TEST_CASE("segment slider fills proportionally", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    KnobFixture f(0.0, 1.0);
    const int none = countSegments(f.render(0.0));
    const int half = countSegments(f.render(0.5));
    const int full = countSegments(f.render(1.0));
    CHECK(none == 0);
    CHECK(full > 10);
    CHECK(std::abs(half - full / 2.0) <= 1.0);
    dgtest::savePng(f.render(0.5), "knob-cell");
}

TEST_CASE("a bipolar slider fills from the centre", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    KnobFixture f(-1.0, 1.0);
    const auto bounds = f.track();
    const int centre = bounds.getX() + bounds.getWidth() / 2;

    CHECK(countSegments(f.render(0.0)) == 0);

    const auto right = accentSpan(f.render(0.5));
    REQUIRE(right.first >= 0);
    CHECK(right.first >= centre - 3);

    const auto left = accentSpan(f.render(-0.5));
    REQUIRE(left.first >= 0);
    CHECK(left.second <= centre + 3);
}

TEST_CASE("a disabled slider is dimmed", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    KnobFixture f(0.0, 1.0);
    CHECK(countSegments(f.render(0.8)) > 0);
    f.knob.slider.setEnabled(false);
    CHECK(countSegments(f.render(0.8)) == 0);
    // Gedämpfte Füllung statt Akzent: die Balkenmitte enthält textDim.
    const auto img = f.render(0.8);
    bool dim = false;
    for (int x = 0; x < img.getWidth(); ++x)
        dim = dim || img.getPixelAt(x, kBarY) == ui::colours::textDim;
    CHECK(dim);
}

TEST_CASE("the LCD text fits for extreme values", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::Knob knob("x");
    const float room = ui::kLcdTextRoom;
    for (const double v : { 0.0, 0.001, 12000.5, -1.0 })
    {
        const auto text = knob.slider.textFromValueFunction(v);
        INFO("value " << v << " -> " << text);
        CHECK(ui::lcdTextWidth(text) <= room);
    }
}

TEST_CASE("the LCD shows up to six characters with two decimals", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::Knob knob("x");
    CHECK(knob.slider.textFromValueFunction(123.45) == "123.45");
    CHECK(knob.slider.textFromValueFunction(-1.5) == "-1.50");
    CHECK(knob.slider.textFromValueFunction(0.25) == "0.25");
    CHECK(ui::kLcdBoxWidth == 52);
    CHECK(ui::kLcdFontHeight == 10.0f);
}

TEST_CASE("clicking away from the thumb does not move the slider", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    KnobFixture f(0.0, 1.0);
    f.knob.slider.setValue(0.2, juce::dontSendNotification);
    CHECK_FALSE(f.knob.slider.getSliderSnapsToMousePosition());

    const auto pos = juce::Point<float>(static_cast<float>(f.knob.slider.getWidth()) * 0.6f, 11.0f);
    const auto now = juce::Time::getCurrentTime();
    const juce::MouseEvent e(juce::Desktop::getInstance().getMainMouseSource(), pos, juce::ModifierKeys::leftButtonModifier,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &f.knob.slider, &f.knob.slider, now, pos, now, 1, false);
    f.knob.slider.mouseDown(e);
    CHECK(f.knob.slider.getValue() == 0.2);
    f.knob.slider.mouseUp(e);
}

TEST_CASE("a disabled slider also dims its LCD text", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    KnobFixture f(0.0, 1.0);
    // Text pixels im LCD-Feld (rechts, unter der Beschriftung), nach Farbton gezählt: Amber ist warm (rot > blau),
    // textDim ist kühl (blau > rot). Anti-aliasing mischt mit dem fast schwarzen Hintergrund, ändert aber den Farbton
    // nicht; so zählen auch Schriften, die auf Linux keine volldeckenden Pixel liefern.
    const auto countPixels = [](const juce::Image& img, bool warm) {
        int n = 0;
        for (int y = 12; y < img.getHeight(); ++y)
            for (int x = img.getWidth() - ui::kLcdBoxWidth + 2; x < img.getWidth() - 2; ++x)
            {
                const auto c = img.getPixelAt(x, y);
                const int r = c.getRed(), b = c.getBlue();
                if (warm ? (r - b > 40) : (b - r >= 8 && b > 0x40))
                    ++n;
            }
        return n;
    };
    CHECK(countPixels(f.render(0.8), true) > 0);
    f.knob.slider.setEnabled(false);
    const auto dimmed = f.render(0.8);
    CHECK(countPixels(dimmed, true) == 0);
    CHECK(countPixels(dimmed, false) > 0);
}

TEST_CASE("the LCD text of an attached knob fits too", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::Knob knob("x");
    knob.attach(p.state(), pid::cutoff);
    const float room = ui::kLcdTextRoom;
    for (const double v : { 20.0, 12000.5, 19999.9993 })
        CHECK(ui::lcdTextWidth(knob.slider.textFromValueFunction(v)) <= room);
}

TEST_CASE("the LCD of a knob uses the amber text on the dark display", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::Knob knob("x");
    CHECK(knob.slider.findColour(juce::Slider::textBoxTextColourId) == ui::colours::amber);
    CHECK(knob.slider.findColour(juce::Slider::textBoxBackgroundColourId) == ui::colours::lcd);
    CHECK(knob.slider.findColour(juce::Slider::textBoxOutlineColourId) == ui::colours::edge);
    CHECK(knob.slider.getSliderStyle() == juce::Slider::LinearHorizontal);
    CHECK(knob.slider.getTextBoxPosition() == juce::Slider::TextBoxRight);
    CHECK(knob.slider.getTextBoxWidth() == ui::kLcdBoxWidth);
    CHECK(knob.slider.getTextBoxHeight() == 18);
}

TEST_CASE("cell labels are upper case", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::Knob knob("Pitch");
    ui::Choice choice("Trigger Mode");
    CHECK(knob.label.getText() == "PITCH");
    CHECK(choice.label.getText() == "TRIGGER MODE");
}

TEST_CASE("choice and toggle cells are 22 px high", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::Choice choice("Mode");
    choice.setSize(120, 40);
    CHECK(choice.box.getHeight() == 22);
    CHECK(choice.label.getHeight() == 12);
    ui::Toggle toggle("Sync");
    toggle.setSize(120, 40);
    CHECK(toggle.button.getHeight() == 22);
}
