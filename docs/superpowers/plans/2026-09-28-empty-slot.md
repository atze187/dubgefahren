# Leerer Slot mit Klangquellen-Auswahl – Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Slots können leer sein (stumm, keine Klangquelle). Ein Klick auf ein leeres Pad öffnet die Auswahl „Synth“ bzw. „Sample“ (ausgegraut). Leere Slots überstehen Host-State und Kit-Dateien (Issue #7).

**Architecture:**
- **Engine:** `SlotParams` bekommt ein Feld `source` (`SourceType`). Es ist kein `SlotField`. Die Engine verwirft Note-On und Preview auf Slots ohne Klang und stoppt Slots, die leer werden.
- **Plugin:** Der Quellentyp liegt als eigener, nicht automatisierbarer APVTS-Parameter `sNN_source` vor. Das Kit-Format steigt auf Version 2.
- **UI:** Pad-Grid, Slot-Editor und Editor-Menüs zeigen und setzen den Quellentyp.

**Tech Stack:** C++20, JUCE 8.0.15, Catch2 v3, MSVC x64, CMake.

**Spec:** `docs/superpowers/specs/2026-09-28-empty-slot-design.md`

## Global Constraints

- `engine/` darf **keine** JUCE-Header einbinden.
- Im Audio-Thread: keine Allokation, keine Locks, kein Datei-I/O.
- Quelltexte UTF-8; Nicht-ASCII-Strings an JUCE über `juce::String::fromUTF8(...)` bzw. `ui::u8(...)`.
- Kommentare im Code auf Deutsch (wie im Bestand), UI-Texte auf Englisch.
- `SourceType`-Reihenfolge ist Teil des Host-States: `Empty = 0, Synth = 1, Sample = 2`. Nie umsortieren.
- Parameter-ID: `sNN_source` (z. B. `s01_source`). Anzeigename: `SNN Source`. Einträge: `Empty`, `Synth`, `Sample`. Default `Synth`. Nicht automatisierbar. `ParameterID`-Versionshinweis **2**, alle bestehenden Parameter behalten **1**.
- Kit-Datei: Version **2**, `source` ist `"synth"` oder `"empty"`. `"sample"` wird bis #8 abgelehnt.
- `Sample` verhält sich überall wie `Empty`, bis #8 umgesetzt ist (Hilfsfunktion `hasSound`).
- Alle Befehle aus dem Repo-Root.
  - Build: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
  - Engine-Tests: `build\tests\Release\DubgefahrenTests.exe "<tag>"`
  - Plugin-Tests: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "<tag>"`
  - Alles: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
- Commits enden mit `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

- **Altes Projekt wird in eine Instanz mit leeren Slots geladen** (Ableton: Preset/Set-Wechsel ohne neue Instanz). Erwartet: alle Slots Synth. `replaceState()` behält fehlende Parameter sonst auf ihrem aktuellen Wert. → Test in Task 4.
- **Slot wird geleert, während er gelatcht oder als One-Shot klingt.** Erwartet: Er verstummt im nächsten Block, das Latch-Bit ist weg, und ein späteres „Synth“ spielt normal. → Test in Task 2.
- **Leerer Slot teilt sich eine Choke-Gruppe mit einem klingenden Slot.** Erwartet: Der klingende Slot läuft weiter. → Test in Task 2.
- **Kit-Datei mit `"source": "empty"`, aber mit (evtl. unsinnigen) `params` oder ohne `name`.** Erwartet: lädt, `params` ignoriert, Name leer. → Test in Task 3.
- **Kopieren eines leeren Slots und Einfügen auf einen Synth-Slot.** Erwartet: Das Ziel wird leer. Das ergibt sich, weil `source` Teil von `SlotParams` ist. → Test in Task 4 (`setSlot` übernimmt `source`).

---

## Dateistruktur

| Datei | Änderung |
|---|---|
| `engine/SlotParams.h` | `SourceType`, `hasSound()`, Feld `SlotParams::source` |
| `engine/Kit.h/.cpp` | `makeEmptyKit()` |
| `engine/PadRouter.h/.cpp` | `killSlot(int, VoiceControl&)` |
| `engine/Engine.h/.cpp` | `hasSound_`-Array, Event-Filter, Übergangserkennung |
| `plugin/KitFile.cpp` | Format Version 2 |
| `plugin/ParameterLayout.h/.cpp` | `slotSourceParamId`, Parameter, Read/Write, `ParamCache` |
| `plugin/PluginProcessor.h/.cpp` | `slotSource`, `clearSlot`, `resetSlotToFactory`, State-Ergänzung |
| `plugin/ui/PadGrid.h/.cpp` | Leer-Darstellung, `onEmptyClick`, `pad()`, `isEmpty()` |
| `plugin/ui/SlotEditor.h/.cpp` | Hinweis statt Regler, `refreshName()` → `refresh()` |
| `plugin/PluginEditor.h/.cpp` | Menü-Builder, Quellen-Popup, neue Menüeinträge |
| `tests/…` | siehe Tasks |
| `README.md` | Abschnitt „Kits“ ergänzen |

---

### Task 1: Datenmodell – `SourceType`, `hasSound`, `makeEmptyKit`

**Files:**
- Modify: `engine/SlotParams.h`
- Modify: `engine/Kit.h`, `engine/Kit.cpp`
- Test: `tests/test_Kit.cpp`

**Interfaces:**
- Produces: `enum class dg::SourceType { Empty, Synth, Sample }`, `constexpr bool dg::hasSound(SourceType)`, `SlotParams::source` (Default `SourceType::Synth`), `Kit dg::makeEmptyKit()`

