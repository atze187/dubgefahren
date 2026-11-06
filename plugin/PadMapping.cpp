#include "plugin/PadMapping.h"
#include <cmath>
#include "plugin/UserSettings.h"

namespace dg {

namespace {
const juce::String kDefaultProfile = "Default";

#ifdef DG_NO_USER_SETTINGS
// Ein Ordner je Testprozess, der beim Beenden wieder verschwindet.
struct TempProfilesFolder
{
    juce::File dir = juce::File::createTempFile("dgprofiles");
    ~TempProfilesFolder() { dir.deleteRecursively(); }
};
#endif
} // namespace

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

juce::File padProfilesFolder()
{
#ifdef DG_NO_USER_SETTINGS
    static TempProfilesFolder temp;
    return temp.dir;
#else
    return userSettingsFile().getParentDirectory().getChildFile("Profiles");
#endif
}

PadMapping& PadMapping::instance()
{
    static PadMapping mapping(padProfilesFolder(), UserSettings::instance()); // wird im Konstruktor des Processors angelegt, nie im Audio-Thread
    return mapping;
}

PadMapping::PadMapping(juce::File profilesFolder, UserSettings& settings)
    : store_(std::move(profilesFolder)), settings_(settings)
{
    // Beim ersten Start (noch keine Profile) wird Default aus den alten Einstellungen angelegt.
    const bool fresh = store_.names().isEmpty();
    if (!store_.load(kDefaultProfile).has_value())
    {
        PadMappingSettings start;
        if (fresh)
            if (const auto legacy = settings_.legacyPad())
                start = *legacy;
        store_.save(makeConsecutiveProfile(kDefaultProfile, start.firstNote, start.origin));
    }
    auto chosen = settings_.profile().isNotEmpty() ? store_.load(settings_.profile()) : std::nullopt;
    if (!chosen)
        chosen = store_.load(kDefaultProfile);
    activate(chosen ? *chosen : makeConsecutiveProfile(kDefaultProfile, kDefaultPadFirstNote, PadOrigin::BottomLeft));
}

void PadMapping::activate(const PadProfile& profile)
{
    active_ = profile;
    noteMap_.setAll(profile.notes);
    active_.notes = noteMap_.notes(); // bereinigt um Duplikate
    origin_.store(static_cast<int>(profile.origin));
    revision_.fetch_add(1);
}

void PadMapping::saveActive()
{
    if (!store_.save(active_))
    {
        DBG("Could not write profile " << active_.name);
    }
}

bool PadMapping::selectProfile(const juce::String& name)
{
    const auto profile = store_.load(name);
    if (!profile)
        return false;
    activate(*profile);
    settings_.setProfile(profile->name);
    return true;
}

juce::String PadMapping::createProfile()
{
    juce::String name = "New profile";
    for (int n = 2; store_.contains(name); ++n)
        name = "New profile " + juce::String(n);
    auto profile = store_.load(kDefaultProfile).value_or(makeConsecutiveProfile(kDefaultProfile, kDefaultPadFirstNote, PadOrigin::BottomLeft));
    profile.name = name;
    store_.save(profile);
    activate(profile);
    settings_.setProfile(name);
    return name;
}

bool PadMapping::renameActiveProfile(const juce::String& newName)
{
    if (active_.name.equalsIgnoreCase(kDefaultProfile) || !store_.rename(active_.name, newName))
        return false;
    active_.name = newName;
    settings_.setProfile(newName);
    revision_.fetch_add(1);
    return true;
}

bool PadMapping::deleteActiveProfile()
{
    if (active_.name.equalsIgnoreCase(kDefaultProfile))
        return false;
    const auto doomed = active_.name;
    if (!store_.remove(doomed))
        return false;
    selectProfile(kDefaultProfile);
    return true;
}

void PadMapping::setPadNote(int slot, int note)
{
    if (slot < 0 || slot >= kNumSlots || note < -1 || note > 127)
        return;
    noteMap_.assign(slot, note);
    active_.notes = noteMap_.notes();
    saveActive();
    revision_.fetch_add(1);
}

void PadMapping::setOrigin(PadOrigin origin)
{
    active_.origin = origin;
    origin_.store(static_cast<int>(origin));
    saveActive();
    revision_.fetch_add(1);
}

} // namespace dg
