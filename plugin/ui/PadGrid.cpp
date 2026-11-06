#include "plugin/ui/PadGrid.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/Controls.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/PadGlow.h"
#include "plugin/ui/PadColours.h"

namespace dg::ui {

namespace {
constexpr int kPadInset = PadGrid::kTileInset;
constexpr int kBarHeight = 3; // Farbbalken unten auf belegten Pads
} // namespace

class PadGrid::Pad final : public juce::Component
{
public:
    Pad(PadGrid& owner, int slot) : owner_(owner), slot_(slot) {}

    ~Pad() override
    {
        if (held_)
            owner_.proc_.previewRelease(slot_);
    }

    // Liefert true, wenn sich der Fokus geändert hat.
    bool setState(bool latched, bool focus, bool selected)
    {
        if (latched == latched_ && focus == focus_ && selected == selected_)
            return false;
        const bool focusChanged = focus != focus_;
        latched_ = latched;
        focus_ = focus;
        selected_ = selected;
        repaint();
        return focusChanged;
    }

    // Schreibt die Helligkeit fort. Leere Pads und Pads mit fehlendem Sample leuchten nie.
    // Liefert true, wenn sich die Helligkeit geändert hat.
    bool advanceGlow(bool active, float dtSeconds)
    {
        const bool lit = active && !empty_ && !missing_;
        const float next = advancePadGlow(brightness_, lit, wasLit_, dtSeconds);
        wasLit_ = lit;
        if (next == brightness_)
            return false;
        brightness_ = next;
        repaint();
        return true;
    }

    void setContent(const juce::String& n, bool empty, bool sample, bool missing)
    {
        name_ = n;
        empty_ = empty;
        sample_ = sample;
        missing_ = missing;
        repaint();
    }

    bool isEmpty() const { return empty_; }
    bool isSample() const { return sample_; }
    bool isMissing() const { return missing_; }
    float brightness() const { return brightness_; }
    juce::Colour baseColour() const { return baseColour_; }

    void setBaseColour(juce::Colour c)
    {
        if (c == baseColour_)
            return;
        baseColour_ = c;
        repaint();
    }

    juce::Colour litTextColour() const { return litText_; }
    void setLitTextColour(juce::Colour c)
    {
        if (c == litText_)
            return;
        litText_ = c;
        repaint();
    }

    juce::Rectangle<int> bodyBounds() const { return getLocalBounds().reduced(kPadInset); }

    void paint(juce::Graphics& g) override
    {
        const auto r = bodyBounds();
        if (r.getWidth() < 8 || r.getHeight() < 8)
            return;

        // Flache Kachel: Leere Pads sind vertieft (LCD-Farbe), belegte tragen die Panelfarbe.
        // Beim Spielen blendet die Füllung zur Pad-Farbe.
        const auto fill = empty_ ? colours::lcd : colours::strip.interpolatedWith(baseColour_, brightness_);
        g.setColour(fill);
        g.fillRect(r);

        if (!empty_)
        {
            // Farbbalken unten in der Pad-Farbe, innerhalb der Kante.
            g.setColour(baseColour_);
            g.fillRect(juce::Rectangle<int>(r.getX() + 1, r.getBottom() - 1 - kBarHeight, r.getWidth() - 2, kBarHeight));
        }

        // Blau zeigt den Fokus, Dicke zeigt die Auswahl. Gewählt ohne Fokus: zwei Pixel in Textfarbe.
        if (focus_ || selected_)
        {
            g.setColour(focus_ ? colours::accent : colours::text);
            g.drawRect(r, selected_ ? 2 : 1);
        }
        else
        {
            g.setColour(colours::edge);
            g.drawRect(r, 1);
        }

        const auto inner = r.reduced(6, 4);
        // Ab dem Haltewert steht der Text voll in der dunklen Farbe, damit er auf der hellen Fläche lesbar bleibt.
        const float textMix = juce::jlimit(0.0f, 1.0f, brightness_ / 0.6f);
        const auto dim = colours::textDim.interpolatedWith(litText_, textMix);
        g.setColour(dim);
        g.setFont(font(10.0f));
        g.drawText(juce::String(slot_ + 1), inner, juce::Justification::topLeft, false);

        // Fehlendes Sample: Name in Warnfarbe, der Slot ist stumm.
        g.setColour(empty_ ? colours::textDim
                           : (missing_ ? colours::warning : colours::text.interpolatedWith(litText_, textMix)));
        g.setFont(font(11.0f, true));
        const auto label = inner.withTrimmedTop(12).withTrimmedBottom(kBarHeight + 1);
        // Lange Namen (bis 32 Zeichen) nutzen zwei Zeilen, bevor sie gekürzt werden.
        g.drawFittedText(empty_ ? juce::String("EMPTY") : name_.toUpperCase(), label, juce::Justification::centred, 2, 0.85f);

        if (sample_ && !empty_)
        {
            const auto mark = sampleMark();
            if (missing_)
            {
                // Flache Warnmarke: Dreieck mit Ausrufezeichen.
                juce::Path tri;
                tri.addTriangle(mark.getCentreX(), mark.getY(), mark.getRight(), mark.getBottom(), mark.getX(), mark.getBottom());
                g.setColour(colours::warning);
                g.fillPath(tri);
                g.setColour(colours::lcd);
                g.fillRect(mark.getCentreX() - 1.0f, mark.getY() + 5.0f, 2.0f, 4.0f);
                g.fillRect(mark.getCentreX() - 1.0f, mark.getBottom() - 3.0f, 2.0f, 2.0f);
            }
            else
            {
                g.setColour(dim);
                g.setFont(font(13.0f));
                g.drawText(u8("∿"), mark, juce::Justification::centred);
            }
        }

        if (latched_)
        {
            // Heller Kern mit dunklem Ring: lesbar auf jeder Pad-Farbe, gedimmt wie leuchtend.
            const juce::Rectangle<float> outer(static_cast<float>(r.getRight() - 15), static_cast<float>(r.getY() + 5), 10.0f, 10.0f);
            g.setColour(latchDotRing);
            g.fillEllipse(outer);
            g.setColour(latchDotCore);
            g.fillEllipse(outer.reduced(1.5f));
        }
    }

