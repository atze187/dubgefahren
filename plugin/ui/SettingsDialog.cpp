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
    showTab(Tab::Midi);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel);
    g.setColour(colours::outline);
    g.drawHorizontalLine(44, 0.0f, static_cast<float>(getWidth()));
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

    // Kits
    auto kits = r;
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
