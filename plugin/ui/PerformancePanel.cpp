#include "plugin/ui/PerformancePanel.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"

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
    g.setColour(colours::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(colours::textDim);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("PERFORMANCE", 12, 0, 120, getHeight(), juce::Justification::centredLeft);
}

void PerformancePanel::resized()
{
    int x = 130;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &pitch_, &rate_, &depth_, &sweep_ })
    {
        c->setBounds(x, 4, 72, getHeight() - 8);
        x += 76;
    }
    constexpr int kChoiceHeight = 44;
    const int choiceY = (getHeight() - kChoiceHeight) / 2;
    target_.setBounds(x + 20, choiceY, 120, kChoiceHeight);
    latchStop_.setBounds(x + 160, choiceY, 170, kChoiceHeight);
}

} // namespace dg::ui
