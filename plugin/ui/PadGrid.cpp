#include "plugin/ui/PadGrid.h"
#include "melatonin_blur/melatonin_blur.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/Controls.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/PadGlow.h"

namespace dg::ui {

namespace {
constexpr float kPadInset = 5.0f;  // Abstand des Pad-Körpers zum Rand seiner Zelle: Platz für Schatten und Glow
constexpr float kPadCorner = 6.0f;
constexpr float kGlowAlpha = 0.75f; // Deckkraft des Glows bei voller Helligkeit
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

    // Liefert true, wenn sich etwas geändert hat, das den Glow betrifft (Fokus).
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

    juce::Rectangle<float> bodyBounds() const { return getLocalBounds().toFloat().reduced(kPadInset); }

    // Glow in den Koordinaten des Rasters.
    void paintGlow(juce::Graphics& g)
    {
        const auto r = bodyBounds().translated(static_cast<float>(getX()), static_cast<float>(getY()));
        if (r.isEmpty())
            return;
        juce::Path body;
        body.addRoundedRectangle(r, kPadCorner);
        if (!empty_ && brightness_ > 0.0f)
        {
            updateGlowImage(r, g.getInternalContext().getPhysicalPixelScaleFactor());
            g.setOpacity(kGlowAlpha * brightness_);
            g.drawImage(glowImage_, r.expanded(static_cast<float>(kGlowReach)));
            g.setOpacity(1.0f);
        }
        if (focus_)
            focusGlow_.render(g, body);
    }

