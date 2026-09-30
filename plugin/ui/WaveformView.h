#pragma once
#include <memory>
#include <optional>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "melatonin_blur/melatonin_blur.h"
#include "engine/SampleData.h"
#include "engine/SampleRegion.h"
#include "engine/SlotFields.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

struct PeakColumn
{
    float min = 0.0f;
    float max = 0.0f;
};

// Minimum und Maximum pro Pixelspalte, begrenzt auf -1..1. Leere Daten oder columns <= 0
// ergeben einen leeren Vektor.
std::vector<PeakColumn> computePeaks(const std::vector<float>& samples, int columns);

// Wellenform eines Sample-Slots mit den ziehbaren Markern Start (S), Loop-Start (L) und End (E).
// Gezeichnet und begrenzt wird mit den aufgelösten Positionen aus resolveSampleRegion, damit
// die Anzeige genau das zeigt, was die Engine spielt.
class WaveformView final : public juce::Component, private juce::Timer
{
public:
    enum class Marker { Start, Loop, End };

    explicit WaveformView(DubgefahrenProcessor& proc);
    ~WaveformView() override;

    void setSlot(int slot);
    // Holt die Sample-Daten des Slots neu (nach dem Laden, einem Sample- oder Slot-Wechsel).
    void refresh();

    bool hasData() const { return data_ != nullptr && !data_->samples.empty(); }
    // Text statt der Kurve, wenn keine Daten da sind; sonst leer.
    juce::String hintText() const;
    bool isMarkerVisible(Marker m) const;
    float markerX(Marker m) const;

    // Bedienung in lokalen x-Koordinaten; die Maus-Callbacks leiten hierher, Tests rufen sie direkt auf.
    void pressAt(float x);
    void dragTo(float x);
    void release();

    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;
    void mouseDown(const juce::MouseEvent& e) override { pressAt(e.position.x); }
    void mouseDrag(const juce::MouseEvent& e) override { dragTo(e.position.x); }
    void mouseUp(const juce::MouseEvent&) override { release(); }

private:
    void timerCallback() override;
    float value(SlotField f) const;
    bool loopOn() const { return value(SlotField::Loop) > 0.5f; }
    bool reverseOn() const { return value(SlotField::Reverse) > 0.5f; }
    SampleRegion region() const;
    float xFor(double position) const;
    juce::RangedAudioParameter* parameter(Marker m) const;
    bool canMove(Marker m, float direction) const;
    void begin(Marker m);
    void moveTo(Marker m, float x);
    void drawMarker(juce::Graphics& g, float x, juce::Colour colour, const char* letter) const;

    DubgefahrenProcessor& proc_;
    int slot_ = -1;
    std::shared_ptr<const SampleData> data_;
    std::vector<PeakColumn> peaks_;
    const SampleData* peaksData_ = nullptr;
    int peaksWidth_ = 0;
    std::vector<Marker> candidates_; // Marker unter dem Mausklick, solange noch keiner gewählt ist
    std::optional<Marker> active_;
    float pressX_ = 0.0f;
    float seen_[5] = {}; // zuletzt gezeichnete Parameterwerte, für das Nachführen bei Automation
    melatonin::InnerShadow inset_ { { juce::Colours::black.withAlpha(0.8f), 6, { 0, 2 } } };
};

} // namespace dg::ui
