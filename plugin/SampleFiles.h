#pragma once
#include <juce_core/juce_core.h>
#include "engine/Kit.h"

namespace dg {

// Endungen, die als Sample gelten (klein, mit Punkt).
const juce::StringArray& sampleFileExtensions();
// Muster für den Dateidialog, z. B. "*.wav;*.aif;…".
juce::String sampleFileWildcard();
bool isSampleFile(const juce::File& file);
// Ordner <Kit>/ neben <Kit>.dgkit; leeres File, wenn kitFile leer ist.
juce::File sampleFolderFor(const juce::File& kitFile);
// Dateinamen der Samples im Ordner, alphabetisch ohne Groß-/Kleinschreibung.
juce::StringArray listSampleFiles(const juce::File& folder);
// Kopiert source in folder (legt ihn an). Eine gleichnamige Datei mit gleichem Inhalt wird
// wiederverwendet, sonst entsteht "name (2).ext" usw. Liefert die Zieldatei oder ein leeres
// File und setzt error.
juce::File importSampleFile(const juce::File& source, const juce::File& folder, juce::String& error);
// Kopiert die Dateien aller Sample-Slots von fromFolder nach toFolder (überschreibt).
// Liefert eine Problemliste "datei – grund".
juce::StringArray copyKitSamples(const Kit& kit, const juce::File& fromFolder, const juce::File& toFolder);

} // namespace dg
