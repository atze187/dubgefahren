# Pad-Belegung: einstellbare Pad-1-Note und Raster-Ursprung – Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Die Note des ersten Pads und der Ursprung des Pad-Rasters (unten links oder oben links) sind einstellbar, damit das Plugin zum Grid BU16 passt (Noten ab 32, zeilenweise von oben links).

**Architecture:**
- **engine (JUCE-frei):** `translateNote(note, firstNote)` rechnet eine MIDI-Note in die Engine-Note (36 bis 51) um oder liefert −1. Die Engine selbst bleibt unverändert.
- **plugin:** `PadMapping` ist ein prozessweites Objekt mit zwei atomaren Werten, geladen aus und gespeichert in `%APPDATA%\Dubgefahren\settings.json`; der Processor übersetzt die Noten vor der Engine.
- **UI:** `PadGrid` kennt den Ursprung; der Editor bekommt einen Button „MIDI“ mit Menü und einem Dialog für die Pad-1-Note.

**Tech Stack:** C++20, JUCE 8.0.15, Catch2 v3, MSVC x64, CMake.

**Spec:** `docs/superpowers/specs/2026-10-01-pad-mapping-design.md`

## Global Constraints

- `engine/` darf **keine** JUCE-Header einbinden.
- Im Audio-Thread: keine Allokation, keine Locks, kein Datei-I/O. `PadMapping::instance()` wird im Konstruktor des Processors angelegt, nicht in `processBlock`.
- Einstellungen und Standardwerte: Note für Pad 1 = **36**, gültig 0 bis **112** (`kMaxPadFirstNote = 127 - (kNumSlots - 1)`); Ursprung = **unten links**.
- Datei: `%APPDATA%\Dubgefahren\settings.json` (`juce::File::userApplicationDataDirectory`), Inhalt `{ "firstNote": 32, "padOrigin": "topLeft" }`; erlaubte Werte für `padOrigin`: `"bottomLeft"`, `"topLeft"`. Fehlt die Datei oder ein Wert, ist er ungültig oder hat er den falschen Typ (Text, Bool, null, Dezimalzahl, außerhalb des Bereichs), gilt für **diesen Wert** der Standard.
- Test-Builds definieren `DG_NO_USER_SETTINGS`: Dort ist `padMappingFile()` leer, das Objekt liest und schreibt keine Datei, und kein Test berührt die Einstellungen des Entwicklers.
- Mit den Standardwerten verhält sich das Plugin exakt wie bisher; alle bestehenden Tests bleiben ohne Anpassung grün, ausgenommen die Parameterzahl (die ändert sich hier nicht).
- Das Projekt (Host-Zustand) und die Kits enthalten die Einstellung nicht.
- UI im Look von #15: Schrift nur über `ui::font(...)`; Menüs über `styled(...)`, Dialoge über `createDialog(...)`. Quelltexte UTF-8; Nicht-ASCII-Strings an JUCE über `juce::String::fromUTF8(...)`. Kommentare Deutsch, UI-Texte Englisch.
- Befehle aus dem Repo-Root:
  - Build: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
  - Neu konfigurieren (nach CMake-Änderungen): `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
  - Engine-Tests: `build\tests\Release\DubgefahrenTests.exe "<tag>"`
  - Plugin-Tests: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "<tag>"`
  - Alles: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
  - Validator: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
- Jeder Task endet mit einem grünen Build und grünen Tests; Commits enden mit `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

- **Noten an den Rändern des Blocks und Basen an den Enden des Bereichs** (31/32/47/48 bei Basis 32; Basis 0 und Basis 112 mit Note 127/128). Erwartet: nur die 16 Noten des Blocks lösen Pads aus, nichts davor oder dahinter, keine Überläufe. → Test in Task 1 („translateNote maps the pad block …“).
- **Eine beschädigte, teilweise gefüllte oder falsch getippte Einstellungsdatei** (kein JSON, Array statt Objekt, `"firstNote": "32"`, `36.5`, `-1`, `113`, `null`, `true`, unbekannter Ursprung). Erwartet: kein Fehler, pro Wert der Standard, übrige Werte bleiben. → Tests in Task 2.
- **Audio-Thread liest die Einstellung ohne Sperre, und eine Änderung wirkt im nächsten Block.** Erwartet: Eine Änderung der Basis zwischen zwei Blöcken gilt sofort für die nächste Note; Note-Aus nach einer Änderung klemmt nichts dauerhaft (Panic beendet). → Test in Task 3 („changing the first pad note takes effect for the next block“).
- **Zwei Instanzen teilen die Einstellung.** Erwartet: Ändert man sie über das Menü des einen Editors, übernimmt der zweite Editor sie beim nächsten Abfragen, ohne etwas neu zu laden. → Test in Task 5 („a second editor follows a change made elsewhere“).
- **Unsinnige Eingaben im Dialog** (leer, `abc`, `-3`, `113`, `1e2`, `0x20`, `36.5`, `1234`, Leerzeichen um die Zahl). Erwartet: „OK“ ist nicht wählbar, und ein erzwungener Aufruf ändert nichts. → Tests in Task 2 („the first pad note field accepts only whole numbers from 0 to 112“) und Task 5 („the first note dialog accepts only valid numbers“).

---

### Task 1: Notenübersetzung in der Engine

**Files:**
- Modify: `engine/PadRouter.h`, `engine/PadRouter.cpp`
- Test: `tests/test_PadRouter.cpp`

**Interfaces:**
- Consumes: `kFirstNote`, `kNumSlots` aus `engine/SlotParams.h`.
- Produces: `constexpr int kMaxPadFirstNote` (112) und `int translateNote(int note, int firstNote)` – liefert die Engine-Note `kFirstNote + (note - firstNote)` oder −1, wenn `note - firstNote` nicht in 0 bis 15 liegt.

- [ ] **Step 1: Failing test schreiben**

In `tests/test_PadRouter.cpp` nach dem Test, der `slotForNote(36) == 0` prüft, einfügen (den dort verwendeten Tag übernehmen, falls er nicht `[padrouter]` heißt):

```cpp
TEST_CASE("translateNote maps the pad block starting at the first note onto the engine notes", "[padrouter]")
{
    for (int n = 36; n <= 51; ++n)
        CHECK(translateNote(n, 36) == n); // Standard: Identität
    CHECK(translateNote(35, 36) == -1);
    CHECK(translateNote(52, 36) == -1);

    CHECK(translateNote(32, 32) == 36); // BU16: Note 32 ist Pad 1
    CHECK(translateNote(36, 32) == 40); // und Note 36 ist Pad 5
    CHECK(translateNote(47, 32) == 51);
    CHECK(translateNote(31, 32) == -1);
    CHECK(translateNote(48, 32) == -1);

    CHECK(translateNote(0, 0) == 36);
    CHECK(translateNote(15, 0) == 51);
    CHECK(translateNote(16, 0) == -1);

    static_assert(kMaxPadFirstNote == 112);
    CHECK(translateNote(112, kMaxPadFirstNote) == 36);
    CHECK(translateNote(127, kMaxPadFirstNote) == 51);
    CHECK(translateNote(128, kMaxPadFirstNote) == -1);
    CHECK(translateNote(111, kMaxPadFirstNote) == -1);
}
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Build-FEHLER (`translateNote`, `kMaxPadFirstNote` unbekannt).

