#include <catch2/catch_test_macros.hpp>
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
    newer.getDynamicObject()->setProperty("version", 3);
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

TEST_CASE("kits with empty slots are written as version 2 and survive a round-trip", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[2].source = SourceType::Empty;
    k.names[2].clear();

    const auto v = parsed(k);
    CHECK(static_cast<int>(v["version"]) == 2);
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
    sample["slots"][4].getDynamicObject()->setProperty("source", "sample");
    const auto r = reparse(sample);
    CHECK_FALSE(r.kit.has_value());
    CHECK(r.error == juce::String("Slot 5: unknown sound source."));

    auto missing = parsed(makeFactoryKit());
    missing["slots"][0].getDynamicObject()->removeProperty("source");
    CHECK_FALSE(reparse(missing).kit.has_value());
}
