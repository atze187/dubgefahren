#include "plugin/ui/FxPanel.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/Surfaces.h"

namespace dg::ui {

namespace {
constexpr int kCell = 60;
constexpr int kCells = 16;
constexpr int kRateCell = 9;   // Rate-Zelle: Hz-Regler oder Teilung, darüber der Sync-Schalter
constexpr int kToggleRow = 14; // Höhe der Beschriftungszeile, in der der Sync-Schalter sitzt
struct Group { const char* title; int firstCell; int cells; };
constexpr Group kGroups[] = { { "DRIVE", 0, 1 }, { "DELAY", 1, 4 }, { "FILTER", 5, 3 },
                              { "LFO", 8, 4 },   { "REVERB", 12, 3 }, { "MASTER", 15, 1 } };
} // namespace

FxPanel::FxPanel(DubgefahrenProcessor& proc)
{
    auto& s = proc.state();
    drive_.attach(s, pid::drive);
    delayTime_.attach(s, pid::delayTime);
    delayFeedback_.attach(s, pid::delayFeedback);
    delayWow_.attach(s, pid::delayWow);
    delayMix_.attach(s, pid::delayMix);
    fltType_.attach(s, pid::delayFltType);
    fltCutoff_.attach(s, pid::delayFltCutoff);
    fltRes_.attach(s, pid::delayFltRes);
    lfoShape_.attach(s, pid::delayLfoShape);
    lfoRate_.attach(s, pid::delayLfoRate);
    lfoDiv_.attach(s, pid::delayLfoSyncDiv);
    lfoSync_.attach(s, pid::delayLfoSync);
    lfoCutDepth_.attach(s, pid::delayLfoCutDepth);
    lfoResDepth_.attach(s, pid::delayLfoResDepth);
    reverbDecay_.attach(s, pid::reverbDecay);
    reverbTone_.attach(s, pid::reverbTone);
    reverbMix_.attach(s, pid::reverbMix);
    master_.attach(s, pid::masterVol);

    // Der Sync-Schalter ersetzt die Beschriftungszeile der Rate-Zelle.
    lfoRate_.label.setVisible(false);
    lfoDiv_.label.setVisible(false);
    lfoSync_.setName("sync");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &drive_, &delayTime_, &delayFeedback_, &delayWow_, &delayMix_, &fltType_, &fltCutoff_, &fltRes_,
             &lfoShape_, &lfoRate_, &lfoDiv_, &lfoSync_, &lfoCutDepth_, &lfoResDepth_, &reverbDecay_, &reverbTone_,
             &reverbMix_, &master_ })
        addAndMakeVisible(*c);

    // Auch bei Automation: Der Schalter folgt dem Parameter, die Zelle zeigt Hz oder Teilung.
    lfoSync_.button.onStateChange = [this] { updateRateCell(); };
    updateRateCell();
}

void FxPanel::updateRateCell()
{
    const bool sync = lfoSync_.button.getToggleState();
    lfoRate_.setVisible(!sync);
    lfoDiv_.setVisible(sync);
}

void FxPanel::paint(juce::Graphics& g)
{
    drawPanelBody(g, getLocalBounds().toFloat());
    g.setFont(font(11.0f, true));
    for (const auto& grp : kGroups)
    {
        const int x = 12 + grp.firstCell * kCell;
        g.setColour(colours::textDim);
        g.drawText(grp.title, x, 4, grp.cells * kCell, 16, juce::Justification::centredLeft);
        drawDivider(g, static_cast<float>(x - 5), 6.0f, static_cast<float>(getHeight() - 6));
    }
}

void FxPanel::resized()
{
    const auto cell = [this](int i) {
        return juce::Rectangle<int>(12 + i * kCell, 20, kCell - 6, getHeight() - 24);
    };
    jassert(cell(kCells - 1).getRight() <= getWidth());

    drive_.setBounds(cell(0));
    delayTime_.setBounds(cell(1));
    delayFeedback_.setBounds(cell(2));
    delayWow_.setBounds(cell(3));
    delayMix_.setBounds(cell(4));
    fltType_.setBounds(cell(5));
    fltCutoff_.setBounds(cell(6));
    fltRes_.setBounds(cell(7));
    lfoShape_.setBounds(cell(8));
    lfoRate_.setBounds(cell(kRateCell));
    lfoDiv_.setBounds(cell(kRateCell));
    lfoSync_.setBounds(cell(kRateCell).withHeight(kToggleRow));
    lfoCutDepth_.setBounds(cell(10));
    lfoResDepth_.setBounds(cell(11));
    reverbDecay_.setBounds(cell(12));
    reverbTone_.setBounds(cell(13));
    reverbMix_.setBounds(cell(14));
    master_.setBounds(cell(15));
}

} // namespace dg::ui
