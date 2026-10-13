#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <juce_core/juce_core.h>
#include "engine/Kit.h"
#include "engine/SlotFields.h"
#include "plugin/KitFile.h"

using namespace dg;

namespace {
juce::var parsed(const Kit& k) { return juce::JSON::parse(kitToJsonString(k)); }

KitParseResult reparse(const juce::var& v) { return kitFromJsonString(juce::JSON::toString(v)); }

struct TempDir
{
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("dgkit", "", false);
    TempDir() { dir.createDirectory(); }
    ~TempDir() { dir.deleteRecursively(); }
};
} // namespace

TEST_CASE("factory kit survives a JSON round-trip", "[kitfile]")
{
    const Kit k = makeFactoryKit();
    const auto r = kitFromJsonString(kitToJsonString(k));
    REQUIRE(r.kit.has_value());
    CHECK(r.error.isEmpty());
    CHECK(r.kit->slots == k.slots);
    CHECK(r.kit->names == k.names);
}

TEST_CASE("names with umlauts survive save and load", "[kitfile]")
{
    TempDir tmp;
    Kit k = makeFactoryKit();
    k.names[3] = juce::String::fromUTF8("Größenwahn äöü").toStdString();
    const auto file = tmp.dir.getChildFile(juce::String("test") + kKitExtension);
    juce::String error;
    REQUIRE(saveKitFile(k, file, error));
    const auto r = loadKitFile(file);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->names[3] == k.names[3]);
}

TEST_CASE("invalid kit files are rejected with a message", "[kitfile]")
{
    const Kit k = makeFactoryKit();

    const auto badJson = kitFromJsonString("{ not json");
    CHECK_FALSE(badJson.kit.has_value());
    CHECK(badJson.error == juce::String("The file is not valid JSON."));

    auto wrongFormat = parsed(k);
    wrongFormat.getDynamicObject()->setProperty("format", "something-else");
    CHECK_FALSE(reparse(wrongFormat).kit.has_value());

    auto newer = parsed(k);
    newer.getDynamicObject()->setProperty("version", 7);
    const auto rNewer = reparse(newer);
    CHECK_FALSE(rNewer.kit.has_value());
    CHECK(rNewer.error.isNotEmpty());

    auto fewer = parsed(k);
    fewer["slots"].getArray()->removeLast();
    CHECK_FALSE(reparse(fewer).kit.has_value());

    auto notNumber = parsed(k);
    notNumber["slots"][0]["params"].getDynamicObject()->setProperty("pitch", "laut");
    CHECK_FALSE(reparse(notNumber).kit.has_value());

    auto noName = parsed(k);
    noName["slots"][2].getDynamicObject()->removeProperty("name");
    CHECK_FALSE(reparse(noName).kit.has_value());

    CHECK_FALSE(loadKitFile(juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("does_not_exist_dg.dgkit")).kit.has_value());
}

TEST_CASE("kit values are clamped, missing keys default and unknown keys are ignored", "[kitfile]")
{
    auto v = parsed(makeFactoryKit());
    auto* params = v["slots"][0]["params"].getDynamicObject();
    params->setProperty("pitch", 99999.0);
    params->removeProperty("pw");
    params->setProperty("futureThing", 1.0);
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->slots[0].pitchHz == fieldSpec(SlotField::Pitch).max);
    CHECK(r.kit->slots[0].pulseWidth == fieldSpec(SlotField::PulseWidth).def);
}

TEST_CASE("kits with empty slots survive a round-trip", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[2].source = SourceType::Empty;
    k.names[2].clear();

    const auto v = parsed(k);
    CHECK(static_cast<int>(v["version"]) == 6);
    CHECK(v["slots"][0]["source"].toString() == "synth");
    CHECK(v["slots"][2]["source"].toString() == "empty");
    CHECK_FALSE(v["slots"][2].getDynamicObject()->hasProperty("params"));

    const auto r = kitFromJsonString(kitToJsonString(k));
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->slots == k.slots);
    CHECK(r.kit->names == k.names);

    const Kit empty = makeEmptyKit();
    const auto rEmpty = kitFromJsonString(kitToJsonString(empty));
    REQUIRE(rEmpty.kit.has_value());
    CHECK(rEmpty.kit->slots == empty.slots);
}