- [ ] **Step 3: Implementieren**

`engine/PadRouter.h` hinter `int slotForNote(int note);` einfügen:

```cpp
// Höchste Note für das erste Pad, bei der alle 16 Pads noch im MIDI-Bereich 0..127 liegen.
constexpr int kMaxPadFirstNote = 127 - (kNumSlots - 1);

// Rechnet eine MIDI-Note anhand der Note des ersten Pads in die Engine-Note (kFirstNote + Slot) um.
// Liefert -1, wenn die Note keinem der 16 Pads entspricht.
int translateNote(int note, int firstNote);
```

`engine/PadRouter.cpp` hinter `slotForNote` einfügen:

```cpp
int translateNote(int note, int firstNote)
{
    const int slot = note - firstNote;
    return (slot >= 0 && slot < kNumSlots) ? kFirstNote + slot : -1;
}
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\Release\DubgefahrenTests.exe "[padrouter]"`
Expected: PASS.

- [ ] **Step 5: Alles testen und committen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

```bash
git add engine tests
git commit -m "feat(engine): translate MIDI notes relative to a configurable first pad note

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Einstellungen und `PadMapping`

**Files:**
- Create: `plugin/PadMapping.h`, `plugin/PadMapping.cpp`, `tests/plugin/test_PadMapping.cpp`, `tests/plugin/PadMappingTestHelpers.h`
- Modify: `plugin/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `kMaxPadFirstNote` (Task 1).
- Produces:
  - `enum class PadOrigin { BottomLeft, TopLeft }`, `constexpr int kDefaultPadFirstNote = 36`.
  - `struct PadMappingSettings { int firstNote = 36; PadOrigin origin = PadOrigin::BottomLeft; bool operator==(...) const = default; }`.
  - `PadMappingSettings parsePadMappingSettings(const juce::String& json)`, `juce::String padMappingSettingsToJson(const PadMappingSettings&)`, `std::optional<int> parsePadFirstNote(const juce::String& text)`.
  - `PadMappingSettings loadPadMappingSettings(const juce::File&)` (fehlende Datei → Standard), `bool savePadMappingSettings(const juce::File&, const PadMappingSettings&)`, `juce::File padMappingFile()` (in Test-Builds leer).
  - `class PadMapping { static PadMapping& instance(); int firstNote() const; PadOrigin origin() const; PadMappingSettings settings() const; void setFirstNote(int); void setOrigin(PadOrigin); }` – die Setter klemmen bzw. übernehmen den Wert, setzen ihn atomar und speichern, wenn `padMappingFile()` nicht leer ist.
  - Test-Helfer `dgtest::ScopedPadMapping` (stellt die globale Belegung beim Verlassen des Tests wieder her).

- [ ] **Step 1: Failing tests schreiben**

`tests/plugin/PadMappingTestHelpers.h` anlegen:

```cpp
#pragma once
#include "plugin/PadMapping.h"

namespace dgtest {

// Stellt die globale Pad-Belegung beim Verlassen des Tests auf den Zustand davor.
class ScopedPadMapping
{
public:
    ScopedPadMapping() : saved_(dg::PadMapping::instance().settings()) {}
    ~ScopedPadMapping()
    {
        auto& m = dg::PadMapping::instance();
        m.setFirstNote(saved_.firstNote);
        m.setOrigin(saved_.origin);
    }
    ScopedPadMapping(const ScopedPadMapping&) = delete;
    ScopedPadMapping& operator=(const ScopedPadMapping&) = delete;

private:
    dg::PadMappingSettings saved_;
};

} // namespace dgtest
```

`tests/plugin/test_PadMapping.cpp` anlegen:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "plugin/PadMapping.h"
#include "PadMappingTestHelpers.h"

using namespace dg;

TEST_CASE("pad mapping settings parse a valid file", "[padmapping]")
{
    const auto s = parsePadMappingSettings(R"({ "firstNote": 32, "padOrigin": "topLeft" })");
    CHECK(s.firstNote == 32);
    CHECK(s.origin == PadOrigin::TopLeft);
}

