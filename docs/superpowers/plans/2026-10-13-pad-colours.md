# Pad-Farben Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Jeder Slot mit Klangquelle bekommt eine von acht Farben, gespeichert in Plugin-State und Kit, wählbar im Pad-Kontextmenü; die Latch-Anzeige wird farbunabhängig kontrastreich.

**Architecture:** Der Farbindex ist Metadatum wie der Slot-Name: `Kit::colours` in der Engine (nur mitgetragen), ein `SLOTCOLOURS`-Knoten im APVTS-State, der Schlüssel `"colour"` im Kit-Format 6. Die UI übersetzt den Index über eine Palette (`plugin/ui/PadColours.h`) in Grund- und Schriftfarbe; `PadGrid` holt sie bei jedem `refreshNames()`, `WaveformView` beim Zeichnen.

**Tech Stack:** C++20, JUCE 8.0.15, Catch2 v3, CMake/MSVC, `build.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-13-pad-colours-design.md`

## Global Constraints

- Branch `feature/pad-colours`; Commits enden mit `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Build/Test: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test` (Release). Fokussiert: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[tag]"`.
- Palette: genau 8 Farben, Index 0 = Green = `colours::padBase` mit Schrift `colours::padTextLit`; Indizes außerhalb 0 … 7 gelten als 0.
- Die Farbe ist **kein** Host-Parameter (Parameteranzahl bleibt 490).
- Kit-Format **Version 6**, Schlüssel `"colour"` (int) pro Slot; Versionen 1 … 5 laden mit Farbe 0; fehlender, nicht numerischer oder ungültiger Wert → 0, kein Ladefehler.
- State-Knoten `SLOTCOLOURS`, Eigenschaften `c1` … `c16`; fehlend → 0.
- Leere Slots haben Farbe 0 (Leeren setzt zurück); Werkseinstellung setzt 0; Kopieren/Einfügen übernimmt die Farbe; Sample ersetzen lässt sie unverändert.
- Kontrast (WCAG) mindestens 3:1 für Latch-Punkt (besserer Wert aus Kern und Ring) gegen gedimmt / leuchtend oben / leuchtend unten und für die Schrift auf leuchtendem Pad gegen leuchtend oben / unten.
- Menütexte englisch; Untermenü heißt „Colour“.
- `engine/` bleibt ohne JUCE.
- Nach grüner Suite auf dem Branch: VST3-Ordner als `C:\Users\atzet\OneDrive\Dokumente\musik\dubgefahren\dubgefahren.vst3.feature-pad-colours.zip` zippen (`Compress-Archive -Path E:\Repos\dubgefahren\build\plugin\Dubgefahren_artefacts\Release\VST3 -DestinationPath <zip> -Force`).

## Review Focus

- **Host-Projekt aus einer älteren Version (ohne `SLOTCOLOURS`)** muss mit Farbe 0 laden, ohne Fehler → Test in Task 3 (State ohne Knoten).
- **Einfügen eines leeren Slots über ein gefärbtes Pad** darf keine Farbe auf einem leeren Pad hinterlassen → Test in Task 5.
- **Kit mit `"colour": 9`, `-1`, `"blau"` oder `true`** lädt mit Farbe 0 statt abzulehnen → Test in Task 2.
- **Farbe setzen, während ein anderes Pad im Editor angezeigt wird**, muss sofort auf dem richtigen Pad sichtbar sein (über `stateGeneration`) → Test in Task 4.
- **Sample ersetzen auf einem gefärbten Pad** lässt die Farbe stehen → Test in Task 3.

---

## Dateistruktur

| Datei | Verantwortung |
|---|---|
| `engine/Kit.h` | `Kit::colours`, `kNumPadColours`, `clampPadColour()` (Datenmodell) |
| `plugin/KitFile.cpp` | Kit-Format 6, Schlüssel `"colour"` |
| `plugin/PluginProcessor.h/.cpp` | `SLOTCOLOURS`, `slotColour()`, `setSlotColour()`, Kit-Übernahme, Leeren/Zurücksetzen |
| `plugin/ui/PadColours.h/.cpp` (neu) | Palette, Latch-Farben, WCAG-Kontrast |
| `plugin/ui/PadGrid.cpp/.h` | Farbe pro Pad aus dem Processor, Schriftfarbe, neuer Latch-Punkt, Legende |
| `plugin/PluginEditor.h/.cpp` | Untermenü „Colour“, `applyPadMenuResult`, Zwischenablage mit Farbe |
| `plugin/ui/WaveformView.h/.cpp` | Wellenform in Slot-Farbe |
| `README.md` | Doku |

---

### Task 1: Palette und Kontrastfunktionen

**Files:**
- Modify: `engine/Kit.h`
- Create: `plugin/ui/PadColours.h`, `plugin/ui/PadColours.cpp`, `tests/plugin/test_PadColours.cpp`
- Modify: `plugin/CMakeLists.txt` (nach `ui/PadGrid.cpp` die zwei neuen Dateien), `tests/CMakeLists.txt` (`plugin/test_PadColours.cpp` an die `target_sources(DubgefahrenPluginTests …)`-Liste anhängen)

