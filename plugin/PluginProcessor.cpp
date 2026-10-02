#include "plugin/PluginProcessor.h"
#include <algorithm>
#include <cmath>
#include "engine/SlotFields.h"
#include "plugin/KitFile.h"
#include "plugin/PluginEditor.h"
#include "plugin/SampleFiles.h"

namespace dg {

namespace {
const juce::Identifier kNamesId { "SLOTNAMES" };
const juce::Identifier kUiScaleId { "uiScale" };
const juce::Identifier kFollowFocusId { "followFocus" };
const juce::Identifier kVersionId { "version" };

const juce::Identifier kSamplesId { "SLOTSAMPLES" };
const juce::Identifier kKitFileId { "kitFile" };

juce::Identifier sampleKey(int slot) { return juce::Identifier("s" + juce::String(slot + 1)); }
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
    startTimerHz(20);
}

DubgefahrenProcessor::~DubgefahrenProcessor() { stopTimer(); }

void DubgefahrenProcessor::ensureStateChildren()
{
    apvts_.state.getOrCreateChildWithName(kSamplesId, nullptr);
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
    loadMeasurer_.reset(sampleRate, samplesPerBlock);
    audioActive_.store(true);
}

void DubgefahrenProcessor::releaseResources() { audioActive_.store(false); }

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

void DubgefahrenProcessor::previewRelease(int slot)
{
    pushUiEvent(EngineEvent::Type::PreviewOff, slot);
    if (slot >= 0 && slot < kNumSlots)
        pendingRelease_[static_cast<std::size_t>(slot)].store(true, std::memory_order_relaxed);
}

void DubgefahrenProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::AudioProcessLoadMeasurer::ScopedTimer loadTimer(loadMeasurer_, buffer.getNumSamples());
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)
    {
        buffer.clear();
        return;
    }

    cache_.read(engineParams_);
    for (int s = 0; s < kNumSlots; ++s)
        engineParams_.samples[static_cast<std::size_t>(s)] =
            samplePtrs_[static_cast<std::size_t>(s)].load(std::memory_order_seq_cst);
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

    // Eine previewRelease() darf nie verloren gehen, auch wenn die FIFO oben voll war:
    // für jeden Slot mit gesetztem Flag zusätzlich ein PreviewOff erzeugen. Ein doppeltes
    // PreviewOff ist harmlos (PadRouter::previewOff ignoriert nicht gehaltene Slots).
    for (int s = 0; s < kNumSlots; ++s)
        if (pendingRelease_[static_cast<std::size_t>(s)].exchange(false, std::memory_order_relaxed))
            push(EngineEvent { EngineEvent::Type::PreviewOff, 0, s });

    const bool panic = cache_.panicPressed();
    if (panic && !lastPanic_)
        push(EngineEvent { EngineEvent::Type::Panic, 0, 0 });
    lastPanic_ = panic;

    const int firstPadNote = padMapping_.firstNote();
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
        // Die Engine kennt nur Note 36 bis 51 als Pad 1 bis 16: dazwischen liegt die einstellbare Basis.
        const int engineNote = translateNote(note, firstPadNote);
        if (engineNote < 0)
            continue;
        const int offset = std::clamp(meta.samplePosition, 0, std::max(0, numSamples - 1));
        if (status == 0x90 && vel > 0)
            push(EngineEvent { EngineEvent::Type::NoteOn, offset, engineNote });
        else if (status == 0x80 || (status == 0x90 && vel == 0)) // Note-On mit Velocity 0 = Note-Off
            push(EngineEvent { EngineEvent::Type::NoteOff, offset, engineNote });
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
    blocksProcessed_.fetch_add(1, std::memory_order_seq_cst);
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
    // Fehlende Parameter (z. B. sNN_source aus Projekten vor #7) setzt APVTS beim
    // replaceState() auf ihren Default zurück; eine eigene Migration ist nicht nötig.
    apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    ensureStateChildren();
    if (juce::MessageManager::existsAndIsCurrentThread())
        reloadAllSamples();
    else
        reloadPending_.store(true); // Timer lädt auf dem Message-Thread neu
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

void DubgefahrenProcessor::setSlot(int slot, const SlotParams& params, const juce::String& name, const juce::String& sample)
{
    JUCE_ASSERT_MESSAGE_THREAD
    writeSlotToParameters(apvts_, slot, params);
    setSlotName(slot, name);
    const bool isSample = params.source == SourceType::Sample;
    setSlotSampleRef(slot, isSample ? sample : juce::String());
    if (isSample)
        requestSampleLoad(slot);
    else
        releaseSample(slot);
    ++stateGeneration_;
}