TEST_CASE("version 1 kits load as all synth", "[kitfile]")
{
    auto v = parsed(makeFactoryKit());
    v.getDynamicObject()->setProperty("version", 1);
    for (auto& slot : *v["slots"].getArray())
        slot.getDynamicObject()->removeProperty("source");
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    for (const auto& s : r.kit->slots)
        CHECK(s.source == SourceType::Synth);
}

TEST_CASE("empty slots ignore params and may omit the name", "[kitfile]")
{
    auto v = parsed(makeFactoryKit());
    auto* slot = v["slots"][4].getDynamicObject();
    slot->setProperty("source", "empty");
    slot->removeProperty("name");
    v["slots"][4]["params"].getDynamicObject()->setProperty("pitch", 1234.0);
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    SlotParams expected = makeFactoryKit().slots[4];
    expected.source = SourceType::Empty;
    CHECK(r.kit->slots[4] == expected);
    CHECK(r.kit->names[4].empty());
}

TEST_CASE("unknown or missing sound sources are rejected in version 2", "[kitfile]")
{
    auto sample = parsed(makeFactoryKit());
    sample.getDynamicObject()->setProperty("version", 2);
    sample["slots"][4].getDynamicObject()->setProperty("source", "sample");
    const auto r = reparse(sample);
    CHECK_FALSE(r.kit.has_value());
    CHECK(r.error == juce::String("Slot 5: unknown sound source."));

    auto missing = parsed(makeFactoryKit());
    missing["slots"][0].getDynamicObject()->removeProperty("source");
    CHECK_FALSE(reparse(missing).kit.has_value());
}

TEST_CASE("sample slots survive a version 6 round-trip", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[3].source = SourceType::Sample;
    k.slots[3].tuneSemis = 5.0f;
    k.samples[3] = juce::String::fromUTF8("Hörner.wav").toStdString();
    k.names[3] = "Horn";

    k.slots[3].sampleStart = 0.25f;
    k.slots[3].loopStart = 0.5f;
    k.slots[3].sampleEnd = 0.75f;
    k.slots[3].loop = true;
    k.slots[3].reverse = true;
    k.slots[3].loopXfadePct = 12.5f;
    k.slots[3].stretchMode = StretchMode::Loop;
    k.slots[3].stretchPct = 140.0f;
    k.slots[3].grainMs = 35.0f;
    const auto v = parsed(k);
    CHECK(static_cast<int>(v["version"]) == 6);
    CHECK(v["slots"][3]["source"].toString() == "sample");
    CHECK(v["slots"][3]["sample"].toString() == juce::String::fromUTF8("Hörner.wav"));
    CHECK(static_cast<double>(v["slots"][3]["params"]["tune"]) == 5.0);
    CHECK(static_cast<double>(v["slots"][3]["params"]["smpStart"]) == 0.25);
    CHECK(static_cast<double>(v["slots"][3]["params"]["loopStart"]) == 0.5);
    CHECK(static_cast<double>(v["slots"][3]["params"]["smpEnd"]) == 0.75);
    CHECK(static_cast<double>(v["slots"][3]["params"]["loop"]) == 1.0);
    CHECK(static_cast<double>(v["slots"][3]["params"]["reverse"]) == 1.0);
    CHECK(static_cast<double>(v["slots"][3]["params"]["xfade"]) == 12.5);
    CHECK(static_cast<double>(v["slots"][3]["params"]["stretch"]) == 2.0);
    CHECK(static_cast<double>(v["slots"][3]["params"]["stretchPct"]) == 140.0);
    CHECK(static_cast<double>(v["slots"][3]["params"]["grainMs"]) == 35.0);
    CHECK_FALSE(v["slots"][0].getDynamicObject()->hasProperty("sample"));

    const auto r = kitFromJsonString(kitToJsonString(k));
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->slots == k.slots);
    CHECK(r.kit->samples == k.samples);
    CHECK(r.kit->names == k.names);
}

TEST_CASE("sample slots need version 3 and a plain file name", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[3].source = SourceType::Sample;
    k.samples[3] = "horn.wav";

    auto v2 = parsed(k);
    v2.getDynamicObject()->setProperty("version", 2);
    const auto r2 = reparse(v2);
    CHECK_FALSE(r2.kit.has_value());
    CHECK(r2.error == juce::String("Slot 4: unknown sound source."));

    for (const char* bad : { "", "../x.wav", "a/b.wav", "a\\b.wav", "C:x.wav", " x.wav" })
    {
        auto v = parsed(k);
        v["slots"][3].getDynamicObject()->setProperty("sample", bad);
        const auto r = reparse(v);
        CHECK_FALSE(r.kit.has_value());
        CHECK(r.error == juce::String("Slot 4: invalid sample file name."));
    }

    auto missing = parsed(k);
    missing["slots"][3].getDynamicObject()->removeProperty("sample");
    CHECK(reparse(missing).error == juce::String("Slot 4: invalid sample file name."));

    CHECK(isValidSampleFileName("horn.wav"));
    CHECK(isValidSampleFileName(juce::String::fromUTF8("Hörner (2).flac")));
}

