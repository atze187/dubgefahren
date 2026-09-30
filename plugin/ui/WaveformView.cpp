#include "plugin/ui/WaveformView.h"
#include <algorithm>
#include <cmath>
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"

namespace dg::ui {

namespace {
constexpr float kCorner = 6.0f;
constexpr float kGrab = 8.0f;    // Fangbereich eines Markers links und rechts
constexpr float kHandle = 14.0f; // Kantenlänge des Griffs
constexpr SlotField kWatched[5] = { SlotField::SampleStart, SlotField::LoopStart, SlotField::SampleEnd, SlotField::Loop,
                                    SlotField::Reverse };

SlotField fieldFor(WaveformView::Marker m)
{
    switch (m)
    {
        case WaveformView::Marker::Start: return SlotField::SampleStart;
        case WaveformView::Marker::Loop:  return SlotField::LoopStart;
        case WaveformView::Marker::End:   return SlotField::SampleEnd;
    }
    return SlotField::SampleStart;
}
} // namespace

std::vector<PeakColumn> computePeaks(const std::vector<float>& samples, int columns)
{
    std::vector<PeakColumn> out;
    if (samples.empty() || columns <= 0)
        return out;
    out.resize(static_cast<std::size_t>(columns));
    const std::size_t n = samples.size();
    const auto cols = static_cast<std::size_t>(columns);
    for (std::size_t c = 0; c < cols; ++c)
    {
        const std::size_t from = std::min(n - 1, n * c / cols);
        const std::size_t to = std::max(from + 1, std::min(n, n * (c + 1) / cols));
        float lo = 1.0f, hi = -1.0f;
        bool any = false;
        for (std::size_t i = from; i < to; ++i)
        {
            if (!std::isfinite(samples[i]))
                continue;
            const float v = std::clamp(samples[i], -1.0f, 1.0f);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
            any = true;
        }
        out[c] = any ? PeakColumn { lo, hi } : PeakColumn {};
    }
    return out;
}

WaveformView::WaveformView(DubgefahrenProcessor& proc) : proc_(proc) {}

WaveformView::~WaveformView()
{
    stopTimer();
    release();
}

void WaveformView::setSlot(int slot)
{
    release(); // eine offene Geste gehört zum alten Slot
    slot_ = slot;
    refresh();
}

void WaveformView::refresh()
{
    data_ = slot_ >= 0 ? proc_.slotSampleData(slot_) : nullptr;
    repaint();
}

juce::String WaveformView::hintText() const
{
    if (hasData())
        return {};
    return slot_ >= 0 && proc_.isSampleMissing(slot_) ? "Sample missing" : "No sample loaded";
}

float WaveformView::value(SlotField f) const
{
    if (slot_ < 0)
        return fieldSpec(f).def;
    const auto* v = proc_.state().getRawParameterValue(slotParamId(slot_, f));
    return v != nullptr ? v->load() : fieldSpec(f).def;
}

SampleRegion WaveformView::region() const
{
    return resolveSampleRegion(value(SlotField::SampleStart), value(SlotField::LoopStart), value(SlotField::SampleEnd),
                               hasData() ? data_->samples.size() : 0);
}

float WaveformView::xFor(double position) const
{
    if (!hasData())
        return 0.0f;
    return static_cast<float>(position / static_cast<double>(data_->samples.size()) * getWidth());
}

bool WaveformView::isMarkerVisible(Marker m) const { return hasData() && (m != Marker::Loop || loopOn()); }

float WaveformView::markerX(Marker m) const
{
    const auto r = region();
    return xFor(m == Marker::Start ? r.start : m == Marker::Loop ? r.loopStart : r.end);
}

juce::RangedAudioParameter* WaveformView::parameter(Marker m) const
{
    return slot_ >= 0 ? proc_.state().getParameter(slotParamId(slot_, fieldFor(m))) : nullptr;
}

bool WaveformView::canMove(Marker m, float direction) const
{
    const auto r = region();
    const double n = static_cast<double>(data_->samples.size());
    const double minLen = std::min(kMinRegionSamples, n);
    const bool loop = loopOn();
    switch (m)
    {
        case Marker::Start:
            return direction < 0.0f ? r.start > 0.0 : r.start < std::min(r.end - minLen, loop ? r.loopStart : n);
        case Marker::Loop:
            return direction < 0.0f ? r.loopStart > r.start : r.loopStart < r.end;
        case Marker::End:
            return direction > 0.0f ? r.end < n : r.end > std::max(r.start + minLen, loop ? r.loopStart : 0.0);
    }
    return false;
}

void WaveformView::begin(Marker m)
{
    if (auto* p = parameter(m))
    {
        p->beginChangeGesture();
        active_ = m;
    }
    candidates_.clear();
}

void WaveformView::pressAt(float x)
{
    release();
    if (!hasData() || getWidth() <= 0)
        return;
    pressX_ = x;
    for (const auto m : { Marker::Loop, Marker::End, Marker::Start })
        if (isMarkerVisible(m) && std::abs(markerX(m) - x) <= kGrab)
            candidates_.push_back(m);
    if (candidates_.size() == 1)
        begin(candidates_.front());
}

void WaveformView::dragTo(float x)
{
    if (!hasData() || getWidth() <= 0)
        return;
    if (!active_)
    {
        if (candidates_.empty() || x == pressX_)
            return;
        // Mehrere Griffe übereinander: der Marker, der sich in Zugrichtung noch bewegen lässt
        // (Reihenfolge L, E, S, siehe pressAt).
        const float direction = x - pressX_;
        Marker pick = candidates_.front();
        for (const auto m : candidates_)
            if (canMove(m, direction))
            {
                pick = m;
                break;
            }
        begin(pick);
    }
    if (active_)
        moveTo(*active_, x);
}

void WaveformView::moveTo(Marker m, float x)
{
    auto* p = parameter(m);
    if (p == nullptr)
        return;
    const auto r = region();
    const double n = static_cast<double>(data_->samples.size());
    const double minLen = std::min(kMinRegionSamples, n);
    const bool loop = loopOn();
    double pos = std::clamp(static_cast<double>(x) / getWidth(), 0.0, 1.0) * n;
    switch (m)
    {
        case Marker::Start: pos = std::clamp(pos, 0.0, std::min(r.end - minLen, loop ? r.loopStart : n)); break;
        case Marker::Loop:  pos = std::clamp(pos, r.start, r.end); break;
        case Marker::End:   pos = std::clamp(pos, std::max(r.start + minLen, loop ? r.loopStart : 0.0), n); break;
    }
    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(pos / n)));
    repaint();
}

