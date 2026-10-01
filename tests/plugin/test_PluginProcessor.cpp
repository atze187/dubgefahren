#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <array>
#include <cmath>
#include <set>
#include "engine/Kit.h"
#include "engine/SlotFields.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
bool approxEqual(const SlotParams& a, const SlotParams& b)
{
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        const float va = getSlotField(a, f);
        const float vb = getSlotField(b, f);
        if (std::abs(va - vb) > 1.0e-3f * std::max(1.0f, std::abs(va)))
            return false;
    }
    return true;
}

void setParam(DubgefahrenProcessor& p, const juce::String& id, float realValue)
{
    auto* param = p.state().getParameter(id);
    REQUIRE(param != nullptr);
    param->setValueNotifyingHost(param->convertTo0to1(realValue));
}

void processBlocks(DubgefahrenProcessor& p, int blocks, juce::MidiBuffer firstMidi = {})
{
    juce::AudioBuffer<float> buf(2, 512);
    for (int i = 0; i < blocks; ++i)
    {
        buf.clear();
        juce::MidiBuffer midi;
        if (i == 0)
            midi = firstMidi;
        p.processBlock(buf, midi);
    }
}

void prepare(DubgefahrenProcessor& p)
{
    p.setPlayConfigDetails(0, 2, 48000.0, 512);
    p.prepareToPlay(48000.0, 512);
}
} // namespace

TEST_CASE("the plugin exposes 433 uniquely named parameters", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::set<juce::String> ids;
    for (auto* param : p.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            ids.insert(withId->paramID);
    CHECK(p.getParameters().size() == 16 * 26 + 17);
    CHECK(ids.size() == 433);
    CHECK(slotParamId(0, SlotField::Wave) == "s01_wave");
    CHECK(slotParamId(15, SlotField::FxSend) == "s16_send");
    CHECK(slotParamId(0, SlotField::Tune) == "s01_tune");
    CHECK(slotParamId(0, SlotField::SampleStart) == "s01_smpStart");
    CHECK(slotParamId(15, SlotField::LoopXfade) == "s16_xfade");
    CHECK(slotSourceParamId(15) == "s16_source");
    CHECK_FALSE(p.state().getParameter("s01_source")->isAutomatable());
    CHECK(p.state().getParameter("s01_tune")->isAutomatable());
}

TEST_CASE("default program is named for VST3 hosts and validators", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    CHECK(p.getProgramName(0) == "Default");
    CHECK(p.getNumPrograms() == 1);
}

TEST_CASE("slot parameters and names default to the factory kit", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    const Kit k = makeFactoryKit();
    for (int s = 0; s < kNumSlots; ++s)
    {
        CHECK(approxEqual(readSlotFromParameters(p.state(), s), k.slots[static_cast<std::size_t>(s)]));
        CHECK(p.slotName(s) == juce::String::fromUTF8(k.names[static_cast<std::size_t>(s)].c_str()));
        CHECK(p.slotSource(s) == SourceType::Synth);
    }
}

TEST_CASE("state round-trip keeps parameters, names with umlauts and UI settings", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    a.state().getParameter("s03_pitch")->setValueNotifyingHost(0.25f);
    a.state().getParameter(pid::delayMix)->setValueNotifyingHost(0.8f);
    a.setSlotName(2, juce::String::fromUTF8("Größe äöü"));
    a.setUiScale(1.5f);
    a.setEditorFollowsFocus(false);

    juce::MemoryBlock mb;
    a.getStateInformation(mb);

    DubgefahrenProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    CHECK_THAT(b.state().getParameter("s03_pitch")->getValue(), WithinAbs(0.25, 1e-4));
    CHECK_THAT(b.state().getParameter(pid::delayMix)->getValue(), WithinAbs(0.8, 1e-4));
    CHECK(b.slotName(2) == juce::String::fromUTF8("Größe äöü"));
    CHECK(b.slotName(0) == "Classic");
    CHECK_THAT(b.uiScale(), WithinAbs(1.5, 1e-6));
    CHECK_FALSE(b.editorFollowsFocus());
}

TEST_CASE("invalid state data is ignored", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.setStateInformation("garbage", 7);
    CHECK(p.slotName(0) == "Classic");
    CHECK(approxEqual(readSlotFromParameters(p.state(), 0), makeFactoryKit().slots[0]));
}

TEST_CASE("applyKit and currentKit round-trip", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    Kit k = makeFactoryKit();
    k.slots[0].pitchHz = 1234.0f;
    k.slots[7].trigMode = TriggerMode::OneShot;
    k.names[0] = "Neu";
    p.applyKit(k);
    const Kit c = p.currentKit();
    CHECK(approxEqual(c.slots[0], k.slots[0]));
    CHECK(c.slots[7].trigMode == TriggerMode::OneShot);
    CHECK(c.names[0] == "Neu");
}

TEST_CASE("processBlock plays note 36 and treats velocity 0 as note off", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    prepare(p);

    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    p.processBlock(buf, on);
    CHECK(buf.getMagnitude(0, 0, 512) > 0.0f);
    CHECK(p.activeMask() == 1u);

    juce::MidiBuffer off;
    off.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(0)), 0);
    processBlocks(p, 60, off); // Classic: Release 0,4 s
    CHECK(p.activeMask() == 0u);
}

