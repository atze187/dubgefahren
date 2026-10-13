#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <set>
#include "engine/Kit.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/PadColours.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
float bestLatchContrast(juce::Colour surface)
{
    return std::max(ui::contrastRatio(ui::latchDotCore, surface), ui::contrastRatio(ui::latchDotRing, surface));
}
} // namespace

TEST_CASE("the palette has eight named colours with green as the default", "[padcolour]")
{
    STATIC_CHECK(kNumPadColours == 8);
    CHECK(ui::padColour(0).base == ui::colours::padBase);
    CHECK(ui::padColour(0).litText == ui::colours::padTextLit);
    std::set<juce::String> names;
    std::set<juce::uint32> bases;
    for (int i = 0; i < kNumPadColours; ++i)
    {
        names.insert(ui::padColour(i).name);
        bases.insert(ui::padColour(i).base.getARGB());
    }
    CHECK(names.size() == 8);
    CHECK(bases.size() == 8);
    CHECK(juce::String(ui::padColour(0).name) == "Green");
    CHECK(juce::String(ui::padColour(7).name) == "Yellow");
}

TEST_CASE("out-of-range colour indices fall back to the default", "[padcolour]")
{
    CHECK(clampPadColour(-1) == 0);
    CHECK(clampPadColour(8) == 0);
    CHECK(clampPadColour(5) == 5);
    CHECK(&ui::padColour(-1) == &ui::padColour(0));
    CHECK(&ui::padColour(1000) == &ui::padColour(0));
    Kit k;
    CHECK(std::all_of(k.colours.begin(), k.colours.end(), [](int c) { return c == 0; }));
}

TEST_CASE("contrast ratios follow the WCAG formula", "[padcolour]")
{
    CHECK_THAT(ui::contrastRatio(juce::Colours::white, juce::Colours::black), WithinAbs(21.0, 0.01));
    CHECK_THAT(ui::contrastRatio(juce::Colours::black, juce::Colours::white), WithinAbs(21.0, 0.01));
    CHECK_THAT(ui::contrastRatio(ui::colours::padBase, ui::colours::padBase), WithinAbs(1.0, 1e-6));
}

TEST_CASE("the latch dot is readable on every pad colour, dimmed and lit", "[padcolour]")
{
    const auto dim = ui::colours::padTop.interpolatedWith(ui::colours::padBottom, 0.5f);
    CHECK(bestLatchContrast(dim) >= 3.0f);
    for (int i = 0; i < kNumPadColours; ++i)
    {
        const auto& c = ui::padColour(i);
        INFO(c.name);
        CHECK(bestLatchContrast(c.base.brighter(0.7f)) >= 3.0f);
        CHECK(bestLatchContrast(c.base) >= 3.0f);
    }
}

TEST_CASE("lit pad text is readable on every pad colour", "[padcolour]")
{
    for (int i = 0; i < kNumPadColours; ++i)
    {
        const auto& c = ui::padColour(i);
        INFO(c.name);
        CHECK(ui::contrastRatio(c.litText, c.base) >= 3.0f);
        CHECK(ui::contrastRatio(c.litText, c.base.brighter(0.7f)) >= 3.0f);
    }
}
