#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include "plugin/PluginEditor.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/Controls.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/PadColours.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/PadGlow.h"
#include "plugin/ui/PadGrid.h"
#include "plugin/ui/Surfaces.h"
#include "plugin/SampleFiles.h"
#include "engine/Kit.h"
#include "RenderTestHelpers.h"
#include "SampleTestHelpers.h"

using namespace dg;

namespace {
bool isMono(const juce::Font& f)
{
    const juce::Font reference(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    return f.getTypefacePtr() != nullptr && reference.getTypefacePtr() != nullptr
        && f.getTypefacePtr()->getName() == reference.getTypefacePtr()->getName();
}
} // namespace

TEST_CASE("fonts are monospace", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    CHECK(isMono(juce::Font(ui::font(12.0f))));
    CHECK(isMono(juce::Font(ui::font(12.0f, true))));
    CHECK(juce::Font(ui::font(12.0f)).getHeight() == 12.0f);

    juce::Label label;
    label.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    const auto labelFont = lnf.getLabelFont(label);
    CHECK(isMono(labelFont));
    CHECK(labelFont.getHeight() == 14.0f);

    juce::TextButton button("x");
    CHECK(isMono(lnf.getTextButtonFont(button, 24)));
    juce::ComboBox box;
    box.setSize(100, 24);
    CHECK(isMono(lnf.getComboBoxFont(box)));
    CHECK(isMono(lnf.getPopupMenuFont()));
    CHECK(isMono(lnf.getAlertWindowTitleFont()));
    CHECK(isMono(lnf.getAlertWindowMessageFont()));
    CHECK(isMono(lnf.getAlertWindowFont()));
}

TEST_CASE("the monospace font keeps a label's letter spacing", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Label label;
    label.setFont(juce::Font(ui::font(22.0f, true).withKerningFactor(0.12f)));
    CHECK(lnf.getLabelFont(label).getExtraKerningFactor() == 0.12f);
}

TEST_CASE("a panel body is flat with a 1 px edge", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Image img(juce::Image::ARGB, 100, 60, true);
    {
        juce::Graphics g(img);
        ui::drawPanelBody(g, img.getBounds().toFloat());
    }
    CHECK(img.getPixelAt(50, 8) == img.getPixelAt(50, 52));
    CHECK(img.getPixelAt(50, 30) == ui::colours::strip);
    CHECK(img.getPixelAt(0, 30) == ui::colours::edge);
    CHECK(img.getPixelAt(50, 0) == ui::colours::edge);
}

TEST_CASE("the window background is one flat colour", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Image img(juce::Image::ARGB, 40, 60, true);
    {
        juce::Graphics g(img);
        ui::drawWindowBackground(g, img.getBounds());
    }
    CHECK(img.getPixelAt(5, 2) == ui::colours::window);
    CHECK(img.getPixelAt(5, 57) == ui::colours::window);
}

TEST_CASE("the palette has the flat panel values", "[look]")
{
    CHECK(ui::colours::window == juce::Colour(0xff14161a));
    CHECK(ui::colours::lcd == juce::Colour(0xff0b0d0f));
    CHECK(ui::colours::amber == juce::Colour(0xffffb347));
    CHECK(ui::colours::accent == juce::Colour(0xff5aa9e6));
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
        CHECK(img.getWidth() == juce::roundToInt(1200.0f * scale));
        // Punkt unten rechts im Slot-Editor-Panel (keine Controls) gegen den Fensterrand.
        const auto panel = img.getPixelAt(juce::roundToInt(1170.0f * scale), juce::roundToInt(420.0f * scale));
        const auto window = img.getPixelAt(juce::roundToInt(4.0f * scale), juce::roundToInt(4.0f * scale));
        CHECK(panel != window);
        dgtest::savePng(img, "editor-" + juce::String(scale, 2));
    }
}