TEST_CASE("non-note MIDI and out-of-range notes are ignored", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    prepare(p);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 0);
    std::array<juce::uint8, 20> sysExData {};
    midi.addEvent(juce::MidiMessage::createSysExMessage(sysExData.data(), static_cast<int>(sysExData.size())), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);

    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    p.processBlock(buf, midi);
    CHECK(p.activeMask() == 0u);
    CHECK(buf.getMagnitude(0, 0, 512) == 0.0f);

    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    buf.clear();
    p.processBlock(buf, on);
    CHECK(p.activeMask() == 1u);
}

TEST_CASE("panic parameter stops latched voices on its rising edge", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    prepare(p);
    setParam(p, slotParamId(0, SlotField::TrigMode), 1.0f); // Latch

    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    processBlocks(p, 1, on);
    CHECK(p.latchedMask() == 1u);

    setParam(p, pid::panic, 1.0f);
    processBlocks(p, 2);
    CHECK(p.activeMask() == 0u);
    CHECK(p.latchedMask() == 0u);
    setParam(p, pid::panic, 0.0f);
}

TEST_CASE("UI preview starts a slot and moves the focus", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    prepare(p);
    p.previewPress(4);
    processBlocks(p, 1);
    CHECK((p.activeMask() & (1u << 4)) != 0u);
    CHECK(p.focusSlot() == 4);
    p.previewRelease(4);
    processBlocks(p, 1);
}

TEST_CASE("preview release is never dropped", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    prepare(p);

    // Slot 0 ("Classic") gaten und halten, dann die FIFO mit Presses auf Slot 5
    // überfüllen, damit ein per FIFO gepushtes PreviewOff für Slot 0 verworfen würde.
    p.previewPress(0);
    for (int i = 0; i < 200; ++i)
        p.previewPress(5);
    p.previewRelease(0);

    processBlocks(p, 60);
    CHECK((p.activeMask() & 1u) == 0u);
}

TEST_CASE("cpu load is zero before processing and a sane proportion afterwards", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    CHECK(p.cpuLoad() == 0.0);

    prepare(p);
    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    processBlocks(p, 20, on);

    const double load = p.cpuLoad();
    CHECK(std::isfinite(load));
    CHECK(load > 0.0);
    CHECK(load < 1.0);
}

TEST_CASE("clearSlot empties a slot and resetSlotToFactory restores its siren", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    setParam(p, "s04_pitch", 1000.0f);
    p.clearSlot(3);
    CHECK(p.slotSource(3) == SourceType::Empty);
    CHECK(p.slotName(3).isEmpty());
    CHECK_THAT(readSlotFromParameters(p.state(), 3).pitchHz, WithinAbs(1000.0, 1.0)); // Synth-Werte bleiben
    CHECK(p.slotSource(2) == SourceType::Synth);

    p.resetSlotToFactory(3);
    CHECK(p.slotSource(3) == SourceType::Synth);
    CHECK(p.slotName(3) == "Laser");
    CHECK(approxEqual(readSlotFromParameters(p.state(), 3), makeFactoryKit().slots[3]));
}

TEST_CASE("setSlot copies the source, so pasting an empty slot empties the target", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.clearSlot(0);
    const auto copied = p.currentKit().slots[0];
    p.setSlot(5, copied, {});
    CHECK(p.slotSource(5) == SourceType::Empty);
}

TEST_CASE("empty slots survive a state round-trip", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    a.clearSlot(5);
    juce::MemoryBlock mb;
    a.getStateInformation(mb);

    DubgefahrenProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    CHECK(b.slotSource(5) == SourceType::Empty);
    CHECK(b.slotSource(0) == SourceType::Synth);
}

TEST_CASE("a state without source parameters loads as all synth even over empty slots", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    juce::MemoryBlock mb;
    a.getStateInformation(mb);
    auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), static_cast<int>(mb.getSize()));
    REQUIRE(xml != nullptr);
    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        if (child->getStringAttribute("id").endsWith("_source"))
            xml->removeChildElement(child, true);
        child = next;
    }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml, old);

    DubgefahrenProcessor b;
    b.applyKit(makeEmptyKit());
    REQUIRE(b.slotSource(0) == SourceType::Empty);
    b.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    for (int s = 0; s < kNumSlots; ++s)
        CHECK(b.slotSource(s) == SourceType::Synth);
}

TEST_CASE("applyKit with an empty kit keeps the plugin silent", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.applyKit(makeEmptyKit());
    for (const auto& s : p.currentKit().slots)
        CHECK(s.source == SourceType::Empty);

    prepare(p);
    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    p.processBlock(buf, on);
    CHECK(buf.getMagnitude(0, 0, 512) == 0.0f);
    CHECK(p.activeMask() == 0u);
}
