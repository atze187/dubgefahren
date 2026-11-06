#include "plugin/ui/AdvancedPanel.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/Surfaces.h"

namespace dg::ui {

namespace {
constexpr int kCellWidth = 150;
constexpr int kCellHeight = 34; // Beschriftung 12 + Slider 22
constexpr int kCellPitch = kCellWidth + 8;
constexpr int kGroupGap = 24;   // Abstand zwischen Gruppen, der Trenner liegt in der Mitte
constexpr int kTitleHeight = 16;
constexpr int kRowPitch = 54;   // Titel + Zelle + Abstand
constexpr int kFirstRowY = 4;

int cellX(int cells) { return 12 + cells * kCellPitch; }
// Gruppen in zwei Zeilen: DRIVE + DELAY oben, PHASER + REVERB unten.
struct Group { const char* title; int row; int x; int cells; };
const Group kGroups[] = {
    { "DRIVE", 0, cellX(0), 1 },
    { "DELAY", 0, cellX(1) + kGroupGap, 4 },
    { "PHASER", 1, cellX(0), 3 },
    { "REVERB", 1, cellX(3) + kGroupGap, 3 },
};
} // namespace

AdvancedPanel::AdvancedPanel(DubgefahrenProcessor& proc)
{
    auto& s = proc.state();
    drive_.attach(s, pid::drive);
    delayFeedback_.attach(s, pid::delayFeedback);
    delayTone_.attach(s, pid::delayTone);
    delayWow_.attach(s, pid::delayWow);
    delayMix_.attach(s, pid::delayMix);
    phaserRate_.attach(s, pid::phaserRate);
    phaserDepth_.attach(s, pid::phaserDepth);
    phaserMix_.attach(s, pid::phaserMix);
    reverbDecay_.attach(s, pid::reverbDecay);
    reverbTone_.attach(s, pid::reverbTone);
    reverbMix_.attach(s, pid::reverbMix);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &drive_, &delayFeedback_, &delayTone_, &delayWow_, &delayMix_, &phaserRate_, &phaserDepth_,
             &phaserMix_, &reverbDecay_, &reverbTone_, &reverbMix_ })
        addAndMakeVisible(*c);
}

void AdvancedPanel::paint(juce::Graphics& g)
{
    drawPanelBody(g, getLocalBounds().toFloat());
    g.setFont(font(11.0f, true));
    for (const auto& grp : kGroups)
    {
        const int y = kFirstRowY + grp.row * kRowPitch;
        g.setColour(colours::textDim);
        g.drawText(grp.title, grp.x, y, grp.cells * kCellPitch, kTitleHeight, juce::Justification::centredLeft);
        // Trenner vor jeder zweiten Gruppe einer Zeile
        if (grp.x > cellX(0))
            drawDivider(g, static_cast<float>(grp.x - kGroupGap / 2), static_cast<float>(y), static_cast<float>(y + kTitleHeight + kCellHeight + 2));
    }
}

void AdvancedPanel::resized()
{
    const auto place = [](int row, int x, std::initializer_list<juce::Component*> cells) {
        for (auto* c : cells)
        {
            c->setBounds(x, kFirstRowY + row * kRowPitch + kTitleHeight, kCellWidth, kCellHeight);
            x += kCellPitch;
        }
    };
    place(0, kGroups[0].x, { &drive_ });
    place(0, kGroups[1].x, { &delayFeedback_, &delayTone_, &delayWow_, &delayMix_ });
    place(1, kGroups[2].x, { &phaserRate_, &phaserDepth_, &phaserMix_ });
    place(1, kGroups[3].x, { &reverbDecay_, &reverbTone_, &reverbMix_ });
}

} // namespace dg::ui