**Interfaces:**
- Produces: in `engine/Kit.h`: `constexpr int kNumPadColours = 8;`, `inline int clampPadColour(int index)`, `Kit::colours` (`std::array<int, kNumSlots>`, alle 0). In `plugin/ui/PadColours.h` (Namespace `dg::ui`): `struct PadColour { const char* name; juce::Colour base; juce::Colour litText; }`, `const PadColour& padColour(int index)`, `latchDotCore`, `latchDotRing`, `float relativeLuminance(juce::Colour)`, `float contrastRatio(juce::Colour, juce::Colour)`.

- [ ] **Step 1: Failing test `tests/plugin/test_PadColours.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <set>
#include "engine/Kit.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/PadColours.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
float bestLatchContrast(juce::Colour surface)
{
    return std::max(ui::contrastRatio(ui::latchDotCore, surface), ui::contrastRatio(ui::latchDotRing, surface));
}
} // namespace

TEST_CASE("the palette has eight named colours with green as the default", "[padcolour]")
{
    STATIC_CHECK(kNumPadColours == 8);
    CHECK(ui::padColour(0).base == ui::colours::padBase);
    CHECK(ui::padColour(0).litText == ui::colours::padTextLit);
    std::set<juce::String> names;
    std::set<juce::uint32> bases;
    for (int i = 0; i < kNumPadColours; ++i)
    {
        names.insert(ui::padColour(i).name);
        bases.insert(ui::padColour(i).base.getARGB());
    }
    CHECK(names.size() == 8);
    CHECK(bases.size() == 8);
    CHECK(juce::String(ui::padColour(0).name) == "Green");
    CHECK(juce::String(ui::padColour(7).name) == "Yellow");
}

TEST_CASE("out-of-range colour indices fall back to the default", "[padcolour]")
{
    CHECK(clampPadColour(-1) == 0);
    CHECK(clampPadColour(8) == 0);
    CHECK(clampPadColour(5) == 5);
    CHECK(&ui::padColour(-1) == &ui::padColour(0));
    CHECK(&ui::padColour(1000) == &ui::padColour(0));
    Kit k;
    CHECK(std::all_of(k.colours.begin(), k.colours.end(), [](int c) { return c == 0; }));
}

TEST_CASE("contrast ratios follow the WCAG formula", "[padcolour]")
{
    CHECK_THAT(ui::contrastRatio(juce::Colours::white, juce::Colours::black), WithinAbs(21.0, 0.01));
    CHECK_THAT(ui::contrastRatio(juce::Colours::black, juce::Colours::white), WithinAbs(21.0, 0.01));
    CHECK_THAT(ui::contrastRatio(ui::colours::padBase, ui::colours::padBase), WithinAbs(1.0, 1e-6));
}

TEST_CASE("the latch dot is readable on every pad colour, dimmed and lit", "[padcolour]")
{
    const auto dim = ui::colours::padTop.interpolatedWith(ui::colours::padBottom, 0.5f);
    CHECK(bestLatchContrast(dim) >= 3.0f);
    for (int i = 0; i < kNumPadColours; ++i)
    {
        const auto& c = ui::padColour(i);
        INFO(c.name);
        CHECK(bestLatchContrast(c.base.brighter(0.7f)) >= 3.0f);
        CHECK(bestLatchContrast(c.base) >= 3.0f);
    }
}

TEST_CASE("lit pad text is readable on every pad colour", "[padcolour]")
{
    for (int i = 0; i < kNumPadColours; ++i)
    {
        const auto& c = ui::padColour(i);
        INFO(c.name);
        CHECK(ui::contrastRatio(c.litText, c.base) >= 3.0f);
        CHECK(ui::contrastRatio(c.litText, c.base.brighter(0.7f)) >= 3.0f);
    }
}
```

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'plugin/ui/PadColours.h'` (bzw. CMake: Quelldatei fehlt).

- [ ] **Step 3: `engine/Kit.h` erweitern**

```cpp
#pragma once
#include <array>
#include <string>
#include "engine/SlotParams.h"

namespace dg {

// Pad-Farben: Index in die Palette der Oberfläche; die Engine trägt ihn nur mit.
constexpr int kNumPadColours = 8;
inline int clampPadColour(int index) { return (index >= 0 && index < kNumPadColours) ? index : 0; }

struct Kit
{
    std::array<SlotParams, kNumSlots> slots {};
    std::array<std::string, kNumSlots> names {}; // UTF-8
    std::array<std::string, kNumSlots> samples {}; // UTF-8, relativ zu <Kit>/, nur bei Sample-Slots
    std::array<int, kNumSlots> colours {}; // Palettenindex 0 … kNumPadColours-1, 0 = Standard
};

Kit makeFactoryKit();
// Alle Slots leer und ohne Namen; die (unsichtbaren) Synth-Werte entsprechen dem Factory-Kit.
Kit makeEmptyKit();

} // namespace dg
```

- [ ] **Step 4: `plugin/ui/PadColours.h` und `.cpp` anlegen**

`plugin/ui/PadColours.h`:

```cpp
#pragma once
#include <juce_graphics/juce_graphics.h>
#include "engine/Kit.h"