void WaveformView::release()
{
    if (active_)
        if (auto* p = parameter(*active_))
            p->endChangeGesture();
    active_.reset();
    candidates_.clear();
}

void WaveformView::visibilityChanged()
{
    if (isVisible())
        startTimerHz(30);
    else
        stopTimer();
}

void WaveformView::timerCallback()
{
    // Nachführen bei Automation, Slot-Wechsel und fertig geladenen Daten.
    const auto latest = slot_ >= 0 ? proc_.slotSampleData(slot_) : nullptr;
    bool changed = latest != data_;
    data_ = latest;
    for (int i = 0; i < 5; ++i)
    {
        const float v = value(kWatched[i]);
        changed = changed || v != seen_[i];
        seen_[i] = v;
    }
    if (changed)
        repaint();
}

void WaveformView::drawMarker(juce::Graphics& g, float x, juce::Colour colour, const char* letter) const
{
    const float h = static_cast<float>(getHeight());
    const float w = static_cast<float>(getWidth());
    g.setColour(colour);
    g.fillRect(juce::Rectangle<float>(std::clamp(x - 0.75f, 0.0f, std::max(0.0f, w - 1.5f)), 0.0f, 1.5f, h));
    const juce::Rectangle<float> handle(std::clamp(x - kHandle * 0.5f, 0.0f, std::max(0.0f, w - kHandle)), 0.0f, kHandle, kHandle);
    g.fillRoundedRectangle(handle, 3.0f);
    g.setColour(colours::padEmpty);
    g.setFont(font(10.0f, true));
    g.drawText(letter, handle, juce::Justification::centred);
}

void WaveformView::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
        return;
    juce::Path body;
    body.addRoundedRectangle(bounds, kCorner);
    g.setColour(colours::padEmpty);
    g.fillPath(body);

    if (!hasData())
    {
        inset_.render(g, body);
        g.setColour(colours::textDim);
        g.setFont(font(13.0f));
        g.drawText(hintText(), bounds, juce::Justification::centred);
        return;
    }

    // Spitzenwerte einmal pro Daten und Breite berechnen.
    if (peaksData_ != data_.get() || peaksWidth_ != getWidth())
    {
        peaks_ = computePeaks(data_->samples, getWidth());
        peaksData_ = data_.get();
        peaksWidth_ = getWidth();
    }

    const auto r = region();
    const float xStart = xFor(r.start);
    const float xEnd = xFor(r.end);
    const float h = bounds.getHeight();
    {
        const juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(body);

        if (loopOn())
        {
            const auto seg = resolveLoopSegment(r, reverseOn(), data_->samples.size());
            g.setColour(colours::latched.withAlpha(0.13f));
            g.fillRect(juce::Rectangle<float>(xFor(seg.lo), 0.0f, xFor(seg.hi) - xFor(seg.lo), h));
        }

        const float mid = h * 0.5f;
        const float half = std::max(1.0f, mid - 4.0f);
        g.setColour(colours::padBase);
        for (std::size_t c = 0; c < peaks_.size(); ++c)
        {
            const float top = mid - peaks_[c].max * half;
            const float bottom = mid - peaks_[c].min * half;
            g.fillRect(juce::Rectangle<float>(static_cast<float>(c), top, 1.0f, std::max(1.0f, bottom - top)));
        }

        // Der nicht gespielte Teil ist abgedunkelt.
        g.setColour(colours::padEmpty.withAlpha(0.72f));
        g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, xStart, h));
        g.fillRect(juce::Rectangle<float>(xEnd, 0.0f, bounds.getWidth() - xEnd, h));
    }
    inset_.render(g, body);

    drawMarker(g, xStart, colours::text, "S");
    drawMarker(g, xEnd, colours::text, "E");
    if (loopOn())
        drawMarker(g, xFor(r.loopStart), colours::latched, "L");
}

} // namespace dg::ui
