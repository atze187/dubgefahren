#include "plugin/UserSettings.h"

namespace dg {

juce::File userSettingsFile()
{
#ifdef DG_NO_USER_SETTINGS
    return {};
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Dubgefahren")
        .getChildFile("settings.json");
#endif
}

UserSettings::UserSettings(juce::File file) : file_(std::move(file))
{
    if (file_ == juce::File() || !file_.existsAsFile())
        return;
    const auto text = file_.loadFileAsString();
    juce::var root;
    if (juce::JSON::parse(text, root).failed() || !root.isObject())
        return;
    profile_ = root["profile"].toString();
    kitFolder_ = root["kitFolder"].toString();
    if (root.hasProperty("firstNote") || root.hasProperty("padOrigin"))
        legacy_ = parsePadMappingSettings(text);
}

UserSettings& UserSettings::instance()
{
    static UserSettings settings(userSettingsFile());
    return settings;
}

void UserSettings::setProfile(const juce::String& name)
{
    profile_ = name;
    save();
}

void UserSettings::setKitFolder(const juce::String& path)
{
    kitFolder_ = path;
    save();
}

void UserSettings::save()
{
    legacy_.reset(); // die alten Schlüssel werden beim Speichern nicht mehr geschrieben
    if (file_ == juce::File())
        return;
    auto* object = new juce::DynamicObject();
    if (profile_.isNotEmpty())
        object->setProperty("profile", profile_);
    if (kitFolder_.isNotEmpty())
        object->setProperty("kitFolder", kitFolder_);
    file_.getParentDirectory().createDirectory();
    if (!file_.replaceWithText(juce::JSON::toString(juce::var(object))))
    {
        DBG("Could not write " << file_.getFullPathName());
    }
}

} // namespace dg
