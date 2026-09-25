#include "plugin/PluginProcessor.h"
#include <algorithm>
#include "plugin/PluginEditor.h"

namespace dg {

namespace {
const juce::Identifier kNamesId { "SLOTNAMES" };
const juce::Identifier kUiScaleId { "uiScale" };
const juce::Identifier kFollowFocusId { "followFocus" };
const juce::Identifier kVersionId { "version" };

juce::Identifier nameKey(int slot) { return juce::Identifier("n" + juce::String(slot + 1)); }
} // namespace

DubgefahrenProcessor::DubgefahrenProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "DUBGEFAHREN", createParameterLayout(makeFactoryKit())),
      cache_(apvts_)
{
    events_.reserve(2048);
    ensureStateChildren();
    kitFolder_ = resolveKitFolder(pluginConfigFile(), defaultKitFolder());
    engine_.prepare(44100.0, 512); // gültiger Zustand schon vor prepareToPlay
}

void DubgefahrenProcessor::ensureStateChildren()
{
    auto names = apvts_.state.getOrCreateChildWithName(kNamesId, nullptr);
    const Kit factory = makeFactoryKit();
    for (int s = 0; s < kNumSlots; ++s)
        if (!names.hasProperty(nameKey(s)))
            names.setProperty(nameKey(s), juce::String::fromUTF8(factory.names[static_cast<std::size_t>(s)].c_str()), nullptr);
    if (!apvts_.state.hasProperty(kUiScaleId))
        apvts_.state.setProperty(kUiScaleId, 1.0f, nullptr);
    if (!apvts_.state.hasProperty(kFollowFocusId))
        apvts_.state.setProperty(kFollowFocusId, true, nullptr);
}

juce::ValueTree DubgefahrenProcessor::namesTree() const { return apvts_.state.getChildWithName(kNamesId); }

void DubgefahrenProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepare(sampleRate, samplesPerBlock);
    lastPanic_ = false;
}

bool DubgefahrenProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void DubgefahrenProcessor::pushUiEvent(EngineEvent::Type type, int slot)
{
    const auto scope = uiFifo_.write(1);
    if (scope.blockSize1 > 0)
        uiEvents_[static_cast<std::size_t>(scope.startIndex1)] = EngineEvent { type, 0, slot };
}

void DubgefahrenProcessor::previewPress(int slot) { pushUiEvent(EngineEvent::Type::PreviewOn, slot); }
void DubgefahrenProcessor::previewRelease(int slot) { pushUiEvent(EngineEvent::Type::PreviewOff, slot); }

void DubgefahrenProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)
    {
        buffer.clear();
        return;
    }

    cache_.read(engineParams_);
    events_.clear();
    const auto push = [this](const EngineEvent& e) {
        if (events_.size() < events_.capacity())
            events_.push_back(e);
    };

    {
        const auto scope = uiFifo_.read(uiFifo_.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i)
            push(uiEvents_[static_cast<std::size_t>(scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i)
            push(uiEvents_[static_cast<std::size_t>(scope.startIndex2 + i)]);
    }

    const bool panic = cache_.panicPressed();
    if (panic && !lastPanic_)
        push(EngineEvent { EngineEvent::Type::Panic, 0, 0 });
    lastPanic_ = panic;

    for (const auto meta : midi)
    {
        // Keine juce::MidiMessage konstruieren: das würde bei SysEx (> 8 Bytes) im
        // Audio-Thread allozieren. Stattdessen die rohen Bytes direkt auswerten.
        if (meta.numBytes != 3)
            continue;
        const auto* d = meta.data;
        const int status = d[0] & 0xF0;
        const int note = d[1] & 0x7F;
        const int vel = d[2] & 0x7F;
        const int offset = std::clamp(meta.samplePosition, 0, std::max(0, numSamples - 1));
        if (status == 0x90 && vel > 0)
            push(EngineEvent { EngineEvent::Type::NoteOn, offset, note });
        else if (status == 0x80 || (status == 0x90 && vel == 0)) // Note-On mit Velocity 0 = Note-Off
            push(EngineEvent { EngineEvent::Type::NoteOff, offset, note });
    }

    TransportInfo transport;
    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            transport.isPlaying = pos->getIsPlaying();
            if (const auto bpm = pos->getBpm())
                transport.bpm = *bpm;
        }
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
    engine_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples, engineParams_,
                    events_.data(), static_cast<int>(events_.size()), transport);
    midi.clear();
}

juce::AudioProcessorEditor* DubgefahrenProcessor::createEditor()
{
    return new DubgefahrenEditor(*this);
}

void DubgefahrenProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    state.setProperty(kVersionId, kStateVersion, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void DubgefahrenProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(apvts_.state.getType()))
        return;
    // Versionsfeld: ältere Stände werden hier bei Bedarf migriert (Version 1: nichts zu tun).
    apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    ensureStateChildren();
    ++stateGeneration_;
}

juce::String DubgefahrenProcessor::slotName(int slot) const
{
    return namesTree().getProperty(nameKey(slot)).toString();
}

void DubgefahrenProcessor::setSlotName(int slot, const juce::String& name)
{
    auto names = apvts_.state.getOrCreateChildWithName(kNamesId, nullptr);
    names.setProperty(nameKey(slot), name.substring(0, 32), nullptr);
    ++stateGeneration_;
}

void DubgefahrenProcessor::setSlot(int slot, const SlotParams& params, const juce::String& name)
{
    writeSlotToParameters(apvts_, slot, params);
    setSlotName(slot, name);
    ++stateGeneration_;
}

Kit DubgefahrenProcessor::currentKit()
{
    Kit k;
    for (int s = 0; s < kNumSlots; ++s)
    {
        k.slots[static_cast<std::size_t>(s)] = readSlotFromParameters(apvts_, s);
        k.names[static_cast<std::size_t>(s)] = slotName(s).toStdString();
    }
    return k;
}

void DubgefahrenProcessor::applyKit(const Kit& kit)
{
    for (int s = 0; s < kNumSlots; ++s)
        setSlot(s, kit.slots[static_cast<std::size_t>(s)],
                juce::String::fromUTF8(kit.names[static_cast<std::size_t>(s)].c_str()));
    ++stateGeneration_;
}

float DubgefahrenProcessor::uiScale() const
{
    return static_cast<float>(apvts_.state.getProperty(kUiScaleId, 1.0f));
}

void DubgefahrenProcessor::setUiScale(float scale)
{
    apvts_.state.setProperty(kUiScaleId, std::clamp(scale, 0.75f, 2.0f), nullptr);
}

bool DubgefahrenProcessor::editorFollowsFocus() const
{
    return static_cast<bool>(apvts_.state.getProperty(kFollowFocusId, true));
}

void DubgefahrenProcessor::setEditorFollowsFocus(bool follow)
{
    apvts_.state.setProperty(kFollowFocusId, follow, nullptr);
}

} // namespace dg

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dg::DubgefahrenProcessor(); }
