#include "plugin/ui/LivePanel.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/Surfaces.h"

namespace dg::ui {

LivePanel::LivePanel(DubgefahrenProcessor& proc)
{
    auto& s = proc.state();
    space_.attach(s, pid::space);
    grit_.attach(s, pid::grit);
    throw_.attach(s, pid::throwAmount);
    cutoff_.attach(s, pid::cutoff);
    resonance_.attach(s, pid::resonance);
    filterType_.attach(s, pid::filterType);
    delayTime_.attach(s, pid::delayTime);
    master_.attach(s, pid::masterVol);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &space_, &grit_, &throw_, &cutoff_, &resonance_, &filterType_, &delayTime_, &master_ })
        addAndMakeVisible(*c);
}

void LivePanel::paint(juce::Graphics& g)
{
    drawPanelBody(g, getLocalBounds().toFloat());
    g.setFont(font(11.0f, true));
    g.setColour(colours::textDim);
    const auto title = [&g](const juce::String& text, int x, int y, int w) {
        g.drawText(text, x, y, w, 16, juce::Justification::centredLeft);
    };
    const auto divider = [&](int x) {
        drawDivider(g, static_cast<float>(x), 6.0f, static_cast<float>(getHeight() - 6));
    };
    if (getWidth() >= 900)
    {
        title("DUB FX", space_.getX(), 4, 270);
        title("FILTER", cutoff_.getX(), 4, 3 * 72);
        title("DELAY", delayTime_.getX(), 4, 110);
        title("MASTER", master_.getX(), 4, 72);
        divider(cutoff_.getX() - 7);
        divider(delayTime_.getX() - 7);
        divider(master_.getX() - 7);
    }
    else
    {
        title("DUB FX", 12, 4, getWidth() - 24);
        title(u8("FILTER · DELAY · MASTER"), 12, 122, getWidth() - 24);
    }
}

void LivePanel::resized()
{
    constexpr int kCell = 72;
    if (getWidth() >= 900)
    {
        // One row (Edit): DUB FX | FILTER | DELAY | MASTER
        int x = 12;
        const int h = getHeight() - 24;
        for (auto* k : { &space_, &grit_, &throw_ })
        {
            k->setBounds(x, 20, 90 - 6, h);
            x += 90;
        }
        x += 12;
        for (auto* k : { &cutoff_, &resonance_, &filterType_ })
        {
            k->setBounds(x, 20, kCell - 6, h);
            x += kCell;
        }
        x += 12;
        delayTime_.setBounds(x, 20, 110, h);
        x += 122;
        master_.setBounds(x, 20, kCell - 6, h);
    }
    else
    {
        // Compact (Live): three big knobs on top, filter, time and master below
        const int colW = (getWidth() - 24) / 3;
        int x = 12;
        for (auto* k : { &space_, &grit_, &throw_ })
        {
            k->setBounds(x, 20, colW - 6, 100);
            x += colW;
        }
        x = 12;
        for (auto* k : { &cutoff_, &resonance_, &filterType_ })
        {
            k->setBounds(x, 140, kCell - 6, 88);
            x += kCell;
        }
        delayTime_.setBounds(x + 4, 150, 110, 70);
        x += 126;
        master_.setBounds(x, 140, kCell - 6, 88);
    }
}

} // namespace dg::ui