namespace dg::ui {

struct PadColour
{
    const char* name;     // englisch, für das Menü
    juce::Colour base;    // Grundfarbe: Schimmer, Leuchten, Glow, Wellenform
    juce::Colour litText; // Schrift auf leuchtendem Pad
};

// Eintrag zum gespeicherten Index; Werte außerhalb 0 … kNumPadColours-1 liefern die Standardfarbe.
const PadColour& padColour(int index);

// Latch-Anzeige: heller Kern mit dunklem Ring, lesbar auf jeder Pad-Farbe.
inline const juce::Colour latchDotCore { 0xfff4f6f8 };
inline const juce::Colour latchDotRing { 0xff0a0c0e };

// WCAG 2.x: relative Leuchtdichte und Kontrastverhältnis (1 … 21).
float relativeLuminance(juce::Colour c);
float contrastRatio(juce::Colour a, juce::Colour b);

} // namespace dg::ui
```

`plugin/ui/PadColours.cpp`:

```cpp
#include "plugin/ui/PadColours.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

namespace {
// Funktionslokal, damit die Reihenfolge der statischen Initialisierung keine Rolle spielt.
const std::array<PadColour, kNumPadColours>& palette()
{
    static const std::array<PadColour, kNumPadColours> p { {
        { "Green",     colours::padBase,          colours::padTextLit },
        { "Turquoise", juce::Colour(0xff2fd3c7), juce::Colour(0xff042624) },
        { "Blue",      juce::Colour(0xff4f9dff), juce::Colour(0xff061a33) },
        { "Violet",    juce::Colour(0xffa98bff), juce::Colour(0xff1a0f3a) },
        { "Pink",      juce::Colour(0xffff6fc0), juce::Colour(0xff3a0a24) },
        { "Red",       juce::Colour(0xffff5a52), juce::Colour(0xff3a0806) },
        { "Orange",    juce::Colour(0xffff9a3c), juce::Colour(0xff3a1a03) },
        { "Yellow",    juce::Colour(0xfff5e663), juce::Colour(0xff332c02) },
    } };
    return p;
}

float linearChannel(juce::uint8 v)
{
    const float c = static_cast<float>(v) / 255.0f;
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
} // namespace

const PadColour& padColour(int index) { return palette()[static_cast<std::size_t>(clampPadColour(index))]; }

float relativeLuminance(juce::Colour c)
{
    return 0.2126f * linearChannel(c.getRed()) + 0.7152f * linearChannel(c.getGreen()) + 0.0722f * linearChannel(c.getBlue());
}

float contrastRatio(juce::Colour a, juce::Colour b)
{
    const float la = relativeLuminance(a);
    const float lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f);
}

} // namespace dg::ui
```

In `plugin/CMakeLists.txt` nach der Zeile `${CMAKE_CURRENT_SOURCE_DIR}/ui/PadGrid.cpp` einfügen:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/PadColours.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/PadColours.cpp
```

In `tests/CMakeLists.txt` in der Liste `target_sources(DubgefahrenPluginTests PRIVATE …)` am Ende `plugin/test_PadColours.cpp` ergänzen.

- [ ] **Step 5: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und dann `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[padcolour]"`
Expected: PASS. Schlägt ein Kontrasttest fehl, nur den betroffenen RGB-Wert der Palette anpassen (Grundfarbe heller bzw. Schriftfarbe dunkler, Farbton beibehalten) und die Änderung im Report nennen.

- [ ] **Step 6: Commit**

```bash
git add engine/Kit.h plugin/ui/PadColours.h plugin/ui/PadColours.cpp plugin/CMakeLists.txt tests/CMakeLists.txt tests/plugin/test_PadColours.cpp
git commit -m "feat(ui): pad colour palette with contrast checks" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Kit-Format 6 mit `"colour"`

**Files:**
- Modify: `plugin/KitFile.cpp`
- Test: `tests/plugin/test_KitFile.cpp`

**Interfaces:**
- Consumes: `Kit::colours`, `clampPadColour` (Task 1).
- Produces: Kit-JSON Version 6 mit `"colour"` pro Slot.

- [ ] **Step 1: Tests anpassen und ergänzen**

In `tests/plugin/test_KitFile.cpp` den Test `"version 6 kits are rejected as too new"` umbenennen und auf Version 7 ändern:

```cpp
TEST_CASE("version 7 kits are rejected as too new", "[kitfile]")
{
    auto v = parsed(makeFactoryKit());
    v.getDynamicObject()->setProperty("version", 7);
    const auto r = reparse(v);
    CHECK_FALSE(r.kit.has_value());
    CHECK(r.error.contains("newer version"));
}
```

Am Dateiende anhängen:

```cpp
TEST_CASE("pad colours survive a version 6 round-trip", "[kitfile][padcolour]")
{
    Kit k = makeFactoryKit();
    k.colours[0] = 3;
    k.colours[15] = 7;
    const auto json = kitToJsonString(k);
    CHECK(static_cast<int>(juce::JSON::parse(json)["version"]) == 6);
    const auto r = kitFromJsonString(json);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->colours == k.colours);
}

