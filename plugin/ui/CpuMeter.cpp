#include "plugin/ui/CpuMeter.h"
#include <cmath>
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

namespace {
constexpr double kWarnLoad = 0.5;
constexpr double kDangerLoad = 0.8;
constexpr int kRefreshHz = 4;
} // namespace

CpuMeter::CpuMeter()
{
    setFont(juce::FontOptions(13.0f));
    setJustificationType(juce::Justification::centredLeft);
    setLoad(0.0);
}

CpuMeter::~CpuMeter() { stopTimer(); }

void CpuMeter::setSource(std::function<double()> source)
{
    source_ = std::move(source);
    if (source_)
        startTimerHz(kRefreshHz);
    else
        stopTimer();
}

void CpuMeter::setLoad(double proportion)
{
    if (!std::isfinite(proportion) || proportion < 0.0)
        proportion = 0.0;
    setText("CPU " + juce::String(juce::roundToInt(proportion * 100.0)) + " %", juce::dontSendNotification);
    const auto colour = proportion >= kDangerLoad ? colours::danger
                      : proportion >= kWarnLoad   ? colours::warning
                                                  : colours::textDim;
    setColour(juce::Label::textColourId, colour);
}

void CpuMeter::timerCallback()
{
    if (source_)
        setLoad(source_());
}

} // namespace dg::ui
