#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <vector>
#include "engine/Kit.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/SampleFiles.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/WaveformView.h"
#include "RenderTestHelpers.h"
#include "SampleTestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;
using Marker = ui::WaveformView::Marker;

namespace {
// Processor mit einem geladenen Sample (4800 Samples) in Slot 3.
struct Fixture
{
    dgtest::TempDir tmp;
    juce::File kitFile = tmp.dir.getChildFile("Dub.dgkit");
    DubgefahrenProcessor p;
    Fixture()
    {
        sampleFolderFor(kitFile).createDirectory();
        juce::WavAudioFormat wav;
        dgtest::writeSine(wav, sampleFolderFor(kitFile).getChildFile("horn.wav"), 441.0f, 48000.0, 4800);
        p.applyKit(makeFactoryKit(), kitFile);
        p.setSlotSample(2, "horn.wav");
        p.waitForSampleLoads();
    }
    float value(SlotField f) { return p.state().getRawParameterValue(slotParamId(2, f))->load(); }
    void set(SlotField f, float v)
    {
        auto* param = p.state().getParameter(slotParamId(2, f));
        param->setValueNotifyingHost(param->convertTo0to1(v));
    }
};

struct GestureCounter final : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0;
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override { starting ? ++begins : ++ends; }
};
} // namespace

TEST_CASE("peaks for empty, tiny and long data", "[waveform]")
{
    CHECK(ui::computePeaks({}, 100).empty());
    CHECK(ui::computePeaks({ 0.5f }, 0).empty());
    CHECK(ui::computePeaks({ 0.5f }, -3).empty());

    const auto one = ui::computePeaks({ 0.5f }, 4); // weniger Samples als Spalten
    REQUIRE(one.size() == 4u);
    for (const auto& c : one)
    {
        CHECK(c.min == 0.5f);
        CHECK(c.max == 0.5f);
    }

    std::vector<float> data(1000, 0.0f);
    data[10] = 0.8f;
    data[20] = -0.6f;
    data[990] = 0.3f;
    const auto peaks = ui::computePeaks(data, 10); // 100 Samples pro Spalte
    REQUIRE(peaks.size() == 10u);
    CHECK(peaks[0].max == 0.8f);
    CHECK(peaks[0].min == -0.6f);
    CHECK(peaks[5].max == 0.0f);
    CHECK(peaks[9].max == 0.3f);

    const auto bad = ui::computePeaks({ std::numeric_limits<float>::quiet_NaN(), 5.0f, -5.0f }, 1);
    REQUIRE(bad.size() == 1u);
    CHECK(bad[0].max == 1.0f); // begrenzt auf -1..1, NaN ignoriert
    CHECK(bad[0].min == -1.0f);
}

TEST_CASE("the waveform shows start and end markers, the loop marker only with loop on", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    REQUIRE(view.hasData());
    CHECK(view.hintText().isEmpty());
    CHECK(view.isMarkerVisible(Marker::Start));
    CHECK(view.isMarkerVisible(Marker::End));
    CHECK_FALSE(view.isMarkerVisible(Marker::Loop));
    CHECK(view.markerX(Marker::Start) == 0.0f);
    CHECK(view.markerX(Marker::End) == 580.0f);

    f.set(SlotField::Loop, 1.0f);
    f.set(SlotField::LoopStart, 0.5f);
    CHECK(view.isMarkerVisible(Marker::Loop));
    CHECK_THAT(view.markerX(Marker::Loop), WithinAbs(290.0, 0.5));
}

TEST_CASE("dragging a marker writes its parameter as one gesture", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    GestureCounter counter;
    auto* end = f.p.state().getParameter(slotParamId(2, SlotField::SampleEnd));
    end->addListener(&counter);

    view.pressAt(578.0f);
    view.dragTo(400.0f);
    view.dragTo(290.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::SampleEnd), WithinAbs(0.5, 0.002));
    CHECK(counter.begins == 1);
    CHECK(counter.ends == 1);
    CHECK_THAT(view.markerX(Marker::End), WithinAbs(290.0, 0.5));

    view.pressAt(100.0f); // kein Marker in der Nähe
    view.dragTo(50.0f);
    view.release();
    CHECK(counter.begins == 1);
    CHECK_THAT(f.value(SlotField::SampleEnd), WithinAbs(0.5, 0.002));
    end->removeListener(&counter);
}

TEST_CASE("markers stop at their neighbours", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    f.set(SlotField::SampleEnd, 0.5f); // 2400 Samples, x = 290
    const float minFrac = static_cast<float>(kMinRegionSamples) / 4800.0f;

    // Start bleibt die Mindestlänge vor End stehen.
    view.pressAt(1.0f);
    view.dragTo(560.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::SampleStart), WithinAbs(0.5 - minFrac, 1e-4));

    // End bleibt die Mindestlänge hinter Start stehen.
    view.pressAt(view.markerX(Marker::End));
    view.dragTo(0.0f);
    view.release();
    CHECK(f.value(SlotField::SampleEnd) >= f.value(SlotField::SampleStart) + minFrac - 1e-4f);

    // Mit Loop begrenzt L beide Nachbarn und bleibt selbst zwischen ihnen.
    f.set(SlotField::SampleStart, 0.2f);
    f.set(SlotField::SampleEnd, 0.8f);
    f.set(SlotField::Loop, 1.0f);
    f.set(SlotField::LoopStart, 0.5f);
    view.pressAt(view.markerX(Marker::Start));
    view.dragTo(500.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::SampleStart), WithinAbs(0.5, 1e-3));
    view.pressAt(view.markerX(Marker::End));
    view.dragTo(10.0f);
    view.release();
    CHECK(f.value(SlotField::SampleEnd) >= 0.5f - 1e-3f);
    f.set(SlotField::SampleStart, 0.2f);
    f.set(SlotField::SampleEnd, 0.8f);
    view.pressAt(view.markerX(Marker::Loop));
    view.dragTo(580.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::LoopStart), WithinAbs(0.8, 1e-3));
    view.pressAt(view.markerX(Marker::Loop));
    view.dragTo(0.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::LoopStart), WithinAbs(0.2, 1e-3));
}

