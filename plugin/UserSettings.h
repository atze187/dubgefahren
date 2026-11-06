#pragma once
#include <optional>
#include <juce_core/juce_core.h>
#include "plugin/PadMapping.h"

namespace dg {

// %APPDATA%\Dubgefahren\settings.json; in Test-Builds (DG_NO_USER_SETTINGS) leer.
juce::File userSettingsFile();

// Einstellungen des Nutzers (aktives Pad-Profil, Kit-Ordner). Eine kaputte oder fehlende Datei gilt als
// leer; Schreibfehler werden nur gemeldet, die Werte bleiben im Speicher. Nur im Message-Thread benutzen.
class UserSettings
{
public:
    explicit UserSettings(juce::File file); // leerer Pfad: nur im Speicher
    static UserSettings& instance();

    juce::String profile() const { return profile_; }
    void setProfile(const juce::String& name);
    juce::String kitFolder() const { return kitFolder_; }
    void setKitFolder(const juce::String& path); // leer entfernt den Schlüssel
    // Die alten Schlüssel firstNote/padOrigin, solange die Datei sie enthält (Migration nach Default.json).
    std::optional<PadMappingSettings> legacyPad() const { return legacy_; }

private:
    void save();

    juce::File file_;
    juce::String profile_;
    juce::String kitFolder_;
    std::optional<PadMappingSettings> legacy_;
};

} // namespace dg
