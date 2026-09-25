#include "plugin/ui/SlotEditor.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

namespace {
// 4 Zeilen passen in die verfügbare Höhe von 384 px (Basisgröße 1000 × 640,
// abzüglich 24 px Rand, 36 px Kopfzeile, 84 px Performance-Leiste und 2x 8 px Abstand
// plus 96 px FX-Panel): kTopOffset + 4 * kRowHeight = 40 + 4*84 = 376 <= 384.
constexpr int kNumRows = 4;
constexpr int kTopOffset = 40;
constexpr int kRowHeight = 84;
constexpr int kRowLabelWidth = 64;
constexpr int kCellWidth = 104;
const char* const kRowNames[kNumRows] = { "OSZ\nAMP", "LFO", "SWEEP\nTRIG", "MIX" };
} // namespace

SlotEditor::SlotEditor(DubgefahrenProcessor& proc) : proc_(proc)
{
    header_.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    header_.setColour(juce::Label::textColourId, colours::text);
    addAndMakeVisible(header_);
    renameButton_.onClick = [this] {
        if (onRename)
            onRename();
    };
    addAndMakeVisible(renameButton_);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &wave_, &pitch_, &pw_, &lfoShape_, &lfoRate_, &lfoSync_, &lfoSyncDiv_, &lfoDepth_, &sweepAmt_,
             &sweepTime_, &attack_, &release_, &trigMode_, &oneShot_, &choke_, &vol_, &pan_, &send_ })
        addAndMakeVisible(*c);
}

void SlotEditor::setSlot(int slot)
{
    if (slot == slot_)
        return;
    slot_ = slot;
    auto& s = proc_.state();
    wave_.attach(s, slotParamId(slot, SlotField::Wave));
    pitch_.attach(s, slotParamId(slot, SlotField::Pitch));
    pw_.attach(s, slotParamId(slot, SlotField::PulseWidth));
    lfoShape_.attach(s, slotParamId(slot, SlotField::LfoShape));
    lfoRate_.attach(s, slotParamId(slot, SlotField::LfoRate));
    lfoSync_.attach(s, slotParamId(slot, SlotField::LfoSync));
    lfoSyncDiv_.attach(s, slotParamId(slot, SlotField::LfoSyncDiv));
    lfoDepth_.attach(s, slotParamId(slot, SlotField::LfoDepth));
    sweepAmt_.attach(s, slotParamId(slot, SlotField::SweepAmount));
    sweepTime_.attach(s, slotParamId(slot, SlotField::SweepTime));
    attack_.attach(s, slotParamId(slot, SlotField::Attack));
    release_.attach(s, slotParamId(slot, SlotField::Release));
    trigMode_.attach(s, slotParamId(slot, SlotField::TrigMode));
    oneShot_.attach(s, slotParamId(slot, SlotField::OneShotLength));
    choke_.attach(s, slotParamId(slot, SlotField::Choke));
    vol_.attach(s, slotParamId(slot, SlotField::Volume));
    pan_.attach(s, slotParamId(slot, SlotField::Pan));
    send_.attach(s, slotParamId(slot, SlotField::FxSend));
    refreshName();
}

void SlotEditor::refreshName()
{
    header_.setText("Slot " + juce::String(slot_ + 1) + u8(" · ") + proc_.slotName(slot_), juce::dontSendNotification);
}

void SlotEditor::paint(juce::Graphics& g)
{
    g.setColour(colours::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(colours::textDim);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    for (int row = 0; row < kNumRows; ++row)
        g.drawFittedText(kRowNames[row], 12, kTopOffset + row * kRowHeight, kRowLabelWidth - 12, kRowHeight,
                         juce::Justification::centredLeft, 2);
}

void SlotEditor::resized()
{
    auto top = getLocalBounds().reduced(12, 8).removeFromTop(32);
    renameButton_.setBounds(top.removeFromRight(110));
    header_.setBounds(top);

    const auto place = [this](int row, int col, juce::Component& c) {
        c.setBounds(kRowLabelWidth + col * kCellWidth, kTopOffset + row * kRowHeight, kCellWidth - 8, kRowHeight - 6);
    };
    place(0, 0, wave_);     place(0, 1, pitch_);     place(0, 2, pw_);       place(0, 3, attack_);     place(0, 4, release_);
    place(1, 0, lfoShape_); place(1, 1, lfoRate_);   place(1, 2, lfoSync_);  place(1, 3, lfoSyncDiv_); place(1, 4, lfoDepth_);
    place(2, 0, sweepAmt_); place(2, 1, sweepTime_); place(2, 2, trigMode_); place(2, 3, oneShot_);    place(2, 4, choke_);
    place(3, 0, vol_);      place(3, 1, pan_);       place(3, 2, send_);
}

} // namespace dg::ui
