#pragma once
#include <juce_core/juce_core.h>

namespace dg {

struct KitFolderInfo
{
    juce::File folder;
    juce::String warning; // leer, wenn alles in Ordnung ist
};

juce::String expandEnvironmentVariables(const juce::String& text);
// Reihenfolge: Einstellung des Nutzers (userKitFolder) → Bundle-Config → Standardordner. Ein gesetzter, aber
// unbrauchbarer userKitFolder erzeugt eine Warnung und fällt auf die nächste Stufe zurück.
KitFolderInfo resolveKitFolder(const juce::String& userKitFolder, const juce::File& configFile, const juce::File& defaultFolder);
juce::File defaultKitFolder();
juce::File pluginConfigFile();

} // namespace dg