TEST_CASE("the live view paints at every window scale", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const float scale : { 0.75f, 1.0f, 2.0f })
    {
        DubgefahrenProcessor p;
        p.setUiScale(scale);
        p.setLiveView(true);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* e = static_cast<DubgefahrenEditor*>(editor.get());
        const auto img = dgtest::snapshot(*editor);
        REQUIRE(img.isValid());
        CHECK(img.getWidth() == juce::roundToInt(880.0f * scale));
        CHECK(img.getHeight() == juce::roundToInt(460.0f * scale));
        const auto centre = e->livePanel().getBounds().getCentre();
        const auto panel = img.getPixelAt(juce::roundToInt(centre.x * scale), juce::roundToInt(centre.y * scale));
        const auto window = img.getPixelAt(juce::roundToInt(4.0f * scale), juce::roundToInt(4.0f * scale));
        CHECK(panel != window);
        dgtest::savePng(img, "editor-live-" + juce::String(scale, 2));
    }
}

TEST_CASE("the edit view without the advanced strip paints at its base size", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.setUiScale(1.0f);
    p.setAdvancedOpen(false);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    const auto img = dgtest::snapshot(*editor);
    REQUIRE(img.isValid());
    CHECK(img.getWidth() == 1200);
    CHECK(img.getHeight() == 640);
    dgtest::savePng(img, "editor-edit-1.00");
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
        g.fillAll(ui::colours::window); // wie der Editor: jeder Frame beginnt mit dem Hintergrund
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

namespace {
juce::Image drawKnob(ui::DgLookAndFeel& lnf, juce::Slider& slider, int size, float pos)
{
    juce::Image img(juce::Image::ARGB, std::max(1, size), std::max(1, size), true);
    juce::Graphics g(img);
    lnf.drawRotarySlider(g, 0, 0, size, size, pos, juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, slider);
    return img;
}

// Akzentgelb: viel Rot, mittleres Grün, wenig Blau.
bool isAccent(juce::Colour c) { return c.getAlpha() > 200 && c.getRed() > 200 && c.getGreen() > 140 && c.getBlue() < 110; }

int countAccent(const juce::Image& img, int maxX)
{
    int n = 0;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < std::min(maxX, img.getWidth()); ++x)
            n += isAccent(img.getPixelAt(x, y)) ? 1 : 0;
    return n;
}
} // namespace

TEST_CASE("controls draw nothing harmful into tiny or empty areas", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Slider slider;
    slider.setRange(0.0, 1.0);
    for (const int size : { 0, 1, 3, 8, 11 })
        CHECK_NOTHROW(drawKnob(lnf, slider, size, 0.5f));
    CHECK_FALSE(dgtest::hasVisiblePixel(drawKnob(lnf, slider, 3, 0.5f)));

    juce::Image img(juce::Image::ARGB, 8, 8, true);
    juce::Graphics g(img);
    juce::TextButton button("x");
    button.setSize(0, 0);
    CHECK_NOTHROW(lnf.drawButtonBackground(g, button, juce::Colours::grey, false, false));
    juce::ToggleButton toggle("x");
    toggle.setSize(0, 0);
    CHECK_NOTHROW(lnf.drawToggleButton(g, toggle, false, false));
    juce::ComboBox box;
    CHECK_NOTHROW(lnf.drawComboBox(g, 0, 0, false, 0, 0, 0, 0, box));
    CHECK_FALSE(dgtest::hasVisiblePixel(img));
}

TEST_CASE("a knob shows its value arc in the accent colour and dims when disabled", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Slider slider;
    slider.setRange(0.0, 1.0);

    const auto low = drawKnob(lnf, slider, 64, 0.1f);
    const auto high = drawKnob(lnf, slider, 64, 0.9f);
    CHECK(countAccent(high, 64) > countAccent(low, 64));
    CHECK(countAccent(low, 64) > 0);

    slider.setEnabled(false);
    CHECK(countAccent(drawKnob(lnf, slider, 64, 0.9f), 64) == 0);
    dgtest::savePng(high, "knob-64");
}

TEST_CASE("a knob has a body in its centre", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Slider slider;
    slider.setRange(0.0, 1.0);
    // Zeiger zeigt bei 0.5 nach oben; links und rechts der Mitte liegt der Körper.
    const auto img = drawKnob(lnf, slider, 64, 0.5f);
    CHECK(img.getPixelAt(24, 34).getAlpha() == 255);
    CHECK(img.getPixelAt(40, 34).getAlpha() == 255);
}

