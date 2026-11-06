#include <catch2/catch_test_macros.hpp>
#include "plugin/UserSettings.h"

using namespace dg;

namespace {
struct TempFolder
{
    juce::File dir = juce::File::createTempFile("dgsettings");
    ~TempFolder() { dir.deleteRecursively(); }
};
} // namespace

TEST_CASE("missing file gives empty values", "[usersettings]")
{
    TempFolder t;
    UserSettings s(t.dir.getChildFile("settings.json"));
    CHECK(s.profile() == "");
    CHECK(s.kitFolder() == "");
    CHECK_FALSE(s.legacyPad().has_value());
}

TEST_CASE("values round-trip through the file and keep the folder creation", "[usersettings]")
{
    TempFolder t;
    const auto file = t.dir.getChildFile("nested").getChildFile("settings.json");
    {
        UserSettings s(file);
        s.setProfile("Launchpad");
        s.setKitFolder("C:/k");
    }
    CHECK(file.getParentDirectory().isDirectory());
    UserSettings again(file);
    CHECK(again.profile() == "Launchpad");
    CHECK(again.kitFolder() == "C:/k");
    again.setKitFolder("");
    CHECK(UserSettings(file).kitFolder() == "");
    CHECK(UserSettings(file).profile() == "Launchpad");
}

TEST_CASE("saving keeps no legacy keys but reading still sees them", "[usersettings]")
{
    TempFolder t;
    const auto file = t.dir.getChildFile("settings.json");
    file.getParentDirectory().createDirectory();
    file.replaceWithText(R"({"firstNote":32,"padOrigin":"topLeft"})");
    UserSettings s(file);
    REQUIRE(s.legacyPad().has_value());
    CHECK((*s.legacyPad() == PadMappingSettings { 32, PadOrigin::TopLeft }));

    s.setProfile("x");
    const auto text = file.loadFileAsString();
    CHECK_FALSE(text.contains("firstNote"));
    CHECK_FALSE(text.contains("padOrigin"));
    CHECK_FALSE(s.legacyPad().has_value());
    CHECK_FALSE(UserSettings(file).legacyPad().has_value());
}

TEST_CASE("a broken file is treated as empty and does not crash", "[usersettings]")
{
    for (const char* content : { "{ kaputt", "[1]", "" })
    {
        INFO(content);
        TempFolder t;
        const auto file = t.dir.getChildFile("settings.json");
        file.getParentDirectory().createDirectory();
        file.replaceWithText(content);
        UserSettings s(file);
        CHECK(s.profile() == "");
        CHECK(s.kitFolder() == "");
        CHECK_FALSE(s.legacyPad().has_value());
        s.setProfile("a");
        CHECK(UserSettings(file).profile() == "a");
    }
}

TEST_CASE("an unwritable path does not crash", "[usersettings]")
{
    TempFolder t;
    t.dir.createDirectory();
    const auto blocker = t.dir.getChildFile("blocker");
    blocker.replaceWithText("x");
    UserSettings s(blocker.getChildFile("settings.json")); // der Elternordner ist eine Datei
    s.setProfile("a");
    CHECK(s.profile() == "a");
}

TEST_CASE("an empty file path keeps everything in memory", "[usersettings]")
{
    UserSettings s { juce::File() };
    s.setKitFolder("C:/k");
    CHECK(s.kitFolder() == "C:/k");
}

TEST_CASE("in test builds the default file is empty", "[usersettings]")
{
    CHECK(userSettingsFile() == juce::File());
}
