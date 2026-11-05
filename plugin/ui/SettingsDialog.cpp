#include "plugin/ui/SettingsDialog.h"
#include "plugin/PadMapping.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"

namespace dg::ui {

SettingsDialog::SettingsDialog(DubgefahrenProcessor& proc, Host host) : proc_(proc), host_(std::move(host))
{
    setSize(kWidth, kHeight);

    for (auto* tab : { &midiTab_, &kitsTab_ })
    {
        tab->setRadioGroupId(1);
        tab->setClickingTogglesState(true);
        tab->setColour(juce::TextButton::buttonOnColourId, colours::accent);
        tab->setColour(juce::TextButton::textColourOnId, colours::padTextLit);
        addAndMakeVisible(*tab);
    }
    midiTab_.onClick = [this] { showTab(Tab::Midi); };
    kitsTab_.onClick = [this] { showTab(Tab::Kits); };
    closeButton_.onClick = [this] {
        if (host_.close)
            host_.close();
    };
    addAndMakeVisible(closeButton_);

    profileBox_.onChange = [this] {
        if (const auto name = profileBox_.getText(); name.isNotEmpty() && name != selectedProfile())
            selectProfile(name);
    };
    newButton_.onClick = [this] { newProfileClicked(); };
    renameButton_.onClick = [this] { renameProfileClicked(); };
    deleteButton_.onClick = [this] { deleteProfileClicked(); };
    folderButton_.onClick = [this] { openProfileFolderClicked(); };
    hint_.setFont(font(13.0f));
    hint_.setColour(juce::Label::textColourId, colours::warning);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &profileBox_, &newButton_, &renameButton_, &deleteButton_, &folderButton_, &hint_ })
        addChildComponent(*c);

    originToggle_.setToggleState(originTopLeft(), juce::dontSendNotification);
    originToggle_.onClick = [this] { setOriginTopLeft(originToggle_.getToggleState()); };
    addChildComponent(originToggle_);

    kitCaption_.setText("Kit folder", juce::dontSendNotification);
    kitCaption_.setFont(font(13.0f, true));
    kitCaption_.setColour(juce::Label::textColourId, colours::textDim);
    kitPath_.setFont(font(14.0f));
    kitPath_.setMinimumHorizontalScale(1.0f);
    kitPath_.setColour(juce::Label::outlineColourId, colours::outline);
    kitPath_.setBorderSize({ 4, 8, 4, 8 });
    kitWarning_.setFont(font(13.0f));
    kitWarning_.setColour(juce::Label::textColourId, colours::warning);
    changeButton_.onClick = [this] { chooseKitFolder(); };
    openButton_.onClick = [this] { openKitFolderClicked(); };
    defaultButton_.onClick = [this] { useDefaultKitFolderClicked(); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &kitCaption_, &kitPath_, &kitWarning_, &changeButton_, &openButton_, &defaultButton_ })
        addChildComponent(*c);

    refreshKits();
    setWantsKeyboardFocus(true);
    showTab(Tab::Midi);
    refreshFromMapping();
    startTimerHz(30);
}

SettingsDialog::~SettingsDialog() { stopLearning(); }

void SettingsDialog::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel);
    g.setColour(colours::outline);
    g.drawHorizontalLine(44, 0.0f, static_cast<float>(getWidth()));
    if (tab_ == Tab::Midi)
        for (int s = 0; s < 16; ++s)
        {
            const juce::Rectangle<float> cell(padCellPosition(s).toFloat(), juce::Point<float>(padCellPosition(s).toFloat()) + juce::Point<float>(kCellWidth, kCellHeight));
            const bool learningCell = s == learning_;
            const bool assigned = PadMapping::instance().noteMap().noteForSlot(s) >= 0;
            g.setColour(assigned ? colours::padTop : colours::padEmpty);
            g.fillRoundedRectangle(cell, 6.0f);
            g.setColour(learningCell ? colours::accent : assigned ? colours::padBase.withAlpha(0.5f) : colours::outline);
            g.drawRoundedRectangle(cell.reduced(0.5f), 6.0f, learningCell ? 2.0f : 1.0f);

            const auto lines = juce::StringArray::fromLines(padCellText(s));
            g.setColour(colours::textDim);
            g.setFont(font(11.0f));
            g.drawText(lines[0], cell.withTrimmedBottom(cell.getHeight() / 2.0f).reduced(8.0f, 0.0f), juce::Justification::bottomLeft);
            g.setColour(learningCell ? colours::accent : assigned ? colours::padBase : colours::textDim);
            g.setFont(font(learningCell ? 14.0f : 18.0f, true));
            g.drawText(lines[1], cell.withTrimmedTop(cell.getHeight() / 2.0f).reduced(8.0f, 0.0f), juce::Justification::centred);
        }
}

