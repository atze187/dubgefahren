#pragma once
#include <functional>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

// Einstellungen des Plugins in einem modalen Fenster: Reiter MIDI (Pad-Profile, Lernen) und Kits (Kit-Ordner).
// Alles gilt sofort und wird gespeichert; es gibt kein „Apply“.
class SettingsDialog final : public juce::Component
{
public:
    // Vom Editor bereitgestellt: Eingabe- und Rückfragedialoge im Look des Editors, Schließen des Fensters.
    struct Host
    {
        std::function<void(const juce::String& title, const juce::String& initial, std::function<void(const juce::String&)> onOk)> promptText;
        std::function<void(const juce::String& title, const juce::String& text, std::function<void()> onYes)> confirm;
        std::function<void()> close;
    };
    enum class Tab { Midi, Kits };

    SettingsDialog(DubgefahrenProcessor& proc, Host host);
    ~SettingsDialog() override;

    static constexpr int kWidth = 560;
    static constexpr int kHeight = 440;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void showTab(Tab tab);
    Tab currentTab() const { return tab_; }

    // Kits
    juce::String kitFolderText() const;
    juce::String kitWarningText() const { return kitWarning_.getText(); }
    void applyKitFolder(const juce::File& folder); // von „Change…“ nach der Auswahl aufgerufen
    void useDefaultKitFolderClicked();
    void openKitFolderClicked();

    // MIDI: Profile
    juce::StringArray profileNames() const; // liest den Ordner bei jedem Aufruf neu
    juce::String selectedProfile() const;
    void selectProfile(const juce::String& name);
    void newProfileClicked();
    void renameProfileClicked();
    void deleteProfileClicked();
    void openProfileFolderClicked();
    bool renameEnabled() const;
    bool deleteEnabled() const;
    juce::String hintText() const { return hint_.getText(); }

    // MIDI: Pad-Raster
    bool originTopLeft() const;
    void setOriginTopLeft(bool topLeft); // Schalter „Pad 1 at top left“

private:
    void refreshProfiles();
    void refreshKits();
    void chooseKitFolder();

    DubgefahrenProcessor& proc_;
    Host host_;
    Tab tab_ = Tab::Midi;

    juce::TextButton midiTab_ { "MIDI" };
    juce::TextButton kitsTab_ { "Kits" };
    juce::TextButton closeButton_ { "Close" };

    juce::ComboBox profileBox_;
    juce::TextButton newButton_ { "New" };
    juce::TextButton renameButton_ { "Rename" };
    juce::TextButton deleteButton_ { "Delete" };
    juce::TextButton folderButton_ { "Open profile folder" };
    juce::Label hint_;
    juce::ToggleButton originToggle_ { "Pad 1 at top left" };

    juce::Label kitCaption_;
    juce::Label kitPath_;
    juce::Label kitWarning_;
    juce::TextButton changeButton_ { juce::String::fromUTF8("Change…") };
    juce::TextButton openButton_ { "Open folder" };
    juce::TextButton defaultButton_ { "Use default" };
    std::unique_ptr<juce::FileChooser> chooser_;
};

} // namespace dg::ui
