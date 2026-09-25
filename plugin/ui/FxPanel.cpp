#include "plugin/ui/FxPanel.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

namespace {
constexpr int kCell = 72;
struct Group { const char* title; int firstCell; int cells; };
constexpr Group kGroups[] = { { "DRIVE", 0, 1 }, { "FILTER", 1, 3 }, { "DELAY", 4, 5 }, { "HALL", 9, 3 }, { "MASTER", 12, 1 } };
} // namespace

FxPanel::FxPanel(DubgefahrenProcessor& proc)
{
    auto& s = proc.state();
    drive_.attach(s, pid::drive);
    cutoff_.attach(s, pid::cutoff);
    resonance_.attach(s, pid::resonance);
    filterType_.attach(s, pid::filterType);
    delayTime_.attach(s, pid::delayTime);
    delayFeedback_.attach(s, pid::delayFeedback);
    delayTone_.attach(s, pid::delayTone);
    delayWow_.attach(s, pid::delayWow);
    delayMix_.attach(s, pid::delayMix);
    reverbDecay_.attach(s, pid::reverbDecay);
    reverbTone_.attach(s, pid::reverbTone);
    reverbMix_.attach(s, pid::reverbMix);
    master_.attach(s, pid::masterVol);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &drive_, &cutoff_, &resonance_, &filterType_, &delayTime_, &delayFeedback_, &delayTone_, &delayWow_,
             &delayMix_, &reverbDecay_, &reverbTone_, &reverbMix_, &master_ })
        addAndMakeVisible(*c);
}

void FxPanel::paint(juce::Graphics& g)
{
    g.setColour(colours::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    for (const auto& grp : kGroups)
    {
        const int x = 12 + grp.firstCell * kCell;
        g.setColour(colours::textDim);
        g.drawText(grp.title, x, 4, grp.cells * kCell, 16, juce::Justification::centredLeft);
        g.setColour(colours::outline);
        g.drawVerticalLine(x - 4, 6.0f, static_cast<float>(getHeight() - 6));
    }
}

void FxPanel::resized()
{
    juce::Component* order[] = { &drive_, &cutoff_, &resonance_, &filterType_, &delayTime_, &delayFeedback_,
                                 &delayTone_, &delayWow_, &delayMix_, &reverbDecay_, &reverbTone_, &reverbMix_, &master_ };
    for (int i = 0; i < 13; ++i)
        order[i]->setBounds(12 + i * kCell, 20, kCell - 6, getHeight() - 24);
}

} // namespace dg::ui