- [ ] **Step 1: Failing tests** – am Ende von `tests/test_Kit.cpp` anfügen:

```cpp
TEST_CASE("factory kit slots are synths", "[kit]")
{
    for (const auto& s : makeFactoryKit().slots)
        CHECK(s.source == SourceType::Synth);
}

TEST_CASE("empty kit has 16 empty, unnamed slots with factory synth values", "[kit]")
{
    const Kit f = makeFactoryKit();
    const Kit e = makeEmptyKit();
    for (std::size_t s = 0; s < kNumSlots; ++s)
    {
        CHECK(e.slots[s].source == SourceType::Empty);
        CHECK(e.names[s].empty());
        SlotParams asSynth = e.slots[s];
        asSynth.source = SourceType::Synth;
        CHECK(asSynth == f.slots[s]);
    }
}

TEST_CASE("only synth slots have sound until the sample player exists", "[kit]")
{
    CHECK(hasSound(SourceType::Synth));
    CHECK_FALSE(hasSound(SourceType::Empty));
    CHECK_FALSE(hasSound(SourceType::Sample));
}
```

- [ ] **Step 2: Build und prüfen, dass es fehlschlägt**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler `SourceType`/`makeEmptyKit` nicht deklariert.

- [ ] **Step 3: Implementierung**

`engine/SlotParams.h`, nach `enum class SyncDivision …`:

```cpp
// Klangquelle eines Slots. Reihenfolge ist Teil des Host-States (Parameterwert), nie umsortieren.
enum class SourceType { Empty, Synth, Sample };

// Ob ein Slot mit dieser Quelle klingt. Sample bleibt stumm, bis der Sample-Player (#8) existiert.
constexpr bool hasSound(SourceType t) { return t == SourceType::Synth; }
```

In `struct SlotParams` direkt vor `bool operator==`:

```cpp
    SourceType source = SourceType::Synth; // kein SlotField, wird gesondert gespeichert
```

`engine/Kit.h`, unter `Kit makeFactoryKit();`:

```cpp
// Alle Slots leer und ohne Namen; die (unsichtbaren) Synth-Werte entsprechen dem Factory-Kit.
Kit makeEmptyKit();
```

`engine/Kit.cpp`, nach `makeFactoryKit()`:

```cpp
Kit makeEmptyKit()
{
    Kit k = makeFactoryKit();
    for (std::size_t s = 0; s < kNumSlots; ++s)
    {
        k.slots[s].source = SourceType::Empty;
        k.names[s].clear();
    }
    return k;
}
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build; build\tests\Release\DubgefahrenTests.exe "[kit]"`
Expected: alle `[kit]`-Tests PASS.

- [ ] **Step 5: Commit**

```bash
git add engine/SlotParams.h engine/Kit.h engine/Kit.cpp tests/test_Kit.cpp
git commit -m "feat(engine): add slot source type and empty kit (#7)"
```

---

### Task 2: Engine – leere Slots stumm schalten

**Files:**
- Modify: `engine/PadRouter.h`, `engine/PadRouter.cpp`
- Modify: `engine/Engine.h`, `engine/Engine.cpp`
- Test: `tests/test_PadRouter.cpp`, `tests/test_Engine.cpp`

**Interfaces:**
- Consumes: `SourceType`, `hasSound` (Task 1)
- Produces: `void PadRouter::killSlot(int slot, VoiceControl& voices)`

- [ ] **Step 1: Failing tests**

`tests/test_PadRouter.cpp`, am Ende:

```cpp
TEST_CASE("killSlot stops the voice and forgets latch and one-shot state", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Latch);
    s[1].mode = TriggerMode::OneShot;
    r.noteOn(36, s, v);
    r.noteOn(37, s, v);
    CHECK(r.isLatched(0));

    r.killSlot(0, v);
    r.killSlot(1, v);
    CHECK_FALSE(v.active[0]);
    CHECK_FALSE(v.active[1]);
    CHECK_FALSE(r.isLatched(0));
    CHECK(r.samplesUntilNextExpiry() == INT_MAX);

    v.log.clear();
    r.killSlot(0, v); // Stimme schon aus: kein weiteres kill
    r.killSlot(-1, v);
    r.killSlot(16, v);
    CHECK(v.log.empty());
}
```

`tests/test_Engine.cpp`, am Ende:

```cpp
TEST_CASE("empty slots ignore notes and previews", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[5].source = SourceType::Empty;
    p.slots[6].source = SourceType::Sample; // bis #8 ebenfalls stumm
    const auto o = run(e, p, 4800, { noteOn(41), noteOn(42), { EngineEvent::Type::PreviewOn, 0, 5 } });
    CHECK(dgtest::peakAbs(o.l) == 0.0f);
    CHECK(e.activeMask() == 0u);
    CHECK(e.focusSlot() == 0);
    run(e, p, 512, { noteOff(41), { EngineEvent::Type::PreviewOff, 0, 5 } }); // harmlos
    CHECK(e.activeMask() == 0u);
}

TEST_CASE("an empty slot does not choke its group", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[0].chokeGroup = 1;
    p.slots[1].chokeGroup = 1;
    p.slots[1].source = SourceType::Empty;
    run(e, p, 4800, { noteOn(36) });
    run(e, p, 4800, { noteOn(37) });
    CHECK(e.activeMask() == 1u);
}

TEST_CASE("clearing a playing latched slot stops it in the next block", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[0].trigMode = TriggerMode::Latch;
    run(e, p, 4800, { noteOn(36) });
    REQUIRE(e.latchedMask() == 1u);

    p.slots[0].source = SourceType::Empty;
    const auto o = run(e, p, 4800);
    CHECK(e.activeMask() == 0u);
    CHECK(e.latchedMask() == 0u);
    CHECK(dgtest::peakAbs(o.l, 300) < 1.0e-4f);

    p.slots[0].source = SourceType::Synth;
    run(e, p, 4800, { noteOn(36) });
    CHECK(e.activeMask() == 1u);
    CHECK(e.latchedMask() == 1u);
}
```