void SettingsDialog::resized()
{
    auto r = getLocalBounds().reduced(16);
    auto tabs = r.removeFromTop(28);
    midiTab_.setBounds(tabs.removeFromLeft(90));
    tabs.removeFromLeft(6);
    kitsTab_.setBounds(tabs.removeFromLeft(90));
    r.removeFromTop(20);

    closeButton_.setBounds(r.removeFromBottom(30).removeFromRight(90));
    r.removeFromBottom(8);

    auto kits = r; // der Kits-Reiter nutzt dieselbe Fläche wie der MIDI-Reiter

    // MIDI
    auto profileRow = r.removeFromTop(30);
    newButton_.setBounds(profileRow.removeFromRight(60));
    profileRow.removeFromRight(6);
    deleteButton_.setBounds(profileRow.removeFromRight(70));
    profileRow.removeFromRight(6);
    renameButton_.setBounds(profileRow.removeFromRight(70));
    profileRow.removeFromRight(10);
    profileBox_.setBounds(profileRow);
    r.removeFromTop(6);
    auto hintRow = r.removeFromTop(26);
    folderButton_.setBounds(hintRow.removeFromLeft(160));
    hintRow.removeFromLeft(10);
    hint_.setBounds(hintRow);
    r.removeFromTop(8);
    originToggle_.setBounds(r.removeFromBottom(26));
    r.removeFromBottom(6);
    gridOrigin_ = { r.getX() + (r.getWidth() - kGridWidth) / 2, r.getY() + (r.getHeight() - kGridHeight) / 2 };

    // Kits
    kitCaption_.setBounds(kits.removeFromTop(20));
    kitPath_.setBounds(kits.removeFromTop(30));
    kits.removeFromTop(6);
    kitWarning_.setBounds(kits.removeFromTop(36));
    kits.removeFromTop(8);
    auto buttons = kits.removeFromTop(30);
    changeButton_.setBounds(buttons.removeFromLeft(100));
    buttons.removeFromLeft(8);
    openButton_.setBounds(buttons.removeFromLeft(110));
    buttons.removeFromLeft(8);
    defaultButton_.setBounds(buttons.removeFromLeft(110));
}

void SettingsDialog::showTab(Tab tab)
{
    tab_ = tab;
    stopLearning();
    repaint(); // das Pad-Raster malt der Dialog selbst und muss beim Reiterwechsel neu gezeichnet werden
    const bool midi = tab == Tab::Midi;
    midiTab_.setToggleState(midi, juce::dontSendNotification);
    kitsTab_.setToggleState(!midi, juce::dontSendNotification);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &profileBox_, &newButton_, &renameButton_, &deleteButton_, &folderButton_, &hint_, &originToggle_ })
        c->setVisible(midi);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &kitCaption_, &kitPath_, &kitWarning_, &changeButton_, &openButton_, &defaultButton_ })
        c->setVisible(!midi);
    if (!midi)
        refreshKits();
    else
    {
        refreshProfiles();
        originToggle_.setToggleState(originTopLeft(), juce::dontSendNotification);
    }
}

juce::String SettingsDialog::kitFolderText() const
{
    return proc_.kitFolder().folder.getFullPathName();
}

void SettingsDialog::refreshKits()
{
    kitPath_.setText(kitFolderText(), juce::dontSendNotification);
    kitWarning_.setText(proc_.kitFolder().warning, juce::dontSendNotification);
}

void SettingsDialog::applyKitFolder(const juce::File& folder)
{
    proc_.setKitFolder(folder);
    refreshKits();
}

void SettingsDialog::useDefaultKitFolderClicked()
{
    proc_.useDefaultKitFolder();
    refreshKits();
}

void SettingsDialog::openKitFolderClicked()
{
    proc_.kitFolder().folder.startAsProcess();
}

void SettingsDialog::chooseKitFolder()
{
    chooser_ = std::make_unique<juce::FileChooser>("Kit folder", proc_.kitFolder().folder);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe = juce::Component::SafePointer<SettingsDialog>(this)](const juce::FileChooser& fc) {
                              if (safe != nullptr && fc.getResult() != juce::File())
                                  safe->applyKitFolder(fc.getResult());
                          });
}

juce::StringArray SettingsDialog::profileNames() const
{
    return PadMapping::instance().profileNames();
}

juce::String SettingsDialog::selectedProfile() const
{
    return PadMapping::instance().activeProfile();
}

bool SettingsDialog::renameEnabled() const { return !selectedProfile().equalsIgnoreCase("Default"); }
bool SettingsDialog::deleteEnabled() const { return renameEnabled(); }

void SettingsDialog::refreshProfiles()
{
    const auto names = profileNames();
    profileBox_.clear(juce::dontSendNotification);
    for (int i = 0; i < names.size(); ++i)
        profileBox_.addItem(names[i], i + 1);
    profileBox_.setSelectedItemIndex(names.indexOf(selectedProfile()), juce::dontSendNotification);
    renameButton_.setEnabled(renameEnabled());
    deleteButton_.setEnabled(deleteEnabled());
}

