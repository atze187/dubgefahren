#include "plugin/PadProfile.h"
#include <cmath>
#include <set>

namespace dg {

PadProfile makeConsecutiveProfile(const juce::String& name, int firstNote, PadOrigin origin)
{
    PadProfile p;
    p.name = name;
    p.origin = origin;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int note = firstNote + s;
        p.notes[static_cast<std::size_t>(s)] = (note >= 0 && note <= 127) ? note : -1;
    }
    return p;
}

juce::String padProfileToJson(const PadProfile& profile)
{
    juce::Array<juce::var> notes;
    for (const int n : profile.notes)
        notes.add(n >= 0 ? juce::var(n) : juce::var());
    auto* object = new juce::DynamicObject();
    object->setProperty("padOrigin", profile.origin == PadOrigin::TopLeft ? "topLeft" : "bottomLeft");
    object->setProperty("notes", notes);
    return juce::JSON::toString(juce::var(object));
}

std::optional<PadProfile> parsePadProfile(const juce::String& name, const juce::String& json)
{
    juce::var root;
    if (juce::JSON::parse(json, root).failed() || !root.isObject())
        return std::nullopt;
    const auto notes = root["notes"];
    if (!notes.isArray())
        return std::nullopt;

    PadProfile p;
    p.name = name;
    p.notes.fill(-1);
    p.origin = root["padOrigin"].toString() == "topLeft" ? PadOrigin::TopLeft : PadOrigin::BottomLeft;
    std::set<int> seen;
    for (int i = 0; i < kNumSlots && i < notes.size(); ++i)
    {
        const auto& v = notes[i];
        if (!(v.isInt() || v.isInt64() || v.isDouble())) // Zahlen, keine Texte oder Wahrheitswerte
            continue;
        const double d = static_cast<double>(v);
        if (d < 0.0 || d > 127.0 || d != std::floor(d))
            continue;
        const int n = static_cast<int>(d);
        if (seen.insert(n).second)
            p.notes[static_cast<std::size_t>(i)] = n;
    }
    return p;
}

bool isValidProfileName(const juce::String& name)
{
    return name.trim().isNotEmpty() && name.length() <= 64 && name != "." && name != ".."
        && juce::File::createLegalFileName(name) == name;
}

ProfileStore::ProfileStore(juce::File folder) : folder_(std::move(folder)) {}

juce::File ProfileStore::fileFor(const juce::String& name) const
{
    return folder_.getChildFile(name + ".json");
}

juce::StringArray ProfileStore::names() const
{
    juce::StringArray out;
    for (const auto& f : folder_.findChildFiles(juce::File::findFiles, false, "*.json"))
    {
        const auto name = f.getFileNameWithoutExtension();
        if (isValidProfileName(name) && parsePadProfile(name, f.loadFileAsString()).has_value())
            out.add(name);
    }
    out.sort(true);
    const int d = out.indexOf("Default", true);
    if (d > 0)
    {
        const auto actual = out[d];
        out.remove(d);
        out.insert(0, actual);
    }
    return out;
}

juce::String ProfileStore::findName(const juce::String& name) const
{
    for (const auto& n : names())
        if (n.equalsIgnoreCase(name))
            return n;
    return {};
}

bool ProfileStore::contains(const juce::String& name) const
{
    return findName(name).isNotEmpty();
}

std::optional<PadProfile> ProfileStore::load(const juce::String& name) const
{
    const auto actual = findName(name);
    if (actual.isEmpty())
        return std::nullopt;
    return parsePadProfile(actual, fileFor(actual).loadFileAsString());
}

bool ProfileStore::save(const PadProfile& profile)
{
    if (!isValidProfileName(profile.name))
        return false;
    folder_.createDirectory();
    return fileFor(profile.name).replaceWithText(padProfileToJson(profile));
}

bool ProfileStore::remove(const juce::String& name)
{
    const auto actual = findName(name);
    return actual.isNotEmpty() && fileFor(actual).deleteFile();
}

bool ProfileStore::rename(const juce::String& from, const juce::String& to)
{
    const auto source = findName(from);
    if (source.isEmpty() || !isValidProfileName(to))
        return false;
    const auto other = findName(to);
    if (other.isNotEmpty() && !other.equalsIgnoreCase(source))
        return false;
    return fileFor(source).moveFileTo(fileFor(to));
}

} // namespace dg
