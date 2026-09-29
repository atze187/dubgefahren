#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <juce_audio_formats/juce_audio_formats.h>
#include "engine/SampleData.h"

namespace dg {

constexpr double kMaxSampleSeconds = 60.0;

struct SampleLoadResult
{
    int slot = 0;
    std::uint64_t ticket = 0;
    std::shared_ptr<const SampleData> data; // nullptr bei Fehler
    juce::String error;
};

// Dekodiert Audiodateien zu SampleData (Mono-Mittelwert aller Kanäle) auf einem eigenen Thread.
class SampleLoader
{
public:
    SampleLoader() = default;
    ~SampleLoader();

    // Dekodiert synchron. Bei Fehler nullptr und error ("file not found",
    // "unsupported or damaged file", "longer than 60 seconds").
    static std::shared_ptr<const SampleData> decode(const juce::File& file, juce::String& error);

    void request(int slot, std::uint64_t ticket, const juce::File& file);
    std::vector<SampleLoadResult> takeResults();
    // Blockiert, bis alle Aufträge fertig sind (Tests, Aufräumen).
    void waitForAll();

private:
    juce::ThreadPool pool_ { juce::ThreadPoolOptions {}.withNumberOfThreads(1).withThreadName("Dubgefahren samples") };
    juce::CriticalSection lock_;
    std::vector<SampleLoadResult> results_;

    JUCE_DECLARE_NON_COPYABLE(SampleLoader)
};

} // namespace dg