- [ ] **Step 2: Build – schlägt fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler `killSlot` ist kein Member von `PadRouter`.

- [ ] **Step 3: Implementierung**

`engine/PadRouter.h`, im `public`-Teil nach `void panic(VoiceControl& voices);`:

```cpp
    // Stoppt die Stimme des Slots sofort und vergisst Latch-, Halte- und One-Shot-Zustand.
    void killSlot(int slot, VoiceControl& voices);
```

`engine/PadRouter.cpp`, nach `PadRouter::panic`:

```cpp
void PadRouter::killSlot(int slot, VoiceControl& voices)
{
    if (slot < 0 || slot >= kNumSlots)
        return;
    if (voices.isVoiceActive(slot))
        voices.killVoice(slot);
    clearSlot(slot);
}
```

`engine/Engine.h`, im `private`-Teil nach `std::array<PerfOffsets, kNumSlots> applied_ {};`:

```cpp
    // Quellentyp-Zustand des vorigen Blocks: erkennt, wann ein Slot stumm geschaltet wird.
    std::array<bool, kNumSlots> hasSound_ {};
```

`engine/Engine.cpp`:

In `Engine::prepare` nach `applied_.fill(PerfOffsets {});`:

```cpp
    hasSound_.fill(true);
```

In `Engine::process` direkt nach der `trig_`-Schleife:

```cpp
    // Ein Slot, der leer wird, verstummt sofort und vergisst Latch/One-Shot.
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto i = static_cast<std::size_t>(s);
        const bool has = hasSound(params.slots[i].source);
        if (!has && hasSound_[i])
            router_.killSlot(s, bank_);
        hasSound_[i] = has;
    }
```

`Engine::handleEvent` komplett ersetzen:

```cpp
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
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build; build\tests\Release\DubgefahrenTests.exe`
Expected: alle Engine-Tests PASS, auch die bestehenden.

- [ ] **Step 5: Commit**

```bash
git add engine/PadRouter.h engine/PadRouter.cpp engine/Engine.h engine/Engine.cpp tests/test_PadRouter.cpp tests/test_Engine.cpp
git commit -m "feat(engine): keep empty slots silent and stop slots that become empty (#7)"
```

---

### Task 3: Kit-Format Version 2

**Files:**
- Modify: `plugin/KitFile.cpp`
- Test: `tests/plugin/test_KitFile.cpp`

**Interfaces:**
- Consumes: `SourceType`, `makeEmptyKit`, `makeFactoryKit` (Task 1)
- Produces: unverändert `kitToJsonString` / `kitFromJsonString`, jetzt mit `source`

- [ ] **Step 1: Failing tests**

In `tests/plugin/test_KitFile.cpp` im Test „invalid kit files are rejected with a message“ die Zeile
`newer.getDynamicObject()->setProperty("version", 2);` ersetzen durch:

```cpp
    newer.getDynamicObject()->setProperty("version", 3);
```

Am Ende anfügen:

```cpp
TEST_CASE("kits with empty slots are written as version 2 and survive a round-trip", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[2].source = SourceType::Empty;
    k.names[2].clear();

    const auto v = parsed(k);
    CHECK(static_cast<int>(v["version"]) == 2);
    CHECK(v["slots"][0]["source"].toString() == "synth");
    CHECK(v["slots"][2]["source"].toString() == "empty");
    CHECK_FALSE(v["slots"][2].getDynamicObject()->hasProperty("params"));

    const auto r = kitFromJsonString(kitToJsonString(k));
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->slots == k.slots);
    CHECK(r.kit->names == k.names);

    const Kit empty = makeEmptyKit();
    const auto rEmpty = kitFromJsonString(kitToJsonString(empty));
    REQUIRE(rEmpty.kit.has_value());
    CHECK(rEmpty.kit->slots == empty.slots);
}

TEST_CASE("version 1 kits load as all synth", "[kitfile]")
{
    auto v = parsed(makeFactoryKit());
    v.getDynamicObject()->setProperty("version", 1);
    for (auto& slot : *v["slots"].getArray())
        slot.getDynamicObject()->removeProperty("source");
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    for (const auto& s : r.kit->slots)
        CHECK(s.source == SourceType::Synth);
}

TEST_CASE("empty slots ignore params and may omit the name", "[kitfile]")
{
    auto v = parsed(makeFactoryKit());
    auto* slot = v["slots"][4].getDynamicObject();
    slot->setProperty("source", "empty");
    slot->removeProperty("name");
    v["slots"][4]["params"].getDynamicObject()->setProperty("pitch", 1234.0);
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    SlotParams expected = makeFactoryKit().slots[4];
    expected.source = SourceType::Empty;
    CHECK(r.kit->slots[4] == expected);
    CHECK(r.kit->names[4].empty());
}

TEST_CASE("unknown or missing sound sources are rejected in version 2", "[kitfile]")
{
    auto sample = parsed(makeFactoryKit());
    sample["slots"][4].getDynamicObject()->setProperty("source", "sample");
    const auto r = reparse(sample);
    CHECK_FALSE(r.kit.has_value());
    CHECK(r.error == juce::String("Slot 5: unknown sound source."));

    auto missing = parsed(makeFactoryKit());
    missing["slots"][0].getDynamicObject()->removeProperty("source");
    CHECK_FALSE(reparse(missing).kit.has_value());
}
```