TEST_CASE("a bipolar knob fills from its centre", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Slider slider;
    slider.setRange(-1.0, 1.0);
    // In der Mitte ist der Wertebogen leer; voll rechts läuft er nur durch die rechte Hälfte.
    CHECK(countAccent(drawKnob(lnf, slider, 64, 0.5f), 64) == 0);
    const auto right = drawKnob(lnf, slider, 64, 1.0f);
    CHECK(countAccent(right, 28) == 0);
    CHECK(countAccent(right, 64) > 0);
}

TEST_CASE("a toggle shows a lamp instead of a tick box", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::ToggleButton toggle("Sync");
    toggle.setLookAndFeel(&lnf);
    toggle.setSize(100, 24);

    const auto lamp = [&toggle] {
        const auto img = dgtest::snapshot(toggle);
        return img.getPixelAt(9, 12); // Mitte der Lampe
    };
    toggle.setToggleState(false, juce::dontSendNotification);
    const auto off = lamp();
    toggle.setToggleState(true, juce::dontSendNotification);
    const auto on = lamp();
    CHECK(on.getBrightness() > off.getBrightness() + 0.3f);
    CHECK(isAccent(on));
    CHECK(off.getAlpha() == 255); // ausgeschaltet: dunkel gefüllte Lampe statt leerem Kästchen
    toggle.setLookAndFeel(nullptr);
}

TEST_CASE("buttons and combo boxes are flat with a light edge and darker when down", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::TextButton button("Rename");
    button.setSize(110, 28);

    const auto render = [&](bool down) {
        juce::Image img(juce::Image::ARGB, 110, 28, true);
        juce::Graphics g(img);
        lnf.drawButtonBackground(g, button, ui::colours::button, false, down);
        return img;
    };
    const auto up = render(false);
    const auto down = render(true);
    CHECK(up.getPixelAt(55, 5) == up.getPixelAt(55, 22)); // kein Verlauf
    CHECK(up.getPixelAt(55, 14) == ui::colours::button);
    CHECK(up.getPixelAt(55, 0) == ui::colours::edgeLight); // 1-px-Kante
    CHECK(down.getPixelAt(55, 14).getBrightness() < up.getPixelAt(55, 14).getBrightness());

    juce::ComboBox box;
    box.setLookAndFeel(&lnf);
    box.setSize(100, 24);
    juce::Image img(juce::Image::ARGB, 100, 24, true);
    {
        juce::Graphics g(img);
        lnf.drawComboBox(g, 100, 24, false, 0, 0, 0, 0, box);
    }
    CHECK(img.getPixelAt(30, 5) == img.getPixelAt(30, 19));
    CHECK(img.getPixelAt(30, 0) == ui::colours::edge);
    box.setLookAndFeel(nullptr);
}

TEST_CASE("a label set to the bold monospace cut stays monospace", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Label label;
    label.setFont(juce::Font(ui::font(22.0f, true)));
    CHECK(isMono(lnf.getLabelFont(label)));
    CHECK(lnf.getLabelFont(label).isBold());
    label.setFont(juce::Font(ui::font(22.0f)));
    CHECK_FALSE(lnf.getLabelFont(label).isBold());
}

namespace {
bool usesDgLook(juce::Component& c) { return dynamic_cast<ui::DgLookAndFeel*>(&c.getLookAndFeel()) != nullptr; }
} // namespace

TEST_CASE("message dialogs use the editor's look and close with the editor", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    CHECK(e->topDialog() == nullptr);

    // Eine Datei als "Ordner": Speichern schlägt fehl und meldet das per Dialog.
    const auto blocker = juce::File::createTempFile("dgblock");
    REQUIRE(blocker.create().wasOk());
    CHECK_FALSE(e->exportKitTo(blocker.getChildFile("kit.dgkit")));

    juce::Component::SafePointer<juce::AlertWindow> dialog(e->topDialog());
    REQUIRE(dialog != nullptr);
    CHECK(usesDgLook(*dialog));
    editor.reset();
    CHECK(dialog == nullptr); // der Dialog nutzt das Look-and-Feel des Editors und darf ihn nicht überleben
    blocker.deleteFile();
}

