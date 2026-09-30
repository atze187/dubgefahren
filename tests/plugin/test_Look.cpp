#include <catch2/catch_test_macros.hpp>
#include <memory>
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/Surfaces.h"
#include "RenderTestHelpers.h"

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

TEST_CASE("a panel body is a vertical gradient with rounded corners", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Image img(juce::Image::ARGB, 100, 60, true);
    {
        juce::Graphics g(img);
        ui::drawPanelBody(g, img.getBounds().toFloat());
    }
    CHECK(img.getPixelAt(0, 0).getAlpha() == 0);         // Ecke bleibt frei
    CHECK(img.getPixelAt(50, 30).getAlpha() == 255);
    CHECK(img.getPixelAt(50, 8).getBrightness() > img.getPixelAt(50, 52).getBrightness());
}

TEST_CASE("a panel shadow reaches beyond the panel", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Image img(juce::Image::ARGB, 140, 100, true);
    ui::PanelShadow shadow;
    {
        juce::Graphics g(img);
        shadow.render(g, { 20.0f, 20.0f, 100.0f, 60.0f });
    }
    CHECK(img.getPixelAt(70, 84).getAlpha() > 0);  // unter dem Panel
    CHECK(img.getPixelAt(2, 2).getAlpha() == 0);   // weit weg: nichts
}

TEST_CASE("the editor paints at every window scale", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const float scale : { 0.75f, 1.0f, 2.0f })
    {
        DubgefahrenProcessor p;
        p.clearSlot(2);
        p.setUiScale(scale);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        const auto img = dgtest::snapshot(*editor);
        REQUIRE(img.isValid());
        CHECK(img.getWidth() == juce::roundToInt(1000.0f * scale));
        // Punkt unten rechts im Slot-Editor-Panel (keine Controls) gegen den Fensterrand.
        const auto panel = img.getPixelAt(juce::roundToInt(970.0f * scale), juce::roundToInt(420.0f * scale));
        const auto window = img.getPixelAt(juce::roundToInt(4.0f * scale), juce::roundToInt(4.0f * scale));
        CHECK(panel != window);
        dgtest::savePng(img, "editor-" + juce::String(scale, 2));
    }
}
