#pragma once
#include <cmath>
#include <memory>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>

namespace dgtest {

struct TempDir
{
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("dgsamples", "", false);
    TempDir() { dir.createDirectory(); }
    ~TempDir() { dir.deleteRecursively(); }
};

// Schreibt einen Sinus in Kanal 0; weitere Kanäle bleiben still.
inline void writeSine(juce::AudioFormat& format, const juce::File& file, float hz, double sampleRate, int numSamples,
                      int numChannels = 1, float amp = 0.5f)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> out = std::make_unique<juce::FileOutputStream>(file);
    auto writer = format.createWriterFor(out, juce::AudioFormatWriterOptions {}
                                                  .withSampleRate(sampleRate)
                                                  .withNumChannels(numChannels)
                                                  .withBitsPerSample(24));
    REQUIRE(writer != nullptr);
    juce::AudioBuffer<float> buf(numChannels, numSamples);
    buf.clear();
    for (int i = 0; i < numSamples; ++i)
        buf.setSample(0, i, amp * static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * hz * i / sampleRate)));
    REQUIRE(writer->writeFromAudioSampleBuffer(buf, 0, numSamples));
}

} // namespace dgtest
