#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/FxPanel.h"
#include "RenderTestHelpers.h"

using namespace dg;

TEST_CASE("the FX panel fits all controls without overlap", "[fxpanel]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor proc;
    ui::DgLookAndFeel lnf;
    ui::FxPanel panel(proc);
    panel.setLookAndFeel(&lnf);
    panel.setSize(976, 96);

    // Der Sync-Schalter liegt bewusst über der Beschriftungszeile der Rate-Zelle und wird beim
    // Überlappungstest ausgelassen; die übrigen sichtbaren Zellen dürfen sich nicht berühren.
    const auto panelBounds = panel.getLocalBounds();
    std::vector<juce::Rectangle<int>> cells;
    for (auto* c : panel.getChildren())
    {
        if (!c->isVisible())
            continue;
        CHECK(panelBounds.contains(c->getBounds()));
        if (c->getName() != "sync")
            cells.push_back(c->getBounds());
    }
    CHECK(cells.size() == 16);
    for (std::size_t i = 0; i < cells.size(); ++i)
        for (std::size_t j = i + 1; j < cells.size(); ++j)
            CHECK_FALSE(cells[i].intersects(cells[j]));

    for (const float scale : { 0.75f, 1.0f, 2.0f })
    {
        const auto img = dgtest::snapshot(panel, scale);
        CHECK(dgtest::hasVisiblePixel(img));
        dgtest::savePng(img, "fxpanel_" + juce::String(static_cast<int>(scale * 100)));
    }
    panel.setLookAndFeel(nullptr);
}

TEST_CASE("the rate cell shows the division when sync is on", "[fxpanel]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor proc;
    ui::FxPanel panel(proc);
    panel.setSize(976, 96);
    CHECK_FALSE(panel.showsSyncedRate());

    auto* sync = proc.state().getParameter(pid::delayLfoSync);
    sync->setValueNotifyingHost(1.0f);
    CHECK(panel.showsSyncedRate());
    sync->setValueNotifyingHost(0.0f);
    CHECK_FALSE(panel.showsSyncedRate());
}
