#pragma once
#include <array>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Kit.h"
#include "engine/SampleData.h"
#include "plugin/Config.h"
#include "plugin/ParameterLayout.h"
#include "plugin/SampleLoader.h"

namespace dg {

class DubgefahrenProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    DubgefahrenProcessor();
    ~DubgefahrenProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
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
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- Message-Thread-API (Editor, Tests) ---
    // Sample-Zustand (Slots, Tickets, Garbage, Probleme) gehört dem Message-Thread. setStateInformation
    // kann von einem anderen Thread kommen und verschiebt das Neuladen der Samples dann auf den Timer.
    juce::AudioProcessorValueTreeState& state() { return apvts_; }
    juce::String slotName(int slot) const;
    void setSlotName(int slot, const juce::String& name);
    void setSlot(int slot, const SlotParams& params, const juce::String& name, const juce::String& sample = {});
    SourceType slotSource(int slot) const;
    // Leert den Slot: Quelle Empty, Name leer; die Synth-Parameter bleiben erhalten.
    void clearSlot(int slot);
    // Setzt den Slot auf die Factory-Sirene dieses Slots (Quelle Synth, Factory-Name).
    void resetSlotToFactory(int slot);
    juce::File kitFile() const;
    void setKitFile(const juce::File& file); // nach einem Export
    juce::File sampleFolder() const;
    juce::String slotSample(int slot) const;
    // Wählt ein Sample aus dem Kit-Ordner (Quelle Sample, Name = Dateiname ohne Endung).
    void setSlotSample(int slot, const juce::String& fileName);
    bool isSampleLoaded(int slot) const;
    // Sample-Slot, dessen Datei nicht geladen werden konnte: Verweis bleibt, der Slot ist stumm.
    bool isSampleMissing(int slot) const;
    // Geladene Daten eines Sample-Slots für die Anzeige (nullptr: keine). Nur im Message-Thread.
    std::shared_ptr<const SampleData> slotSampleData(int slot) const;
    // Liefert die gesammelten Ladeprobleme ("Slot N: datei – grund") und leert die Liste.
    juce::StringArray takeSampleProblems();
    // True, solange ein Ladeauftrag läuft, dessen Ergebnis noch nicht übernommen wurde (oder ein Reload ansteht).
    bool hasPendingSampleLoads() const;
    // Wartet auf alle Ladeaufträge und übernimmt die Ergebnisse (Tests).
    void waitForSampleLoads();
    std::size_t pendingSampleGarbage() const { return garbage_.size(); }
    Kit currentKit();
    // Übernimmt ein Kit samt zugehöriger Kit-Datei (leer = keine Kit-Datei, z. B. Factory-Kit).
    void applyKit(const Kit& kit, const juce::File& kitFile = {});
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
    // Erhöht sich, wenn sich Slot-Namen/-Parameter oder Editor-Settings von außerhalb des
    // Editors ändern (Host-Restore, applyKit, setSlot, setSlotName), damit der Editor per
    // Timer erkennen kann, dass er seine Anzeige neu einlesen muss.
    int stateGeneration() const { return stateGeneration_.load(); }
    // Anteil der Rechenzeit von processBlock am Echtzeit-Budget eines Blocks (geglättet, 1 = 100 %).
    double cpuLoad() const { return loadMeasurer_.getLoadAsProportion(); }

private:
    static constexpr int kStateVersion = 1;
    static constexpr int kUiFifoSize = 128;

    void pushUiEvent(EngineEvent::Type type, int slot);
    void ensureStateChildren();
    juce::ValueTree namesTree() const;
    void timerCallback() override;
    void handleSampleResults();
    void reloadAllSamples();
    void requestSampleLoad(int slot);
    void releaseSample(int slot);
    void sampleLoadFailed(int slot, const juce::String& name, const juce::String& reason);
    void setSlotSampleRef(int slot, const juce::String& name);
    void collectGarbage();

    juce::AudioProcessorValueTreeState apvts_;
    ParamCache cache_;
    Engine engine_;
    EngineParams engineParams_;
    std::vector<EngineEvent> events_;
    juce::AbstractFifo uiFifo_ { kUiFifoSize };
    std::array<EngineEvent, kUiFifoSize> uiEvents_ {};
    // Fängt eine previewRelease() auf, die eine volle FIFO verwirft: processBlock erzeugt
    // nach dem Leeren der FIFO für jeden gesetzten Slot zusätzlich ein PreviewOff.
    std::array<std::atomic<bool>, kNumSlots> pendingRelease_ {};
    bool lastPanic_ = false;
    KitFolderInfo kitFolder_;
    std::atomic<int> stateGeneration_ { 0 };
    juce::AudioProcessLoadMeasurer loadMeasurer_;
    SampleLoader loader_;
    std::array<std::shared_ptr<const SampleData>, kNumSlots> sampleData_ {};
    std::array<std::atomic<const SampleData*>, kNumSlots> samplePtrs_ {};
    std::array<std::uint64_t, kNumSlots> sampleTickets_ {};
    std::array<std::uint64_t, kNumSlots> pendingTickets_ {}; // 0 = kein offener Auftrag
    // Ersetzte Daten mit dem Blockzähler beim Tausch; frei, sobald danach ein Block fertig ist.
    std::vector<std::pair<std::shared_ptr<const SampleData>, std::uint64_t>> garbage_;
    std::atomic<std::uint64_t> blocksProcessed_ { 0 };
    std::atomic<bool> audioActive_ { false };
    std::atomic<bool> reloadPending_ { false };
    juce::StringArray sampleProblems_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DubgefahrenProcessor)
};

} // namespace dg
