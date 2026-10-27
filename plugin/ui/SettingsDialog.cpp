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
    originToggle_.setVisible(midi);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &kitCaption_, &kitPath_, &kitWarning_, &changeButton_, &openButton_, &defaultButton_ })
        c->setVisible(!midi);
    if (!midi)
        refreshKits();
    else
        originToggle_.setToggleState(originTopLeft(), juce::dontSendNotification);
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
