#include "plugin/SampleLoader.h"
#include <limits>
#include <new>

namespace dg {

SampleLoader::~SampleLoader()
{
    // Laufende Aufträge greifen auf lock_ und results_ zu: ohne Timeout (-1) warten,
    // sonst würden diese bei einem langen Decode vor dem Job zerstört.
    pool_.removeAllJobs(true, -1);
}

std::shared_ptr<const SampleData> SampleLoader::decode(const juce::File& file, juce::String& error)
{
    if (!file.existsAsFile())
    {
        error = "file not found";
        return nullptr;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->numChannels == 0 || reader->lengthInSamples <= 0)
    {
        error = "unsupported or damaged file";
        return nullptr;
    }
    if (static_cast<double>(reader->lengthInSamples) > kMaxSampleSeconds * reader->sampleRate)
    {
        error = "longer than 60 seconds";
        return nullptr;
    }

    // Header-Werte sind nicht vertrauenswürdig: vor dem Allokieren begrenzen.
    constexpr juce::int64 kMaxFrames = static_cast<juce::int64>(kMaxSampleSeconds * 384000.0);
    constexpr unsigned int kMaxChannels = 32;
    if (reader->lengthInSamples > kMaxFrames || reader->lengthInSamples > std::numeric_limits<int>::max()
        || reader->numChannels > kMaxChannels)
    {
        error = "unsupported or damaged file";
        return nullptr;
    }

    std::shared_ptr<SampleData> data;
    try
    {
        const int length = static_cast<int>(reader->lengthInSamples);
        const int channels = static_cast<int>(reader->numChannels);
        juce::AudioBuffer<float> buffer(channels, length);
        if (!reader->read(buffer.getArrayOfWritePointers(), channels, 0, length))
        {
            error = "unsupported or damaged file";
            return nullptr;
        }

        data = std::make_shared<SampleData>();
        data->sampleRate = reader->sampleRate;
        data->samples.assign(static_cast<std::size_t>(length), 0.0f);
        const float scale = 1.0f / static_cast<float>(channels);
        for (int c = 0; c < channels; ++c)
        {
            const float* src = buffer.getReadPointer(c);
            for (int i = 0; i < length; ++i)
                data->samples[static_cast<std::size_t>(i)] += src[i] * scale;
        }
    }
    catch (const std::bad_alloc&)
    {
        error = "unsupported or damaged file";
        return nullptr;
    }
    return data;
}

void SampleLoader::request(int slot, std::uint64_t ticket, const juce::File& file)
{
    pool_.addJob([this, slot, ticket, file] {
        SampleLoadResult r;
        r.slot = slot;
        r.ticket = ticket;
        r.data = decode(file, r.error);
        const juce::ScopedLock sl(lock_);
        results_.push_back(std::move(r));
    });
}

std::vector<SampleLoadResult> SampleLoader::takeResults()
{
    const juce::ScopedLock sl(lock_);
    std::vector<SampleLoadResult> out;
    out.swap(results_);
    return out;
}

void SampleLoader::waitForAll()
{
    while (pool_.getNumJobs() > 0)
        juce::Thread::sleep(1);
}

} // namespace dg
