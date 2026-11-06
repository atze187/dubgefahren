#pragma once
#include <array>
#include <optional>
#include <juce_core/juce_core.h>
#include "engine/SlotParams.h"

namespace dg {

enum class PadOrigin { BottomLeft, TopLeft };

// Belegung der 16 Pads mit MIDI-Noten, wie sie als Datei <Name>.json im Profilordner liegt.
struct PadProfile
{
    juce::String name;
    std::array<int, kNumSlots> notes {}; // -1 = unbelegt
    PadOrigin origin = PadOrigin::BottomLeft;
    bool operator==(const PadProfile&) const = default;
};

// Noten firstNote..firstNote+15; was über 127 hinausragt, bleibt unbelegt.
PadProfile makeConsecutiveProfile(const juce::String& name, int firstNote, PadOrigin origin);
// {"padOrigin":"bottomLeft"|"topLeft","notes":[36,37,…,null]}; der Name steht nicht in der Datei.
juce::String padProfileToJson(const PadProfile& profile);
// Dateien von Dritten werden repariert statt abgelehnt; nullopt nur, wenn kein Objekt mit "notes"-Array vorliegt.
std::optional<PadProfile> parsePadProfile(const juce::String& name, const juce::String& json);
bool isValidProfileName(const juce::String& name);

// Die Profile eines Ordners: eine Datei <Name>.json je Profil.
class ProfileStore
{
public:
    explicit ProfileStore(juce::File folder);

    juce::File folder() const { return folder_; }
    juce::StringArray names() const; // "Default" zuerst, der Rest alphabetisch ohne Beachtung der Schreibweise
    bool contains(const juce::String& name) const;
    std::optional<PadProfile> load(const juce::String& name) const;
    bool save(const PadProfile& profile);
    bool remove(const juce::String& name);
    bool rename(const juce::String& from, const juce::String& to);

private:
    juce::File fileFor(const juce::String& name) const;
    juce::String findName(const juce::String& name) const; // tatsächliche Schreibweise oder leer

    juce::File folder_;
};

} // namespace dg
