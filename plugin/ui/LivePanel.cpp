#include "plugin/ui/LivePanel.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/Surfaces.h"

namespace dg::ui {

namespace {
constexpr int kCellHeight = 34;    // Beschriftung 12 + Slider 22
constexpr int kTitleHeight = 16;
constexpr int kStripCellWidth = 170; // Edit: eine Gruppe je Spalte
constexpr int kStripGap = 24;        // Abstand zwischen Gruppen, der Trenner liegt in der Mitte
constexpr int kStripRowPitch = 38;
constexpr int kCompactRowPitch = 58; // Live: Titel + Zelle je Gruppenzeile
constexpr int kCompactGap = 10;
} // namespace

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
        g.drawText(text, x, y, w, kTitleHeight, juce::Justification::centredLeft);
    };
    if (getWidth() >= 900)
    {
        // Edit: vier Gruppen als Spalten mit je einem Titel und einem Trenner davor
        title("DUB FX", space_.getX(), 4, kStripCellWidth);
        title("FILTER", cutoff_.getX(), 4, kStripCellWidth);
        title("DELAY", delayTime_.getX(), 4, kStripCellWidth);
        title("MASTER", master_.getX(), 4, kStripCellWidth);
        for (const int x : { cutoff_.getX(), delayTime_.getX(), master_.getX() })
            drawDivider(g, static_cast<float>(x - kStripGap / 2), 6.0f, static_cast<float>(getHeight() - 6));
    }
    else
    {
        // Live: DUB FX und FILTER je eine Zeile, DELAY und MASTER teilen sich die dritte
        title("DUB FX", space_.getX(), space_.getY() - kTitleHeight - 2, 200);
        title("FILTER", cutoff_.getX(), cutoff_.getY() - kTitleHeight - 2, 200);
        title("DELAY", delayTime_.getX(), delayTime_.getY() - kTitleHeight - 2, 200);
        title("MASTER", master_.getX(), master_.getY() - kTitleHeight - 2, 200);
        drawDivider(g, static_cast<float>(master_.getX() - kCompactGap / 2), static_cast<float>(master_.getY() - kTitleHeight - 2),
                    static_cast<float>(master_.getBottom()));
    }
}

void LivePanel::resized()
{
    if (getWidth() >= 900)
    {
        // Edit: DUB FX | FILTER | DELAY | MASTER, je drei Zeilen
        const auto column = [this](int index, std::initializer_list<juce::Component*> cells) {
            const int x = 12 + index * (kStripCellWidth + kStripGap);
            int y = 22;
            for (auto* c : cells)
            {
                c->setBounds(x, y, kStripCellWidth, kCellHeight);
                y += kStripRowPitch;
            }
        };
        column(0, { &space_, &grit_, &throw_ });
        column(1, { &cutoff_, &resonance_, &filterType_ });
        column(2, { &delayTime_ });
        column(3, { &master_ });
    }
    else
    {
        // Compact (Live): drei Zellen je Zeile
        const int cw = (getWidth() - 24 - 2 * kCompactGap) / 3;
        const auto row = [&](int rowIndex, std::initializer_list<juce::Component*> cells) {
            int x = 12;
            for (auto* c : cells)
            {
                c->setBounds(x, 22 + rowIndex * kCompactRowPitch, cw, kCellHeight);
                x += cw + kCompactGap;
            }
        };
        row(0, { &space_, &grit_, &throw_ });
        row(1, { &cutoff_, &resonance_, &filterType_ });
        row(2, { &delayTime_, &master_ });
    }
}

} // namespace dg::ui
