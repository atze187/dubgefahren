#pragma once
#include <atomic>
#include <optional>
#include <juce_core/juce_core.h>
#include "engine/PadRouter.h"

namespace dg {

enum class PadOrigin { BottomLeft, TopLeft };

constexpr int kDefaultPadFirstNote = 36;

struct PadMappingSettings
{
    int firstNote = kDefaultPadFirstNote; // 0 .. kMaxPadFirstNote
    PadOrigin origin = PadOrigin::BottomLeft;
    bool operator==(const PadMappingSettings&) const = default;
};

// Liest die Einstellungen aus JSON-Text. Fehlt ein Wert oder ist er ungültig, gilt für ihn der Standard.
PadMappingSettings parsePadMappingSettings(const juce::String& json);
juce::String padMappingSettingsToJson(const PadMappingSettings& settings);
// Zahlenfeld des Dialogs: eine ganze Zahl von 0 bis kMaxPadFirstNote, sonst nichts.
std::optional<int> parsePadFirstNote(const juce::String& text);

// Eine fehlende Datei ergibt die Standardwerte.
PadMappingSettings loadPadMappingSettings(const juce::File& file);
// Legt den Ordner an; liefert false, wenn die Datei nicht geschrieben werden konnte.
bool savePadMappingSettings(const juce::File& file, const PadMappingSettings& settings);

// %APPDATA%\Dubgefahren\settings.json; in Test-Builds (DG_NO_USER_SETTINGS) leer.
juce::File padMappingFile();

// Prozessweit geteilte Belegung: Alle Instanzen im Host lesen dieselben Werte, der Audio-Thread
// ohne Sperre. Setter nur im Message-Thread aufrufen.
class PadMapping
{
public:
    static PadMapping& instance();

    int firstNote() const { return firstNote_.load(std::memory_order_relaxed); }
    PadOrigin origin() const { return static_cast<PadOrigin>(origin_.load(std::memory_order_relaxed)); }
    PadMappingSettings settings() const { return { firstNote(), origin() }; }

    void setFirstNote(int note); // wird auf 0 .. kMaxPadFirstNote begrenzt
    void setOrigin(PadOrigin origin);

private:
    PadMapping();
    void persist() const;

    juce::File file_;
    std::atomic<int> firstNote_ { kDefaultPadFirstNote };
    std::atomic<int> origin_ { static_cast<int>(PadOrigin::BottomLeft) };
};

} // namespace dg
