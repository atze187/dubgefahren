#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>
#include "engine/FxChain.h"
#include "engine/FxParams.h"
#include "engine/PadRouter.h"
#include "engine/SirenVoice.h"

namespace dg {

enum class PerfTarget { Focus, All };

struct GlobalParams
{
    FxParams fx {};
    PerfOffsets perf {};
    PerfTarget perfTarget = PerfTarget::Focus;
    LatchStopAction latchStop = LatchStopAction::Release;
};

struct EngineParams
{
    std::array<SlotParams, kNumSlots> slots {};
    GlobalParams global {};
};

struct TransportInfo
{
    bool isPlaying = false;
    double bpm = 120.0;
};

struct EngineEvent
{
    enum class Type { NoteOn, NoteOff, PreviewOn, PreviewOff, Panic };
    Type type = Type::NoteOn;
    int sampleOffset = 0;
    int value = 0; // Note (NoteOn/NoteOff) oder Slot (Preview)
};

class Engine
{
public:
    void prepare(double sampleRate, int maxBlockSize);

    // Überschreibt outL/outR. events müssen nach sampleOffset sortiert sein.
    void process(float* outL, float* outR, int numSamples, const EngineParams& params,
                 const EngineEvent* events, int numEvents, const TransportInfo& transport);

    std::uint32_t activeMask() const { return activeMask_.load(std::memory_order_relaxed); }
    std::uint32_t latchedMask() const { return latchedMask_.load(std::memory_order_relaxed); }
    int focusSlot() const { return focus_.load(std::memory_order_relaxed); }
    // Nur im Audio-Thread bzw. in Tests lesen.
    const PerfOffsets& appliedPerf(int slot) const { return applied_[static_cast<std::size_t>(slot)]; }

private:
    class Bank final : public VoiceControl
    {
    public:
        explicit Bank(Engine& e) : e_(e) {}
        void startVoice(int slot) override;
        void releaseVoice(int slot) override;
        void killVoice(int slot) override;
        bool isVoiceActive(int slot) const override;
        bool isVoiceReleasing(int slot) const override;

    private:
        Engine& e_;
    };

    void processChunk(float* outL, float* outR, int n, const EngineEvent* events, int numEvents, int base);
    void handleEvent(const EngineEvent& ev);
    void renderSegment(int start, int len);
    void renderSubSegment(int start, int len);
    VoiceContext contextFor(int slot) const;

    double sampleRate_ = 44100.0;
    int maxBlock_ = 512;
    std::array<SirenVoice, kNumSlots> voices_ {};
    std::array<PerfOffsets, kNumSlots> applied_ {};
    // 20 ms Ein-Pol-Glättung der Slot-Mischwerte gegen Zipper-Rauschen; springt auf das
    // Ziel, wenn eine Stimme aus dem Leerlauf startet.
    std::array<float, kNumSlots> smGl_ {};
    std::array<float, kNumSlots> smGr_ {};
    std::array<float, kNumSlots> smSend_ {};
    std::array<bool, kNumSlots> smInit_ {};
    PadRouter router_;
    Bank bank_ { *this };
    FxChain fx_;
    std::vector<float> mainL_, mainR_, sendL_, sendR_, voiceBuf_;

    const EngineParams* params_ = nullptr;
    TriggerSettingsArray trig_ {};
    double bpm_ = 120.0;
    bool wasPlaying_ = false;
    std::uint32_t seed_ = 1;

    std::atomic<std::uint32_t> activeMask_ { 0 };
    std::atomic<std::uint32_t> latchedMask_ { 0 };
    std::atomic<int> focus_ { 0 };
};

} // namespace dg
