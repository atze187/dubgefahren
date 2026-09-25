#pragma once
#include <juce_core/juce_core.h>

namespace dg {

struct KitFolderInfo
{
    juce::File folder;
    juce::String warning; // leer, wenn alles in Ordnung ist
};

juce::String expandEnvironmentVariables(const juce::String& text);
KitFolderInfo resolveKitFolder(const juce::File& configFile, const juce::File& defaultFolder);
juce::File defaultKitFolder();
juce::File pluginConfigFile();

} // namespace dg