SourceType DubgefahrenProcessor::slotSource(int slot) const
{
    const auto* v = apvts_.getRawParameterValue(slotSourceParamId(slot));
    jassert(v != nullptr);
    return static_cast<SourceType>(std::clamp(static_cast<int>(std::lround(v->load())), 0, 2));
}

void DubgefahrenProcessor::clearSlot(int slot)
{
    SlotParams p = readSlotFromParameters(apvts_, slot);
    p.source = SourceType::Empty;
    setSlot(slot, p, {});
}

void DubgefahrenProcessor::resetSlotToFactory(int slot)
{
    const Kit factory = makeFactoryKit();
    const auto s = static_cast<std::size_t>(slot);
    setSlot(slot, factory.slots[s], juce::String::fromUTF8(factory.names[s].c_str()));
}

Kit DubgefahrenProcessor::currentKit()
{
    Kit k;
    for (int s = 0; s < kNumSlots; ++s)
    {
        k.slots[static_cast<std::size_t>(s)] = readSlotFromParameters(apvts_, s);
        k.names[static_cast<std::size_t>(s)] = slotName(s).toStdString();
        if (k.slots[static_cast<std::size_t>(s)].source == SourceType::Sample)
            k.samples[static_cast<std::size_t>(s)] = slotSample(s).toStdString();
    }
    return k;
}

void DubgefahrenProcessor::applyKit(const Kit& kit, const juce::File& kitFile)
{
    JUCE_ASSERT_MESSAGE_THREAD
    apvts_.state.setProperty(kKitFileId, kitFile.getFullPathName(), nullptr);
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto i = static_cast<std::size_t>(s);
        setSlot(s, kit.slots[i], juce::String::fromUTF8(kit.names[i].c_str()), juce::String::fromUTF8(kit.samples[i].c_str()));
    }
    ++stateGeneration_;
}

juce::File DubgefahrenProcessor::kitFile() const
{
    const auto path = apvts_.state.getProperty(kKitFileId).toString();
    return juce::File::isAbsolutePath(path) ? juce::File(path) : juce::File();
}

void DubgefahrenProcessor::setKitFile(const juce::File& file)
{
    apvts_.state.setProperty(kKitFileId, file.getFullPathName(), nullptr);
    ++stateGeneration_;
}

juce::File DubgefahrenProcessor::sampleFolder() const { return sampleFolderFor(kitFile()); }

juce::String DubgefahrenProcessor::slotSample(int slot) const
{
    return apvts_.state.getChildWithName(kSamplesId).getProperty(sampleKey(slot)).toString();
}

void DubgefahrenProcessor::setSlotSampleRef(int slot, const juce::String& name)
{
    apvts_.state.getOrCreateChildWithName(kSamplesId, nullptr).setProperty(sampleKey(slot), name, nullptr);
}

void DubgefahrenProcessor::setSlotSample(int slot, const juce::String& fileName)
{
    JUCE_ASSERT_MESSAGE_THREAD
    SlotParams p = readSlotFromParameters(apvts_, slot);
    // Marker, Loop und Reverse gehören zu einer bestimmten Datei: bei einem anderen Sample zurücksetzen.
    const bool otherSample = p.source != SourceType::Sample || slotSample(slot) != fileName;
    if (p.source != SourceType::Sample)
    {
        // Startwerte beim Wechsel auf Sample; Choke, Volume, Pan und Send bleiben.
        p.source = SourceType::Sample;
        p.tuneSemis = 0.0f;
        p.attackS = 0.0f;
        p.releaseS = 0.05f;
        p.trigMode = TriggerMode::OneShot;
    }
    if (otherSample)
        resetSampleRegionFields(p);
    const auto name = fileName.containsChar('.') ? fileName.upToLastOccurrenceOf(".", false, false) : fileName;
    setSlot(slot, p, name, fileName);
}

bool DubgefahrenProcessor::isSampleLoaded(int slot) const
{
    return samplePtrs_[static_cast<std::size_t>(slot)].load() != nullptr;
}
std::shared_ptr<const SampleData> DubgefahrenProcessor::slotSampleData(int slot) const
{
    JUCE_ASSERT_MESSAGE_THREAD
    return sampleData_[static_cast<std::size_t>(slot)];
}

bool DubgefahrenProcessor::isSampleMissing(int slot) const
{
    const auto s = static_cast<std::size_t>(slot);
    return slotSource(slot) == SourceType::Sample && !isSampleLoaded(slot) && pendingTickets_[s] == 0;
}