TEST_CASE("version 3 kits load with default region fields", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[3].source = SourceType::Sample;
    k.slots[3].tuneSemis = 5.0f;
    k.samples[3] = "horn.wav";

    auto v = parsed(k);
    v.getDynamicObject()->setProperty("version", 3);
    for (int s = 0; s < kNumSlots; ++s)
        if (auto* params = v["slots"][s]["params"].getDynamicObject())
            for (const char* key : { "smpStart", "loopStart", "smpEnd", "loop", "reverse", "xfade", "stretch", "stretchPct", "grainMs" })
                params->removeProperty(key);

    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->slots == k.slots); // k trägt die Standardwerte der neuen Felder
    CHECK(r.kit->slots[3].sampleEnd == 1.0f);
    CHECK_FALSE(r.kit->slots[3].loop);
    CHECK(r.kit->samples == k.samples);
}

TEST_CASE("version 7 kits are rejected as too new", "[kitfile]")
{
    auto v = parsed(makeFactoryKit());
    v.getDynamicObject()->setProperty("version", 7);
    const auto r = reparse(v);
    CHECK_FALSE(r.kit.has_value());
    CHECK(r.error.contains("newer version"));
}

TEST_CASE("a version 4 kit without stretch keys loads with stretch off", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[3].source = SourceType::Sample;
    k.samples[3] = "horn.wav";
    k.names[3] = "Horn";
    auto v = parsed(k);
    v.getDynamicObject()->setProperty("version", 4);
    auto* params = v["slots"][3]["params"].getDynamicObject();
    params->removeProperty("stretch");
    params->removeProperty("stretchPct");
    params->removeProperty("grainMs");
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->slots[3].stretchMode == StretchMode::Off);
    CHECK(r.kit->slots[3].stretchPct == 100.0f);
    CHECK(r.kit->slots[3].grainMs == 20.0f);
}

TEST_CASE("pad colours survive a version 6 round-trip", "[kitfile][padcolour]")
{
    Kit k = makeFactoryKit();
    k.colours[0] = 3;
    k.colours[15] = 7;
    const auto json = kitToJsonString(k);
    CHECK(static_cast<int>(juce::JSON::parse(json)["version"]) == 6);
    const auto r = kitFromJsonString(json);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->colours == k.colours);
}

TEST_CASE("a version 5 kit without colours loads with the default colour", "[kitfile][padcolour]")
{
    Kit k = makeFactoryKit();
    k.colours[2] = 4;
    auto v = parsed(k);
    v.getDynamicObject()->setProperty("version", 5);
    for (int s = 0; s < kNumSlots; ++s)
        v["slots"][s].getDynamicObject()->removeProperty("colour");
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(std::all_of(r.kit->colours.begin(), r.kit->colours.end(), [](int c) { return c == 0; }));
}

TEST_CASE("invalid colour values load as the default colour", "[kitfile][padcolour]")
{
    auto v = parsed(makeFactoryKit());
    v["slots"][0].getDynamicObject()->setProperty("colour", 9);
    v["slots"][1].getDynamicObject()->setProperty("colour", -1);
    v["slots"][2].getDynamicObject()->setProperty("colour", "blau");
    v["slots"][3].getDynamicObject()->setProperty("colour", true);
    v["slots"][4].getDynamicObject()->setProperty("colour", 2.0);
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->colours[0] == 0);
    CHECK(r.kit->colours[1] == 0);
    CHECK(r.kit->colours[2] == 0);
    CHECK(r.kit->colours[3] == 0);
    CHECK(r.kit->colours[4] == 2);
}

TEST_CASE("empty slots always load with the default colour", "[kitfile][padcolour]")
{
    Kit k = makeFactoryKit();
    k.slots[6].source = SourceType::Empty;
    auto v = parsed(k);
    v["slots"][6].getDynamicObject()->setProperty("colour", 5);
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->colours[6] == 0);
}
