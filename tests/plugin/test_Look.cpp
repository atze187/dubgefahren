#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/PadGlow.h"
#include "plugin/ui/PadGrid.h"
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

TEST_CASE("a pad lights up on trigger and fades after release", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    grid.setSize(360, 384);

    CHECK(grid.padBrightness(0) == 0.0f);
    grid.setPadStates(1u, 0u, 0);
    CHECK(grid.padBrightness(0) == 1.0f);
    CHECK(grid.padBrightness(1) == 0.0f);

    juce::Thread::sleep(400);
    grid.setPadStates(1u, 0u, 0);
    CHECK(grid.padBrightness(0) == ui::kPadGlowHold);

    grid.setPadStates(0u, 0u, 0);
    juce::Thread::sleep(250);
    grid.setPadStates(0u, 0u, 0);
    CHECK(grid.padBrightness(0) == 0.0f);
}

TEST_CASE("an empty pad never lights up", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.clearSlot(2);
    ui::PadGrid grid(p);
    grid.setSize(360, 384);
    grid.setPadStates(1u << 2, 0u, 0);
    CHECK(grid.padBrightness(2) == 0.0f);

    // Ein leuchtendes Pad wird geleert: Es klingt aus, statt weiter zu leuchten.
    grid.setPadStates(1u << 5, 0u, 0);
    CHECK(grid.padBrightness(5) == 1.0f);
    p.clearSlot(5);
    grid.refreshNames();
    juce::Thread::sleep(250);
    grid.setPadStates(1u << 5, 0u, 0);
    CHECK(grid.padBrightness(5) == 0.0f);
}

TEST_CASE("a changing glow reports the pad area plus its reach", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    grid.setSize(360, 384);
    grid.setPadStates(0u, 0u, 0); // Ausgangszustand, Fokus auf Pad 1

    std::vector<juce::Rectangle<int>> areas;
    grid.onGlowChanged = [&areas](juce::Rectangle<int> a) { areas.push_back(a); };

    grid.setPadStates(1u << 3, 0u, 0);
    REQUIRE(areas.size() == 1);
    CHECK(areas[0] == grid.pad(3).getBounds().expanded(ui::PadGrid::kGlowReach));

    areas.clear();
    juce::Thread::sleep(20); // messbarer Zeitschritt, damit Pad 4 sicher vom Trigger-Wert abfällt
    grid.setPadStates(1u << 3, 0u, 1); // Fokus wandert von Pad 1 zu Pad 2
    CHECK(areas.size() == 3);          // Pad 4 fällt auf den Haltewert, Pad 1 und 2 wechseln den Fokus-Glow
}

TEST_CASE("pads use the shared base colour until one is assigned", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    CHECK(grid.padBaseColour(0) == ui::colours::padBase);
    grid.setPadBaseColour(0, juce::Colours::red);
    CHECK(grid.padBaseColour(0) == juce::Colours::red);
    CHECK(grid.padBaseColour(1) == ui::colours::padBase);
}

TEST_CASE("pad glows are drawn outside the pads and only for lit or focused pads", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    grid.setSize(360, 384);

    const auto render = [&grid] {
        juce::Image img(juce::Image::ARGB, 360 + 2 * ui::PadGrid::kGlowReach, 384 + 2 * ui::PadGrid::kGlowReach, true);
        juce::Graphics g(img);
        g.setOrigin(ui::PadGrid::kGlowReach, ui::PadGrid::kGlowReach);
        grid.paintGlows(g);
        return img;
    };

    grid.setPadStates(0u, 0u, -1);
    CHECK_FALSE(dgtest::hasVisiblePixel(render()));

    grid.setPadStates(1u, 0u, -1); // Pad 1 liegt unten links
    const auto lit = render();
    CHECK(dgtest::hasVisiblePixel(lit));
    // Links neben dem Raster, auf Höhe von Pad 1: Glow ragt über das Raster hinaus.
    const auto padBounds = grid.pad(0).getBounds();
    CHECK(lit.getPixelAt(ui::PadGrid::kGlowReach - 3, ui::PadGrid::kGlowReach + padBounds.getCentreY()).getAlpha() > 0);
}

TEST_CASE("sixteen glowing pads paint within one timer tick", "[look][perf]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::DgLookAndFeel lnf;
    ui::PadGrid grid(p);
    grid.setLookAndFeel(&lnf);
    grid.setSize(360, 384);

    juce::Image img(juce::Image::ARGB, 720, 768, true); // 2× Fenstergröße
    constexpr int frames = 60;
    const double start = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < frames; ++i)
    {
        grid.setPadStates(i % 8 < 4 ? 0x0f0fu : 0x00ffu, 0x00f0u, i % 16); // Trigger, Halten, Ausklingen im Wechsel
        juce::Graphics g(img);
        g.fillAll(ui::colours::background); // wie der Editor: jeder Frame beginnt mit dem Hintergrund
        g.addTransform(juce::AffineTransform::scale(2.0f));
        grid.paintGlows(g);
        grid.paintEntireComponent(g, false);
    }
    const double perFrame = (juce::Time::getMillisecondCounterHiRes() - start) / frames;
    WARN("pad frame at 2x: " << perFrame << " ms");
    CHECK(perFrame < 33.0); // ein Timer-Tick bei 30 Hz
    dgtest::savePng(img, "pads-2.00");
    grid.setLookAndFeel(nullptr);
}
