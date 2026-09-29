#include "plugin/SampleFiles.h"
#include "plugin/KitFile.h"

namespace dg {

const juce::StringArray& sampleFileExtensions()
{
    static const juce::StringArray extensions { ".wav", ".aif", ".aiff", ".mp3", ".flac" };
    return extensions;
}

juce::String sampleFileWildcard()
{
    juce::StringArray patterns;
    for (const auto& e : sampleFileExtensions())
        patterns.add("*" + e);
    return patterns.joinIntoString(";");
}

bool isSampleFile(const juce::File& file)
{
    return sampleFileExtensions().contains(file.getFileExtension().toLowerCase());
}

juce::File sampleFolderFor(const juce::File& kitFile)
{
    if (kitFile == juce::File())
        return {};
    return kitFile.getParentDirectory().getChildFile(kitFile.getFileNameWithoutExtension());
}

juce::StringArray listSampleFiles(const juce::File& folder)
{
    juce::StringArray names;
    if (folder == juce::File() || !folder.isDirectory())
        return names;
    for (const auto& f : folder.findChildFiles(juce::File::findFiles, false))
        if (isSampleFile(f))
            names.add(f.getFileName());
    names.sort(true);
    return names;
}

juce::File importSampleFile(const juce::File& source, const juce::File& folder, juce::String& error)
{
    if (!source.existsAsFile())
    {
        error = "File not found: " + source.getFullPathName();
        return {};
    }
    if (source.getParentDirectory() == folder)
        return source;
    if (!folder.createDirectory())
    {
        error = "Could not create folder: " + folder.getFullPathName();
        return {};
    }
    const auto base = source.getFileNameWithoutExtension();
    const auto ext = source.getFileExtension();
    for (int n = 1; n < 1000; ++n)
    {
        const auto target = folder.getChildFile(n == 1 ? base + ext : base + " (" + juce::String(n) + ")" + ext);
        if (target.existsAsFile())
        {
            if (target.hasIdenticalContentTo(source))
                return target;
            continue;
        }
        if (source.copyFileTo(target))
            return target;
        error = "Could not copy file to: " + target.getFullPathName();
        return {};
    }
    error = "Too many files named " + source.getFileName();
    return {};
}

juce::StringArray copyKitSamples(const Kit& kit, const juce::File& fromFolder, const juce::File& toFolder)
{
    juce::StringArray problems;
    if (fromFolder == toFolder)
        return problems;
    juce::StringArray done;
    for (std::size_t s = 0; s < kNumSlots; ++s)
    {
        if (kit.slots[s].source != SourceType::Sample)
            continue;
        const auto name = juce::String::fromUTF8(kit.samples[s].c_str());
        if (done.contains(name))
            continue;
        done.add(name);
        const bool known = isValidSampleFileName(name) && fromFolder != juce::File();
        if (!known || !fromFolder.getChildFile(name).existsAsFile())
        {
            problems.add(name + juce::String::fromUTF8(" – file not found"));
            continue;
        }
        if (!toFolder.createDirectory() || !fromFolder.getChildFile(name).copyFileTo(toFolder.getChildFile(name)))
            problems.add(name + juce::String::fromUTF8(" – could not copy"));
    }
    return problems;
}

} // namespace dg