TEST_CASE("a version 5 kit without colours loads with the default colour", "[kitfile][padcolour]")
{
    Kit k = makeFactoryKit();
    k.colours[2] = 4;
    auto v = parsed(k);
    v.getDynamicObject()->setProperty("version", 5);
    for (int s = 0; s < kNumSlots; ++s)
        v["slots"][s].getDynamicObject()->removeProperty("colour");
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(std::all_of(r.kit->colours.begin(), r.kit->colours.end(), [](int c) { return c == 0; }));
}

TEST_CASE("invalid colour values load as the default colour", "[kitfile][padcolour]")
{
    auto v = parsed(makeFactoryKit());
    v["slots"][0].getDynamicObject()->setProperty("colour", 9);
    v["slots"][1].getDynamicObject()->setProperty("colour", -1);
    v["slots"][2].getDynamicObject()->setProperty("colour", "blau");
    v["slots"][3].getDynamicObject()->setProperty("colour", true);
    v["slots"][4].getDynamicObject()->setProperty("colour", 2.0);
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->colours[0] == 0);
    CHECK(r.kit->colours[1] == 0);
    CHECK(r.kit->colours[2] == 0);
    CHECK(r.kit->colours[3] == 0);
    CHECK(r.kit->colours[4] == 2);
}

TEST_CASE("empty slots always load with the default colour", "[kitfile][padcolour]")
{
    Kit k = makeFactoryKit();
    k.slots[6].source = SourceType::Empty;
    auto v = parsed(k);
    v["slots"][6].getDynamicObject()->setProperty("colour", 5);
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->colours[6] == 0);
}
```

Oben in der Datei `#include <algorithm>` ergänzen, falls nicht vorhanden.

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[kitfile]"`
Expected: FAIL – Round-Trip liefert Version 5 und Farben 0; Version-7-Test schlägt nicht fehl, aber der Round-Trip-Test.

- [ ] **Step 3: `plugin/KitFile.cpp` ändern**

`constexpr int kVersion = 5;` → `constexpr int kVersion = 6;`

In `kitToJsonString` direkt nach `slot->setProperty("name", …);`:

```cpp
        slot->setProperty("colour", src == SourceType::Empty ? 0 : clampPadColour(kit.colours[i]));
```

Im anonymen Namespace ergänzen:

```cpp
// Farbe eines Slots: nur ganze bzw. gebrochene Zahlen zählen; alles andere ist die Standardfarbe.
int readColour(const juce::var& v)
{
    if (v.isInt() || v.isInt64() || v.isDouble())
        return clampPadColour(static_cast<int>(v));
    return 0;
}
```

In `kitFromJsonString`: Im Zweig `if (source == SourceType::Empty)` vor `continue;` ergänzen:

```cpp
            kit.colours[static_cast<std::size_t>(s)] = 0;
```

und am Ende der Schleife nach `kit.samples[…] = sampleName;`:

```cpp
        kit.colours[static_cast<std::size_t>(s)] = readColour(slot["colour"]);
```

- [ ] **Step 4: Tests ausführen**

Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[kitfile]"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/KitFile.cpp tests/plugin/test_KitFile.cpp
git commit -m "feat(plugin): kit format version 6 with pad colours" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Farbe im Plugin-State

**Files:**
- Modify: `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`
- Test: `tests/plugin/test_PluginProcessor.cpp`

**Interfaces:**
- Consumes: `Kit::colours`, `clampPadColour` (Task 1).
- Produces: `int DubgefahrenProcessor::slotColour(int slot) const`, `void DubgefahrenProcessor::setSlotColour(int slot, int colour)` (Message-Thread, klemmt, erhöht `stateGeneration`). `currentKit()`/`applyKit()` tragen `colours`; `clearSlot` und `resetSlotToFactory` setzen 0.

- [ ] **Step 1: Tests am Ende von `tests/plugin/test_PluginProcessor.cpp` anhängen**

