#include "engine/Engine.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

namespace {
bool slotPlayable(const SlotParams& p, const SampleData* d)
{
    if (p.source == SourceType::Synth)
        return true;
    return p.source == SourceType::Sample && d != nullptr && !d->samples.empty();
}
} // namespace

void Engine::Bank::startVoice(int slot)
{
    const auto s = static_cast<std::size_t>(slot);
    e_.applied_[s] = e_.params_->global.perf; // der gestartete Slot wird Fokus
    const bool sample = e_.params_->slots[s].source == SourceType::Sample;
    // Wechselt die Stimmenart, blendet die bisherige per Kill-Fade aus.
    if (sample)
        e_.voices_[s].kill();
    else
        e_.samplers_[s].kill();
    e_.useSample_[s] = sample;
    if (sample)
        e_.samplers_[s].start(e_.contextFor(slot), e_.seed_++);
    else
        e_.voices_[s].start(e_.contextFor(slot), e_.seed_++);
}

void Engine::Bank::releaseVoice(int slot)
{
    const auto s = static_cast<std::size_t>(slot);
    const auto ctx = e_.contextFor(slot);
    e_.voices_[s].release(ctx);
    e_.samplers_[s].release(ctx);
}

void Engine::Bank::killVoice(int slot)
{
    const auto s = static_cast<std::size_t>(slot);
    e_.voices_[s].kill();
    e_.samplers_[s].kill();
}

bool Engine::Bank::isVoiceActive(int slot) const
{
    const auto s = static_cast<std::size_t>(slot);
    return e_.voices_[s].isActive() || e_.samplers_[s].isActive();
}

bool Engine::Bank::isVoiceReleasing(int slot) const
{
    const auto s = static_cast<std::size_t>(slot);
    return e_.useSample_[s] ? e_.samplers_[s].isReleasing() : e_.voices_[s].isReleasing();
}

void Engine::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    maxBlock_ = std::max(1, maxBlockSize);
    for (auto& v : voices_)
        v.prepare(sampleRate);
    for (auto& v : samplers_)
        v.prepare(sampleRate);
    useSample_.fill(false);
    lastSample_.fill(nullptr);
    applied_.fill(PerfOffsets {});
    hasSound_.fill(true);
    smGl_.fill(0.0f);
    smGr_.fill(0.0f);
    smSend_.fill(0.0f);
    smInit_.fill(false);
    router_.prepare(sampleRate);
    fx_.prepare(sampleRate);
    for (auto* b : { &mainL_, &mainR_, &sendL_, &sendR_, &voiceBuf_, &sampleBuf_ })
        b->assign(static_cast<std::size_t>(maxBlock_), 0.0f);
    wasPlaying_ = false;
    activeMask_.store(0);
    latchedMask_.store(0);
    focus_.store(0);
}

VoiceContext Engine::contextFor(int slot) const
{
    const auto s = static_cast<std::size_t>(slot);
    return VoiceContext { &params_->slots[s], bpm_, applied_[s], params_->samples[s] };
}

void Engine::process(float* outL, float* outR, int numSamples, const EngineParams& params,
                     const EngineEvent* events, int numEvents, const TransportInfo& transport)
{
    params_ = &params;
    bpm_ = transport.bpm > 0.0 ? transport.bpm : 120.0;
    for (std::size_t s = 0; s < trig_.size(); ++s)
    {
        const SlotParams& sp = params.slots[s];
        if (sp.source == SourceType::Sample)
        {
            // Sample: One Shot spielt bis zum Ende des Bereichs. Latch gilt nur für geloopte
            // Samples, sonst wirkt es wie Gate.
            const bool oneShot = sp.trigMode == TriggerMode::OneShot;
            const bool latch = sp.trigMode == TriggerMode::Latch && sp.loop;
            const TriggerMode mode = oneShot ? TriggerMode::OneShot : latch ? TriggerMode::Latch : TriggerMode::Gate;
            trig_[s] = { mode, sp.oneShotS, sp.chokeGroup, oneShot };
        }
        else
            trig_[s] = { sp.trigMode, sp.oneShotS, sp.chokeGroup, false };
    }

    // Ein Slot, der stumm wird oder dessen Sample-Daten wechseln, verstummt sofort und
    // vergisst Latch/One-Shot. Der Player liest danach nie mehr die alten Daten.
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto i = static_cast<std::size_t>(s);
        const SampleData* data = params.samples[i];
        const bool has = slotPlayable(params.slots[i], data);
        const bool dataChanged = data != lastSample_[i] && samplers_[i].isActive();
        if ((!has && hasSound_[i]) || dataChanged)
            router_.killSlot(s, bank_);
        hasSound_[i] = has;
        lastSample_[i] = data;
    }

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
        if (bank_.isVoiceActive(s))
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
    // Leere Slots starten keine Stimme, lösen keinen Choke aus und werden nicht gelatcht.
    const auto startable = [this](int slot) {
        return slot >= 0 && slot < kNumSlots && hasSound_[static_cast<std::size_t>(slot)];
    };
    switch (ev.type)
    {
        case EngineEvent::Type::NoteOn:
            if (startable(slotForNote(ev.value)))
                router_.noteOn(ev.value, trig_, bank_);
            break;
        case EngineEvent::Type::NoteOff:    router_.noteOff(ev.value, bank_); break;
        case EngineEvent::Type::PreviewOn:
            if (startable(ev.value))
                router_.previewOn(ev.value, trig_, bank_);
            break;
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
        SamplePlayer& sampler = samplers_[s];
        if (!voice.isActive() && !sampler.isActive())
        {
            applied_[s] = PerfOffsets {};
            smInit_[s] = false;
            continue;
        }
        if (g.perfTarget == PerfTarget::All || slot == focus)
            applied_[s] = g.perf;

        const VoiceContext ctx = contextFor(slot);
        voice.render(voiceBuf_.data(), len, ctx);
        if (sampler.isActive())
        {
            sampler.render(sampleBuf_.data(), len, ctx);
            for (int i = 0; i < len; ++i)
                voiceBuf_[static_cast<std::size_t>(i)] += sampleBuf_[static_cast<std::size_t>(i)];
        }

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