    // Ort der Sample-/Warnmarke oben rechts.
    juce::Rectangle<float> sampleMark() const
    {
        const auto r = bodyBounds();
        return { static_cast<float>(r.getRight() - 34), static_cast<float>(r.getY() + 3), 16.0f, 14.0f };
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (held_)
            {
                owner_.proc_.previewRelease(slot_);
                held_ = false;
            }
            if (owner_.onContextMenu)
                owner_.onContextMenu(slot_);
            return;
        }
        if (empty_)
        {
            // Leerer Slot: keine Vorschau, sondern Slot wählen und Quellen-Auswahl öffnen.
            if (owner_.onSelect)
                owner_.onSelect(slot_);
            if (owner_.onEmptyClick)
                owner_.onEmptyClick(slot_);
            return;
        }
        held_ = true;
        owner_.proc_.previewPress(slot_);
        if (owner_.onSelect)
            owner_.onSelect(slot_);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (held_)
            owner_.proc_.previewRelease(slot_);
        held_ = false;
    }

private:
    PadGrid& owner_;
    const int slot_;
    juce::String name_;
    juce::Colour baseColour_ { colours::padBase };
    juce::Colour litText_ { colours::padTextLit };
    float brightness_ = 0.0f;
    bool wasLit_ = false;
    bool latched_ = false, focus_ = false, selected_ = false, held_ = false, empty_ = false, sample_ = false, missing_ = false;
};

PadGrid::PadGrid(DubgefahrenProcessor& proc) : proc_(proc)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        pads_[static_cast<std::size_t>(s)] = std::make_unique<Pad>(*this, s);
        addAndMakeVisible(*pads_[static_cast<std::size_t>(s)]);
    }
    refreshNames();
}

PadGrid::~PadGrid() = default;

void PadGrid::setPadStates(std::uint32_t active, std::uint32_t latched, int focus)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = lastTickMs_ > 0.0 ? static_cast<float>((now - lastTickMs_) / 1000.0) : 0.0f;
    lastTickMs_ = now;

    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& pad = *pads_[static_cast<std::size_t>(s)];
        const bool selected = pad.getProperties()["selected"];
        const bool focusChanged = pad.setState((latched >> s) & 1u, s == focus, selected);
        const bool glowMoved = pad.advanceGlow((active >> s) & 1u, dt);
        if (focusChanged || glowMoved)
            glowChanged(s);
    }
}

void PadGrid::setSelected(int slot)
{
    for (int s = 0; s < kNumSlots; ++s)
        pads_[static_cast<std::size_t>(s)]->getProperties().set("selected", s == slot);
    setPadStates(proc_.activeMask(), proc_.latchedMask(), proc_.focusSlot());
    for (auto& p : pads_)
        p->repaint();
}