```cpp
TEST_CASE("slot colours default to 0, clamp and bump the state generation", "[plugin][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    for (int s = 0; s < kNumSlots; ++s)
        CHECK(p.slotColour(s) == 0);
    const int gen = p.stateGeneration();
    p.setSlotColour(4, 6);
    CHECK(p.slotColour(4) == 6);
    CHECK(p.stateGeneration() != gen);
    p.setSlotColour(4, 42);
    CHECK(p.slotColour(4) == 0);
}

TEST_CASE("slot colours survive a state round-trip and old states load with 0", "[plugin][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    a.setSlotColour(1, 3);
    a.setSlotColour(9, 7);
    juce::MemoryBlock mb;
    a.getStateInformation(mb);

    DubgefahrenProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    CHECK(b.slotColour(1) == 3);
    CHECK(b.slotColour(9) == 7);
    CHECK(b.slotColour(0) == 0);

    // Projekt aus einer älteren Version: kein SLOTCOLOURS-Knoten
    auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), static_cast<int>(mb.getSize()));
    REQUIRE(xml != nullptr);
    if (auto* coloursNode = xml->getChildByName("SLOTCOLOURS"))
        xml->removeChildElement(coloursNode, true);
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml, old);
    DubgefahrenProcessor c;
    c.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    CHECK(c.slotColour(1) == 0);
    CHECK(c.slotColour(9) == 0);
}

TEST_CASE("applyKit and currentKit carry the slot colours", "[plugin][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    Kit k = makeFactoryKit();
    k.colours[0] = 2;
    k.colours[12] = 5;
    p.applyKit(k);
    CHECK(p.slotColour(0) == 2);
    CHECK(p.slotColour(12) == 5);
    CHECK(p.currentKit().colours == k.colours);
}

TEST_CASE("clearing or resetting a slot resets its colour", "[plugin][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.setSlotColour(3, 4);
    p.clearSlot(3);
    CHECK(p.slotColour(3) == 0);
    p.setSlotColour(5, 4);
    p.resetSlotToFactory(5);
    CHECK(p.slotColour(5) == 0);
}
```

Zusätzlich (Review Focus „Sample ersetzen“) am Ende von `tests/plugin/test_Samples.cpp` anhängen (die Fixture `SampleKit` aus `SampleTestHelpers.h` legt einen Kit-Ordner mit `horn.wav` an):

```cpp
TEST_CASE("replacing a sample keeps the pad colour", "[plugin][samples][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(3, "horn.wav");
    p.setSlotColour(3, 6);
    p.setSlotSample(3, "missing.wav"); // anderes Sample: Marker und Stretch werden zurückgesetzt, die Farbe nicht
    CHECK(p.slotSample(3) == "missing.wav");
    CHECK(p.slotColour(3) == 6);
}
```

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL beim Kompilieren – `slotColour`/`setSlotColour` unbekannt.

- [ ] **Step 3: Implementieren**

`plugin/PluginProcessor.h` im öffentlichen Teil neben `slotName`/`setSlotName`:

```cpp
    // Pad-Farbe (Palettenindex 0 … kNumPadColours-1); Metadatum wie der Name, kein Host-Parameter.
    int slotColour(int slot) const;
    void setSlotColour(int slot, int colour);
```

`plugin/PluginProcessor.cpp`:

Im anonymen Namespace:

```cpp
const juce::Identifier kColoursId { "SLOTCOLOURS" };
juce::Identifier colourKey(int slot) { return juce::Identifier("c" + juce::String(slot + 1)); }
```

In `ensureStateChildren()` als erste Zeile ergänzen:

```cpp
    apvts_.state.getOrCreateChildWithName(kColoursId, nullptr);
```

Neue Methoden (nach `setSlotName`):

```cpp
int DubgefahrenProcessor::slotColour(int slot) const
{
    const auto v = apvts_.state.getChildWithName(kColoursId).getProperty(colourKey(slot));
    return v.isVoid() ? 0 : clampPadColour(static_cast<int>(v));
}

void DubgefahrenProcessor::setSlotColour(int slot, int colour)
{
    JUCE_ASSERT_MESSAGE_THREAD
    apvts_.state.getOrCreateChildWithName(kColoursId, nullptr).setProperty(colourKey(slot), clampPadColour(colour), nullptr);
    ++stateGeneration_;
}
```

`clearSlot`: nach `setSlot(slot, p, {});` ergänzen `setSlotColour(slot, 0);`
`resetSlotToFactory`: nach dem `setSlot(...)` ergänzen `setSlotColour(slot, 0);`
`currentKit`: in der Schleife ergänzen `k.colours[static_cast<std::size_t>(s)] = slotColour(s);`
`applyKit`: in der Schleife nach `setSlot(...)` ergänzen `setSlotColour(s, kit.colours[i]);`
`setSlotSample` bleibt unverändert (die Farbe wird dort nicht angefasst).

- [ ] **Step 4: Tests ausführen**

Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[plugin]"` und `"[samples]"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/PluginProcessor.h plugin/PluginProcessor.cpp tests/plugin/test_PluginProcessor.cpp tests/plugin/test_Samples.cpp
git commit -m "feat(plugin): store pad colours in the plugin state" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Pads zeichnen in Slot-Farbe, neuer Latch-Punkt

**Files:**
- Modify: `plugin/ui/PadGrid.cpp`, `plugin/ui/PadGrid.h`
- Test: `tests/plugin/test_Look.cpp`

**Interfaces:**
- Consumes: `ui::padColour`, `ui::latchDotCore`, `ui::latchDotRing` (Task 1), `DubgefahrenProcessor::slotColour` (Task 3).
- Produces: `PadGrid::refreshNames()` setzt pro Pad Grund- und Schriftfarbe aus `slotColour`; `PadGrid::padLitTextColour(int slot) const` (für Tests).

- [ ] **Step 1: Tests in `tests/plugin/test_Look.cpp`**

Oben `#include "plugin/ui/PadColours.h"` ergänzen.