void SettingsDialog::selectProfile(const juce::String& name)
{
    PadMapping::instance().selectProfile(name);
    hint_.setText({}, juce::dontSendNotification);
    refreshProfiles();
}

void SettingsDialog::newProfileClicked()
{
    PadMapping::instance().createProfile();
    hint_.setText({}, juce::dontSendNotification);
    refreshProfiles();
}

void SettingsDialog::renameProfileClicked()
{
    if (!renameEnabled() || !host_.promptText)
        return;
    host_.promptText("Rename profile", selectedProfile(), [safe = juce::Component::SafePointer<SettingsDialog>(this)](const juce::String& name) {
        if (safe == nullptr)
            return;
        const bool ok = PadMapping::instance().renameActiveProfile(name);
        safe->hint_.setText(ok ? juce::String() : juce::String("Name not allowed or already taken"), juce::dontSendNotification);
        safe->refreshProfiles();
    });
}

void SettingsDialog::deleteProfileClicked()
{
    if (!deleteEnabled() || !host_.confirm)
        return;
    host_.confirm("Delete profile", "Delete profile " + selectedProfile() + "?", [safe = juce::Component::SafePointer<SettingsDialog>(this)] {
        if (safe == nullptr)
            return;
        PadMapping::instance().deleteActiveProfile();
        safe->hint_.setText({}, juce::dontSendNotification);
        safe->refreshProfiles();
    });
}

void SettingsDialog::openProfileFolderClicked()
{
    const auto folder = PadMapping::instance().profilesFolder();
    folder.createDirectory();
    folder.startAsProcess();
}

juce::String SettingsDialog::padCellText(int slot) const
{
    const juce::String number(slot + 1);
    if (slot == learning_)
        return number + "\n" + juce::String::fromUTF8("press…");
    const int note = PadMapping::instance().noteMap().noteForSlot(slot);
    return number + "\n" + (note >= 0 ? juce::String(note) : juce::String("-"));
}

juce::Point<int> SettingsDialog::padCellPosition(int slot) const
{
    const int column = slot % 4;
    const int row = slot / 4;
    const int visualRow = originTopLeft() ? row : 3 - row; // Pad 1 unten (Standard) oder oben links
    return { gridOrigin_.x + column * (kCellWidth + kCellGap), gridOrigin_.y + visualRow * (kCellHeight + kCellGap) };
}

int SettingsDialog::cellAt(juce::Point<int> p) const
{
    for (int s = 0; s < 16; ++s)
        if (juce::Rectangle<int>(padCellPosition(s), padCellPosition(s) + juce::Point<int>(kCellWidth, kCellHeight)).contains(p))
            return s;
    return -1;
}

void SettingsDialog::startLearning(int slot)
{
    if (slot < 0 || slot >= 16)
        return;
    if (slot == learning_)
    {
        stopLearning();
        return;
    }
    proc_.armMidiLearn(); // setzt eine noch nicht abgeholte Note zurück
    learning_ = slot;
    grabKeyboardFocus();
    repaint();
}

void SettingsDialog::stopLearning()
{
    proc_.disarmMidiLearn();
    if (learning_ >= 0)
    {
        learning_ = -1;
        repaint();
    }
}

void SettingsDialog::pollLearning()
{
    if (learning_ < 0)
        return;
    if (const auto note = proc_.takeLearnedNote())
    {
        PadMapping::instance().setPadNote(learning_, *note);
        learning_ = -1;
        refreshFromMapping();
        repaint();
    }
}

void SettingsDialog::refreshFromMapping()
{
    const int revision = PadMapping::instance().revision();
    if (revision == lastRevision_)
        return;
    lastRevision_ = revision;
    refreshProfiles();
    originToggle_.setToggleState(originTopLeft(), juce::dontSendNotification);
    repaint();
}

void SettingsDialog::timerCallback()
{
    pollLearning();
    refreshFromMapping();
}

bool SettingsDialog::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && learning_ >= 0)
    {
        stopLearning();
        return true;
    }
    return false;
}

void SettingsDialog::mouseDown(const juce::MouseEvent& e)
{
    if (tab_ != Tab::Midi)
        return;
    if (const int slot = cellAt(e.getPosition()); slot >= 0)
        startLearning(slot);
}

bool SettingsDialog::originTopLeft() const
{
    return PadMapping::instance().origin() == PadOrigin::TopLeft;
}

void SettingsDialog::setOriginTopLeft(bool topLeft)
{
    PadMapping::instance().setOrigin(topLeft ? PadOrigin::TopLeft : PadOrigin::BottomLeft);
    originToggle_.setToggleState(topLeft, juce::dontSendNotification);
}

} // namespace dg::ui
