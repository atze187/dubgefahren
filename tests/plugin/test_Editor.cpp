#include <catch2/catch_test_macros.hpp>
#include <memory>
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

using namespace dg;

TEST_CASE("editor opens with the stored scale and follows focus", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.setUiScale(1.5f);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    REQUIRE(dynamic_cast<DubgefahrenEditor*>(editor.get()) != nullptr);
    CHECK(editor->getWidth() == 1500);
    CHECK(editor->getHeight() == 960);

    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    CHECK(e->selectedSlot() == 0);
    e->selectSlot(6);
    CHECK(e->selectedSlot() == 6);
}

TEST_CASE("editor refreshes after the host restores state", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    DubgefahrenProcessor a;
    a.setSlotName(1, "Alpha");
    a.setEditorFollowsFocus(false);
    juce::MemoryBlock state;
    a.getStateInformation(state);

    DubgefahrenProcessor b;
    std::unique_ptr<juce::AudioProcessorEditor> editor(b.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    b.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    e->pollProcessorState();

    CHECK(b.slotName(1) == "Alpha");
    CHECK(e->followFocusToggleState() == false);
}