TEST_CASE("missing, partial or broken settings fall back to the defaults value by value", "[padmapping]")
{
    const PadMappingSettings defaults;
    CHECK(defaults.firstNote == 36);
    CHECK(defaults.origin == PadOrigin::BottomLeft);

    CHECK(parsePadMappingSettings("") == defaults);
    CHECK(parsePadMappingSettings("not json") == defaults);
    CHECK(parsePadMappingSettings("[1, 2]") == defaults);
    CHECK(parsePadMappingSettings("{}") == defaults);
    // Ein gültiger Wert bleibt erhalten, wenn der andere fehlt.
    CHECK(parsePadMappingSettings(R"({ "firstNote": 32 })").firstNote == 32);
    CHECK(parsePadMappingSettings(R"({ "firstNote": 32 })").origin == PadOrigin::BottomLeft);
    CHECK(parsePadMappingSettings(R"({ "padOrigin": "topLeft" })").firstNote == 36);
    CHECK(parsePadMappingSettings(R"({ "padOrigin": "topLeft" })").origin == PadOrigin::TopLeft);
}

TEST_CASE("settings values outside the range or of the wrong type are ignored", "[padmapping]")
{
    for (const char* json : { R"({ "firstNote": 113 })", R"({ "firstNote": -1 })", R"({ "firstNote": 36.5 })",
                              R"({ "firstNote": "32" })", R"({ "firstNote": null })", R"({ "firstNote": true })" })
    {
        INFO(json);
        CHECK(parsePadMappingSettings(json).firstNote == 36);
    }
    CHECK(parsePadMappingSettings(R"({ "firstNote": 112 })").firstNote == 112);
    CHECK(parsePadMappingSettings(R"({ "firstNote": 0 })").firstNote == 0);
    CHECK(parsePadMappingSettings(R"({ "padOrigin": "middle" })").origin == PadOrigin::BottomLeft);
    CHECK(parsePadMappingSettings(R"({ "padOrigin": 1 })").origin == PadOrigin::BottomLeft);
}

TEST_CASE("settings round-trip through a file and create the folder", "[padmapping]")
{
    const auto dir = juce::File::createTempFile("dgpad");
    const auto file = dir.getChildFile("nested").getChildFile("settings.json");
    REQUIRE_FALSE(file.existsAsFile());
    CHECK(loadPadMappingSettings(file) == PadMappingSettings {}); // fehlende Datei: Standard

    const PadMappingSettings written { 32, PadOrigin::TopLeft };
    REQUIRE(savePadMappingSettings(file, written));
    CHECK(loadPadMappingSettings(file) == written);
    const auto text = file.loadFileAsString();
    CHECK(text.contains("firstNote"));
    CHECK(text.contains("topLeft"));
    CHECK(padMappingSettingsToJson(written).contains("32"));
    dir.deleteRecursively();
}

TEST_CASE("the first pad note field accepts only whole numbers from 0 to 112", "[padmapping]")
{
    CHECK(parsePadFirstNote("0") == 0);
    CHECK(parsePadFirstNote("36") == 36);
    CHECK(parsePadFirstNote("  40 ") == 40);
    CHECK(parsePadFirstNote("112") == 112);
    CHECK(parsePadFirstNote("007") == 7);
    for (const char* bad : { "", " ", "abc", "-3", "113", "1e2", "0x20", "36.5", "1234", "3 6" })
    {
        INFO(bad);
        CHECK_FALSE(parsePadFirstNote(bad).has_value());
    }
}

TEST_CASE("the shared pad mapping clamps the note and keeps the origin", "[padmapping]")
{
    dgtest::ScopedPadMapping restore;
    auto& m = PadMapping::instance();
    CHECK(padMappingFile() == juce::File()); // Test-Builds berühren keine Einstellungsdatei
    CHECK(m.firstNote() == 36);
    CHECK(m.origin() == PadOrigin::BottomLeft);

    m.setFirstNote(32);
    CHECK(m.firstNote() == 32);
    m.setFirstNote(200);
    CHECK(m.firstNote() == 112);
    m.setFirstNote(-5);
    CHECK(m.firstNote() == 0);

    m.setOrigin(PadOrigin::TopLeft);
    CHECK(m.origin() == PadOrigin::TopLeft);
    CHECK((m.settings() == PadMappingSettings { 0, PadOrigin::TopLeft }));
    CHECK(&PadMapping::instance() == &m); // ein Objekt je Prozess
}
```

`tests/CMakeLists.txt`: in der Quellenliste von `DubgefahrenPluginTests` `plugin/test_PadMapping.cpp` ergänzen und hinter dem `target_link_libraries(DubgefahrenPluginTests …)` einfügen:

```cmake
# Tests lesen und schreiben nie die Einstellungsdatei des Entwicklers.
target_compile_definitions(DubgefahrenPluginTests PRIVATE DG_NO_USER_SETTINGS=1)
```

`plugin/CMakeLists.txt`: in der Liste `target_sources(dg_plugin_shared INTERFACE …)` nach `Config.cpp` ergänzen:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/PadMapping.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PadMapping.cpp
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure` und danach `build`
Expected: Build-FEHLER (`plugin/PadMapping.h` fehlt).

- [ ] **Step 3: Implementieren**

`plugin/PadMapping.h` anlegen:

