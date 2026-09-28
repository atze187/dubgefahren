#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <utility>
#include <vector>
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

TEST_CASE("editor shows the cpu meter in the header", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    CHECK(e->cpuMeterText() == "CPU 0 %");
}

namespace {
std::vector<std::pair<juce::String, bool>> menuItems(const juce::PopupMenu& m)
{
    std::vector<std::pair<juce::String, bool>> out;
    for (juce::PopupMenu::MenuItemIterator it(m); it.next();)
    {
        const auto& item = it.getItem();
        if (!item.isSeparator)
            out.emplace_back(item.text, item.isEnabled);
    }
    return out;
}
using Items = std::vector<std::pair<juce::String, bool>>;
} // namespace

TEST_CASE("empty slots show a hint in the slot editor and 'Empty' on the pad", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.clearSlot(2);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    CHECK(e->padShowsEmpty(2));
    CHECK_FALSE(e->padShowsEmpty(0));
    e->selectSlot(2);
    CHECK(e->slotEditorShowsEmptyHint());
    e->selectSlot(0);
    CHECK_FALSE(e->slotEditorShowsEmptyHint());

    p.clearSlot(0); // von außen geändert: Timer-Poll frischt auf
    e->pollProcessorState();
    CHECK(e->padShowsEmpty(0));
    CHECK(e->slotEditorShowsEmptyHint());
}

TEST_CASE("source menu offers synth and a disabled sample entry", "[editor]")
{
    CHECK(menuItems(DubgefahrenEditor::buildSourceMenu()) == Items { { "Synth", true }, { "Sample", false } });
}

TEST_CASE("pad menu disables rename and clear on empty slots", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.clearSlot(1);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    CHECK(menuItems(e->buildPadMenu(0)) == Items { { "Copy", true }, { "Paste", false }, { "Reset to Factory Default", true },
                                                   { "Rename", true }, { "Clear Slot", true } });
    CHECK(menuItems(e->buildPadMenu(1)) == Items { { "Copy", true }, { "Paste", false }, { "Reset to Factory Default", true },
                                                   { "Rename", false }, { "Clear Slot", false } });
}

TEST_CASE("kit menu offers a new empty kit", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    const auto items = menuItems(e->buildKitMenu({}));
    REQUIRE(items.size() >= 2);
    CHECK(items[0] == std::make_pair(juce::String("Load Factory Kit"), true));
    CHECK(items[1] == std::make_pair(juce::String("New Empty Kit"), true));
}
