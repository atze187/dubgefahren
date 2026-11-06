#include <catch2/catch_test_macros.hpp>
#include <limits>
#include "plugin/ui/CpuMeter.h"
#include "plugin/ui/DgLookAndFeel.h"

using namespace dg::ui;

namespace {
juce::Colour textColour(const CpuMeter& m) { return m.findColour(juce::Label::textColourId); }
} // namespace

TEST_CASE("cpu meter shows whole percent and warns by colour", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    CpuMeter m;
    CHECK(m.getText() == "CPU 0 %");

    m.setLoad(0.042);
    CHECK(m.getText() == "CPU 4 %");
    CHECK(textColour(m) == colours::textDim);

    m.setLoad(0.5);
    CHECK(m.getText() == "CPU 50 %");
    CHECK(textColour(m) == colours::warning);

    m.setLoad(0.8);
    CHECK(textColour(m) == colours::danger);

    m.setLoad(1.7);
    CHECK(m.getText() == "CPU 170 %");
    CHECK(textColour(m) == colours::danger);
}

TEST_CASE("cpu meter treats invalid loads as zero", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    CpuMeter m;
    m.setLoad(0.3);
    m.setLoad(-0.5);
    CHECK(m.getText() == "CPU 0 %");
    m.setLoad(0.3);
    m.setLoad(std::numeric_limits<double>::quiet_NaN());
    CHECK(m.getText() == "CPU 0 %");
    CHECK(textColour(m) == colours::textDim);
}

TEST_CASE("cpu meter looks like a flat LCD readout", "[editor][look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    CpuMeter m;
    CHECK(m.findColour(juce::Label::backgroundColourId) == colours::lcd);
    CHECK(m.findColour(juce::Label::outlineColourId) == colours::edge);
    CHECK(m.getFont().getHeight() == 11.0f);
    CHECK(m.getBorderSize().getLeft() == 6);
    CHECK(m.getBorderSize().getRight() == 6);
    CHECK(m.getJustificationType() == juce::Justification::centred);

    juce::Label l;
    styleLcd(l, colours::amber, juce::Justification::centredLeft);
    CHECK(l.findColour(juce::Label::textColourId) == colours::amber);
    CHECK(l.findColour(juce::Label::backgroundColourId) == colours::lcd);
    CHECK(l.getJustificationType() == juce::Justification::centredLeft);
}