```cpp
#pragma once
#include <atomic>
#include <optional>
#include <juce_core/juce_core.h>
#include "engine/PadRouter.h"

namespace dg {

enum class PadOrigin { BottomLeft, TopLeft };

constexpr int kDefaultPadFirstNote = 36;

struct PadMappingSettings
{
    int firstNote = kDefaultPadFirstNote; // 0 .. kMaxPadFirstNote
    PadOrigin origin = PadOrigin::BottomLeft;
    bool operator==(const PadMappingSettings&) const = default;
};

// Liest die Einstellungen aus JSON-Text. Fehlt ein Wert oder ist er ungültig, gilt für ihn der Standard.
PadMappingSettings parsePadMappingSettings(const juce::String& json);
juce::String padMappingSettingsToJson(const PadMappingSettings& settings);
// Zahlenfeld des Dialogs: eine ganze Zahl von 0 bis kMaxPadFirstNote, sonst nichts.
std::optional<int> parsePadFirstNote(const juce::String& text);

// Eine fehlende Datei ergibt die Standardwerte.
PadMappingSettings loadPadMappingSettings(const juce::File& file);
// Legt den Ordner an; liefert false, wenn die Datei nicht geschrieben werden konnte.
bool savePadMappingSettings(const juce::File& file, const PadMappingSettings& settings);

// %APPDATA%\Dubgefahren\settings.json; in Test-Builds (DG_NO_USER_SETTINGS) leer.
juce::File padMappingFile();

// Prozessweit geteilte Belegung: Alle Instanzen im Host lesen dieselben Werte, der Audio-Thread
// ohne Sperre. Setter nur im Message-Thread aufrufen.
class PadMapping
{
public:
    static PadMapping& instance();

    int firstNote() const { return firstNote_.load(std::memory_order_relaxed); }
    PadOrigin origin() const { return static_cast<PadOrigin>(origin_.load(std::memory_order_relaxed)); }
    PadMappingSettings settings() const { return { firstNote(), origin() }; }

    void setFirstNote(int note); // wird auf 0 .. kMaxPadFirstNote begrenzt
    void setOrigin(PadOrigin origin);

private:
    PadMapping();
    void persist() const;

    juce::File file_;
    std::atomic<int> firstNote_ { kDefaultPadFirstNote };
    std::atomic<int> origin_ { static_cast<int>(PadOrigin::BottomLeft) };
};

} // namespace dg
```

`plugin/PadMapping.cpp` anlegen:

```cpp
#include "plugin/PadMapping.h"
#include <algorithm>
#include <cmath>

namespace dg {

PadMappingSettings parsePadMappingSettings(const juce::String& json)
{
    PadMappingSettings s;
    juce::var root;
    if (juce::JSON::parse(json, root).failed() || !root.isObject())
        return s;

    const auto note = root["firstNote"];
    if (note.isInt() || note.isInt64() || note.isDouble()) // Zahlen, keine Texte oder Wahrheitswerte
    {
        const double v = static_cast<double>(note);
        if (v >= 0.0 && v <= kMaxPadFirstNote && v == std::floor(v))
            s.firstNote = static_cast<int>(v);
    }
    if (root["padOrigin"].toString() == "topLeft")
        s.origin = PadOrigin::TopLeft;
    return s;
}

juce::String padMappingSettingsToJson(const PadMappingSettings& s)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("firstNote", s.firstNote);
    object->setProperty("padOrigin", s.origin == PadOrigin::TopLeft ? "topLeft" : "bottomLeft");
    return juce::JSON::toString(juce::var(object));
}

std::optional<int> parsePadFirstNote(const juce::String& text)
{
    const auto t = text.trim();
    if (t.isEmpty() || t.length() > 3 || !t.containsOnly("0123456789"))
        return std::nullopt;
    const int value = t.getIntValue();
    if (value > kMaxPadFirstNote)
        return std::nullopt;
    return value;
}

PadMappingSettings loadPadMappingSettings(const juce::File& file)
{
    if (!file.existsAsFile())
        return {};
    return parsePadMappingSettings(file.loadFileAsString());
}

bool savePadMappingSettings(const juce::File& file, const PadMappingSettings& settings)
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText(padMappingSettingsToJson(settings));
}

juce::File padMappingFile()
{
#ifdef DG_NO_USER_SETTINGS
    return {};
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Dubgefahren")
        .getChildFile("settings.json");
#endif
}

PadMapping& PadMapping::instance()
{
    static PadMapping mapping; // wird im Konstruktor des Processors angelegt, nie im Audio-Thread
    return mapping;
}

PadMapping::PadMapping() : file_(padMappingFile())
{
    if (file_ != juce::File())
    {
        const auto s = loadPadMappingSettings(file_);
        firstNote_.store(s.firstNote);
        origin_.store(static_cast<int>(s.origin));
    }
}

void PadMapping::setFirstNote(int note)
{
    firstNote_.store(std::clamp(note, 0, kMaxPadFirstNote));
    persist();
}

void PadMapping::setOrigin(PadOrigin origin)
{
    origin_.store(static_cast<int>(origin));
    persist();
}

void PadMapping::persist() const
{
    if (file_ == juce::File())
        return;
    if (!savePadMappingSettings(file_, settings()))
        DBG("Could not write " << file_.getFullPathName());
}

} // namespace dg
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[padmapping]"`
Expected: PASS. Fällt ein JSON-Fall durch (z. B. weil `JSON::parse` eine ganze Zahl wie `36.0` als Dezimalzahl liefert), die Ausgabe lesen und den Parser anpassen, nicht den Test, solange der Test der Spec entspricht.

- [ ] **Step 5: Alles testen und committen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

```bash
git add plugin tests
git commit -m "feat(plugin): pad mapping settings shared by all instances

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Der Processor übersetzt die Noten

**Files:**
- Modify: `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`
- Test: `tests/plugin/test_PluginProcessor.cpp`

**Interfaces:**
- Consumes: `PadMapping::instance()`, `translateNote` (Task 1 und 2).
- Produces: `DubgefahrenProcessor` übersetzt jede Note-An-/Note-Aus-Nachricht mit `translateNote(note, padMapping_.firstNote())`; nicht zugeordnete Noten erzeugen kein Ereignis.

- [ ] **Step 1: Failing tests schreiben**

In `tests/plugin/test_PluginProcessor.cpp`: `#include "PadMappingTestHelpers.h"` und `#include "plugin/PadMapping.h"` ergänzen, im anonymen Namespace einfügen:

```cpp
juce::MidiBuffer noteMessage(int note, bool on)
{
    juce::MidiBuffer b;
    b.addEvent(on ? juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)) : juce::MidiMessage::noteOff(1, note), 0);
    return b;
}
```

Nach dem Test `processBlock plays note 36 and treats velocity 0 as note off` einfügen:

```cpp
TEST_CASE("the first pad note moves the note block that triggers the pads", "[plugin][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    PadMapping::instance().setFirstNote(32); // BU16: Note 32 ist Pad 1

    DubgefahrenProcessor p;
    prepare(p);

    processBlocks(p, 1, noteMessage(32, true));
    CHECK(p.activeMask() == (1u << 0));
    processBlocks(p, 1, noteMessage(36, true));
    CHECK(p.activeMask() == ((1u << 0) | (1u << 4))); // Note 36 ist jetzt Pad 5
    processBlocks(p, 1, noteMessage(47, true));
    CHECK((p.activeMask() & (1u << 15)) != 0u);       // Pad 16

    // Davor und dahinter löst nichts aus.
    DubgefahrenProcessor q;
    prepare(q);
    processBlocks(q, 1, noteMessage(31, true));
    processBlocks(q, 1, noteMessage(48, true));
    CHECK(q.activeMask() == 0u);
}

TEST_CASE("with the default first note nothing changes", "[plugin][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    PadMapping::instance().setFirstNote(36);
    PadMapping::instance().setOrigin(PadOrigin::BottomLeft);

    DubgefahrenProcessor p;
    prepare(p);
    processBlocks(p, 1, noteMessage(35, true));
    CHECK(p.activeMask() == 0u);
    processBlocks(p, 1, noteMessage(36, true));
    CHECK(p.activeMask() == 1u);
    processBlocks(p, 1, noteMessage(51, true));
    CHECK((p.activeMask() & (1u << 15)) != 0u);
}

TEST_CASE("changing the first pad note takes effect for the next block", "[plugin][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    DubgefahrenProcessor p;
    prepare(p);

    PadMapping::instance().setFirstNote(36);
    processBlocks(p, 1, noteMessage(36, true));
    CHECK(p.activeMask() == 1u);

    PadMapping::instance().setFirstNote(40);
    processBlocks(p, 1, noteMessage(36, true)); // jetzt vor dem Block: ohne Wirkung
    CHECK(p.activeMask() == 1u);
    processBlocks(p, 1, noteMessage(41, true)); // Pad 2
    CHECK(p.activeMask() == ((1u << 0) | (1u << 1)));
}

TEST_CASE("note off follows the same mapping and releases the pad", "[plugin][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    PadMapping::instance().setFirstNote(32);
    DubgefahrenProcessor p;
    prepare(p);

    processBlocks(p, 1, noteMessage(32, true));
    CHECK(p.activeMask() == 1u);
    processBlocks(p, 60, noteMessage(32, false)); // Classic: Release 0,4 s
    CHECK(p.activeMask() == 0u);
}
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[padmapping]"`
Expected: FAIL (die ersten Tests: mit Basis 32 löst Note 32 nichts aus; „next block“: Note 41 löst Pad 6 statt Pad 2 aus).

- [ ] **Step 3: Implementieren**

`plugin/PluginProcessor.h`: `#include "plugin/PadMapping.h"` ergänzen und als Member einfügen (neben `cache_`):

```cpp
    // Prozessweit geteilte Pad-Belegung; hier gehalten, damit das Objekt nie im Audio-Thread entsteht.
    PadMapping& padMapping_ = PadMapping::instance();
```

`plugin/PluginProcessor.cpp`, in `processBlock` vor der Schleife `for (const auto meta : midi)` einfügen:

```cpp
    const int firstPadNote = padMapping_.firstNote();
```

und in der Schleife die Zeilen von `const int note = …` bis zum Ende des if/else ersetzen:

```cpp
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
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[plugin]"`
Expected: PASS, auch die bestehenden `[plugin]`-Tests (Note 36 → Pad 1, Noten außerhalb ignoriert).

- [ ] **Step 5: Alles testen und committen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

```bash
git add plugin tests
git commit -m "feat(plugin): map incoming MIDI notes through the configurable first pad note

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 4: `PadGrid` kennt den Ursprung

**Files:**
- Modify: `plugin/ui/PadGrid.h`, `plugin/ui/PadGrid.cpp`
- Test: `tests/plugin/test_Editor.cpp`

**Interfaces:**
- Consumes: `PadOrigin` (Task 2).
- Produces: `bool PadGrid::setOrigin(PadOrigin origin)` – legt das Raster neu an und liefert `true`, wenn sich der Ursprung geändert hat, sonst `false`. Standard: `PadOrigin::BottomLeft`.

- [ ] **Step 1: Failing test schreiben**

In `tests/plugin/test_Editor.cpp`: `#include "plugin/ui/PadGrid.h"` ergänzen und am Dateiende anfügen:

```cpp
TEST_CASE("the pad grid lays out pad 1 at the bottom left or the top left", "[editor][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    grid.setSize(360, 392);
    const auto at = [&](int slot) { return grid.pad(slot).getBounds().getPosition(); };

    // Standard: Pad 1 unten links, Pad 5 darüber, Pad 13 in der obersten Reihe.
    CHECK(at(0).x == at(4).x);
    CHECK(at(0).x < at(1).x);
    CHECK(at(0).y == at(1).y);
    CHECK(at(0).y > at(4).y);
    CHECK(at(4).y > at(12).y);

    CHECK(grid.setOrigin(PadOrigin::TopLeft));
    // Oben links: Pad 1 in der obersten Reihe, Pad 13 unten; die Spalten bleiben gleich.
    CHECK(at(0).x == at(4).x);
    CHECK(at(0).x < at(1).x);
    CHECK(at(0).y == at(3).y);
    CHECK(at(0).y < at(4).y);
    CHECK(at(4).y < at(12).y);
    CHECK(at(0).y == at(12).y - 3 * (at(4).y - at(0).y));

    CHECK_FALSE(grid.setOrigin(PadOrigin::TopLeft)); // unverändert
    CHECK(grid.setOrigin(PadOrigin::BottomLeft));
    CHECK(at(0).y > at(12).y);
}
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Build-FEHLER (`PadGrid::setOrigin` fehlt).

- [ ] **Step 3: Implementieren**

`plugin/ui/PadGrid.h`: `#include "plugin/PadMapping.h"` ergänzen; in der Klasse nach `refreshNames();` einfügen:

```cpp
    // Pad 1 unten links (Standard) oder oben links, wie das BU16 seine Tasten sendet. Legt das Raster
    // neu an; liefert true, wenn sich der Ursprung geändert hat.
    bool setOrigin(PadOrigin origin);
```

und im privaten Teil `PadOrigin origin_ = PadOrigin::BottomLeft;`.

`plugin/ui/PadGrid.cpp`: `setOrigin` ergänzen (neben `refreshNames`) und `resized` anpassen:

```cpp
bool PadGrid::setOrigin(PadOrigin origin)
{
    if (origin == origin_)
        return false;
    origin_ = origin;
    resized();
    repaint();
    return true;
}
```

In `PadGrid::resized()` die zwei Zeilen mit `rowFromBottom` und `setBounds` ersetzen:

```cpp
        const int col = s % 4;
        const int row = s / 4; // 0 = die Reihe, in der Pad 1 liegt
        // Unten links: Pad 1 in der untersten Reihe; oben links: in der obersten (wie die Tasten des BU16).
        const int y = origin_ == PadOrigin::TopLeft ? row : 3 - row;
        pads_[static_cast<std::size_t>(s)]->setBounds(area.getX() + col * w, area.getY() + y * h, w, h);
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[editor]"`
Expected: PASS.

- [ ] **Step 5: Alles testen und committen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

```bash
git add plugin tests
git commit -m "feat(ui): pad grid with pad 1 at the bottom left or the top left

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 5: MIDI-Button, Menü und Dialog im Editor

**Files:**
- Modify: `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`
- Test: `tests/plugin/test_Editor.cpp`

**Interfaces:**
- Consumes: `PadMapping`, `parsePadFirstNote`, `PadGrid::setOrigin` (Task 2 und 4), `createDialog`, `styled`.
- Produces: im Editor öffentlich `enum MidiMenuId { kMidiFirstNote = 1, kMidiOriginTop = 2 }`, `juce::PopupMenu buildMidiMenu() const`, `void applyMidiMenuResult(int result)`, `bool applyFirstNoteText(const juce::String& text)` (setzt die Note und liefert true, wenn der Text gültig war), `void showFirstNoteDialog()`, `juce::Point<int> padPosition(int slot)`. `pollProcessorState()` gleicht den Ursprung der Pads mit `PadMapping::instance()` ab.

- [ ] **Step 1: Failing tests schreiben**

In `tests/plugin/test_Editor.cpp`: `#include "PadMappingTestHelpers.h"`, `#include "plugin/PadMapping.h"` und `#include <tuple>` ergänzen und am Dateiende anfügen:

```cpp
namespace {
// (Text, Häkchen) der Einträge eines Menüs.
std::vector<std::pair<juce::String, bool>> tickedItems(const juce::PopupMenu& m)
{
    std::vector<std::pair<juce::String, bool>> out;
    for (juce::PopupMenu::MenuItemIterator it(m); it.next();)
        if (!it.getItem().isSeparator)
            out.emplace_back(it.getItem().text, it.getItem().isTicked);
    return out;
}
} // namespace

TEST_CASE("the MIDI menu shows the pad 1 note and the origin and changes the origin", "[editor][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    auto& mapping = PadMapping::instance();
    mapping.setFirstNote(36);
    mapping.setOrigin(PadOrigin::BottomLeft);
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    using Items = std::vector<std::pair<juce::String, bool>>;
    CHECK(tickedItems(e->buildMidiMenu()) == Items { { juce::String::fromUTF8("Pad 1 note: 36…"), false },
                                                      { "Pad 1 at top left", false } });

    e->applyMidiMenuResult(DubgefahrenEditor::kMidiOriginTop);
    CHECK(mapping.origin() == PadOrigin::TopLeft);
    CHECK(tickedItems(e->buildMidiMenu())[1].second); // Häkchen gesetzt
    CHECK(e->padPosition(0).y < e->padPosition(4).y); // Pad 1 in der obersten Reihe

    e->applyMidiMenuResult(DubgefahrenEditor::kMidiOriginTop);
    CHECK(mapping.origin() == PadOrigin::BottomLeft);
    CHECK(e->padPosition(0).y > e->padPosition(4).y);
}

TEST_CASE("a second editor follows a change made elsewhere", "[editor][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    PadMapping::instance().setOrigin(PadOrigin::BottomLeft);
    DubgefahrenProcessor a, b;
    std::unique_ptr<juce::AudioProcessorEditor> editorA(a.createEditor());
    std::unique_ptr<juce::AudioProcessorEditor> editorB(b.createEditor());
    auto* ea = static_cast<DubgefahrenEditor*>(editorA.get());
    auto* eb = static_cast<DubgefahrenEditor*>(editorB.get());
    CHECK(eb->padPosition(0).y > eb->padPosition(4).y);

    ea->applyMidiMenuResult(DubgefahrenEditor::kMidiOriginTop);
    CHECK(eb->padPosition(0).y > eb->padPosition(4).y); // noch nicht abgefragt
    eb->pollProcessorState();
    CHECK(eb->padPosition(0).y < eb->padPosition(4).y);
}

TEST_CASE("the first note dialog accepts only valid numbers", "[editor][padmapping]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::ScopedPadMapping restore;
    PadMapping::instance().setFirstNote(36);
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    e->showFirstNoteDialog();
    auto* dialog = e->topDialog();
    REQUIRE(dialog != nullptr);
    auto* field = dialog->getTextEditor("note");
    auto* ok = dialog->getButton("OK");
    REQUIRE(field != nullptr);
    REQUIRE(ok != nullptr);
    CHECK(field->getText() == "36");
    CHECK(ok->isEnabled());

    for (const char* bad : { "", "abc", "113", "1e2", "0x20", "36.5" })
    {
        INFO(bad);
        field->setText(bad, true);
        CHECK_FALSE(ok->isEnabled());
        CHECK_FALSE(e->applyFirstNoteText(bad));
        CHECK(PadMapping::instance().firstNote() == 36); // ein erzwungener Aufruf ändert nichts
    }
    field->setText(" 32 ", true);
    CHECK(ok->isEnabled());
    CHECK(e->applyFirstNoteText(field->getText()));
    CHECK(PadMapping::instance().firstNote() == 32);
    CHECK(tickedItems(e->buildMidiMenu())[0].first == juce::String::fromUTF8("Pad 1 note: 32…"));
}
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Build-FEHLER (`buildMidiMenu`, `kMidiOriginTop`, `padPosition`, `showFirstNoteDialog`, `applyFirstNoteText` fehlen).

- [ ] **Step 3: Implementieren**

`plugin/PluginEditor.h`: `#include "plugin/PadMapping.h"` ergänzen. Im öffentlichen Teil, nach `showPadMenu`:

```cpp
    // MIDI-Menü: Pad-1-Note (öffnet einen Dialog) und Ursprung des Pad-Rasters. Öffentlich für Tests.
    enum MidiMenuId { kMidiFirstNote = 1, kMidiOriginTop = 2 };
    juce::PopupMenu buildMidiMenu() const;
    void applyMidiMenuResult(int result);
    void showFirstNoteDialog();
    // Setzt die Pad-1-Note aus dem Zahlenfeld; liefert false (und ändert nichts), wenn der Text ungültig ist.
    bool applyFirstNoteText(const juce::String& text);
    // Position eines Pads im Raster (Basis-Koordinaten des Rasters).
    juce::Point<int> padPosition(int slot) { return pads_.pad(slot).getPosition(); }
```

Im privaten Teil `void showMidiMenu();` ergänzen und als Member (neben `kitButton_`):

```cpp
    juce::TextButton midiButton_ { juce::String::fromUTF8("MIDI ▾") };
```

`plugin/PluginEditor.cpp`:

Konstruktor: nach `kitButton_.onClick = …` einfügen:

```cpp
    midiButton_.onClick = [this] { showMidiMenu(); };
```

in die `content_.addAndMakeVisible`-Liste `&midiButton_` nach `&kitButton_` aufnehmen, und vor `layoutContent();` einfügen:

```cpp
    pads_.setOrigin(PadMapping::instance().origin());
```

`layoutContent()`: nach `kitButton_.setBounds(…)` einfügen:

```cpp
    midiButton_.setBounds(header.removeFromRight(80).reduced(2));
```

`pollProcessorState()`: ganz am Anfang einfügen:

```cpp
    // Die Pad-Belegung gilt für alle Instanzen: Eine Änderung in einem anderen Editor übernehmen.
    if (pads_.setOrigin(PadMapping::instance().origin()))
        repaint();
```

Neue Funktionen (z. B. nach `showPadMenu`):

```cpp
juce::PopupMenu DubgefahrenEditor::buildMidiMenu() const
{
    const auto& mapping = PadMapping::instance();
    juce::PopupMenu menu;
    menu.addItem(kMidiFirstNote, juce::String::fromUTF8("Pad 1 note: ") + juce::String(mapping.firstNote()) + juce::String::fromUTF8("…"));
    menu.addItem(kMidiOriginTop, "Pad 1 at top left", true, mapping.origin() == PadOrigin::TopLeft);
    return menu;
}

void DubgefahrenEditor::showMidiMenu()
{
    styled(buildMidiMenu()).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(midiButton_),
                                          [safe = juce::Component::SafePointer<DubgefahrenEditor>(this)](int result) {
                                              if (safe != nullptr)
                                                  safe->applyMidiMenuResult(result);
                                          });
}

void DubgefahrenEditor::applyMidiMenuResult(int result)
{
    auto& mapping = PadMapping::instance();
    if (result == kMidiFirstNote)
    {
        showFirstNoteDialog();
    }
    else if (result == kMidiOriginTop)
    {
        mapping.setOrigin(mapping.origin() == PadOrigin::TopLeft ? PadOrigin::BottomLeft : PadOrigin::TopLeft);
        if (pads_.setOrigin(mapping.origin()))
            repaint();
    }
}

bool DubgefahrenEditor::applyFirstNoteText(const juce::String& text)
{
    const auto note = parsePadFirstNote(text);
    if (!note)
        return false;
    PadMapping::instance().setFirstNote(*note);
    return true;
}

void DubgefahrenEditor::showFirstNoteDialog()
{
    auto* window = createDialog("Pad 1 note", "MIDI note of the first pad (0 to 112):", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor("note", juce::String(PadMapping::instance().firstNote()));
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    // OK ist nur bei einer gültigen Zahl wählbar. Das Zahlenfeld nimmt nur Ziffern an.
    if (auto* field = window->getTextEditor("note"))
    {
        field->setInputRestrictions(3, "0123456789");
        field->onTextChange = [field, ok = juce::Component::SafePointer<juce::Button>(window->getButton("OK"))] {
            if (ok != nullptr)
                ok->setEnabled(parsePadFirstNote(field->getText()).has_value());
        };
    }
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), window](int result) {
                                    if (safe == nullptr || result != 1)
                                        return;
                                    safe->applyFirstNoteText(window->getTextEditorContents("note"));
                                }),
                            true);
}
```