TEST_CASE("with overlapping handles the drag direction picks the marker that can move", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    f.set(SlotField::Loop, 1.0f); // S und L liegen beide bei 0

    view.pressAt(2.0f);
    view.dragTo(200.0f); // nach rechts: L kann, S müsste L überholen
    view.release();
    CHECK(f.value(SlotField::LoopStart) > 0.3f);
    CHECK(f.value(SlotField::SampleStart) == 0.0f);

    // L auf E: nach links kann L, nach rechts keiner.
    f.set(SlotField::LoopStart, 1.0f);
    view.pressAt(579.0f);
    view.dragTo(300.0f);
    view.release();
    CHECK(f.value(SlotField::LoopStart) < 0.6f);
    CHECK(f.value(SlotField::SampleEnd) == 1.0f);
}

TEST_CASE("hidden loop marker does not block start and end", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    f.set(SlotField::LoopStart, 0.1f); // Loop ist aus: L unsichtbar
    view.pressAt(1.0f);
    view.dragTo(290.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::SampleStart), WithinAbs(0.5, 0.002));
    CHECK_THAT(f.value(SlotField::LoopStart), WithinAbs(0.1, 1e-4)); // unangetastet
}

TEST_CASE("without data nothing can be dragged", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    f.p.setSlotSample(4, "gone.wav");
    f.p.waitForSampleLoads();
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(4);
    CHECK_FALSE(view.hasData());
    CHECK(view.hintText() == "Sample missing");
    CHECK_FALSE(view.isMarkerVisible(Marker::Start));
    const float before = f.p.state().getRawParameterValue(slotParamId(4, SlotField::SampleEnd))->load();
    view.pressAt(578.0f);
    view.dragTo(100.0f);
    view.release();
    CHECK(f.p.state().getRawParameterValue(slotParamId(4, SlotField::SampleEnd))->load() == before);

    view.setSlot(0); // Synth-Slot
    CHECK(view.hintText() == "No sample loaded");

    ui::WaveformView empty(f.p); // Breite 0, kein Slot
    CHECK_NOTHROW(empty.pressAt(0.0f));
    CHECK_NOTHROW(empty.dragTo(10.0f));
    CHECK_NOTHROW(empty.release());
    CHECK_NOTHROW(dgtest::snapshot(view));
}

TEST_CASE("switching the slot ends an open drag", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    GestureCounter counter;
    auto* end = f.p.state().getParameter(slotParamId(2, SlotField::SampleEnd));
    end->addListener(&counter);
    view.pressAt(578.0f);
    view.dragTo(400.0f);
    REQUIRE(counter.begins == 1);
    view.setSlot(5);
    CHECK(counter.ends == 1);
    const float value = f.value(SlotField::SampleEnd);
    view.dragTo(100.0f); // nach dem Wechsel wirkungslos
    view.release();
    CHECK(f.value(SlotField::SampleEnd) == value);
    CHECK(counter.ends == 1);
    end->removeListener(&counter);
}

TEST_CASE("the waveform draws the curve, the dimmed outside and the loop tint", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::DgLookAndFeel lnf;
    ui::WaveformView view(f.p);
    view.setLookAndFeel(&lnf);
    view.setSize(580, 76);
    view.setSlot(2);
    f.set(SlotField::SampleStart, 0.25f);
    f.set(SlotField::SampleEnd, 0.75f);

    const auto plain = dgtest::snapshot(view);
    // Links vom Start abgedunkelt: dort ist die Kurve dunkler als im Bereich.
    const auto brightest = [&](const juce::Image& img, int x0, int x1) {
        float b = 0.0f;
        for (int x = x0; x < x1; ++x)
            for (int y = 16; y < 76; ++y)
                b = std::max(b, img.getPixelAt(x, y).getBrightness());
        return b;
    };
    CHECK(brightest(plain, 200, 380) > brightest(plain, 20, 120) + 0.2f);

    f.set(SlotField::Loop, 1.0f);
    f.set(SlotField::LoopStart, 0.5f);
    const auto looped = dgtest::snapshot(view);
    // Blaue Tönung zwischen L (290) und E (435), nicht zwischen S (145) und L.
    // Zeile 66: unterhalb der Kurve (Sinus mit halber Aussteuerung) und außerhalb des oberen Innenschattens.
    const auto blueAt = [&](const juce::Image& img, int x) { return static_cast<int>(img.getPixelAt(x, 66).getBlue()); };
    CHECK(blueAt(looped, 360) > blueAt(plain, 360) + 10);
    CHECK(std::abs(blueAt(looped, 220) - blueAt(plain, 220)) < 4);

    f.set(SlotField::Reverse, 1.0f); // gespiegelt: Tönung zwischen S und L
    const auto reversed = dgtest::snapshot(view);
    CHECK(blueAt(reversed, 220) > blueAt(plain, 220) + 10);
    CHECK(std::abs(blueAt(reversed, 360) - blueAt(plain, 360)) < 4);
    dgtest::savePng(looped, "waveform-loop");
    view.setLookAndFeel(nullptr);
}
