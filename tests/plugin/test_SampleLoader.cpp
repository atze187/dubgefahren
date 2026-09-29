#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/SampleLoader.h"
#include "SampleTestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
float peak(const SampleData& d)
{
    float p = 0.0f;
    for (float v : d.samples)
        p = std::max(p, std::abs(v));
    return p;
}
} // namespace

TEST_CASE("wav, aiff and flac files decode to mono at their sample rate", "[loader]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    juce::WavAudioFormat wav;
    juce::AiffAudioFormat aiff;
    juce::FlacAudioFormat flac;
    const std::pair<juce::AudioFormat*, const char*> formats[] = { { &wav, "a.wav" }, { &aiff, "a.aiff" }, { &flac, "a.flac" } };
    for (const auto& [format, name] : formats)
    {
        const auto file = tmp.dir.getChildFile(name);
        dgtest::writeSine(*format, file, 441.0f, 44100.0, 22050);
        juce::String error;
        const auto d = SampleLoader::decode(file, error);
        INFO(name);
        REQUIRE(d != nullptr);
        CHECK(error.isEmpty());
        CHECK(d->sampleRate == 44100.0);
        CHECK(d->samples.size() == 22050);
        CHECK_THAT(peak(*d), WithinAbs(0.5, 0.01));
    }
}

TEST_CASE("multi-channel files are mixed down to mono", "[loader]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    juce::WavAudioFormat wav;
    for (int channels : { 2, 3 })
    {
        const auto file = tmp.dir.getChildFile("multi" + juce::String(channels) + ".wav");
        dgtest::writeSine(wav, file, 441.0f, 48000.0, 4800, channels);
        juce::String error;
        const auto d = SampleLoader::decode(file, error);
        REQUIRE(d != nullptr);
        CHECK_THAT(peak(*d), WithinAbs(0.5 / channels, 0.01));
    }
}

TEST_CASE("mp3 is registered as a basic format on Windows", "[loader]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    CHECK(formats.findFormatForFileExtension(".mp3") != nullptr);
}

TEST_CASE("missing, damaged and too long files are rejected with a reason", "[loader]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    juce::String error;

    CHECK(SampleLoader::decode(tmp.dir.getChildFile("none.wav"), error) == nullptr);
    CHECK(error == "file not found");

    const auto junk = tmp.dir.getChildFile("junk.wav");
    REQUIRE(junk.replaceWithText("not audio at all"));
    CHECK(SampleLoader::decode(junk, error) == nullptr);
    CHECK(error == "unsupported or damaged file");

    juce::WavAudioFormat wav;
    const auto longFile = tmp.dir.getChildFile("long.wav");
    dgtest::writeSine(wav, longFile, 100.0f, 8000.0, 8000 * 61);
    CHECK(SampleLoader::decode(longFile, error) == nullptr);
    CHECK(error == "longer than 60 seconds");
}

TEST_CASE("requests are decoded in the background and returned with their ticket", "[loader]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    juce::WavAudioFormat wav;
    const auto file = tmp.dir.getChildFile("a.wav");
    dgtest::writeSine(wav, file, 441.0f, 48000.0, 4800);

    SampleLoader loader;
    loader.request(3, 7, file);
    loader.request(5, 9, tmp.dir.getChildFile("none.wav"));
    loader.waitForAll();
    auto results = loader.takeResults();
    REQUIRE(results.size() == 2);
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) { return a.slot < b.slot; });
    CHECK(results[0].slot == 3);
    CHECK(results[0].ticket == 7);
    CHECK(results[0].data != nullptr);
    CHECK(results[1].slot == 5);
    CHECK(results[1].data == nullptr);
    CHECK(results[1].error == "file not found");
    CHECK(loader.takeResults().empty());
}
