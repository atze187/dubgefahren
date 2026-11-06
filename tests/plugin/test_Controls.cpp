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
    const float room = static_cast<float>(ui::kLcdBoxWidth - 8);
    for (const double v : { 0.0, 0.001, 12000.5, -1.0 })
    {
        const auto text = knob.slider.textFromValueFunction(v);
        INFO("value " << v << " -> " << text);
        CHECK(ui::lcdTextWidth(text) <= room);
    }
}

TEST_CASE("the LCD text of an attached knob fits too", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::Knob knob("x");
    knob.attach(p.state(), pid::cutoff);
    const float room = static_cast<float>(ui::kLcdBoxWidth - 8);
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
    ui::Segmented seg("Type");
    CHECK(knob.label.getText() == "PITCH");
    CHECK(choice.label.getText() == "TRIGGER MODE");
    CHECK(seg.label.getText() == "TYPE");
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

TEST_CASE("segmented selects the choice and follows the parameter", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    const auto id = slotParamId(0, SlotField::TrigMode); // Gate, Latch, One-Shot
    ui::Segmented seg("Mode");
    seg.setSize(180, 34);
    seg.attach(p.state(), id);
    auto* param = dynamic_cast<juce::AudioParameterChoice*>(p.state().getParameter(id));
    REQUIRE(param != nullptr);
    REQUIRE(seg.numSegments() == 3);
    CHECK(seg.selectedIndex() == param->getIndex());

    seg.selectAt(seg.segmentBounds(2).getCentre());
    CHECK(param->getIndex() == 2);
    CHECK(seg.selectedIndex() == 2);

    *param = 1; // von außen
    CHECK(seg.selectedIndex() == 1);

    // Segmente decken die Breite lückenlos ab.
    CHECK(seg.segmentBounds(0).getX() == 0);
    CHECK(seg.segmentBounds(2).getRight() == seg.getWidth());
}

TEST_CASE("segmented marks the active segment in amber on the button colour", "[controls]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::DgLookAndFeel lnf;
    ui::Segmented seg("Mode");
    seg.setLookAndFeel(&lnf);
    seg.setSize(180, 34);
    seg.attach(p.state(), slotParamId(0, SlotField::TrigMode));
    seg.selectAt(seg.segmentBounds(1).getCentre());
    const auto img = dgtest::snapshot(seg);
    dgtest::savePng(img, "segmented");
    const auto active = seg.segmentBounds(1).reduced(2);
    CHECK(img.getPixelAt(active.getX(), active.getY()) == ui::colours::button);
    const auto idle = seg.segmentBounds(0).reduced(2);
    CHECK(img.getPixelAt(idle.getX(), idle.getY()) != ui::colours::button);
    bool amber = false;
    for (int y = active.getY(); y < active.getBottom(); ++y)
        for (int x = active.getX(); x < active.getRight(); ++x)
            amber = amber || img.getPixelAt(x, y).getRed() > 200;
    CHECK(amber);
    seg.setLookAndFeel(nullptr);
}
