#include <catch2/catch_test_macros.hpp>
#include "plugin/PadProfile.h"

using namespace dg;

namespace {
struct TempFolder
{
    juce::File dir = juce::File::createTempFile("dgprofiles");
    ~TempFolder() { dir.deleteRecursively(); }
};

PadProfile profileWith(const juce::String& name, int firstNote)
{
    return makeConsecutiveProfile(name, firstNote, PadOrigin::BottomLeft);
}
} // namespace

TEST_CASE("consecutive profile maps firstNote upwards and clips at 127", "[padprofile]")
{
    CHECK(makeConsecutiveProfile("x", 36, PadOrigin::BottomLeft).notes[15] == 51);
    const auto high = makeConsecutiveProfile("x", 120, PadOrigin::BottomLeft);
    CHECK(high.notes[7] == 127);
    CHECK(high.notes[8] == -1);
}

TEST_CASE("a profile round-trips through JSON", "[padprofile]")
{
    PadProfile p = profileWith("A", 36);
    p.origin = PadOrigin::TopLeft;
    p.notes[0] = 0;
    p.notes[1] = 127;
    p.notes[4] = -1;
    const auto json = padProfileToJson(p);
    CHECK(json.contains("topLeft"));
    CHECK(json.contains("null"));
    CHECK(parsePadProfile("A", json) == p);
}

TEST_CASE("third-party files are repaired, not rejected", "[padprofile]")
{
    const auto notesOf = [](const char* json) {
        const auto p = parsePadProfile("x", json);
        REQUIRE(p.has_value());
        return p->notes;
    };
    std::array<int, kNumSlots> expected {};

    expected.fill(-1);
    expected[0] = 1;
    expected[1] = 2;
    expected[2] = 3;
    CHECK(notesOf(R"({"notes":[1,2,3]})") == expected);

    for (int i = 0; i < kNumSlots; ++i)
        expected[static_cast<std::size_t>(i)] = i + 1;
    CHECK(notesOf(R"({"notes":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]})") == expected);

    expected.fill(-1);
    expected[0] = 36;
    expected[2] = 38;
    CHECK(notesOf(R"({"notes":[36,36,38]})") == expected);

    expected.fill(-1);
    expected[6] = 36;
    CHECK(notesOf(R"({"notes":[128,-1,"a",null,true,1.5,36]})") == expected);

    CHECK(parsePadProfile("x", R"({"notes":[]})")->origin == PadOrigin::BottomLeft);
    CHECK(parsePadProfile("x", R"({"notes":[],"padOrigin":"middle"})")->origin == PadOrigin::BottomLeft);
    CHECK(parsePadProfile("x", R"({"notes":[],"padOrigin":"topLeft"})")->origin == PadOrigin::TopLeft);
}

TEST_CASE("unusable files yield nullopt", "[padprofile]")
{
    for (const char* json : { "", "not json", "[1,2]", "{}", R"({"notes":5})" })
    {
        INFO(json);
        CHECK_FALSE(parsePadProfile("x", json).has_value());
    }
}

TEST_CASE("profile names", "[padprofile]")
{
    for (const char* ok : { "Launchpad Mini", "Default", "BU16 (alt)" })
        CHECK(isValidProfileName(ok));
    for (const char* bad : { "", "  ", "a/b", "a\\b", "a:b", ".", ".." })
    {
        INFO(bad);
        CHECK_FALSE(isValidProfileName(bad));
    }
    CHECK_FALSE(isValidProfileName(juce::String::repeatedString("a", 65)));
    CHECK(isValidProfileName(juce::String::repeatedString("a", 64)));
}

TEST_CASE("the store saves, lists and loads profiles", "[padprofile]")
{
    TempFolder t;
    ProfileStore store(t.dir);
    REQUIRE(store.save(profileWith("Zeta", 10)));
    REQUIRE(store.save(profileWith("alpha", 20)));
    REQUIRE(store.save(profileWith("Default", 36)));
    t.dir.getChildFile("broken.json").replaceWithText("{");
    t.dir.getChildFile("readme.txt").replaceWithText("hi");

    CHECK(store.names() == juce::StringArray { "Default", "alpha", "Zeta" });
    CHECK(store.load("alpha") == profileWith("alpha", 20));
    CHECK(store.contains("ALPHA"));
    CHECK_FALSE(store.contains("broken"));
    CHECK_FALSE(store.save(profileWith("a/b", 1)));
}

TEST_CASE("rename and remove", "[padprofile]")
{
    TempFolder t;
    ProfileStore store(t.dir);
    store.save(profileWith("alpha", 20));
    store.save(profileWith("Zeta", 10));

    CHECK(store.rename("alpha", "beta"));
    CHECK(store.names() == juce::StringArray { "beta", "Zeta" });
    CHECK_FALSE(store.rename("beta", "ZETA"));
    CHECK(store.rename("beta", "Beta"));
    CHECK(store.contains("beta"));
    CHECK_FALSE(store.rename("Beta", "a/b"));
    CHECK_FALSE(store.rename("nope", "x"));
    CHECK(store.remove("Beta"));
    CHECK_FALSE(store.load("beta").has_value());
    CHECK_FALSE(store.remove("nope"));
}

TEST_CASE("the store creates a missing folder on save and tolerates a missing folder on read", "[padprofile]")
{
    TempFolder t;
    ProfileStore store(t.dir.getChildFile("nested").getChildFile("Profiles"));
    CHECK(store.names().isEmpty());
    CHECK_FALSE(store.load("x").has_value());
    REQUIRE(store.save(profileWith("x", 1)));
    CHECK(store.folder().isDirectory());
}