TEST_CASE("popup menus use the editor's look", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    auto& desktop = juce::Desktop::getInstance();
    juce::Array<juce::Component*> before;
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        before.add(desktop.getComponent(i));

    e->showPadMenu(0);
    // Das Menü öffnet eigene Fenster (Menü plus Schatten): mindestens eines davon trägt den Look des Editors.
    int opened = 0, styled = 0;
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        if (auto* c = desktop.getComponent(i); !before.contains(c))
        {
            ++opened;
            styled += usesDgLook(*c) ? 1 : 0;
        }
    CHECK(opened > 0);
    CHECK(styled > 0);
    juce::PopupMenu::dismissAllActiveMenus();
}

TEST_CASE("a latched pad shows the light dot with a dark ring in the editor", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.prepareToPlay(48000.0, 512);
    auto* mode = p.state().getParameter(slotParamId(0, SlotField::TrigMode));
    mode->setValueNotifyingHost(mode->convertTo0to1(1.0f)); // Latch
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    buf.clear();
    p.processBlock(buf, on);
    REQUIRE(p.latchedMask() == 1u);

    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    e->selectSlot(1); // überträgt die Pad-Zustände des Processors auf die Pads
    const auto img = dgtest::snapshot(*editor);
    dgtest::savePng(img, "editor-latched");
    // Mitte des Punkts oben rechts auf Pad 1 (unten links im Raster).
    const auto dot = img.getPixelAt(87, 333);
    CHECK(dot.getRed() > 220);
    CHECK(dot.getGreen() > 220);
    CHECK(dot.getBlue() > 220);
}

namespace {
juce::Label* valueBox(juce::Slider& slider)
{
    for (auto* child : slider.getChildren())
        if (auto* label = dynamic_cast<juce::Label*>(child))
            return label;
    return nullptr;
}
} // namespace

TEST_CASE("a knob's value field has no frame and no box", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    // Wie im Editor: Der Elternteil hat das Look-and-Feel schon, der Regler kommt später dazu.
    ui::DgLookAndFeel lnf;
    juce::Component parent;
    parent.setLookAndFeel(&lnf);
    ui::Knob knob("Pitch");
    parent.addAndMakeVisible(knob);
    auto* box = valueBox(knob.slider);
    REQUIRE(box != nullptr);
    CHECK(box->findColour(juce::Label::outlineColourId).isTransparent());
    CHECK(box->findColour(juce::Label::backgroundColourId).isTransparent());
    CHECK(box->findColour(juce::Label::textColourId) == ui::colours::text);
    parent.removeChildComponent(&knob);
    parent.setLookAndFeel(nullptr);
}

TEST_CASE("a knob shows its value with at most two decimals", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::Knob pitch("Pitch");
    pitch.attach(p.state(), slotParamId(0, SlotField::Pitch));
    CHECK(pitch.slider.getTextFromValue(600.00006) == "600.00");
    CHECK(pitch.slider.getTextFromValue(123.456) == "123.46");
    CHECK(pitch.slider.getTextFromValue(3999.9993) == "4000.00");
    auto* box = valueBox(pitch.slider);
    REQUIRE(box != nullptr);
    CHECK(box->getText() == pitch.slider.getTextFromValue(pitch.slider.getValue()));
    CHECK(box->getText().fromFirstOccurrenceOf(".", false, false).length() == 2);

    ui::Knob volume("Volume");
    volume.attach(p.state(), slotParamId(0, SlotField::Volume));
    CHECK(volume.slider.getTextFromValue(-9.0) == "-9.00");

    ui::Knob pan("Pan");
    pan.attach(p.state(), slotParamId(0, SlotField::Pan));
    CHECK(pan.slider.getTextFromValue(-0.001) == "0.00"); // kein "-0.00"
    CHECK(pan.slider.getTextFromValue(-0.25) == "-0.25");

    // Neu verbinden (Slot-Wechsel) behält die Darstellung.
    pitch.attach(p.state(), slotParamId(1, SlotField::Pitch));
    CHECK(pitch.slider.getTextFromValue(123.456) == "123.46");
    // Eingetippte Werte werden weiter verstanden.
    CHECK(std::abs(pitch.slider.getValueFromText("440.5") - 440.5) < 0.01);
}

