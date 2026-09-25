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
