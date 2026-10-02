#include "plugin/PadMapping.h"
#include <algorithm>
#include <cmath>

namespace dg {

PadMappingSettings parsePadMappingSettings(const juce::String& json)
{
    PadMappingSettings s;
    juce::var root;
    if (juce::JSON::parse(json, root).failed() || !root.isObject())
        return s;

    const auto note = root["firstNote"];
    if (note.isInt() || note.isInt64() || note.isDouble()) // Zahlen, keine Texte oder Wahrheitswerte
    {
        const double v = static_cast<double>(note);
        if (v >= 0.0 && v <= kMaxPadFirstNote && v == std::floor(v))
            s.firstNote = static_cast<int>(v);
    }
    if (root["padOrigin"].toString() == "topLeft")
        s.origin = PadOrigin::TopLeft;
    return s;
}

juce::String padMappingSettingsToJson(const PadMappingSettings& s)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("firstNote", s.firstNote);
    object->setProperty("padOrigin", s.origin == PadOrigin::TopLeft ? "topLeft" : "bottomLeft");
    return juce::JSON::toString(juce::var(object));
}

std::optional<int> parsePadFirstNote(const juce::String& text)
{
    const auto t = text.trim();
    if (t.isEmpty() || t.length() > 3 || !t.containsOnly("0123456789"))
        return std::nullopt;
    const int value = t.getIntValue();
    if (value > kMaxPadFirstNote)
        return std::nullopt;
    return value;
}

PadMappingSettings loadPadMappingSettings(const juce::File& file)
{
    if (!file.existsAsFile())
        return {};
    return parsePadMappingSettings(file.loadFileAsString());
}

bool savePadMappingSettings(const juce::File& file, const PadMappingSettings& settings)
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText(padMappingSettingsToJson(settings));
}

juce::File padMappingFile()
{
#ifdef DG_NO_USER_SETTINGS
    return {};
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Dubgefahren")
        .getChildFile("settings.json");
#endif
}

PadMapping& PadMapping::instance()
{
    static PadMapping mapping; // wird im Konstruktor des Processors angelegt, nie im Audio-Thread
    return mapping;
}

PadMapping::PadMapping() : file_(padMappingFile())
{
    if (file_ != juce::File())
    {
        const auto s = loadPadMappingSettings(file_);
        firstNote_.store(s.firstNote);
        origin_.store(static_cast<int>(s.origin));
    }
}

void PadMapping::setFirstNote(int note)
{
    firstNote_.store(std::clamp(note, 0, kMaxPadFirstNote));
    persist();
}

void PadMapping::setOrigin(PadOrigin origin)
{
    origin_.store(static_cast<int>(origin));
    persist();
}

void PadMapping::persist() const
{
    if (file_ == juce::File())
        return;
    if (!savePadMappingSettings(file_, settings()))
    {
        DBG("Could not write " << file_.getFullPathName());
    }
}

} // namespace dg