- [ ] **Step 2: Build und Test – schlagen fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build; build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[kitfile]"`
Expected: FAIL (Version ist 1, kein `source`).

- [ ] **Step 3: Implementierung** in `plugin/KitFile.cpp`

Anonymer Namespace: `kVersion` auf `2` setzen und ergänzen:

```cpp
constexpr int kVersion = 2;
constexpr const char* kSourceSynth = "synth";
constexpr const char* kSourceEmpty = "empty";
```

In `kitToJsonString` den Schleifenrumpf ersetzen:

```cpp
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto i = static_cast<std::size_t>(s);
        const bool synth = kit.slots[i].source == SourceType::Synth;
        auto* slot = new juce::DynamicObject();
        slot->setProperty("source", synth ? kSourceSynth : kSourceEmpty);
        slot->setProperty("name", juce::String::fromUTF8(kit.names[i].c_str()));
        if (synth)
        {
            auto* params = new juce::DynamicObject();
            for (int f = 0; f < kNumSlotFields; ++f)
            {
                const auto field = static_cast<SlotField>(f);
                params->setProperty(juce::Identifier(fieldSpec(field).key),
                                    static_cast<double>(getSlotField(kit.slots[i], field)));
            }
            slot->setProperty("params", juce::var(params));
        }
        slots.add(juce::var(slot));
    }
```

In `kitFromJsonString`:
- Nach der Prüfung `> kVersion` die Version merken:

```cpp
    const int version = static_cast<int>(root["version"]);
```

- Die bestehende Zeile `if (static_cast<int>(root["version"]) > kVersion)` auf `if (version > kVersion)` umstellen (die Deklaration also davor setzen).
- Vor der Slot-Schleife `const Kit factory = makeFactoryKit();` einfügen.
- In der Schleife direkt nach `if (!slot.isObject()) return fail(...)` einfügen:

```cpp
        SourceType source = SourceType::Synth; // Version 1 kennt nur Sirenen
        if (version >= 2)
        {
            const auto src = slot["source"].toString();
            if (src == kSourceEmpty)
                source = SourceType::Empty;
            else if (src != kSourceSynth)
                return fail(slotLabel(s) + ": unknown sound source.");
        }
        if (source == SourceType::Empty)
        {
            // Parameter eines leeren Slots werden ignoriert; hinterlegt werden die Factory-Werte.
            SlotParams p = factory.slots[static_cast<std::size_t>(s)];
            p.source = SourceType::Empty;
            kit.slots[static_cast<std::size_t>(s)] = p;
            kit.names[static_cast<std::size_t>(s)] =
                slot["name"].isString() ? slot["name"].toString().substring(0, 32).toStdString() : std::string();
            continue;
        }
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build; build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[kitfile]"`
Expected: alle `[kitfile]`-Tests PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/KitFile.cpp tests/plugin/test_KitFile.cpp
git commit -m "feat(plugin): store empty slots in kit format version 2 (#7)"
```

---

### Task 4: Parameter, Host-State und Processor-API

**Files:**
- Modify: `plugin/ParameterLayout.h`, `plugin/ParameterLayout.cpp`
- Modify: `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`
- Test: `tests/plugin/test_PluginProcessor.cpp`

**Interfaces:**
- Consumes: `SourceType`, `hasSound`, `makeEmptyKit` (Task 1), Engine-Verhalten (Task 2)
- Produces:
  - `juce::String dg::slotSourceParamId(int slot)`
  - `constexpr int dg::kSourceParameterVersion = 2`
  - `SourceType DubgefahrenProcessor::slotSource(int slot) const`
  - `void DubgefahrenProcessor::clearSlot(int slot)`
  - `void DubgefahrenProcessor::resetSlotToFactory(int slot)`

- [ ] **Step 1: Failing tests** in `tests/plugin/test_PluginProcessor.cpp`

Den Test „the plugin exposes 308 uniquely named parameters“ ersetzen:

```cpp
TEST_CASE("the plugin exposes 324 uniquely named parameters", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::set<juce::String> ids;
    for (auto* param : p.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            ids.insert(withId->paramID);
    CHECK(p.getParameters().size() == 16 * 19 + 20);
    CHECK(ids.size() == 324);
    CHECK(slotParamId(0, SlotField::Wave) == "s01_wave");
    CHECK(slotParamId(15, SlotField::FxSend) == "s16_send");
    CHECK(slotSourceParamId(15) == "s16_source");
    CHECK_FALSE(p.state().getParameter("s01_source")->isAutomatable());
}
```

Im Test „slot parameters and names default to the factory kit“ in der Schleife ergänzen:

```cpp
        CHECK(p.slotSource(s) == SourceType::Synth);
```

Am Ende anfügen:

```cpp
TEST_CASE("clearSlot empties a slot and resetSlotToFactory restores its siren", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    setParam(p, "s04_pitch", 1000.0f);
    p.clearSlot(3);
    CHECK(p.slotSource(3) == SourceType::Empty);
    CHECK(p.slotName(3).isEmpty());
    CHECK_THAT(readSlotFromParameters(p.state(), 3).pitchHz, WithinAbs(1000.0, 1.0)); // Synth-Werte bleiben
    CHECK(p.slotSource(2) == SourceType::Synth);

    p.resetSlotToFactory(3);
    CHECK(p.slotSource(3) == SourceType::Synth);
    CHECK(p.slotName(3) == "Laser");
    CHECK(approxEqual(readSlotFromParameters(p.state(), 3), makeFactoryKit().slots[3]));
}

TEST_CASE("setSlot copies the source, so pasting an empty slot empties the target", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.clearSlot(0);
    const auto copied = p.currentKit().slots[0];
    p.setSlot(5, copied, {});
    CHECK(p.slotSource(5) == SourceType::Empty);
}

TEST_CASE("empty slots survive a state round-trip", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    a.clearSlot(5);
    juce::MemoryBlock mb;
    a.getStateInformation(mb);

    DubgefahrenProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    CHECK(b.slotSource(5) == SourceType::Empty);
    CHECK(b.slotSource(0) == SourceType::Synth);
}

TEST_CASE("a state without source parameters loads as all synth even over empty slots", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    juce::MemoryBlock mb;
    a.getStateInformation(mb);
    auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), static_cast<int>(mb.getSize()));
    REQUIRE(xml != nullptr);
    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        if (child->getStringAttribute("id").endsWith("_source"))
            xml->removeChildElement(child, true);
        child = next;
    }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml, old);

    DubgefahrenProcessor b;
    b.applyKit(makeEmptyKit());
    REQUIRE(b.slotSource(0) == SourceType::Empty);
    b.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    for (int s = 0; s < kNumSlots; ++s)
        CHECK(b.slotSource(s) == SourceType::Synth);
}

