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

TEST_CASE("the plugin exposes 308 uniquely named parameters", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::set<juce::String> ids;
    for (auto* param : p.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            ids.insert(withId->paramID);
    CHECK(p.getParameters().size() == 16 * 18 + 20);
    CHECK(ids.size() == 308);
    CHECK(slotParamId(0, SlotField::Wave) == "s01_wave");
    CHECK(slotParamId(15, SlotField::FxSend) == "s16_send");
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
