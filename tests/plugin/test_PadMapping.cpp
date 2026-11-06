#include <catch2/catch_test_macros.hpp>
#include "plugin/PadMapping.h"
#include "plugin/UserSettings.h"
#include "PadMappingTestHelpers.h"

using namespace dg;

namespace {
struct TempFolder
{
    juce::File dir = juce::File::createTempFile("dgmapping");
    ~TempFolder() { dir.deleteRecursively(); }
    juce::File profiles() const { return dir.getChildFile("Profiles"); }
};
} // namespace

TEST_CASE("pad mapping settings parse a valid file", "[padmapping]")
{
    const auto s = parsePadMappingSettings(R"({ "firstNote": 32, "padOrigin": "topLeft" })");
    CHECK(s.firstNote == 32);
    CHECK(s.origin == PadOrigin::TopLeft);
}

TEST_CASE("missing, partial or broken settings fall back to the defaults value by value", "[padmapping]")
{
    const PadMappingSettings defaults;
    CHECK(defaults.firstNote == 36);
    CHECK(defaults.origin == PadOrigin::BottomLeft);

    CHECK(parsePadMappingSettings("") == defaults);
    CHECK(parsePadMappingSettings("not json") == defaults);
    CHECK(parsePadMappingSettings("[1, 2]") == defaults);
    CHECK(parsePadMappingSettings("{}") == defaults);
    CHECK(parsePadMappingSettings(R"({ "firstNote": 32 })").firstNote == 32);
    CHECK(parsePadMappingSettings(R"({ "firstNote": 32 })").origin == PadOrigin::BottomLeft);
    CHECK(parsePadMappingSettings(R"({ "padOrigin": "topLeft" })").firstNote == 36);
    CHECK(parsePadMappingSettings(R"({ "padOrigin": "topLeft" })").origin == PadOrigin::TopLeft);
}

TEST_CASE("settings values outside the range or of the wrong type are ignored", "[padmapping]")
{
    for (const char* json : { R"({ "firstNote": 113 })", R"({ "firstNote": -1 })", R"({ "firstNote": 36.5 })",
                              R"({ "firstNote": "32" })", R"({ "firstNote": null })", R"({ "firstNote": true })" })
    {
        INFO(json);
        CHECK(parsePadMappingSettings(json).firstNote == 36);
    }
    CHECK(parsePadMappingSettings(R"({ "firstNote": 112 })").firstNote == 112);
    CHECK(parsePadMappingSettings(R"({ "firstNote": 0 })").firstNote == 0);
    CHECK(parsePadMappingSettings(R"({ "padOrigin": "middle" })").origin == PadOrigin::BottomLeft);
    CHECK(parsePadMappingSettings(R"({ "padOrigin": 1 })").origin == PadOrigin::BottomLeft);
}

TEST_CASE("first start creates Default from the legacy settings", "[padmapping]")
{
    TempFolder t;
    t.dir.createDirectory();
    const auto file = t.dir.getChildFile("settings.json");
    file.replaceWithText(R"({"firstNote":32,"padOrigin":"topLeft"})");
    UserSettings settings(file);
    PadMapping m(t.profiles(), settings);
    CHECK(m.profileNames() == juce::StringArray { "Default" });
    CHECK(m.profile().notes[0] == 32);
    CHECK(m.profile().notes[15] == 47);
    CHECK(m.origin() == PadOrigin::TopLeft);
    CHECK(m.activeProfile() == "Default");
    CHECK(m.noteMap().slotForNote(32) == 0);
}

TEST_CASE("first start without legacy settings gives 36 and bottom left", "[padmapping]")
{
    TempFolder t;
    UserSettings settings { juce::File() };
    PadMapping m(t.profiles(), settings);
    CHECK(m.profile().notes[0] == 36);
    CHECK(m.profile().notes[15] == 51);
    CHECK(m.origin() == PadOrigin::BottomLeft);
}

TEST_CASE("an existing profile folder ignores legacy settings", "[padmapping]")
{
    TempFolder t;
    ProfileStore(t.profiles()).save(makeConsecutiveProfile("Default", 50, PadOrigin::BottomLeft));
    const auto file = t.dir.getChildFile("settings.json");
    file.replaceWithText(R"({"firstNote":32})");
    UserSettings settings(file);
    PadMapping m(t.profiles(), settings);
    CHECK(m.profile().notes[0] == 50);
}

