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

    CHECK_FALSE(kitFromJsonString("{ not json").kit.has_value());

    auto wrongFormat = parsed(k);
    wrongFormat.getDynamicObject()->setProperty("format", "something-else");
    CHECK_FALSE(reparse(wrongFormat).kit.has_value());

    auto newer = parsed(k);
    newer.getDynamicObject()->setProperty("version", 2);
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
