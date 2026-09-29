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
const char* const kRowNames[kNumRows] = { "OSC\nAMP", "LFO", "SWEEP\nTRIG", "MIX" };
constexpr int kNumSampleRows = 3;
const char* const kSampleRowNames[kNumSampleRows] = { "SAMPLE\nAMP", "TRIG", "MIX" };
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

    controls_ = { &wave_, &pitch_, &pw_, &lfoShape_, &lfoRate_, &lfoSync_, &lfoSyncDiv_, &lfoDepth_, &sweepAmt_,
                  &sweepTime_, &attack_, &release_, &trigMode_, &oneShot_, &choke_, &vol_, &pan_, &send_ };
    for (auto* c : controls_)
        addAndMakeVisible(*c);

    emptyHint_.setText(u8("Empty slot – click the pad to choose a sound source"), juce::dontSendNotification);
    emptyHint_.setJustificationType(juce::Justification::centred);
    emptyHint_.setColour(juce::Label::textColourId, colours::textDim);
    emptyHint_.setFont(juce::FontOptions(15.0f));
    addChildComponent(emptyHint_);

    sampleControls_ = { &tune_, &attack_, &release_, &trigMode_, &choke_, &vol_, &pan_, &send_ };
    addChildComponent(tune_);
    sampleButton_.onClick = [this] {
        if (onChooseSample)
            onChooseSample();
    };
    addChildComponent(sampleButton_);
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
    tune_.attach(s, slotParamId(slot, SlotField::Tune));
    refresh();
}

void SlotEditor::refresh()
{
    const auto source = proc_.slotSource(slot_);
    mode_ = source == SourceType::Empty ? Mode::Empty : source == SourceType::Sample ? Mode::Sample : Mode::Synth;
    const bool empty = mode_ == Mode::Empty;
    const auto title = "Slot " + juce::String(slot_ + 1);
    header_.setText(empty ? title : title + u8(" · ") + proc_.slotName(slot_), juce::dontSendNotification);
    emptyHint_.setVisible(empty);
    renameButton_.setVisible(!empty);

    for (auto* c : controls_)
        c->setVisible(mode_ == Mode::Synth);
    tune_.setVisible(false);
    if (mode_ == Mode::Sample)
        for (auto* c : sampleControls_)
            c->setVisible(true);
    sampleButton_.setVisible(mode_ == Mode::Sample);
    const bool missing = proc_.isSampleMissing(slot_);
    sampleButton_.setButtonText(proc_.slotSample(slot_) + (missing ? " (missing)" : ""));
    if (missing)
        sampleButton_.setColour(juce::TextButton::textColourOffId, colours::warning);
    else
        sampleButton_.removeColour(juce::TextButton::textColourOffId);
    trigMode_.box.setItemEnabled(kLatchItemId, mode_ != Mode::Sample); // Latch wirkt bei Samples wie Gate
    resized();
    repaint(); // Zeilenbeschriftungen
}

void SlotEditor::paint(juce::Graphics& g)
{
    g.setColour(colours::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    if (emptyHint_.isVisible())
        return;
    g.setColour(colours::textDim);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    const bool sample = mode_ == Mode::Sample;
    const int rows = sample ? kNumSampleRows : kNumRows;
    for (int row = 0; row < rows; ++row)
        g.drawFittedText(sample ? kSampleRowNames[row] : kRowNames[row], 12, kTopOffset + row * kRowHeight,
                         kRowLabelWidth - 12, kRowHeight, juce::Justification::centredLeft, 2);
}

void SlotEditor::resized()
{
    auto top = getLocalBounds().reduced(12, 8).removeFromTop(32);
    renameButton_.setBounds(top.removeFromRight(110));
    top.removeFromRight(8);
    sampleButton_.setBounds(top.removeFromRight(180));
    header_.setBounds(top);

    const auto place = [this](int row, int col, juce::Component& c) {
        c.setBounds(kRowLabelWidth + col * kCellWidth, kTopOffset + row * kRowHeight, kCellWidth - 8, kRowHeight - 6);
    };
    if (mode_ == Mode::Sample)
    {
        place(0, 0, tune_);     place(0, 1, attack_); place(0, 2, release_);
        place(1, 0, trigMode_); place(1, 1, choke_);
        place(2, 0, vol_);      place(2, 1, pan_);    place(2, 2, send_);
    }
    else
    {
        place(0, 0, wave_);     place(0, 1, pitch_);     place(0, 2, pw_);       place(0, 3, attack_);     place(0, 4, release_);
        place(1, 0, lfoShape_); place(1, 1, lfoRate_);   place(1, 2, lfoSync_);  place(1, 3, lfoSyncDiv_); place(1, 4, lfoDepth_);
        place(2, 0, sweepAmt_); place(2, 1, sweepTime_); place(2, 2, trigMode_); place(2, 3, oneShot_);    place(2, 4, choke_);
        place(3, 0, vol_);      place(3, 1, pan_);       place(3, 2, send_);
    }

    emptyHint_.setBounds(getLocalBounds().withTrimmedTop(kTopOffset).reduced(24));
}

} // namespace dg::ui