Den bestehenden Test `"a latched pad shows the blue dot in the editor"` umbenennen in `"a latched pad shows the light dot with a dark ring in the editor"` und seine letzten zwei `CHECK`-Zeilen ersetzen durch:

```cpp
    CHECK(dot.getRed() > 220);
    CHECK(dot.getGreen() > 220);
    CHECK(dot.getBlue() > 220);
```

Am Dateiende anhängen:

```cpp
TEST_CASE("pads take their colour from the slot", "[look][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    p.setSlotColour(2, 2);
    grid.refreshNames();
    CHECK(grid.padBaseColour(2) == ui::padColour(2).base);
    CHECK(grid.padLitTextColour(2) == ui::padColour(2).litText);
    CHECK(grid.padBaseColour(0) == ui::padColour(0).base);
}

TEST_CASE("the editor shows a colour change on the right pad", "[look][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    e->selectSlot(0);
    p.setSlotColour(7, 5);
    e->pollProcessorState();
    CHECK(e->padGrid().padBaseColour(7) == ui::padColour(5).base);
    CHECK(e->padGrid().padBaseColour(0) == ui::padColour(0).base);
}

TEST_CASE("all pad colours render dimmed and lit with the latch dot", "[look][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    for (int s = 0; s < kNumPadColours; ++s)
        p.setSlotColour(s, s);
    ui::PadGrid grid(p);
    grid.setSize(360, 384);
    grid.refreshNames();
    grid.setPadStates(0u, 0xffu, -1); // alle acht gelatcht, keiner spielt
    dgtest::savePng(dgtest::snapshot(grid), "pad-colours-dimmed");
    grid.setPadStates(0xffu, 0xffu, -1); // alle acht spielen
    const auto lit = dgtest::snapshot(grid);
    dgtest::savePng(lit, "pad-colours-lit");
    CHECK(dgtest::hasVisiblePixel(lit));
}
```

Für den Editor-Test braucht `DubgefahrenEditor` den öffentlichen Zugriff `ui::PadGrid& padGrid() { return pads_; }` (Step 3); `pollProcessorState()` ist bereits öffentlich (wenn nicht: in den öffentlichen Teil verschieben).

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL beim Kompilieren – `padLitTextColour`/`padGrid` unbekannt.

- [ ] **Step 3: Implementieren**

`plugin/PluginEditor.h` im öffentlichen Teil:

```cpp
    ui::PadGrid& padGrid() { return pads_; }
```

`plugin/ui/PadGrid.h`: Kommentar an `setPadBaseColour` anpassen und ergänzen:

```cpp
    // Grundfarbe eines Pads: ruhend gedämpft, spielend hell. refreshNames() setzt sie aus der Slot-Farbe.
    void setPadBaseColour(int slot, juce::Colour colour);
    juce::Colour padBaseColour(int slot) const;
    juce::Colour padLitTextColour(int slot) const;
```

`plugin/ui/PadGrid.cpp`:

1. `#include "plugin/ui/PadColours.h"` ergänzen.
2. In der Klasse `Pad`: Member `juce::Colour litText_ { colours::padTextLit };` neben `baseColour_` und die Methoden

```cpp
    juce::Colour litTextColour() const { return litText_; }
    void setLitTextColour(juce::Colour c)
    {
        if (c == litText_)
            return;
        litText_ = c;
        repaint();
    }
```

3. In `paint()` beide Vorkommen von `colours::padTextLit` durch `litText_` ersetzen.
4. Den Latch-Block in `paint()` ersetzen durch:

```cpp
        if (latched_)
        {
            // Heller Kern mit dunklem Ring: lesbar auf jeder Pad-Farbe, gedimmt wie leuchtend.
            const juce::Rectangle<float> outer(r.getRight() - 15.0f, r.getY() + 5.0f, 10.0f, 10.0f);
            juce::Path dot;
            dot.addEllipse(outer);
            dotShadow_.render(g, dot);
            g.setColour(latchDotRing);
            g.fillEllipse(outer);
            g.setColour(latchDotCore);
            g.fillEllipse(outer.reduced(1.5f));
        }
```

5. Member `melatonin::DropShadow dotGlow_ { … }` ersetzen durch

```cpp
    melatonin::DropShadow dotShadow_ { { juce::Colours::black.withAlpha(0.6f), 4 } };
```

6. `PadGrid::refreshNames()`: in der Schleife nach `setContent(...)` ergänzen:

```cpp
        const auto& colour = padColour(proc_.slotColour(s));
        auto& pad = *pads_[static_cast<std::size_t>(s)];
        if (pad.baseColour() != colour.base)
        {
            pad.setBaseColour(colour.base);
            glowChanged(s);
        }
        pad.setLitTextColour(colour.litText);
```

7. Neue Methode:

```cpp
juce::Colour PadGrid::padLitTextColour(int slot) const { return pads_[static_cast<std::size_t>(slot)]->litTextColour(); }
```

8. Legende in `PadGrid::paint`: die Zeile `item(colours::latched, "latched", 90.0f, false);` ersetzen durch