TEST_CASE("applyKit with an empty kit keeps the plugin silent", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.applyKit(makeEmptyKit());
    for (const auto& s : p.currentKit().slots)
        CHECK(s.source == SourceType::Empty);

    prepare(p);
    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    p.processBlock(buf, on);
    CHECK(buf.getMagnitude(0, 0, 512) == 0.0f);
    CHECK(p.activeMask() == 0u);
}
```

- [ ] **Step 2: Build – schlägt fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler `slotSourceParamId` / `slotSource` / `clearSlot` unbekannt.

- [ ] **Step 3a: `plugin/ParameterLayout.h`**

Unter `constexpr int kParameterVersion = 1;`:

```cpp
// Versionshinweis für Parameter, die mit dem leeren Slot (#7) hinzugekommen sind.
constexpr int kSourceParameterVersion = 2;
```

Unter `juce::String slotParamId(int slot, SlotField f);`:

```cpp
juce::String slotSourceParamId(int slot);
```

In `class ParamCache`, `private`, nach `slots_`:

```cpp
    std::array<Ptr, kNumSlots> sources_ {};
```

- [ ] **Step 3b: `plugin/ParameterLayout.cpp`**

Nach `slotParamId(...)`:

```cpp
juce::String slotSourceParamId(int slot)
{
    return juce::String::formatted("s%02d_source", slot + 1);
}
```

In `createParameterLayout`, in der Slot-Schleife nach der inneren `for`-Schleife über die Felder:

```cpp
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { slotSourceParamId(s), kSourceParameterVersion }, prefix + "Source",
            juce::StringArray { "Empty", "Synth", "Sample" },
            static_cast<int>(defaults.slots[static_cast<std::size_t>(s)].source),
            juce::AudioParameterChoiceAttributes().withAutomatable(false)));
```

`readSlotFromParameters`, vor `return p;`:

```cpp
    p.source = static_cast<SourceType>(loadIndex(raw(apvts, slotSourceParamId(slot)), 2));
```

`writeSlotToParameters`, nach der Schleife:

```cpp
    auto* source = apvts.getParameter(slotSourceParamId(slot));
    jassert(source != nullptr);
    source->beginChangeGesture();
    source->setValueNotifyingHost(source->convertTo0to1(static_cast<float>(p.source)));
    source->endChangeGesture();
```

`ParamCache`-Konstruktor, in der äußeren Slot-Schleife. Sie hat bisher keine geschweiften Klammern, also umbauen:

```cpp
    for (int s = 0; s < kNumSlots; ++s)
    {
        for (int i = 0; i < kNumSlotFields; ++i)
            slots_[static_cast<std::size_t>(s)][static_cast<std::size_t>(i)] =
                raw(apvts, slotParamId(s, static_cast<SlotField>(i)));
        sources_[static_cast<std::size_t>(s)] = raw(apvts, slotSourceParamId(s));
    }
```

`ParamCache::read`, äußere Schleife analog umbauen:

```cpp
    for (std::size_t s = 0; s < slots_.size(); ++s)
    {
        for (std::size_t i = 0; i < slots_[s].size(); ++i)
            setSlotField(out.slots[s], static_cast<SlotField>(i), load(slots_[s][i]));
        out.slots[s].source = static_cast<SourceType>(loadIndex(sources_[s], 2));
    }
```

- [ ] **Step 3c: `plugin/PluginProcessor.h`**

Im Block „Message-Thread-API“ nach `void setSlot(...)`:

```cpp
    SourceType slotSource(int slot) const;
    // Leert den Slot: Quelle Empty, Name leer; die Synth-Parameter bleiben erhalten.
    void clearSlot(int slot);
    // Setzt den Slot auf die Factory-Sirene dieses Slots (Quelle Synth, Factory-Name).
    void resetSlotToFactory(int slot);