Hinweis zum Test: `field->setText(bad, true)` umgeht die Eingabe-Einschränkung (sie gilt nur beim Tippen); deshalb ruft der Test `applyFirstNoteText` auch direkt mit Ungültigem auf. Fehlt in JUCE 8.0.15 die Überladung `AlertWindow::getButton(const String&)`, `getButton(0)` verwenden („OK“ ist der erste Button).

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[editor]"`
Expected: PASS.

- [ ] **Step 5: Sichtprüfung**

Mit gesetztem `DG_SNAPSHOT_DIR` (z. B. dem Scratchpad) einen Editor rendern und prüfen, dass der MIDI-Button in der Kopfzeile Platz hat und weder den Kit-Button noch „Editor follows focus“ überdeckt (`dgtest::snapshot(*editor)` in einem kurzen Wegwerf-Test oder im Test-Lauf `[look]`, falls dort bereits ein Editor-Snapshot entsteht; das PNG mit dem Read-Werkzeug ansehen). Passt die Kopfzeile nicht (Summe der Breiten: 220 + 90 + 80 + 110 + 80 + 80 + 12 + 90 + 170 = 932 von 976 px), den Platz für `followFocus_` verringern.

- [ ] **Step 6: Alles testen und committen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

```bash
git add plugin tests
git commit -m "feat(ui): MIDI menu for the pad 1 note and the pad grid origin

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 6: README, Spec-Status, Validator

**Files:**
- Modify: `README.md`, `docs/superpowers/specs/2026-10-01-pad-mapping-design.md`

**Interfaces:**
- Consumes: alles aus Task 1 bis 5.
- Produces: aktualisierte Doku und die Ableton-Checkliste für den PR.

- [ ] **Step 1: README anpassen**

`README.md` Zeile 3: `playable via MIDI notes 36–51` → `playable via 16 consecutive MIDI notes (default 36–51, adjustable)`.

Hinter dem Abschnitt, der die Installation/Verwendung beschreibt (oder direkt vor „Validation“), einen kurzen Abschnitt einfügen:

```markdown
## MIDI mapping

The 16 pads answer to 16 consecutive MIDI notes. By default pad 1 is note 36 and sits at the bottom left
of the pad grid. The **MIDI** menu in the header changes both: *Pad 1 note* sets the note of the first
pad, *Pad 1 at top left* flips the grid so that pad 1 is at the top left. The setting is stored per user in
`%APPDATA%\Dubgefahren\settings.json` and applies to every project and instance.

The Intech Grid BU16 sends its buttons row by row from the top left starting at note 32: set
*Pad 1 note* to 32 and enable *Pad 1 at top left* to make the grid match the controller.
```

In der Checkliste (Zeile 96) `BU16 pads 1–16 play slots 1–16; the pad display in the plugin lights up accordingly.` ersetzen durch:

```markdown
- [ ] BU16 (MIDI menu: pad 1 note 32, pad 1 at top left) pads 1–16 play slots 1–16; each button lights up the pad at the same position in the plugin.
```

- [ ] **Step 2: Spec-Status**

`docs/superpowers/specs/2026-10-01-pad-mapping-design.md`: `**Status:** Entwurf, Review offen` → `**Status:** Umgesetzt, Abnahme in Ableton offen`.

- [ ] **Step 3: Gesamtlauf mit Validator**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test` und danach `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
Expected: alle Tests grün; VST3-Validator ohne Fehler, pluginval SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add README.md docs/superpowers/specs/2026-10-01-pad-mapping-design.md
git commit -m "docs: document the MIDI mapping settings

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 5: Abnahme-Checkliste für Ableton**

Für den PR-Text, vom Nutzer abzuhaken (Release-Build installieren, Ableton schließen, `.vst3` kopieren):
1. Ohne Einstellung (36, unten links): Das Plugin verhält sich wie bisher (Noten 36 bis 51).
2. MIDI-Menü: Pad 1 note auf 32 stellen, „Pad 1 at top left“ einschalten: Jede der 16 Tasten des BU16 löst das Pad an derselben Stelle auf dem Bildschirm aus (oberste Reihe = Pad 1 bis 4, unterste = Pad 13 bis 16).
3. Ungültige Eingaben im Dialog (leer, 113, Buchstaben): „OK“ ist gesperrt.
4. Plugin zweimal in einem Projekt laden: Eine Änderung im einen Editor erscheint im anderen, ohne ihn neu zu öffnen.
5. Ableton schließen und neu starten, Plugin laden: Die Einstellung ist noch da.
6. `%APPDATA%\Dubgefahren\settings.json` löschen oder mit Unsinn füllen: Das Plugin startet mit 36 / unten links, ohne Fehlermeldung.