TEST_CASE("the saved selection is restored and falls back to Default", "[padmapping]")
{
    TempFolder t;
    ProfileStore(t.profiles()).save(makeConsecutiveProfile("A", 60, PadOrigin::BottomLeft));
    {
        UserSettings settings { juce::File() };
        settings.setProfile("A");
        PadMapping m(t.profiles(), settings);
        CHECK(m.activeProfile() == "A");
        CHECK(m.noteMap().slotForNote(60) == 0);
    }
    UserSettings settings { juce::File() };
    settings.setProfile("gone");
    PadMapping m(t.profiles(), settings);
    CHECK(m.activeProfile() == "Default");
}

TEST_CASE("a missing or broken Default file is recreated with the standard assignment", "[padmapping]")
{
    for (const bool broken : { false, true })
    {
        TempFolder t;
        ProfileStore store(t.profiles());
        store.save(makeConsecutiveProfile("Other", 60, PadOrigin::BottomLeft));
        if (broken)
            t.profiles().getChildFile("Default.json").replaceWithText("{");
        UserSettings settings { juce::File() };
        PadMapping m(t.profiles(), settings);
        CHECK(m.profile().notes[0] == 36);
        CHECK(store.contains("Default"));
    }
}

TEST_CASE("setPadNote persists, frees the previous owner and updates the NoteMap", "[padmapping]")
{
    TempFolder t;
    UserSettings settings { juce::File() };
    PadMapping m(t.profiles(), settings);
    int rev = m.revision();

    m.setPadNote(1, 60);
    CHECK(m.noteMap().slotForNote(60) == 1);
    CHECK(m.revision() > rev);
    {
        PadMapping again(t.profiles(), settings);
        CHECK(again.noteMap().slotForNote(60) == 1);
        CHECK(again.noteMap().slotForNote(37) == -1);
    }

    rev = m.revision();
    m.setPadNote(2, 60);
    CHECK(m.noteMap().noteForSlot(1) == -1);
    CHECK(m.revision() > rev);
    rev = m.revision();
    m.setPadNote(2, -1);
    CHECK(m.noteMap().noteForSlot(2) == -1);
    CHECK(m.revision() > rev);
}

TEST_CASE("setOrigin persists in the profile", "[padmapping]")
{
    TempFolder t;
    UserSettings settings { juce::File() };
    PadMapping m(t.profiles(), settings);
    m.setOrigin(PadOrigin::TopLeft);
    CHECK(m.origin() == PadOrigin::TopLeft);
    PadMapping again(t.profiles(), settings);
    CHECK(again.origin() == PadOrigin::TopLeft);
}

TEST_CASE("create, select, rename and delete", "[padmapping]")
{
    TempFolder t;
    UserSettings settings { juce::File() };
    PadMapping m(t.profiles(), settings);

    CHECK(m.createProfile() == "New profile");
    CHECK(m.createProfile() == "New profile 2");
    CHECK(m.activeProfile() == "New profile 2");
    CHECK(settings.profile() == "New profile 2");

    CHECK(m.renameActiveProfile("Launchpad"));
    CHECK(m.activeProfile() == "Launchpad");
    CHECK(settings.profile() == "Launchpad");
    CHECK(m.profileNames().contains("Launchpad"));
    CHECK_FALSE(m.profileNames().contains("New profile 2"));
    CHECK_FALSE(m.renameActiveProfile("default"));
    CHECK_FALSE(m.renameActiveProfile("a/b"));
    CHECK(m.activeProfile() == "Launchpad");

    CHECK(m.selectProfile("Default"));
    CHECK_FALSE(m.renameActiveProfile("x"));
    CHECK_FALSE(m.deleteActiveProfile());
    CHECK(m.activeProfile() == "Default");

    CHECK(m.selectProfile("Launchpad"));
    CHECK(m.deleteActiveProfile());
    CHECK(m.activeProfile() == "Default");
    CHECK_FALSE(m.profileNames().contains("Launchpad"));

    CHECK_FALSE(m.selectProfile("nope"));
    CHECK(m.activeProfile() == "Default");
}

TEST_CASE("in test builds the singleton uses a temporary profile folder", "[padmapping]")
{
    const auto folder = PadMapping::instance().profilesFolder();
    CHECK(folder.isAChildOf(juce::File::getSpecialLocation(juce::File::tempDirectory)));
    CHECK(userSettingsFile() == juce::File());
}