TEST_CASE("time knobs show three decimals, everything else two", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::Knob knob("x");
    const auto text = [&](SlotField field, double value) {
        knob.attach(p.state(), slotParamId(0, field));
        return knob.slider.getTextFromValue(value);
    };
    // Zeiten (Einheit s)
    CHECK(text(SlotField::Attack, 0.005) == "0.005");
    CHECK(text(SlotField::Release, 0.4) == "0.400");
    CHECK(text(SlotField::SweepTime, 0.4999999) == "0.500");
    CHECK(text(SlotField::OneShotLength, 1.0) == "1.000");
    CHECK(text(SlotField::Attack, 0.0) == "0.000");
    // Alles andere
    CHECK(text(SlotField::Pitch, 600.00006) == "600.00");
    CHECK(text(SlotField::LfoRate, 3.9999993) == "4.00");
    CHECK(text(SlotField::Volume, -9.0) == "-9.00");
}

TEST_CASE("the editor paints a sample slot at every window scale", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    const auto kitFile = tmp.dir.getChildFile("Dub.dgkit");
    sampleFolderFor(kitFile).createDirectory();
    juce::WavAudioFormat wav;
    dgtest::writeSine(wav, sampleFolderFor(kitFile).getChildFile("horn.wav"), 441.0f, 48000.0, 4800);

    for (const float scale : { 0.75f, 1.0f, 2.0f })
    {
        DubgefahrenProcessor p;
        p.applyKit(makeFactoryKit(), kitFile);
        p.setSlotSample(2, "horn.wav");
        p.setSlotSample(3, "gone.wav");
        p.waitForSampleLoads();
        SlotParams sp = readSlotFromParameters(p.state(), 2);
        sp.sampleStart = 0.1f;
        sp.loopStart = 0.4f;
        sp.sampleEnd = 0.8f;
        sp.loop = true;
        p.setSlot(2, sp, "horn", "horn.wav");
        p.waitForSampleLoads();
        p.setUiScale(scale);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* e = static_cast<DubgefahrenEditor*>(editor.get());

        e->selectSlot(2);
        const auto img = dgtest::snapshot(*editor);
        REQUIRE(img.isValid());
        dgtest::savePng(img, "editor-sample-" + juce::String(scale, 2));

        e->selectSlot(3); // fehlendes Sample: Hinweis statt Kurve
        const auto missing = dgtest::snapshot(*editor);
        REQUIRE(missing.isValid());
        dgtest::savePng(missing, "editor-sample-missing-" + juce::String(scale, 2));
    }
}

TEST_CASE("pads take their colour from the slot", "[look][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    p.setSlotColour(2, 2);
    grid.refreshNames();
    CHECK(grid.padBaseColour(2) == ui::padColour(2).base);
    CHECK(grid.padLitTextColour(2) == ui::padColour(2).litText);
    CHECK(grid.padBaseColour(0) == ui::padColour(0).base);
}

TEST_CASE("the editor shows a colour change on the right pad", "[look][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    e->selectSlot(0);
    p.setSlotColour(7, 5);
    e->pollProcessorState();
    CHECK(e->padGrid().padBaseColour(7) == ui::padColour(5).base);
    CHECK(e->padGrid().padBaseColour(0) == ui::padColour(0).base);
}

TEST_CASE("all pad colours render dimmed and lit with the latch dot", "[look][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    for (int s = 0; s < kNumPadColours; ++s)
        p.setSlotColour(s, s);
    ui::PadGrid grid(p);
    grid.setSize(360, 384);
    grid.refreshNames();
    grid.setPadStates(0u, 0xffu, -1); // alle acht gelatcht, keiner spielt
    dgtest::savePng(dgtest::snapshot(grid), "pad-colours-dimmed");
    grid.setPadStates(0xffu, 0xffu, -1); // alle acht spielen
    const auto lit = dgtest::snapshot(grid);
    dgtest::savePng(lit, "pad-colours-lit");
    CHECK(dgtest::hasVisiblePixel(lit));
}
