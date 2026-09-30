#include <catch2/catch_test_macros.hpp>
#include <memory>
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"

using namespace dg;

namespace {
bool isInter(const juce::Font& f) { return f.getTypefacePtr() != nullptr && f.getTypefacePtr()->getName().startsWith("Inter"); }
} // namespace

TEST_CASE("the embedded font is used for regular and bold text", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    const juce::Font regular(ui::font(12.0f));
    const juce::Font bold(ui::font(12.0f, true));
    CHECK(isInter(regular));
    CHECK(isInter(bold));
    CHECK(regular.getTypefacePtr() != bold.getTypefacePtr());
    CHECK(regular.getHeight() == 12.0f);
}

TEST_CASE("the look and feel hands out the embedded font for standard widgets", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;

    juce::Label label;
    label.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    const auto labelFont = lnf.getLabelFont(label);
    CHECK(isInter(labelFont));
    CHECK(labelFont.getHeight() == 14.0f);
    CHECK(labelFont.getTypefacePtr() == juce::Font(ui::font(14.0f, true)).getTypefacePtr());

    juce::TextButton button("x");
    CHECK(isInter(lnf.getTextButtonFont(button, 24)));
    juce::ComboBox box;
    box.setSize(100, 24);
    CHECK(isInter(lnf.getComboBoxFont(box)));
    CHECK(isInter(lnf.getPopupMenuFont()));
    CHECK(isInter(lnf.getAlertWindowTitleFont()));
    CHECK(isInter(lnf.getAlertWindowMessageFont()));
    CHECK(isInter(lnf.getAlertWindowFont()));
}

TEST_CASE("the embedded font keeps a label's letter spacing", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Label label;
    label.setFont(juce::Font(ui::font(22.0f, true).withKerningFactor(0.12f)));
    CHECK(lnf.getLabelFont(label).getExtraKerningFactor() == 0.12f);
}

TEST_CASE("the embedded font survives closing another editor", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a, b;
    std::unique_ptr<juce::AudioProcessorEditor> editorA(a.createEditor());
    std::unique_ptr<juce::AudioProcessorEditor> editorB(b.createEditor());
    const auto before = juce::Font(ui::font(12.0f)).getTypefacePtr();
    editorA.reset();
    const auto after = juce::Font(ui::font(12.0f)).getTypefacePtr();
    CHECK(after == before);
    CHECK(isInter(juce::Font(ui::font(12.0f))));
    editorB.reset();
    CHECK(isInter(juce::Font(ui::font(12.0f)))); // ohne Editor wird neu geladen
}
