#pragma once
#include <atomic>
#include <juce_core/juce_core.h>
#include "engine/NoteMap.h"
#include "engine/PadRouter.h"
#include "plugin/PadProfile.h"

namespace dg {

class UserSettings;

constexpr int kDefaultPadFirstNote = 36;

// Die alten Einstellungen (firstNote, padOrigin) aus settings.json; nur noch für die Migration nach Default.json.
struct PadMappingSettings
{
    int firstNote = kDefaultPadFirstNote; // 0 .. kMaxPadFirstNote
    PadOrigin origin = PadOrigin::BottomLeft;
    bool operator==(const PadMappingSettings&) const = default;
};

// Liest die Einstellungen aus JSON-Text. Fehlt ein Wert oder ist er ungültig, gilt für ihn der Standard.
PadMappingSettings parsePadMappingSettings(const juce::String& json);

// <Ordner von settings.json>/Profiles; in Test-Builds (DG_NO_USER_SETTINGS) ein Temp-Ordner, der beim Beenden verschwindet.
juce::File padProfilesFolder();

// Prozessweit geteilte Pad-Belegung: Alle Instanzen im Host lesen dieselbe NoteMap, der Audio-Thread
// ohne Sperre. Alles außer noteMap() und origin() nur im Message-Thread aufrufen.
class PadMapping
{
public:
    PadMapping(juce::File profilesFolder, UserSettings& settings);
    static PadMapping& instance();

    const NoteMap& noteMap() const noexcept { return noteMap_; }
    PadOrigin origin() const { return static_cast<PadOrigin>(origin_.load(std::memory_order_relaxed)); }
    PadProfile profile() const { return active_; }
    juce::String activeProfile() const { return active_.name; }
    // Steigt bei jeder Änderung (Auswahl, Note, Ursprung, Anlegen, Umbenennen, Löschen), damit Dialoge neu zeichnen.
    int revision() const { return revision_.load(std::memory_order_relaxed); }
    juce::StringArray profileNames() const { return store_.names(); }
    juce::File profilesFolder() const { return store_.folder(); }

    bool selectProfile(const juce::String& name);
    juce::String createProfile(); // "New profile" (bei Bedarf "New profile 2", …), wie Default belegt; wird aktiv
    bool renameActiveProfile(const juce::String& newName);
    bool deleteActiveProfile(); // wählt danach Default
    void setPadNote(int slot, int note); // -1 = unbelegt; speichert das aktive Profil
    void setOrigin(PadOrigin origin);

private:
    void activate(const PadProfile& profile);
    void saveActive();

    ProfileStore store_;
    UserSettings& settings_;
    NoteMap noteMap_;
    PadProfile active_;
    std::atomic<int> origin_ { static_cast<int>(PadOrigin::BottomLeft) };
    std::atomic<int> revision_ { 0 };
};

} // namespace dg