```

- [ ] **Step 3d: `plugin/PluginProcessor.cpp`**

Im anonymen Namespace nach `nameKey`:

```cpp
// Projekte vor #7 kennen die Quellen-Parameter nicht. replaceState() ließe fehlende Parameter
// auf ihrem aktuellen Wert stehen; deshalb werden sie hier explizit als „Synth“ ergänzt.
void addMissingSourceParameters(juce::ValueTree& state)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto id = slotSourceParamId(s);
        if (state.getChildWithProperty("id", id).isValid())
            continue;
        juce::ValueTree param("PARAM");
        param.setProperty("id", id, nullptr);
        param.setProperty("value", static_cast<int>(SourceType::Synth), nullptr);
        state.appendChild(param, nullptr);
    }
}
```

In `setStateInformation` die Zeile `apvts_.replaceState(juce::ValueTree::fromXml(*xml));` ersetzen:

```cpp
    auto tree = juce::ValueTree::fromXml(*xml);
    addMissingSourceParameters(tree);
    apvts_.replaceState(tree);
```

Nach `setSlot(...)`:

```cpp
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
```

`#include <cmath>` am Dateianfang ergänzen (für `std::lround`).

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build; build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[plugin]"`
Expected: alle `[plugin]`-Tests PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/ParameterLayout.h plugin/ParameterLayout.cpp plugin/PluginProcessor.h plugin/PluginProcessor.cpp tests/plugin/test_PluginProcessor.cpp
git commit -m "feat(plugin): add per-slot source parameter and processor API for empty slots (#7)"
```

---

### Task 5: Benutzeroberfläche

**Files:**
- Modify: `plugin/ui/PadGrid.h`, `plugin/ui/PadGrid.cpp`
- Modify: `plugin/ui/SlotEditor.h`, `plugin/ui/SlotEditor.cpp`
- Modify: `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`
- Test: `tests/plugin/test_Editor.cpp`

**Interfaces:**
- Consumes: `slotSource`, `clearSlot`, `resetSlotToFactory` (Task 4), `makeEmptyKit`, `hasSound` (Task 1)
- Produces:
  - `PadGrid::onEmptyClick`
  - `juce::Component& PadGrid::pad(int)`
  - `bool PadGrid::isEmpty(int) const`
  - `SlotEditor::refresh()` (ersetzt `refreshName()`)
  - `bool SlotEditor::showsEmptyHint() const`
  - `DubgefahrenEditor::buildKitMenu`, `buildPadMenu`, `buildSourceMenu`, `slotEditorShowsEmptyHint`, `padShowsEmpty`

- [ ] **Step 1: Failing tests** – am Ende von `tests/plugin/test_Editor.cpp`:

```cpp
namespace {
std::vector<std::pair<juce::String, bool>> menuItems(const juce::PopupMenu& m)
{
    std::vector<std::pair<juce::String, bool>> out;
    for (juce::PopupMenu::MenuItemIterator it(m); it.next();)
    {
        const auto& item = it.getItem();
        if (!item.isSeparator)
            out.emplace_back(item.text, item.isEnabled);
    }
    return out;
}
using Items = std::vector<std::pair<juce::String, bool>>;
} // namespace

TEST_CASE("empty slots show a hint in the slot editor and 'Empty' on the pad", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.clearSlot(2);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    CHECK(e->padShowsEmpty(2));
    CHECK_FALSE(e->padShowsEmpty(0));
    e->selectSlot(2);
    CHECK(e->slotEditorShowsEmptyHint());
    e->selectSlot(0);
    CHECK_FALSE(e->slotEditorShowsEmptyHint());

    p.clearSlot(0); // von außen geändert: Timer-Poll frischt auf
    e->pollProcessorState();
    CHECK(e->padShowsEmpty(0));
    CHECK(e->slotEditorShowsEmptyHint());
}

TEST_CASE("source menu offers synth and a disabled sample entry", "[editor]")
{
    CHECK(menuItems(DubgefahrenEditor::buildSourceMenu()) == Items { { "Synth", true }, { "Sample", false } });
}

TEST_CASE("pad menu disables rename and clear on empty slots", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.clearSlot(1);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    CHECK(menuItems(e->buildPadMenu(0)) == Items { { "Copy", true }, { "Paste", false }, { "Reset to Factory Default", true },
                                                   { "Rename", true }, { "Clear Slot", true } });
    CHECK(menuItems(e->buildPadMenu(1)) == Items { { "Copy", true }, { "Paste", false }, { "Reset to Factory Default", true },
                                                   { "Rename", false }, { "Clear Slot", false } });
}

TEST_CASE("kit menu offers a new empty kit", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    const auto items = menuItems(e->buildKitMenu({}));
    REQUIRE(items.size() >= 2);
    CHECK(items[0] == std::make_pair(juce::String("Load Factory Kit"), true));
    CHECK(items[1] == std::make_pair(juce::String("New Empty Kit"), true));
}
```

Am Dateianfang ergänzen: `#include <utility>` und `#include <vector>`.

- [ ] **Step 2: Build – schlägt fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler (`padShowsEmpty`, `buildSourceMenu` … unbekannt).

- [ ] **Step 3a: `plugin/ui/PadGrid.h`**

Nach `std::function<void(int)> onContextMenu;`:

```cpp
    // Linksklick auf ein leeres Pad (statt Vorschau): Auswahl der Klangquelle öffnen.
    std::function<void(int)> onEmptyClick;

    juce::Component& pad(int slot);
    bool isEmpty(int slot) const;
```

- [ ] **Step 3b: `plugin/ui/PadGrid.cpp`**

Include ergänzen: `#include "engine/SlotParams.h"` ist über den Header schon da, nichts nötig.

In `class PadGrid::Pad`: `setName` ersetzen durch

```cpp
    void setContent(const juce::String& n, bool empty)
    {
        name_ = n;
        empty_ = empty;
        repaint();
    }

    bool isEmpty() const { return empty_; }
```

In `paint` die Rahmen- und Namenszeichnung ersetzen:

```cpp
        g.setColour(focus_ ? colours::accent : (empty_ ? colours::outline.withAlpha(0.5f) : colours::outline));
        g.drawRoundedRectangle(r, 6.0f, selected_ ? 3.0f : 1.5f);

        g.setColour(colours::textDim);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(juce::String(slot_ + 1), r.reduced(6.0f), juce::Justification::topLeft);
        g.setColour(empty_ ? colours::textDim : colours::text);
        g.setFont(juce::FontOptions(13.0f));
        g.drawFittedText(empty_ ? juce::String("Empty") : name_, r.reduced(6.0f).toNearestInt(),
                         juce::Justification::centred, 2);
```

In `mouseDown` vor `held_ = true;`:

```cpp
        if (empty_)
        {
            // Leerer Slot: keine Vorschau, sondern Slot wählen und Quellen-Auswahl öffnen.
            if (owner_.onSelect)
                owner_.onSelect(slot_);
            if (owner_.onEmptyClick)
                owner_.onEmptyClick(slot_);
            return;
        }
```

Member ergänzen: `bool empty_ = false;` (zu den anderen bools).

`PadGrid::refreshNames` ersetzen:

```cpp
void PadGrid::refreshNames()
{
    for (int s = 0; s < kNumSlots; ++s)
        pads_[static_cast<std::size_t>(s)]->setContent(proc_.slotName(s), !hasSound(proc_.slotSource(s)));
}

juce::Component& PadGrid::pad(int slot) { return *pads_[static_cast<std::size_t>(slot)]; }

bool PadGrid::isEmpty(int slot) const { return pads_[static_cast<std::size_t>(slot)]->isEmpty(); }
```

- [ ] **Step 3c: `plugin/ui/SlotEditor.h`**

- `void refreshName();` umbenennen in `void refresh();`.
- Ergänzen: `bool showsEmptyHint() const { return emptyHint_.isVisible(); }`
- Private Member ergänzen:

```cpp
    juce::Label emptyHint_;
    std::vector<juce::Component*> controls_;
```

- `#include <vector>` ergänzen.

- [ ] **Step 3d: `plugin/ui/SlotEditor.cpp`**

Konstruktor: die `for`-Schleife über die Initializer-List ersetzen:

```cpp
    controls_ = { &wave_, &pitch_, &pw_, &lfoShape_, &lfoRate_, &lfoSync_, &lfoSyncDiv_, &lfoDepth_, &sweepAmt_,
                  &sweepTime_, &attack_, &release_, &trigMode_, &oneShot_, &choke_, &vol_, &pan_, &send_ };
    for (auto* c : controls_)
        addAndMakeVisible(*c);

    emptyHint_.setText(u8("Empty slot – click the pad to choose a sound source"), juce::dontSendNotification);
    emptyHint_.setJustificationType(juce::Justification::centred);
    emptyHint_.setColour(juce::Label::textColourId, colours::textDim);
    emptyHint_.setFont(juce::FontOptions(15.0f));
    addChildComponent(emptyHint_);
```

In `setSlot` den Aufruf `refreshName();` durch `refresh();` ersetzen.

`refreshName()` ersetzen durch:

```cpp
void SlotEditor::refresh()
{
    const bool empty = !hasSound(proc_.slotSource(slot_));
    const auto title = "Slot " + juce::String(slot_ + 1);
    header_.setText(empty ? title : title + u8(" · ") + proc_.slotName(slot_), juce::dontSendNotification);
    emptyHint_.setVisible(empty);
    renameButton_.setVisible(!empty);
    for (auto* c : controls_)
        c->setVisible(!empty);
    repaint(); // Zeilenbeschriftungen
}
```

In `paint` nach `fillRoundedRectangle`:

```cpp
    if (emptyHint_.isVisible())
        return;
```

In `resized` am Ende:

```cpp
    emptyHint_.setBounds(getLocalBounds().withTrimmedTop(kTopOffset).reduced(24));
```

- [ ] **Step 3e: `plugin/PluginEditor.h`**

Im `public`-Teil nach `juce::String cpuMeterText() const …`:

```cpp
    bool slotEditorShowsEmptyHint() const { return slotEditor_.showsEmptyHint(); }
    bool padShowsEmpty(int slot) const { return pads_.isEmpty(slot); }

    // Für Tests öffentlich: bauen die Popup-Menüs von Kit-Button, Pad-Rechtsklick und leerem Pad.
    juce::PopupMenu buildKitMenu(const juce::Array<juce::File>& kitFiles) const;
    juce::PopupMenu buildPadMenu(int slot) const;
    static juce::PopupMenu buildSourceMenu();
```

Im `private`-Teil nach `void showPadMenu(int slot);`:

```cpp
    void showSourceMenu(int slot);
```

- [ ] **Step 3f: `plugin/PluginEditor.cpp`**

Nach `namespace dg {` einfügen:

```cpp
namespace {
enum KitMenuId { kKitFactory = 1, kKitNone = 2, kKitEmpty = 3, kKitFileBase = 100 };
enum PadMenuId { kPadCopy = 1, kPadPaste, kPadReset, kPadRename, kPadClear };
enum SourceMenuId { kSourceSynth = 1, kSourceSample };
} // namespace
```

Im Konstruktor nach `pads_.onContextMenu = …`:

```cpp
    pads_.onEmptyClick = [this](int s) { showSourceMenu(s); };
```

`refreshAll`: `slotEditor_.refreshName();` durch `slotEditor_.refresh();` ersetzen.

`showKitMenu` ersetzen:

```cpp
juce::PopupMenu DubgefahrenEditor::buildKitMenu(const juce::Array<juce::File>& kitFiles) const
{
    juce::PopupMenu menu;
    menu.addItem(kKitFactory, "Load Factory Kit");
    menu.addItem(kKitEmpty, "New Empty Kit");
    menu.addSeparator();
    if (kitFiles.isEmpty())
        menu.addItem(kKitNone, "(no kits in folder)", false);
    for (int i = 0; i < kitFiles.size(); ++i)
        menu.addItem(kKitFileBase + i, kitFiles[i].getFileNameWithoutExtension());
    return menu;
}

void DubgefahrenEditor::showKitMenu()
{
    maybeShowConfigWarning();
    auto files = proc_.kitFolder().folder.findChildFiles(juce::File::findFiles, false, juce::String("*") + kKitExtension);
    files.sort();
    buildKitMenu(files).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(kitButton_),
                                      [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), files](int result) {
                                          if (safe == nullptr || result == 0)
                                              return;
                                          if (result == kKitFactory)
                                              safe->proc_.applyKit(makeFactoryKit());
                                          else if (result == kKitEmpty)
                                              safe->proc_.applyKit(makeEmptyKit());
                                          else if (result >= kKitFileBase)
                                              safe->loadKit(files[result - kKitFileBase]);
                                          safe->refreshAll();
                                      });
}
```

`showPadMenu` ersetzen:

```cpp
juce::PopupMenu DubgefahrenEditor::buildPadMenu(int slot) const
{
    const bool empty = !hasSound(proc_.slotSource(slot));
    juce::PopupMenu menu;
    menu.addItem(kPadCopy, "Copy");
    menu.addItem(kPadPaste, "Paste", clipboard_.has_value());
    menu.addItem(kPadReset, "Reset to Factory Default");
    menu.addItem(kPadRename, "Rename", !empty);
    menu.addItem(kPadClear, "Clear Slot", !empty);
    return menu;
}

void DubgefahrenEditor::showPadMenu(int slot)
{
    buildPadMenu(slot).showMenuAsync(juce::PopupMenu::Options(),
                                     [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot](int result) {
                                         if (safe == nullptr)
                                             return;
                                         auto& self = *safe;
                                         switch (result)
                                         {
                                             case kPadCopy:
                                                 self.clipboard_ = std::make_pair(self.proc_.currentKit().slots[static_cast<std::size_t>(slot)],
                                                                                  self.proc_.slotName(slot));
                                                 break;
                                             case kPadPaste:
                                                 if (self.clipboard_)
                                                     self.proc_.setSlot(slot, self.clipboard_->first, self.clipboard_->second);
                                                 break;
                                             case kPadReset:  self.proc_.resetSlotToFactory(slot); break;
                                             case kPadRename: self.renameSlot(slot); break;
                                             case kPadClear:  self.proc_.clearSlot(slot); break;
                                             default: break;
                                         }
                                         self.refreshAll();
                                     });
}

juce::PopupMenu DubgefahrenEditor::buildSourceMenu()
{
    juce::PopupMenu menu;
    menu.addItem(kSourceSynth, "Synth");
    menu.addItem(kSourceSample, "Sample", false); // folgt mit dem Sample-Player (#8)
    return menu;
}

void DubgefahrenEditor::showSourceMenu(int slot)
{
    // Gleiche Optionen wie das Kit-Menü, verankert am angeklickten Pad.
    buildSourceMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(pads_.pad(slot)),
                                    [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot](int result) {
                                        if (safe == nullptr || result != kSourceSynth)
                                            return;
                                        safe->proc_.resetSlotToFactory(slot);
                                        safe->refreshAll();
                                    });
}
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build; build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[editor]"`
Expected: alle `[editor]`-Tests PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/ui/PadGrid.h plugin/ui/PadGrid.cpp plugin/ui/SlotEditor.h plugin/ui/SlotEditor.cpp plugin/PluginEditor.h plugin/PluginEditor.cpp tests/plugin/test_Editor.cpp
git commit -m "feat(ui): show empty slots and let users choose a sound source (#7)"
```

---

### Task 6: README, Gesamttest, Validator

**Files:**
- Modify: `README.md` (Abschnitt „Kits“)

- [ ] **Step 1: README ergänzen** – Abschnitt „Kits“ ersetzen durch:

```markdown
## Kits

Kit menu → factory kit, a new empty kit, or kits from the kit folder. Import/export as `.dgkit`
(JSON, 16 slots, without effects).
Right-click a pad: copy, paste, reset to factory settings, rename, clear.
An empty pad is silent; click it to choose a sound source (Synth; Sample follows in a later version).
```

- [ ] **Step 2: Alle Tests**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: ctest meldet 100 % bestanden.

- [ ] **Step 3: VST3-Validator**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
Expected: Validator ohne Fehler (324 Parameter, `sNN_source` nicht automatisierbar).

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: describe empty slots and the new kit menu entry (#7)"
```

- [ ] **Step 5: Manuelle Abnahme (durch den Nutzer in Ableton)**
  - Ein altes Set mit Dubgefahren lädt unverändert, alle Pads klingen.
  - „New Empty Kit“ setzt alle Pads auf „Empty“, die Pads bleiben stumm.
  - Ein Klick auf ein leeres Pad öffnet das Popup direkt am Pad, im Look des Kit-Menüs. „Sample“ ist ausgegraut, „Synth“ lädt die Factory-Sirene.
  - Rechtsklick → „Clear Slot“ auf einem gelatchten Pad stoppt es sofort.
  - Set speichern und neu laden: leere Pads bleiben leer.
  - Export und Import eines Kits mit leeren Slots.
