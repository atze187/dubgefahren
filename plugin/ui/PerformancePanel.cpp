#include "plugin/ui/PerformancePanel.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/Surfaces.h"

namespace dg::ui {

PerformancePanel::PerformancePanel(DubgefahrenProcessor& proc)
{
    auto& s = proc.state();
    pitch_.attach(s, pid::perfPitch);
    rate_.attach(s, pid::perfRate);
    depth_.attach(s, pid::perfDepth);
    sweep_.attach(s, pid::perfSweep);
    target_.attach(s, pid::perfTarget);
    latchStop_.attach(s, pid::latchStop);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &pitch_, &rate_, &depth_, &sweep_, &target_, &latchStop_ })
        addAndMakeVisible(*c);
}

void PerformancePanel::paint(juce::Graphics& g)
{
    drawPanelBody(g, getLocalBounds().toFloat());
    g.setColour(colours::textDim);
    g.setFont(font(11.0f, true));
    if (getWidth() >= 900)
        g.drawText("PERFORMANCE", 12, 0, 120, getHeight(), juce::Justification::centredLeft);
    else
        g.drawText("PERFORMANCE", 12, 4, 160, 16, juce::Justification::centredLeft);
}

void PerformancePanel::resized()
{
    constexpr int kCellHeight = 34; // Beschriftung 12 + Slider 22
    if (getWidth() >= 900)
    {
        // Edit: eine Zeile, links der Titel
        const int y = (getHeight() - kCellHeight) / 2;
        int x = 140;
        for (juce::Component* c : std::initializer_list<juce::Component*> { &pitch_, &rate_, &depth_, &sweep_ })
        {
            c->setBounds(x, y, 150, kCellHeight);
            x += 160;
        }
        x += 20;
        target_.setBounds(x, y, 130, kCellHeight);
        latchStop_.setBounds(x + 150, y, 150, kCellHeight);
    }
    else
    {
        // Compact (Live): zwei Zeilen zu je drei Zellen
        constexpr int kGap = 10;
        const int cw = (getWidth() - 24 - 2 * kGap) / 3;
        int i = 0;
        for (juce::Component* c : std::initializer_list<juce::Component*> { &pitch_, &rate_, &depth_, &sweep_, &target_, &latchStop_ })
        {
            c->setBounds(12 + (i % 3) * (cw + kGap), 26 + (i / 3) * 50, cw, kCellHeight);
            ++i;
        }
    }
}

} // namespace dg::ui