    // Der Glow bei voller Helligkeit, einmal pro Größe, Skalierung und Farbe gerendert. Beim
    // Animieren wird nur noch dieses Bild mit wechselnder Deckkraft gezeichnet: Ein Farbwechsel
    // am melatonin-Schatten würde ihn in jedem Frame neu zusammensetzen (gemessen ca. 1,5 ms pro Pad).
    void updateGlowImage(juce::Rectangle<float> body, float scale)
    {
        const float reach = static_cast<float>(kGlowReach);
        const int w = std::max(1, juce::roundToInt((body.getWidth() + 2.0f * reach) * scale));
        const int h = std::max(1, juce::roundToInt((body.getHeight() + 2.0f * reach) * scale));
        if (glowImage_.isValid() && glowImage_.getWidth() == w && glowImage_.getHeight() == h && glowColour_ == baseColour_)
            return;
        glowImage_ = juce::Image(juce::Image::ARGB, w, h, true);
        glowColour_ = baseColour_;
        juce::Graphics ig(glowImage_);
        ig.addTransform(juce::AffineTransform::scale(scale));
        juce::Path p;
        p.addRoundedRectangle(juce::Rectangle<float>(reach, reach, body.getWidth(), body.getHeight()), kPadCorner);
        playGlow_.setColor(baseColour_);
        playGlow_.render(ig, p);
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = bodyBounds();
        if (r.isEmpty())
            return;
        juce::Path body;
        body.addRoundedRectangle(r, kPadCorner);

        if (empty_)
        {
            // Leeres Pad: vertieft, ohne Grundfarbe und ohne Schlagschatten.
            g.setColour(colours::padEmpty);
            g.fillPath(body);
            inset_.render(g, body);
        }
        else
        {
            drop_.render(g, body);
            g.setGradientFill(juce::ColourGradient(colours::padTop, 0.0f, r.getY(), colours::padBottom, 0.0f, r.getBottom(), false));
            g.fillPath(body);
            if (!missing_)
            {
                // Schimmer der Grundfarbe am unteren Rand.
                g.setGradientFill(juce::ColourGradient(baseColour_.withAlpha(0.0f), 0.0f, r.getCentreY(), baseColour_.withAlpha(0.35f), 0.0f,
                                                       r.getBottom(), false));
                g.fillPath(body);
            }
            if (brightness_ > 0.0f)
            {
                // Spielend: Körper in der Grundfarbe, oben heller.
                g.setGradientFill(juce::ColourGradient(baseColour_.brighter(0.7f).withAlpha(brightness_), 0.0f, r.getY(),
                                                       baseColour_.withAlpha(brightness_), 0.0f, r.getBottom(), false));
                g.fillPath(body);
            }
            g.setColour(colours::highlight.withMultipliedAlpha(1.0f + brightness_));
            g.fillRect(juce::Rectangle<float>(r.getX() + kPadCorner, r.getY() + 1.0f, r.getWidth() - 2.0f * kPadCorner, 1.0f));
        }

        // Farbe zeigt Fokus, Dicke zeigt Auswahl. Ohne beides nur die dunkle Kontur.
        if (focus_ || selected_)
        {
            g.setColour(focus_ ? colours::accent : colours::text.withAlpha(0.6f));
            g.drawRoundedRectangle(r, kPadCorner, selected_ ? 2.5f : 1.5f);
        }
        else
        {
            g.setColour(colours::groove);
            g.drawRoundedRectangle(r, kPadCorner, 1.0f);
        }

        const auto text = r.reduced(6.0f);
        // Ab dem Haltewert steht der Text voll in der dunklen Farbe, damit er auf der hellen Fläche lesbar bleibt.
        const float textMix = juce::jlimit(0.0f, 1.0f, brightness_ / 0.6f);
        const auto dim = colours::textDim.interpolatedWith(colours::padTextLit, textMix);
        g.setColour(dim);
        g.setFont(font(11.0f));
        g.drawText(juce::String(slot_ + 1), text, juce::Justification::topLeft);
        // Fehlendes Sample: Name und Symbol in Warnfarbe, der Slot ist stumm.
        g.setColour(empty_ ? colours::textDim
                           : (missing_ ? colours::warning : colours::text.interpolatedWith(colours::padTextLit, textMix)));
        g.setFont(font(13.0f));
        g.drawFittedText(empty_ ? juce::String("Empty") : name_, text.toNearestInt(), juce::Justification::centred, 2);

        if (sample_ && !empty_)
        {
            g.setColour(missing_ ? colours::warning : dim);
            g.setFont(font(13.0f));
            g.drawText(u8("∿"), juce::Rectangle<float>(r.getRight() - 34.0f, r.getY() + 3.0f, 16.0f, 14.0f), juce::Justification::centred);
        }

        if (latched_)
        {
            juce::Path dot;
            dot.addEllipse(r.getRight() - 14.0f, r.getY() + 6.0f, 8.0f, 8.0f);
            dotGlow_.render(g, dot);
            g.setColour(colours::latched);
            g.fillPath(dot);
        }
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
    juce::Image glowImage_;
    juce::Colour glowColour_;
    float brightness_ = 0.0f;
    bool wasLit_ = false;
    bool latched_ = false, focus_ = false, selected_ = false, held_ = false, empty_ = false, sample_ = false, missing_ = false;
    // Als Member: melatonin cacht den berechneten Schatten im Objekt.
    // Doppelte Klammern: ein Satz Parameter { Farbe, Radius, Versatz, Spread }.
    melatonin::DropShadow drop_ { { colours::shadow, 4, { 0, 1 } } };
    melatonin::InnerShadow inset_ { { juce::Colours::black.withAlpha(0.8f), 6, { 0, 2 } } };
    melatonin::DropShadow playGlow_ { { colours::padBase, 14, { 0, 0 }, 2 } };
    melatonin::DropShadow focusGlow_ { { colours::accent.withAlpha(0.7f), 9, { 0, 0 }, 1 } };
    melatonin::DropShadow dotGlow_ { { colours::latched.withAlpha(0.9f), 5 } };
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
        pads_[static_cast<std::size_t>(s)]->setContent(proc_.slotName(s), !hasSound(source), source == SourceType::Sample,
                                                       proc_.isSampleMissing(s));
    }
    // Ein eben geleertes Pad verliert seinen Glow.
    if (onGlowChanged)
        onGlowChanged(getLocalBounds().expanded(kGlowReach));
}

bool PadGrid::isSample(int slot) const { return pads_[static_cast<std::size_t>(slot)]->isSample(); }

bool PadGrid::isMissing(int slot) const { return pads_[static_cast<std::size_t>(slot)]->isMissing(); }

juce::Component& PadGrid::pad(int slot) { return *pads_[static_cast<std::size_t>(slot)]; }

bool PadGrid::isEmpty(int slot) const { return pads_[static_cast<std::size_t>(slot)]->isEmpty(); }

void PadGrid::paintGlows(juce::Graphics& g)
{
    for (auto& p : pads_)
        p->paintGlow(g);
}

void PadGrid::glowChanged(int slot)
{
    if (onGlowChanged)
        onGlowChanged(pads_[static_cast<std::size_t>(slot)]->getBounds().expanded(kGlowReach));
}

float PadGrid::padBrightness(int slot) const { return pads_[static_cast<std::size_t>(slot)]->brightness(); }

void PadGrid::setPadBaseColour(int slot, juce::Colour colour)
{
    pads_[static_cast<std::size_t>(slot)]->setBaseColour(colour);
    glowChanged(slot);
}

juce::Colour PadGrid::padBaseColour(int slot) const { return pads_[static_cast<std::size_t>(slot)]->baseColour(); }

void PadGrid::paint(juce::Graphics& g)
{
    // Gezeichnete Marken statt Schriftzeichen: unabhängig von der Glyphen-Abdeckung der Schrift.
    auto legend = getLocalBounds().removeFromBottom(20).toFloat().withTrimmedLeft(kPadInset);
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
    item(colours::padBase, "playing", 80.0f, false);
    item(colours::accent, "focus", 80.0f, true);
    item(colours::latched, "latched", 90.0f, false);
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
