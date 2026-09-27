#include "plugin/Config.h"

namespace dg {

juce::String expandEnvironmentVariables(const juce::String& text)
{
    juce::String out;
    int i = 0;
    while (i < text.length())
    {
        const int start = text.indexOfChar(i, '%');
        const int end = start < 0 ? -1 : text.indexOfChar(start + 1, '%');
        if (start < 0 || end < 0)
        {
            out << text.substring(i);
            break;
        }
        out << text.substring(i, start);
        const auto name = text.substring(start + 1, end);
        const auto value = name.isEmpty() ? juce::String() : juce::SystemStats::getEnvironmentVariable(name, {});
        out << (value.isNotEmpty() ? value : text.substring(start, end + 1));
        i = end + 1;
    }
    return out;
}

KitFolderInfo resolveKitFolder(const juce::File& configFile, const juce::File& defaultFolder)
{
    KitFolderInfo info { defaultFolder, {} };

    if (configFile.existsAsFile())
    {
        juce::var root;
        if (juce::JSON::parse(configFile.loadFileAsString(), root).failed() || !root.isObject())
        {
            info.warning = juce::String("The config file is invalid and will be ignored: ") + configFile.getFullPathName();
        }
        else
        {
            const auto raw = root["kitFolder"].toString().trim();
            if (raw.isNotEmpty())
            {
                const auto expanded = expandEnvironmentVariables(raw);
                if (!juce::File::isAbsolutePath(expanded))
                    info.warning = "kitFolder in the config is not an absolute path: " + expanded;
                else if (!juce::File(expanded).isDirectory())
                    info.warning = "The kit folder from the config does not exist: " + expanded;
                else
                    info.folder = juce::File(expanded);
            }
        }
    }

    if (info.folder == defaultFolder && !defaultFolder.isDirectory())
        defaultFolder.createDirectory();
    return info;
}

juce::File defaultKitFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Dubgefahren")
        .getChildFile("Kits");
}

juce::File pluginConfigFile()
{
    // <Bundle>.vst3/Contents/x86_64-win/<Name>.vst3 → <Bundle>.vst3/Contents/Resources/...
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory()
        .getParentDirectory()
        .getChildFile("Resources")
        .getChildFile("Dubgefahren.config.json");
}

} // namespace dg