juce::StringArray DubgefahrenProcessor::takeSampleProblems()
{
    auto problems = sampleProblems_;
    sampleProblems_.clear();
    return problems;
}

bool DubgefahrenProcessor::hasPendingSampleLoads() const
{
    if (reloadPending_.load())
        return true;
    return std::any_of(pendingTickets_.begin(), pendingTickets_.end(), [](std::uint64_t t) { return t != 0; });
}

void DubgefahrenProcessor::requestSampleLoad(int slot)
{
    releaseSample(slot);
    const auto ticket = ++sampleTickets_[static_cast<std::size_t>(slot)];
    const auto name = slotSample(slot);
    const auto folder = sampleFolder();
    if (folder == juce::File())
    {
        sampleLoadFailed(slot, name, "no kit file");
        return;
    }
    if (!isValidSampleFileName(name))
    {
        sampleLoadFailed(slot, name, "invalid file name");
        return;
    }
    pendingTickets_[static_cast<std::size_t>(slot)] = ticket;
    loader_.request(slot, ticket, folder.getChildFile(name));
}

void DubgefahrenProcessor::releaseSample(int slot)
{
    const auto s = static_cast<std::size_t>(slot);
    ++sampleTickets_[s]; // offene Ladeaufträge für diesen Slot werden ungültig
    pendingTickets_[s] = 0;
    if (sampleData_[s] == nullptr)
        return;
    samplePtrs_[s].store(nullptr, std::memory_order_seq_cst);
    garbage_.emplace_back(std::move(sampleData_[s]), blocksProcessed_.load(std::memory_order_seq_cst));
    sampleData_[s].reset();
}

void DubgefahrenProcessor::sampleLoadFailed(int slot, const juce::String& name, const juce::String& reason)
{
    // Fehlende oder unlesbare Datei: Verweis und Regler bleiben, der Slot ist nur stumm – so geht
    // beim Speichern mit abgestecktem Sample-Laufwerk nichts verloren. Ein ungültiger Name kann nie
    // laden (nur durch manipulierte Daten möglich): dieser Slot wird leer.
    if (!isValidSampleFileName(name))
    {
        SlotParams p = readSlotFromParameters(apvts_, slot);
        p.source = SourceType::Empty;
        writeSlotToParameters(apvts_, slot, p);
        setSlotSampleRef(slot, {});
    }
    releaseSample(slot);
    sampleProblems_.add("Slot " + juce::String(slot + 1) + ": " + (name.isEmpty() ? juce::String("(no file)") : name)
                        + juce::String::fromUTF8(" – ") + reason);
    ++stateGeneration_;
}

void DubgefahrenProcessor::handleSampleResults()
{
    JUCE_ASSERT_MESSAGE_THREAD
    for (auto& r : loader_.takeResults())
    {
        const auto s = static_cast<std::size_t>(r.slot);
        if (r.ticket != sampleTickets_[s])
            continue; // veraltet: Slot wurde inzwischen geändert
        pendingTickets_[s] = 0; // Ergebnis wird jetzt übernommen
        if (r.data == nullptr)
        {
            sampleLoadFailed(r.slot, slotSample(r.slot), r.error);
            continue;
        }
        sampleData_[s] = std::move(r.data);
        samplePtrs_[s].store(sampleData_[s].get(), std::memory_order_release);
        ++stateGeneration_;
    }
    collectGarbage();
}

void DubgefahrenProcessor::collectGarbage()
{
    // Alle Zugriffe (Nullsetzen, Zählerstand, Zeiger-Laden, Zähler-Inkrement) sind seq_cst: in der
    // Gesamtordnung liegt jeder Block, der den alten Zeiger gelesen hat, vor dem Nullsetzen. Da Blöcke
    // nacheinander laufen, ist das erste Inkrement nach dem Zählerstand das Ende des einzigen Blocks,
    // der ihn noch halten kann.
    const bool active = audioActive_.load();
    const auto done = blocksProcessed_.load(std::memory_order_seq_cst);
    std::erase_if(garbage_, [&](const auto& g) { return !active || done > g.second; });
}

void DubgefahrenProcessor::reloadAllSamples()
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        if (slotSource(s) == SourceType::Sample)
            requestSampleLoad(s);
        else
            releaseSample(s);
    }
}

void DubgefahrenProcessor::timerCallback()
{
    if (reloadPending_.exchange(false))
        reloadAllSamples();
    handleSampleResults();
}

void DubgefahrenProcessor::waitForSampleLoads()
{
    if (reloadPending_.exchange(false))
        reloadAllSamples();
    loader_.waitForAll();
    handleSampleResults();
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