```cpp
    {
        auto cell = legend.removeFromLeft(90.0f);
        const auto mark = cell.removeFromLeft(12.0f).withSizeKeepingCentre(9.0f, 9.0f);
        g.setColour(latchDotRing);
        g.fillEllipse(mark);
        g.setColour(latchDotCore);
        g.fillEllipse(mark.reduced(1.5f));
        g.setColour(colours::textDim);
        g.drawText("latched", cell, juce::Justification::centredLeft);
    }
```

- [ ] **Step 4: Tests ausführen**

Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[look]"`
Expected: PASS. Danach mit `$env:DG_SNAPSHOT_DIR = "<scratchpad>\snap"` den Tag `[padcolour]` erneut ausführen und `pad-colours-dimmed.png` sowie `pad-colours-lit.png` ansehen; im Report beschreiben, ob alle acht Farben und der Latch-Punkt gut erkennbar sind.

- [ ] **Step 5: Commit**

```bash
git add plugin/ui/PadGrid.cpp plugin/ui/PadGrid.h plugin/PluginEditor.h tests/plugin/test_Look.cpp
git commit -m "feat(ui): pads draw in their slot colour, latch dot readable on every colour" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Farbmenü, Zwischenablage und Wellenform

**Files:**
- Modify: `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`, `plugin/ui/WaveformView.h`, `plugin/ui/WaveformView.cpp`
- Test: `tests/plugin/test_Editor.cpp`, `tests/plugin/test_Waveform.cpp`

**Interfaces:**
- Consumes: `ui::padColour` (Task 1), `slotColour`/`setSlotColour` (Task 3).
- Produces: `static constexpr int DubgefahrenEditor::kPadColourMenuBase = 100;`, `void DubgefahrenEditor::applyPadMenuResult(int slot, int result);`, `juce::Colour WaveformView::waveColour() const`.

- [ ] **Step 1: Tests**

In `tests/plugin/test_Editor.cpp` den bestehenden Pad-Menü-Test anpassen: beide `Items { … }` bekommen am Ende den Eintrag `{ "Colour", true }` (Pad 0) bzw. `{ "Colour", false }` (Pad 1, leer).

Am Dateiende anhängen:

```cpp
TEST_CASE("the colour submenu lists eight colours and ticks the current one", "[editor][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.setSlotColour(0, 3);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    const auto menu = e->buildPadMenu(0);
    const juce::PopupMenu* sub = nullptr;
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
        if (it.getItem().text == "Colour")
            sub = it.getItem().subMenu.get();
    REQUIRE(sub != nullptr);
    std::vector<std::pair<juce::String, bool>> ticked;
    for (juce::PopupMenu::MenuItemIterator it(*sub); it.next();)
        ticked.emplace_back(it.getItem().text, it.getItem().isTicked);
    REQUIRE(ticked.size() == 8);
    CHECK(ticked[0].first == "Green");
    CHECK(ticked[3] == std::make_pair(juce::String("Violet"), true));
    CHECK_FALSE(ticked[0].second);
}

TEST_CASE("choosing a colour, copying and pasting carry the pad colour", "[editor][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    e->applyPadMenuResult(0, DubgefahrenEditor::kPadColourMenuBase + 6);
    CHECK(p.slotColour(0) == 6);

    e->applyPadMenuResult(0, 1); // Copy
    e->applyPadMenuResult(5, 2); // Paste
    CHECK(p.slotColour(5) == 6);

    // Ein leerer Slot hinterlässt beim Einfügen keine Farbe.
    p.clearSlot(1);
    p.setSlotColour(9, 4);
    e->applyPadMenuResult(1, 1); // Copy (leer)
    e->applyPadMenuResult(9, 2); // Paste
    CHECK(p.slotSource(9) == SourceType::Empty);
    CHECK(p.slotColour(9) == 0);
}
```

Die Ergebnis-IDs 1 (Copy) und 2 (Paste) entsprechen `kPadCopy`/`kPadPaste` in `PluginEditor.cpp`.

In `tests/plugin/test_Waveform.cpp` am Ende:

```cpp
TEST_CASE("the waveform is drawn in the slot colour", "[waveform][padcolour]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    REQUIRE(view.hasData());
    CHECK(view.waveColour() == ui::padColour(0).base);
    f.p.setSlotColour(2, 2);
    view.refresh();
    CHECK(view.waveColour() == ui::padColour(2).base);
    const auto img = dgtest::snapshot(view);
    int hits = 0;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt(x, y) == ui::padColour(2).base)
                ++hits;
    CHECK(hits > 0);
}
```

Oben `#include "plugin/ui/PadColours.h"` ergänzen.

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL beim Kompilieren – `applyPadMenuResult`, `kPadColourMenuBase`, `waveColour` unbekannt.

- [ ] **Step 3: Implementieren**

`plugin/PluginEditor.h` im öffentlichen Teil:

```cpp
    static constexpr int kPadColourMenuBase = 100; // + Palettenindex
    void applyPadMenuResult(int slot, int result);
```

`ClipboardSlot` um `int colour = 0;` ergänzen.

`plugin/PluginEditor.cpp`:

`#include "plugin/ui/PadColours.h"` ergänzen.

`buildPadMenu` vor `return menu;`:

```cpp
    juce::PopupMenu colourMenu;
    const int current = proc_.slotColour(slot);
    for (int i = 0; i < kNumPadColours; ++i)
    {
        const auto& c = ui::padColour(i);
        auto swatch = std::make_unique<juce::DrawableRectangle>();
        swatch->setRectangle(juce::Parallelogram<float>(juce::Rectangle<float>(0.0f, 0.0f, 12.0f, 12.0f)));
        swatch->setFill(c.base);
        juce::PopupMenu::Item item(c.name);
        item.setID(kPadColourMenuBase + i).setTicked(current == i).setImage(std::move(swatch));
        colourMenu.addItem(std::move(item));
    }
    menu.addSubMenu("Colour", colourMenu, !empty);
```

`showPadMenu`: die Lambda auf den Aufruf der neuen Methode reduzieren:

```cpp
void DubgefahrenEditor::showPadMenu(int slot)
{
    styled(buildPadMenu(slot)).showMenuAsync(juce::PopupMenu::Options(),
                                     [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot](int result) {
                                         if (safe != nullptr)
                                             safe->applyPadMenuResult(slot, result);
                                     });
}

void DubgefahrenEditor::applyPadMenuResult(int slot, int result)
{
    if (result >= kPadColourMenuBase && result < kPadColourMenuBase + kNumPadColours)
    {
        proc_.setSlotColour(slot, result - kPadColourMenuBase);
        refreshAll();
        return;
    }
    switch (result)
    {
        case kPadCopy:
            clipboard_ = ClipboardSlot { proc_.currentKit().slots[static_cast<std::size_t>(slot)], proc_.slotName(slot),
                                         proc_.slotSample(slot), proc_.slotColour(slot) };
            break;
        case kPadPaste:
            if (clipboard_)
            {
                proc_.setSlot(slot, clipboard_->params, clipboard_->name, clipboard_->sample);
                proc_.setSlotColour(slot, hasSound(clipboard_->params.source) ? clipboard_->colour : 0);
            }
            break;
        case kPadReset:  proc_.resetSlotToFactory(slot); break;
        case kPadRename: renameSlot(slot); break;
        case kPadClear:  proc_.clearSlot(slot); break;
        default: break;
    }
    refreshAll();
}
```

(Die bisherigen `case`-Zweige aus der Lambda wandern damit in `applyPadMenuResult`; `self.` entfällt.)

`plugin/ui/WaveformView.h` im öffentlichen Teil:

```cpp
    // Farbe der Wellenform: die Pad-Farbe des angezeigten Slots.
    juce::Colour waveColour() const;
```

`plugin/ui/WaveformView.cpp`: `#include "plugin/ui/PadColours.h"` ergänzen,

```cpp
juce::Colour WaveformView::waveColour() const { return padColour(slot_ >= 0 ? proc_.slotColour(slot_) : 0).base; }
```

und in `paint` `g.setColour(colours::padBase);` (vor der Peak-Schleife) durch `g.setColour(waveColour());` ersetzen.

- [ ] **Step 4: Tests ausführen**

Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[editor]"` und `"[waveform]"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/PluginEditor.h plugin/PluginEditor.cpp plugin/ui/WaveformView.h plugin/ui/WaveformView.cpp tests/plugin/test_Editor.cpp tests/plugin/test_Waveform.cpp
git commit -m "feat(ui): colour submenu in the pad menu, waveform in the slot colour" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Doku, volle Suite, Validierung, Zip

**Files:**
- Modify: `README.md`

- [ ] **Step 1: README ergänzen**

Im Abschnitt, der Pads und das Pad-Kontextmenü beschreibt (Suche nach „Clear Slot“ oder „Rename“), einen Absatz ergänzen:

```markdown
Pads with a sound source can be given one of eight colours (Green, Turquoise, Blue, Violet, Pink, Red, Orange, Yellow) via right-click → Colour, e.g. to mark groups of sounds. The colour shows dimmed at rest and bright while the pad plays, and the sample waveform uses it too. Copy/Paste carries the colour; Clear Slot and Reset to Factory Default set it back to green. Colours are saved in the project and in kits (kit format version 6; older kits load all green). A latched pad shows a light dot with a dark ring in its top right corner.
```

In der Abnahme-Checkliste ergänzen:

```markdown
- [ ] Pad colours: all eight colours are easy to tell apart at rest and while playing; the latch dot is clearly visible on every colour, also in poor light.
```

Falls die README das Kit-Format mit Versionsnummer nennt: auf Version 6 und den Schlüssel `"colour"` aktualisieren.

- [ ] **Step 2: Volle Suite und Validierung**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: `100% tests passed`.

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
Expected: Steinberg-Validator `0 tests failed`, pluginval `SUCCESS`.

- [ ] **Step 3: Commit und Zip**

```bash
git add README.md
git commit -m "docs: describe pad colours and the new latch dot" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Danach das Zip laut Global Constraints erzeugen und den Pfad im Report nennen.