bool PadGrid::setOrigin(PadOrigin origin)
{
    if (origin == origin_)
        return false;
    origin_ = origin;
    resized();
    repaint();
    return true;
}

void PadGrid::refreshNames()
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto source = proc_.slotSource(s);
        auto& pad = *pads_[static_cast<std::size_t>(s)];
        pad.setContent(proc_.slotName(s), !hasSound(source), source == SourceType::Sample, proc_.isSampleMissing(s));
        const auto& colour = padColour(proc_.slotColour(s));
        if (pad.baseColour() != colour.base)
        {
            pad.setBaseColour(colour.base);
            glowChanged(s);
        }
        pad.setLitTextColour(colour.litText);
    }
    // Ein eben geleertes Pad verliert seine Füllung.
    if (onGlowChanged)
        onGlowChanged(getLocalBounds());
}

bool PadGrid::isSample(int slot) const { return pads_[static_cast<std::size_t>(slot)]->isSample(); }

bool PadGrid::isMissing(int slot) const { return pads_[static_cast<std::size_t>(slot)]->isMissing(); }

juce::Component& PadGrid::pad(int slot) { return *pads_[static_cast<std::size_t>(slot)]; }

bool PadGrid::isEmpty(int slot) const { return pads_[static_cast<std::size_t>(slot)]->isEmpty(); }

void PadGrid::glowChanged(int slot)
{
    if (onGlowChanged)
        onGlowChanged(pads_[static_cast<std::size_t>(slot)]->getBounds());
}

float PadGrid::padBrightness(int slot) const { return pads_[static_cast<std::size_t>(slot)]->brightness(); }

void PadGrid::setPadBaseColour(int slot, juce::Colour colour)
{
    pads_[static_cast<std::size_t>(slot)]->setBaseColour(colour);
    glowChanged(slot);
}

juce::Colour PadGrid::padBaseColour(int slot) const { return pads_[static_cast<std::size_t>(slot)]->baseColour(); }

juce::Colour PadGrid::padLitTextColour(int slot) const { return pads_[static_cast<std::size_t>(slot)]->litTextColour(); }

void PadGrid::paint(juce::Graphics& g)
{
    // Gezeichnete Marken statt Schriftzeichen: unabhängig von der Glyphen-Abdeckung der Schrift.
    auto legend = getLocalBounds().removeFromBottom(20).toFloat().withTrimmedLeft(static_cast<float>(kPadInset));
    g.setFont(font(11.0f));
    const auto item = [&g, &legend](juce::Colour colour, const char* text, float width, bool diamond) {
        auto cell = legend.removeFromLeft(width);
        const auto mark = cell.removeFromLeft(12.0f).withSizeKeepingCentre(7.0f, 7.0f);
        g.setColour(colour);
        if (diamond)
        {
            juce::Path p;
            p.addQuadrilateral(mark.getCentreX(), mark.getY(), mark.getRight(), mark.getCentreY(), mark.getCentreX(), mark.getBottom(),
                               mark.getX(), mark.getCentreY());
            g.fillPath(p);
        }
        else
            g.fillEllipse(mark);
        g.setColour(colours::textDim);
        g.drawText(text, cell, juce::Justification::centredLeft);
    };
    item(colours::padBase, "PLAYING", 80.0f, false);
    item(colours::accent, "FOCUS", 80.0f, true);
    {
        // Wie der Punkt auf dem Pad: heller Kern mit dunklem Ring.
        auto cell = legend.removeFromLeft(90.0f);
        const auto mark = cell.removeFromLeft(12.0f).withSizeKeepingCentre(9.0f, 9.0f);
        g.setColour(latchDotRing);
        g.fillEllipse(mark);
        g.setColour(latchDotCore);
        g.fillEllipse(mark.reduced(1.5f));
        g.setColour(colours::textDim);
        g.drawText("LATCHED", cell, juce::Justification::centredLeft);
    }
}

void PadGrid::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(24);
    const int w = area.getWidth() / 4;
    const int h = area.getHeight() / 4;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int col = s % 4;
        const int row = s / 4; // 0 = die Reihe, in der Pad 1 liegt
        // Unten links: Pad 1 in der untersten Reihe; oben links: in der obersten (wie die Tasten des BU16).
        const int y = origin_ == PadOrigin::TopLeft ? row : 3 - row;
        pads_[static_cast<std::size_t>(s)]->setBounds(area.getX() + col * w, area.getY() + y * h, w, h);
    }
}

} // namespace dg::ui
