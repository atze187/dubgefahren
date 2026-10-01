#include <catch2/catch_test_macros.hpp>
#include "plugin/PadMapping.h"
#include "PadMappingTestHelpers.h"

using namespace dg;

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
    // Ein gültiger Wert bleibt erhalten, wenn der andere fehlt.
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

TEST_CASE("settings round-trip through a file and create the folder", "[padmapping]")
{
    const auto dir = juce::File::createTempFile("dgpad");
    const auto file = dir.getChildFile("nested").getChildFile("settings.json");
    REQUIRE_FALSE(file.existsAsFile());
    CHECK(loadPadMappingSettings(file) == PadMappingSettings {}); // fehlende Datei: Standard

    const PadMappingSettings written { 32, PadOrigin::TopLeft };
    REQUIRE(savePadMappingSettings(file, written));
    CHECK(loadPadMappingSettings(file) == written);
    const auto text = file.loadFileAsString();
    CHECK(text.contains("firstNote"));
    CHECK(text.contains("topLeft"));
    CHECK(padMappingSettingsToJson(written).contains("32"));
    dir.deleteRecursively();
}

TEST_CASE("the first pad note field accepts only whole numbers from 0 to 112", "[padmapping]")
{
    CHECK(parsePadFirstNote("0") == 0);
    CHECK(parsePadFirstNote("36") == 36);
    CHECK(parsePadFirstNote("  40 ") == 40);
    CHECK(parsePadFirstNote("112") == 112);
    CHECK(parsePadFirstNote("007") == 7);
    for (const char* bad : { "", " ", "abc", "-3", "113", "1e2", "0x20", "36.5", "1234", "3 6" })
    {
        INFO(bad);
        CHECK_FALSE(parsePadFirstNote(bad).has_value());
    }
}

TEST_CASE("the shared pad mapping clamps the note and keeps the origin", "[padmapping]")
{
    dgtest::ScopedPadMapping restore;
    auto& m = PadMapping::instance();
    CHECK(padMappingFile() == juce::File()); // Test-Builds berühren keine Einstellungsdatei
    CHECK(m.firstNote() == 36);
    CHECK(m.origin() == PadOrigin::BottomLeft);

    m.setFirstNote(32);
    CHECK(m.firstNote() == 32);
    m.setFirstNote(200);
    CHECK(m.firstNote() == 112);
    m.setFirstNote(-5);
    CHECK(m.firstNote() == 0);

    m.setOrigin(PadOrigin::TopLeft);
    CHECK(m.origin() == PadOrigin::TopLeft);
    CHECK((m.settings() == PadMappingSettings { 0, PadOrigin::TopLeft }));
    CHECK(&PadMapping::instance() == &m); // ein Objekt je Prozess
}
