#include "engine/Engine.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

void Engine::Bank::startVoice(int slot)
{
    const auto s = static_cast<std::size_t>(slot);
    e_.applied_[s] = e_.params_->global.perf; // der gestartete Slot wird Fokus
    e_.voices_[s].start(e_.contextFor(slot), e_.seed_++);
}

void Engine::Bank::releaseVoice(int slot)
{
    e_.voices_[static_cast<std::size_t>(slot)].release(e_.contextFor(slot));
}

void Engine::Bank::killVoice(int slot) { e_.voices_[static_cast<std::size_t>(slot)].kill(); }

bool Engine::Bank::isVoiceActive(int slot) const { return e_.voices_[static_cast<std::size_t>(slot)].isActive(); }

bool Engine::Bank::isVoiceReleasing(int slot) const
{
    return e_.voices_[static_cast<std::size_t>(slot)].isReleasing();
}

void Engine::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    maxBlock_ = std::max(1, maxBlockSize);
    for (auto& v : voices_)
        v.prepare(sampleRate);
    applied_.fill(PerfOffsets {});
    smGl_.fill(0.0f);
    smGr_.fill(0.0f);
    smSend_.fill(0.0f);
    smInit_.fill(false);
    router_.prepare(sampleRate);
    fx_.prepare(sampleRate);
    for (auto* b : { &mainL_, &mainR_, &sendL_, &sendR_, &voiceBuf_ })
        b->assign(static_cast<std::size_t>(maxBlock_), 0.0f);
    wasPlaying_ = false;
    activeMask_.store(0);
    latchedMask_.store(0);
    focus_.store(0);
}

VoiceContext Engine::contextFor(int slot) const
{
    const auto s = static_cast<std::size_t>(slot);
    return VoiceContext { &params_->slots[s], bpm_, applied_[s] };
}

void Engine::process(float* outL, float* outR, int numSamples, const EngineParams& params,
                     const EngineEvent* events, int numEvents, const TransportInfo& transport)
{
    params_ = &params;
    bpm_ = transport.bpm > 0.0 ? transport.bpm : 120.0;
    for (std::size_t s = 0; s < trig_.size(); ++s)
        trig_[s] = { params.slots[s].trigMode, params.slots[s].oneShotS, params.slots[s].chokeGroup };

    if (wasPlaying_ && !transport.isPlaying)
        router_.transportStopped(params.global.latchStop, bank_);
    wasPlaying_ = transport.isPlaying;

    int ev = 0;
    for (int done = 0; done < numSamples;)
    {
        const int n = std::min(maxBlock_, numSamples - done);
        const int first = ev;
        while (ev < numEvents && events[ev].sampleOffset < done + n)
            ++ev;
        processChunk(outL + done, outR + done, n, events + first, ev - first, done);
        done += n;
    }
    for (; ev < numEvents; ++ev)
        handleEvent(events[ev]);

    std::uint32_t active = 0;
    for (int s = 0; s < kNumSlots; ++s)
        if (voices_[static_cast<std::size_t>(s)].isActive())
            active |= 1u << s;
    activeMask_.store(active, std::memory_order_relaxed);
    latchedMask_.store(router_.latchedMask(), std::memory_order_relaxed);
    focus_.store(router_.focusSlot(), std::memory_order_relaxed);
}

void Engine::processChunk(float* outL, float* outR, int n, const EngineEvent* events, int numEvents, int base)
{
    const auto len = static_cast<std::size_t>(n);
    std::fill_n(mainL_.begin(), len, 0.0f);
    std::fill_n(mainR_.begin(), len, 0.0f);
    std::fill_n(sendL_.begin(), len, 0.0f);
    std::fill_n(sendR_.begin(), len, 0.0f);

    int pos = 0;
    for (int i = 0; i < numEvents; ++i)
    {
        const int offset = std::clamp(events[i].sampleOffset - base, 0, n);
        if (offset > pos)
        {
            renderSegment(pos, offset - pos);
            pos = offset;
        }
        handleEvent(events[i]);
    }
    if (pos < n)
        renderSegment(pos, n - pos);

    fx_.process(mainL_.data(), mainR_.data(), sendL_.data(), sendR_.data(), n, params_->global.fx, bpm_);
    std::copy_n(mainL_.begin(), len, outL);
    std::copy_n(mainR_.begin(), len, outR);
}

void Engine::handleEvent(const EngineEvent& ev)
{
    switch (ev.type)
    {
        case EngineEvent::Type::NoteOn:     router_.noteOn(ev.value, trig_, bank_); break;
        case EngineEvent::Type::NoteOff:    router_.noteOff(ev.value, bank_); break;
        case EngineEvent::Type::PreviewOn:  router_.previewOn(ev.value, trig_, bank_); break;
        case EngineEvent::Type::PreviewOff: router_.previewOff(ev.value, bank_); break;
        case EngineEvent::Type::Panic:      router_.panic(bank_); break;
    }
}

void Engine::renderSegment(int start, int len)
{
    // Ein Segment darf keinen One-Shot-Ablauf überschreiten, sonst wird zu spät released.
    while (len > 0)
    {
        const int n = std::min(len, std::max(1, router_.samplesUntilNextExpiry()));
        renderSubSegment(start, n);
        router_.advance(n, bank_);
        start += n;
        len -= n;
    }
}

void Engine::renderSubSegment(int start, int len)
{
    if (len <= 0)
        return;
    const GlobalParams& g = params_->global;
    const int focus = router_.focusSlot();

    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        const auto s = static_cast<std::size_t>(slot);
        SirenVoice& voice = voices_[s];
        if (!voice.isActive())
        {
            applied_[s] = PerfOffsets {};
            smInit_[s] = false;
            continue;
        }
        if (g.perfTarget == PerfTarget::All || slot == focus)
            applied_[s] = g.perf;

        voice.render(voiceBuf_.data(), len, contextFor(slot));

        const SlotParams& sp = params_->slots[s];
        const float gain = volumeDbToGain(sp.volumeDb);
        const float angle = (std::clamp(sp.pan, -1.0f, 1.0f) + 1.0f) * kPi * 0.25f;
        const float targetGl = std::cos(angle) * gain;
        const float targetGr = std::sin(angle) * gain;
        const float targetSend = std::clamp(sp.fxSend, 0.0f, 1.0f);

        if (!smInit_[s])
        {
            smGl_[s] = targetGl;
            smGr_[s] = targetGr;
            smSend_[s] = targetSend;
            smInit_[s] = true;
        }
        const float coeff = onePoleCoeff(0.02f, sampleRate_);

        for (int i = 0; i < len; ++i)
        {
            smGl_[s] += coeff * (targetGl - smGl_[s]);
            smGr_[s] += coeff * (targetGr - smGr_[s]);
            smSend_[s] += coeff * (targetSend - smSend_[s]);
            const float dry = 1.0f - smSend_[s];

            const float v = voiceBuf_[static_cast<std::size_t>(i)];
            const auto o = static_cast<std::size_t>(start + i);
            mainL_[o] += v * smGl_[s] * dry;
            mainR_[o] += v * smGr_[s] * dry;
            sendL_[o] += v * smGl_[s] * smSend_[s];
            sendR_[o] += v * smGr_[s] * smSend_[s];
        }
    }
}

} // namespace dg
