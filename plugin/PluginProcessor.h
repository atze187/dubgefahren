#pragma once
#include <array>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Kit.h"
#include "plugin/Config.h"
#include "plugin/ParameterLayout.h"

namespace dg {

class DubgefahrenProcessor final : public juce::AudioProcessor
{
public:
    DubgefahrenProcessor();
    ~DubgefahrenProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Dubgefahren"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- Message-Thread-API (Editor, Tests) ---
    juce::AudioProcessorValueTreeState& state() { return apvts_; }
    juce::String slotName(int slot) const;
    void setSlotName(int slot, const juce::String& name);
    void setSlot(int slot, const SlotParams& params, const juce::String& name);
    Kit currentKit();
    void applyKit(const Kit& kit);
    void previewPress(int slot);
    void previewRelease(int slot);
    std::uint32_t activeMask() const { return engine_.activeMask(); }
    std::uint32_t latchedMask() const { return engine_.latchedMask(); }
    int focusSlot() const { return engine_.focusSlot(); }
    float uiScale() const;
    void setUiScale(float scale);
    bool editorFollowsFocus() const;
    void setEditorFollowsFocus(bool follow);
    const KitFolderInfo& kitFolder() const { return kitFolder_; }

private:
    static constexpr int kStateVersion = 1;
    static constexpr int kUiFifoSize = 128;

    void pushUiEvent(EngineEvent::Type type, int slot);
    void ensureStateChildren();
    juce::ValueTree namesTree() const;

    juce::AudioProcessorValueTreeState apvts_;
    ParamCache cache_;
    Engine engine_;
    EngineParams engineParams_;
    std::vector<EngineEvent> events_;
    juce::AbstractFifo uiFifo_ { kUiFifoSize };
    std::array<EngineEvent, kUiFifoSize> uiEvents_ {};
    bool lastPanic_ = false;
    KitFolderInfo kitFolder_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DubgefahrenProcessor)
};

} // namespace dg
