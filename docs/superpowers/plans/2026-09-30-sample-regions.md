# Sample-Bereiche: Start/End, Loop, Reverse – Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ein Sample-Slot bekommt Start-, Loop-Start- und End-Marker in einer Wellenform-Anzeige, dazu Loop mit einstellbarem Crossfade, Reverse und Latch für geloopte Samples (Issue #11).

**Architecture:**
- **engine (JUCE-frei):** sechs neue Slot-Felder über die Feld-Tabelle; `SampleRegion.h` löst die Marker in gültige Positionen auf; der `SamplePlayer` lernt Richtung, Bereichsgrenzen, Rücksprung und einen aus der Position berechneten Crossfade; Engine und Router lassen Latch für geloopte Sample-Slots zu.
- **plugin:** Kit-Format Version 4; der Processor gibt dem Editor die Sample-Daten eines Slots heraus und setzt die Bereichsfelder bei einem neuen Sample zurück.
- **UI:** `WaveformView` zeichnet die Wellenform und drei ziehbare Marker; der Slot-Editor bekommt im Sample-Modus eine vierte Reihe.

**Tech Stack:** C++20, JUCE 8.0.15, melatonin_blur v1.4, Catch2 v3, MSVC x64, CMake.

**Spec:** `docs/superpowers/specs/2026-09-30-sample-regions-design.md`

## Global Constraints

- `engine/` darf **keine** JUCE-Header einbinden.
- Im Audio-Thread: keine Allokation, keine Locks, kein Datei-I/O.
- Neue Slot-Felder in dieser Reihenfolge **hinter** `Tune`, `ParameterID`-Versionshinweis **4**:

  | `SlotField` | Schlüssel | Anzeigename | Art | min | max | Standard | Einheit | `SlotParams`-Feld |
  |---|---|---|---|---|---|---|---|---|
  | `SampleStart` | `smpStart` | `Sample Start` | Float | 0 | 1 | 0 | – | `float sampleStart` |
  | `LoopStart` | `loopStart` | `Loop Start` | Float | 0 | 1 | 0 | – | `float loopStart` |
  | `SampleEnd` | `smpEnd` | `Sample End` | Float | 0 | 1 | 1 | – | `float sampleEnd` |
  | `Loop` | `loop` | `Loop` | Bool | 0 | 1 | 0 | – | `bool loop` |
  | `Reverse` | `reverse` | `Reverse` | Bool | 0 | 1 | 0 | – | `bool reverse` |
  | `LoopXfade` | `xfade` | `Loop X-Fade` | Float | 0 | 50 | 5 | `%` | `float loopXfadePct` |

- Bestehende Felder, ihre Reihenfolge und ihre Versionshinweise ändern sich nicht. `SourceType`-Reihenfolge nie ändern.
- Mindestlänge eines Bereichs: `kMinRegionSamples = 16` (bzw. die Dateilänge, wenn sie kürzer ist).
- Gelooptes Segment: vorwärts `loopStart … end`, Reverse `start … loopStart`; ist es kürzer als die Mindestlänge, kreist der ganze Bereich.
- „Loop aktiv“ für die Stimme heißt: Feld `loop` an **und** `trigMode != OneShot`.
- Mit den Standardwerten ist die Ausgabe eines Sample-Slots unverändert; alle bestehenden Tests bleiben ohne Anpassung grün, ausgenommen die zwei, die eine Anzahl oder Versionsnummer prüfen (`there are 19 slot fields …`, `sample slots survive a version 3 round-trip`).
- Kit-Format **Version 4**; Kits der Versionen 1 bis 3 laden unverändert.
- Der Mausklick auf ein Pad bleibt eine Gate-Vorschau; `PadRouter::previewOn` wird nicht geändert.
- Layout außerhalb des Sample-Modus des Slot-Editors bleibt unverändert.
- UI im Look von #15: Schrift nur über `ui::font(...)`; melatonin-Schatten als Member mit doppelten Klammern; Farben aus `colours` in `plugin/ui/DgLookAndFeel.h`.
- Quelltexte UTF-8; Nicht-ASCII-Strings an JUCE über `ui::u8(...)`. Kommentare Deutsch, UI-Texte Englisch.
- Befehle aus dem Repo-Root:
  - Build: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
  - Neu konfigurieren (nach CMake-Änderungen): `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
  - Engine-Tests: `build\tests\Release\DubgefahrenTests.exe "<tag>"`
  - Plugin-Tests: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "<tag>"`
  - Alles: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
  - Validator: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
- Bilder zur Sichtprüfung: Ist `DG_SNAPSHOT_DIR` gesetzt, schreiben die Render-Tests PNG-Dateien dorthin (`dgtest::savePng`).
- Commits enden mit `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

- **Automation schiebt die Marker in eine ungültige Reihenfolge oder unter der laufenden Stimme weg.** Erwartet: kein Absturz, kein NaN, es bleibt mindestens die Mindestlänge spielbar, und es knackt nicht hart. → Tests in Task 1 („invalid marker order …“) und Task 3 („moving the end marker behind the position …“).
- **Sehr kurze Schleife bei hoher Tonhöhe: der Schritt pro Sample ist größer als die Schleife.** Erwartet: kein Hängen, kein NaN, die Stimme bleibt im Segment. → Test in Task 3 („a loop shorter than one step …“).
- **Die Sample-Daten eines gelatchten, geloopten Slots werden getauscht oder verschwinden.** Erwartet: Die Stimme verstummt, der Latch-Zustand erlischt, nichts liest die alten Daten. → Test in Task 4 („changing the sample data stops a latched looped slot …“).
- **Ein Kit der Version 3 oder ein Host-Projekt aus 0.4.0 wird geladen.** Erwartet: Es klingt wie vorher; die neuen Felder stehen auf ihren Standardwerten. → Tests in Task 5 („version 3 kits load with default region fields“, „a host state without the region parameters …“).
- **Wellenform ohne Daten, mit einem einzigen Sample, mit Breite 0, oder der Slot wechselt mitten im Ziehen.** Erwartet: kein Absturz, kein Parameter wird geschrieben, eine offene Host-Geste wird beendet. → Tests in Task 6 („peaks for empty, tiny and long data“, „without data nothing can be dragged“, „switching the slot ends an open drag“).

---

## Dateistruktur

| Datei | Verantwortung |
|---|---|
| `engine/SlotParams.h`, `engine/SlotFields.h/.cpp` | sechs neue Felder, `resetSampleRegionFields` |
| `engine/SampleRegion.h` (neu) | `resolveSampleRegion`, `resolveLoopSegment` |
| `engine/SamplePlayer.h/.cpp` | Richtung, Bereich, Schleife, X-Fade, Live-Änderungen |
| `engine/Engine.cpp`, `engine/PadRouter.cpp` | Latch für geloopte Samples, Latch erlischt mit der Stimme |
| `plugin/KitFile.cpp` | Format Version 4 |
| `plugin/PluginProcessor.h/.cpp` | `slotSampleData`, Zurücksetzen bei neuem Sample |
| `plugin/ui/WaveformView.h/.cpp` (neu) | Spitzenwerte, Zeichnung, Marker |
| `plugin/ui/SlotEditor.h/.cpp` | Sample-Modus mit vier Reihen |
| `plugin/PluginEditor.h` | Zugriff auf den Slot-Editor für Tests |
| `engine/CMakeLists.txt`, `plugin/CMakeLists.txt`, `tests/CMakeLists.txt` | neue Dateien |
| `tests/test_SampleRegion.cpp`, `tests/plugin/test_Waveform.cpp` (neu) | Tests |

---

### Task 1: Datenmodell und Bereichslogik

**Files:**
- Create: `engine/SampleRegion.h`, `tests/test_SampleRegion.cpp`
- Modify: `engine/SlotParams.h`, `engine/SlotFields.h`, `engine/SlotFields.cpp`, `engine/CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/test_SlotFields.cpp`, `tests/test_SampleRegion.cpp`

**Interfaces:**
- Produces:
  - `SlotParams::sampleStart`, `loopStart`, `sampleEnd` (float), `loop`, `reverse` (bool), `loopXfadePct` (float)
  - `SlotField::SampleStart`, `LoopStart`, `SampleEnd`, `Loop`, `Reverse`, `LoopXfade`
  - `void resetSampleRegionFields(SlotParams& p)` in `engine/SlotFields.h`
  - `struct SampleRegion { double start, loopStart, end; }`
  - `struct LoopSegment { double lo, hi; }`
  - `inline constexpr double kMinRegionSamples = 16.0`
  - `SampleRegion resolveSampleRegion(float start01, float loopStart01, float end01, std::size_t numSamples)`
  - `LoopSegment resolveLoopSegment(const SampleRegion& r, bool reverse, std::size_t numSamples)`

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

In `tests/test_SlotFields.cpp` den Testfall „there are 19 slot fields with unique keys“ umbenennen in „there are 25 slot fields with unique keys“ und beide `19` durch `25` ersetzen. Am Dateiende anhängen:

```cpp
TEST_CASE("the sample region fields default to the whole file without loop", "[fields]")
{
    const SlotParams p = makeDefaultSlotParams();
    CHECK(p.sampleStart == 0.0f);
    CHECK(p.loopStart == 0.0f);
    CHECK(p.sampleEnd == 1.0f);
    CHECK_FALSE(p.loop);
    CHECK_FALSE(p.reverse);
    CHECK(p.loopXfadePct == 5.0f);
    CHECK(SlotParams {} == p); // die Standardwerte der Struktur stimmen mit der Tabelle überein

    for (const auto f : { SlotField::SampleStart, SlotField::LoopStart, SlotField::SampleEnd, SlotField::Loop, SlotField::Reverse,
                          SlotField::LoopXfade })
        CHECK(fieldSpec(f).versionHint == 4);
    CHECK(std::string(fieldSpec(SlotField::SampleStart).key) == "smpStart");
    CHECK(std::string(fieldSpec(SlotField::LoopStart).key) == "loopStart");
    CHECK(std::string(fieldSpec(SlotField::SampleEnd).key) == "smpEnd");
    CHECK(std::string(fieldSpec(SlotField::Loop).key) == "loop");
    CHECK(std::string(fieldSpec(SlotField::Reverse).key) == "reverse");
    CHECK(std::string(fieldSpec(SlotField::LoopXfade).key) == "xfade");
    CHECK(fieldSpec(SlotField::LoopXfade).max == 50.0f);
    CHECK(fieldSpec(SlotField::Tune).versionHint == 3); // bestehende Hinweise bleiben
}

TEST_CASE("resetSampleRegionFields restores only the six region fields", "[fields]")
{
    SlotParams p = makeDefaultSlotParams();
    p.sampleStart = 0.3f;
    p.loopStart = 0.4f;
    p.sampleEnd = 0.6f;
    p.loop = true;
    p.reverse = true;
    p.loopXfadePct = 20.0f;
    p.tuneSemis = 7.0f;
    p.volumeDb = -12.0f;
    resetSampleRegionFields(p);
    SlotParams expected = makeDefaultSlotParams();
    expected.tuneSemis = 7.0f;
    expected.volumeDb = -12.0f;
    CHECK(p == expected);
}
```

Falls `tests/test_SlotFields.cpp` `<string>` noch nicht einbindet, `#include <string>` ergänzen.

`tests/test_SampleRegion.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include "engine/SampleRegion.h"

using namespace dg;

TEST_CASE("default markers resolve to the whole file", "[region]")
{
    const auto r = resolveSampleRegion(0.0f, 0.0f, 1.0f, 4800);
    CHECK(r.start == 0.0);
    CHECK(r.loopStart == 0.0);
    CHECK(r.end == 4800.0);
}

TEST_CASE("markers resolve to positions in samples", "[region]")
{
    const auto r = resolveSampleRegion(0.25f, 0.5f, 0.75f, 4800);
    CHECK(r.start == 1200.0);
    CHECK(r.loopStart == 2400.0);
    CHECK(r.end == 3600.0);
}

TEST_CASE("invalid marker order still leaves the minimum region", "[region]")
{
    // End vor Start: End gewinnt, Start rückt auf die Mindestlänge davor.
    // (Anteile mit exakter Binärdarstellung, damit die Positionen ganzzahlig sind.)
    auto r = resolveSampleRegion(0.875f, 0.5f, 0.125f, 4800);
    CHECK(r.end == 600.0);
    CHECK(r.start == 584.0);
    CHECK(r.loopStart == 600.0); // in den Bereich geklemmt
    CHECK(r.end - r.start == kMinRegionSamples);

    // Start = End
    r = resolveSampleRegion(0.5f, 0.5f, 0.5f, 4800);
    CHECK(r.end == 2400.0);
    CHECK(r.start == 2384.0);
    CHECK(r.loopStart == 2400.0);

    // End ganz links
    r = resolveSampleRegion(0.0f, 0.0f, 0.0f, 4800);
    CHECK(r.start == 0.0);
    CHECK(r.end == 16.0);

    // Loop-Start außerhalb
    r = resolveSampleRegion(0.25f, 0.0f, 0.5f, 4800);
    CHECK(r.loopStart == 1200.0);
    r = resolveSampleRegion(0.25f, 1.0f, 0.5f, 4800);
    CHECK(r.loopStart == 2400.0);
}

TEST_CASE("out-of-range and non-finite markers are made safe", "[region]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    auto r = resolveSampleRegion(-3.0f, 7.0f, 9.0f, 4800);
    CHECK(r.start == 0.0);
    CHECK(r.loopStart == 4800.0);
    CHECK(r.end == 4800.0);
    r = resolveSampleRegion(nan, nan, nan, 4800); // Standardwerte
    CHECK(r.start == 0.0);
    CHECK(r.loopStart == 0.0);
    CHECK(r.end == 4800.0);
    r = resolveSampleRegion(inf, -inf, inf, 4800);
    CHECK(std::isfinite(r.start));
    CHECK(std::isfinite(r.loopStart));
    CHECK(std::isfinite(r.end));
    CHECK(r.start <= r.loopStart);
    CHECK(r.loopStart <= r.end);
}

TEST_CASE("files shorter than the minimum region use their whole length", "[region]")
{
    auto r = resolveSampleRegion(0.5f, 0.5f, 0.5f, 1);
    CHECK(r.start == 0.0);
    CHECK(r.end == 1.0);
    r = resolveSampleRegion(0.875f, 0.0f, 0.125f, 8);
    CHECK(r.start == 0.0);
    CHECK(r.end == 8.0);
    r = resolveSampleRegion(0.0f, 0.0f, 1.0f, 0);
    CHECK(r.start == 0.0);
    CHECK(r.end == 0.0);
}

TEST_CASE("the loop segment depends on the direction and falls back to the whole region", "[region]")
{
    const auto r = resolveSampleRegion(0.25f, 0.5f, 0.75f, 4800);
    auto s = resolveLoopSegment(r, false, 4800);
    CHECK(s.lo == 2400.0);
    CHECK(s.hi == 3600.0);
    s = resolveLoopSegment(r, true, 4800);
    CHECK(s.lo == 1200.0);
    CHECK(s.hi == 2400.0);

    // Loop-Start = End: vorwärts wäre das Segment leer, also der ganze Bereich.
    const auto atEnd = resolveSampleRegion(0.25f, 0.75f, 0.75f, 4800);
    s = resolveLoopSegment(atEnd, false, 4800);
    CHECK(s.lo == 1200.0);
    CHECK(s.hi == 3600.0);

    // Loop-Start = Start: rückwärts wäre das Segment leer, also der ganze Bereich.
    const auto atStart = resolveSampleRegion(0.25f, 0.25f, 0.75f, 4800);
    s = resolveLoopSegment(atStart, true, 4800);
    CHECK(s.lo == 1200.0);
    CHECK(s.hi == 3600.0);

    // Knapp unter der Mindestlänge zählt als leer.
    const SampleRegion tight { 1200.0, 3590.0, 3600.0 };
    s = resolveLoopSegment(tight, false, 4800);
    CHECK(s.lo == 1200.0);
}
```

In `tests/CMakeLists.txt` in der Quellenliste von `DubgefahrenTests` nach `test_SamplePlayer.cpp` die Zeile `test_SampleRegion.cpp` ergänzen.

- [ ] **Step 2: Build ausführen und das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL, `engine/SampleRegion.h` nicht gefunden und `sampleStart` kein Member von `SlotParams`.

- [ ] **Step 3: Felder ergänzen**

`engine/SlotParams.h`, in `SlotParams` nach `tuneSemis`:

```cpp
    // Nur für Samples: Bereich als Anteil der Sample-Länge, Schleife, Richtung.
    float sampleStart = 0.0f;      // 0 .. 1
    float loopStart = 0.0f;        // 0 .. 1
    float sampleEnd = 1.0f;        // 0 .. 1
    bool loop = false;
    bool reverse = false;
    float loopXfadePct = 5.0f;     // 0 .. 50 % der Schleifenlänge
```

`engine/SlotFields.h`: im Enum `SlotField` nach `Tune,` einfügen:

```cpp
    SampleStart, LoopStart, SampleEnd, Loop, Reverse, LoopXfade,
```

und nach `std::optional<SlotField> slotFieldFromKey(...)` ergänzen:

```cpp
// Setzt die sechs Bereichsfelder eines Sample-Slots auf ihre Standardwerte (ganze Datei, kein Loop).
void resetSampleRegionFields(SlotParams& p);
```

`engine/SlotFields.cpp`: in `kSpecs` nach der `tune`-Zeile:

```cpp
    { "smpStart",   "Sample Start",   FieldKind::Float, 0.0f, 1.0f, 0.0f, 0.0f, "", {}, 4 },
    { "loopStart",  "Loop Start",     FieldKind::Float, 0.0f, 1.0f, 0.0f, 0.0f, "", {}, 4 },
    { "smpEnd",     "Sample End",     FieldKind::Float, 0.0f, 1.0f, 1.0f, 0.0f, "", {}, 4 },
    { "loop",       "Loop",           FieldKind::Bool, 0.0f, 1.0f, 0.0f, 0.0f, "", {}, 4 },
    { "reverse",    "Reverse",        FieldKind::Bool, 0.0f, 1.0f, 0.0f, 0.0f, "", {}, 4 },
    { "xfade",      "Loop X-Fade",    FieldKind::Float, 0.0f, 50.0f, 5.0f, 0.0f, "%", {}, 4 },
```

In `getSlotField` vor `case SlotField::Count:`:

```cpp
        case SlotField::SampleStart:   return p.sampleStart;
        case SlotField::LoopStart:     return p.loopStart;
        case SlotField::SampleEnd:     return p.sampleEnd;
        case SlotField::Loop:          return p.loop ? 1.0f : 0.0f;
        case SlotField::Reverse:       return p.reverse ? 1.0f : 0.0f;
        case SlotField::LoopXfade:     return p.loopXfadePct;
```

In `setSlotField` vor `case SlotField::Count:`:

```cpp
        case SlotField::SampleStart:   p.sampleStart = value; break;
        case SlotField::LoopStart:     p.loopStart = value; break;
        case SlotField::SampleEnd:     p.sampleEnd = value; break;
        case SlotField::Loop:          p.loop = i != 0; break;
        case SlotField::Reverse:       p.reverse = i != 0; break;
        case SlotField::LoopXfade:     p.loopXfadePct = value; break;
```

Nach `slotFieldFromKey`:

```cpp
void resetSampleRegionFields(SlotParams& p)
{
    for (const auto f : { SlotField::SampleStart, SlotField::LoopStart, SlotField::SampleEnd, SlotField::Loop, SlotField::Reverse,
                          SlotField::LoopXfade })
        setSlotField(p, f, fieldSpec(f).def);
}
```

- [ ] **Step 4: `engine/SampleRegion.h` schreiben**

```cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace dg {

// Aufgelöster Bereich eines Samples in Positionen (Samples der Datei).
struct SampleRegion
{
    double start = 0.0;     // erste gespielte Position
    double loopStart = 0.0; // Marker L
    double end = 0.0;       // Position hinter dem letzten gespielten Sample
};

// Gelooptes Segment [lo, hi).
struct LoopSegment
{
    double lo = 0.0;
    double hi = 0.0;
};

inline constexpr double kMinRegionSamples = 16.0;

// Rechnet die drei Anteile (0..1) in gültige Positionen um. Es gilt danach immer
// 0 <= start <= loopStart <= end <= n und end - start >= min(kMinRegionSamples, n).
// Nicht endliche Anteile gelten als ihr Standard (0, 0, 1).
inline SampleRegion resolveSampleRegion(float start01, float loopStart01, float end01, std::size_t numSamples)
{
    const double n = static_cast<double>(numSamples);
    const double m = std::min(kMinRegionSamples, n);
    const auto frac = [](float v, double def) { return std::isfinite(v) ? std::clamp(static_cast<double>(v), 0.0, 1.0) : def; };

    SampleRegion r;
    r.end = std::min(n, std::max(frac(end01, 1.0) * n, m));
    r.start = std::min(frac(start01, 0.0) * n, r.end - m);
    r.loopStart = std::clamp(frac(loopStart01, 0.0) * n, r.start, r.end);
    return r;
}

// Vorwärts kreist loopStart..end, rückwärts start..loopStart. Ist das Segment kürzer als die
// Mindestlänge, kreist der ganze Bereich.
inline LoopSegment resolveLoopSegment(const SampleRegion& r, bool reverse, std::size_t numSamples)
{
    const double m = std::min(kMinRegionSamples, static_cast<double>(numSamples));
    LoopSegment s = reverse ? LoopSegment { r.start, r.loopStart } : LoopSegment { r.loopStart, r.end };
    if (s.hi - s.lo < m)
        s = { r.start, r.end };
    return s;
}

} // namespace dg
```

`engine/CMakeLists.txt`: in der Zeile mit `SamplePlayer.h SamplePlayer.cpp` davor `SampleRegion.h` ergänzen:

```cmake
    SoundSource.h SampleData.h SampleRegion.h SirenVoice.h SirenVoice.cpp SamplePlayer.h SamplePlayer.cpp
```

- [ ] **Step 5: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\Release\DubgefahrenTests.exe "[fields],[region]"`
Expected: PASS.

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS (die Kit-Version ändert sich erst in Task 5). Schlägt ein Plugin-Test fehl, weil er eine feste Anzahl von Parametern oder Feldern erwartet, die Zahl dort um 6 Felder pro Slot (96 Parameter) anpassen und das im Commit erwähnen.

- [ ] **Step 6: Commit**

```bash
git add engine/SlotParams.h engine/SlotFields.h engine/SlotFields.cpp engine/SampleRegion.h engine/CMakeLists.txt tests/CMakeLists.txt tests/test_SlotFields.cpp tests/test_SampleRegion.cpp
git commit -m "feat(engine): sample region fields and marker resolution (#11)"
```

---

### Task 2: SamplePlayer – Start, End und Reverse

**Files:**
- Modify: `engine/SamplePlayer.h`, `engine/SamplePlayer.cpp`
- Test: `tests/test_SamplePlayer.cpp`

**Interfaces:**
- Consumes: `resolveSampleRegion`, `SampleRegion` (Task 1); `SlotParams::sampleStart`, `sampleEnd`, `reverse`.
- Produces: `SamplePlayer` spielt den Bereich `start … end` vorwärts oder rückwärts. Neue private Member `dir_`, `fadeDir_`, `overrun_` (Task 3 baut darauf auf).

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

In `tests/test_SamplePlayer.cpp` im anonymen Namespace ergänzen:

```cpp
constexpr float kRampUnit = 1.0e-4f;

// samples[i] = i * kRampUnit: Der Ausgabewert verrät die gelesene Position.
SampleData rampData(int numSamples, double sampleRate = kSr)
{
    SampleData d;
    d.sampleRate = sampleRate;
    d.samples.resize(static_cast<std::size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        d.samples[static_cast<std::size_t>(i)] = static_cast<float>(i) * kRampUnit;
    return d;
}

float rampAt(double pos) { return static_cast<float>(pos) * kRampUnit; }
```

Am Dateiende anhängen:

```cpp
TEST_CASE("start and end markers limit the playback", "[sampler]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.sampleStart = 0.25f; // 1200
    p.sampleEnd = 0.5f;    // 2400
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 2000);
    CHECK_THAT(out[200], WithinAbs(rampAt(1400), 1e-3));
    CHECK_THAT(out[1000], WithinAbs(rampAt(2200), 1e-3));
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 1202) == 0.0f);
    CHECK(out[1199] < out[1150]); // 2-ms-Rampe vor dem End-Marker
}

TEST_CASE("reverse plays the region backwards", "[sampler]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.sampleStart = 0.25f;
    p.sampleEnd = 0.5f;
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 2000);
    CHECK_THAT(out[200], WithinAbs(rampAt(2399 - 200), 1e-3));
    CHECK_THAT(out[1000], WithinAbs(rampAt(2399 - 1000), 1e-3));
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 1202) == 0.0f);
    CHECK(dgtest::allFinite(out));
}

TEST_CASE("reverse of the whole file ends at its first sample", "[sampler]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 6000);
    CHECK_THAT(out[100], WithinAbs(rampAt(4799 - 100), 1e-3));
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 4802) == 0.0f);
    CHECK(dgtest::allFinite(out));
}

TEST_CASE("tune applies in reverse as well", "[sampler]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.sampleStart = 0.25f;
    p.sampleEnd = 0.5f;
    p.reverse = true;
    p.tuneSemis = 12.0f; // doppelte Geschwindigkeit
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 1000);
    CHECK_THAT(out[200], WithinAbs(rampAt(2399 - 400), 2e-3));
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 602) == 0.0f);
}

TEST_CASE("crossed markers still play the minimum region", "[sampler]")
{
    const auto d = rampData(4800);
    for (const bool reverse : { false, true })
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = sampleParams();
        p.sampleStart = 0.9f;
        p.sampleEnd = 0.1f; // Bereich 464..480
        p.reverse = reverse;
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        REQUIRE(v.isActive());
        const auto out = renderN(v, ctx, 200);
        CHECK_FALSE(v.isActive());
        CHECK(dgtest::allFinite(out));
        CHECK(dgtest::peakAbs(out, 0, 16) > 0.0f);
        CHECK(dgtest::peakAbs(out, 18) == 0.0f);
    }
}

TEST_CASE("retrigger in reverse crossfades without a jump", "[sampler]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    auto out = renderN(v, ctx, 4700); // kurz vor dem Dateianfang
    v.start(ctx, 2);
    const auto after = renderN(v, ctx, 600);
    out.insert(out.end(), after.begin(), after.end());
    CHECK(dgtest::allFinite(out));
    CHECK(maxStep(out, 4600, 5300) < 0.005f);
    CHECK(v.isActive());
}
```

- [ ] **Step 2: Build und Tests ausführen, das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\Release\DubgefahrenTests.exe "[sampler]"`
Expected: FAIL in den sechs neuen Testfällen (der Player ignoriert Marker und Reverse). Die bestehenden `[sampler]`-Tests bestehen.

- [ ] **Step 3: `SamplePlayer.h` erweitern**

Die privaten Member ersetzen durch:

```cpp
    double sampleRate_ = 44100.0;
    Envelope env_;
    double pos_ = 0.0;
    double dir_ = 1.0; // +1 vorwärts, -1 rückwärts
    // Zweiter Lesekopf: blendet beim Retrigger und bei einem Sprung die alte Position aus.
    double fadePos_ = 0.0;
    double fadeDir_ = 1.0;
    float fadeGain_ = 0.0f;
    // Der Bereich wurde unter der Stimme weggezogen: Sie blendet per Kill-Fade aus und
    // beachtet das Bereichsende nicht mehr.
    bool overrun_ = false;
    float perfCoeff_ = 1.0f;
    float smPitch_ = 0.0f;
```

- [ ] **Step 4: `SamplePlayer.cpp` umbauen**

Includes ergänzen: `#include <cmath>` und `#include "engine/SampleRegion.h"`.

`cubicAt` rundet für negative Positionen falsch (Abschneiden statt Abrunden). Die zwei Zeilen

```cpp
    const auto i = static_cast<std::int64_t>(pos);
    const float t = static_cast<float>(pos - static_cast<double>(i));
```

ersetzen durch

```cpp
    const double fl = std::floor(pos);
    const auto i = static_cast<std::int64_t>(fl);
    const float t = static_cast<float>(pos - fl);
```

`prepare`: nach `fadeGain_ = 0.0f;` ergänzen:

```cpp
    dir_ = 1.0;
    fadeDir_ = 1.0;
    overrun_ = false;
```

`start` ersetzen:

```cpp
void SamplePlayer::start(const VoiceContext& ctx, std::uint32_t)
{
    if (!usable(ctx.sample))
        return;
    const SlotParams& p = *ctx.params;
    const auto region = resolveSampleRegion(p.sampleStart, p.loopStart, p.sampleEnd, ctx.sample->samples.size());
    // Retrigger: alte Position als auslaufenden Lesekopf weiterlaufen lassen.
    if (env_.isActive())
    {
        fadePos_ = pos_;
        fadeDir_ = dir_;
        fadeGain_ = 1.0f;
    }
    else
        fadeGain_ = 0.0f;
    dir_ = p.reverse ? -1.0 : 1.0;
    pos_ = p.reverse ? std::max(region.start, region.end - 1.0) : region.start;
    overrun_ = false;
    smPitch_ = ctx.perf.pitchSemis;
    env_.noteOn(p.attackS);
}
```

`stop` ersetzen:

```cpp
void SamplePlayer::stop()
{
    env_.prepare(sampleRate_);
    fadeGain_ = 0.0f;
    overrun_ = false;
}
```

`render` ersetzen (ohne Schleife; die kommt in Task 3):

```cpp
void SamplePlayer::render(float* out, int numSamples, const VoiceContext& ctx)
{
    const SampleData* d = ctx.sample;
    if (!env_.isActive() || !usable(d))
    {
        if (env_.isActive())
            stop(); // Daten weg: sofort still, nie auf alte Daten zugreifen
        std::fill(out, out + numSamples, 0.0f);
        return;
    }

    const SlotParams& p = *ctx.params;
    const auto& x = d->samples;
    const SampleRegion region = resolveSampleRegion(p.sampleStart, p.loopStart, p.sampleEnd, x.size());
    dir_ = p.reverse ? -1.0 : 1.0; // wirkt sofort, auch während des Spielens

    const double baseStep = d->sampleRate / sampleRate_;
    const double endRampLen = kEndRampS * sampleRate_;
    const float fadeDec = static_cast<float>(1.0 / (kRetrigFadeS * sampleRate_));
    for (int i = 0; i < numSamples; ++i)
    {
        if (!env_.isActive())
        {
            stop();
            std::fill(out + i, out + numSamples, 0.0f);
            return;
        }
        smPitch_ += perfCoeff_ * (ctx.perf.pitchSemis - smPitch_);
        const double step = baseStep * semitonesToRatio(p.tuneSemis + smPitch_);

        // Ende des Bereichs in Laufrichtung erreicht?
        const bool past = dir_ > 0.0 ? pos_ >= region.end : pos_ < region.start;
        if (past && !overrun_)
        {
            stop();
            std::fill(out + i, out + numSamples, 0.0f);
            return;
        }

        // Am Bereichsende linear auf 0 ausblenden (Restlänge in Ausgabesamples / Rampenlänge).
        const auto endGain = [&](double pos, double dir) {
            if (overrun_)
                return 1.0f;
            const double remaining = (dir > 0.0 ? region.end - pos : pos - region.start + 1.0) / step;
            return remaining < endRampLen ? static_cast<float>(std::max(0.0, remaining) / endRampLen) : 1.0f;
        };

        // Beim Retrigger Überblendung: neuer Kopf blendet ein, alter aus (Summe der Gewichte = 1).
        float y = cubicAt(x, pos_) * endGain(pos_, dir_) * (1.0f - fadeGain_);
        pos_ += dir_ * step;
        if (fadeGain_ > 0.0f)
        {
            // Außerhalb der Datei liest der alte Kopf Nullen; sein Gewicht läuft normal aus,
            // damit das Einblenden des neuen Kopfes stetig bleibt.
            y += cubicAt(x, fadePos_) * endGain(fadePos_, fadeDir_) * fadeGain_;
            fadePos_ += fadeDir_ * step;
            fadeGain_ = std::max(0.0f, fadeGain_ - fadeDec);
        }
        out[i] = y * env_.process();
    }
}
```

- [ ] **Step 5: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\Release\DubgefahrenTests.exe "[sampler]"`
Expected: PASS, alle bestehenden und die sechs neuen Testfälle.

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

- [ ] **Step 6: Commit**

```bash
git add engine/SamplePlayer.h engine/SamplePlayer.cpp tests/test_SamplePlayer.cpp
git commit -m "feat(engine): sample start/end markers and reverse playback (#11)"
```

---

### Task 3: SamplePlayer – Loop, X-Fade und Änderungen während des Spielens

**Files:**
- Modify: `engine/SamplePlayer.cpp`
- Test: `tests/test_SamplePlayer.cpp`

**Interfaces:**
- Consumes: `resolveLoopSegment`, `LoopSegment` (Task 1); `dir_`, `fadeDir_`, `overrun_` (Task 2); `SlotParams::loop`, `loopStart`, `loopXfadePct`, `trigMode`.
- Produces: Der `SamplePlayer` loopt, wenn `loop` an ist und `trigMode != OneShot`; er endet dann nicht von selbst.

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

An `tests/test_SamplePlayer.cpp` anhängen:

```cpp
namespace {
SlotParams loopParams(float start, float loopStart, float end, float xfade = 0.0f)
{
    SlotParams p = sampleParams();
    p.sampleStart = start;
    p.loopStart = loopStart;
    p.sampleEnd = end;
    p.loop = true;
    p.loopXfadePct = xfade;
    return p;
}
} // namespace

TEST_CASE("a forward loop cycles between loop start and end", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 0.75f); // L 2400, E 3600
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 6000);
    CHECK_THAT(out[3000], WithinAbs(rampAt(3000), 1e-3));
    CHECK_THAT(out[3600], WithinAbs(rampAt(2400), 1e-3)); // Rücksprung
    CHECK_THAT(out[3700], WithinAbs(rampAt(2500), 1e-3));
    CHECK_THAT(out[4800], WithinAbs(rampAt(2400), 1e-3)); // zweiter Durchlauf
    CHECK(v.isActive());
}

TEST_CASE("a reverse loop cycles between start and loop start", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.25f, 0.5f, 0.75f); // S 1200, L 2400, E 3600
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 6000);
    CHECK_THAT(out[100], WithinAbs(rampAt(3499), 1e-3));   // erster Durchlauf von End abwärts
    CHECK_THAT(out[2399], WithinAbs(rampAt(1200), 1e-3));  // unten angekommen
    CHECK_THAT(out[2400], WithinAbs(rampAt(2399), 1e-3));  // Sprung zu Loop-Start
    CHECK_THAT(out[2500], WithinAbs(rampAt(2299), 1e-3));
    CHECK_THAT(out[3600], WithinAbs(rampAt(2399), 1e-3));  // zweiter Durchlauf
    CHECK(v.isActive());
}

TEST_CASE("an empty loop segment loops the whole region", "[sampler][loop]")
{
    const auto d = rampData(4800);
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.25f, 0.75f, 0.75f); // Loop-Start = End
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 3000);
        CHECK_THAT(out[2400], WithinAbs(rampAt(1200), 1e-3));
        CHECK(v.isActive());
    }
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.0f, 0.0f, 1.0f); // Standard-Marker, rückwärts
        p.reverse = true;
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 5000);
        CHECK_THAT(out[4800], WithinAbs(rampAt(4799), 1e-3));
        CHECK(v.isActive());
    }
}

TEST_CASE("one shot ignores the loop", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 0.75f);
    p.trigMode = TriggerMode::OneShot;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 5000);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 3602) == 0.0f);
}

TEST_CASE("x-fade 0 jumps hard, a larger value makes the seam continuous", "[sampler][loop]")
{
    const auto d = rampData(4800);
    const auto render = [&](float xfade) {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.0f, 0.5f, 0.75f, xfade);
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        return renderN(v, ctx, 5000);
    };
    const auto hard = render(0.0f);
    CHECK(maxStep(hard, 3500, 3700) > 0.1f); // Sprung von 3600 auf 2400

    const auto soft = render(25.0f); // 300 Samples
    CHECK(maxStep(soft, 3000, 4000) < 0.002f);
    CHECK_THAT(soft[3600], WithinAbs(rampAt(2400), 1e-3));
    CHECK_THAT(soft[3299], WithinAbs(rampAt(3299), 1e-3)); // vor der Zone unverändert
    CHECK(dgtest::allFinite(soft));
}

TEST_CASE("x-fade shrinks when there is not enough material before the loop start", "[sampler][loop]")
{
    const auto d = rampData(4800);
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.0f, 0.01f, 0.5f, 50.0f); // L 48, E 2400: gewünscht 1176, möglich 48
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 3000);
        CHECK(dgtest::allFinite(out));
        CHECK(maxStep(out, 2300, 2500) < 0.01f);
        CHECK_THAT(out[2300], WithinAbs(rampAt(2300), 1e-3)); // außerhalb der verkürzten Zone unverändert
    }
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.0f, 0.0f, 0.5f, 50.0f); // kein Material davor: harter Schnitt
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 3000);
        CHECK(dgtest::allFinite(out));
        CHECK(maxStep(out, 2300, 2500) > 0.2f);
    }
}

TEST_CASE("a reverse loop crossfades into the material above its segment", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.25f, 0.5f, 0.75f, 25.0f); // Segment 1200..2400, xf 300
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 5000);
    CHECK(dgtest::allFinite(out));
    CHECK(maxStep(out, 2000, 2800) < 0.002f); // Naht bei 2400
    CHECK(v.isActive());
}

TEST_CASE("moving the end marker behind the position jumps to the loop start without a click", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.25f, 0.75f); // L 1200, E 3600
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto before = renderN(v, ctx, 3000); // Position 3000
    p.sampleEnd = 0.5f;                        // End 2400: Position liegt dahinter
    const auto after = renderN(v, ctx, 1000);
    CHECK(std::abs(after[0] - before.back()) < 0.002f);
    CHECK(maxStep(after, 1, 1000) < 0.002f);
    CHECK_THAT(after[300], WithinAbs(rampAt(1500), 1e-3)); // 5-ms-Überblendung ist vorbei
    CHECK(v.isActive());
}

TEST_CASE("moving the end marker behind the position without a loop fades the voice out", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto before = renderN(v, ctx, 3000);
    p.sampleEnd = 0.5f;
    const auto after = renderN(v, ctx, 600);
    CHECK_FALSE(v.isActive());
    CHECK(std::abs(after[0] - before.back()) < 0.005f);
    CHECK(maxStep(after, 1, 600) < 0.005f);
    CHECK(dgtest::peakAbs(after, 300) == 0.0f);
    CHECK(dgtest::allFinite(after));
}

TEST_CASE("moving the start marker past the position keeps playing", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 1000);
    p.sampleStart = 0.5f;
    const auto after = renderN(v, ctx, 1000);
    CHECK(v.isActive());
    CHECK_THAT(after[100], WithinAbs(rampAt(1100), 1e-3));
}

TEST_CASE("switching reverse while playing turns around at the current position", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 1000);
    p.reverse = true;
    const auto after = renderN(v, ctx, 500);
    CHECK_THAT(after[0], WithinAbs(rampAt(1000), 1e-3));
    CHECK_THAT(after[100], WithinAbs(rampAt(900), 1e-3));
    CHECK(v.isActive());
}

TEST_CASE("a loop shorter than one step stays inside its segment", "[sampler][loop]")
{
    const auto d = rampData(4800, 96000.0); // doppelte Dateirate: Grundschritt 2
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 2424.0f / 4800.0f, 50.0f); // Segment 2400..2424 (24 Samples)
    p.tuneSemis = 24.0f;
    PerfOffsets perf;
    perf.pitchSemis = 24.0f; // zusammen Faktor 16, Schritt 32: länger als das Segment
    VoiceContext ctx { &p, 120.0, perf, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 4800);
    CHECK(v.isActive());
    CHECK(dgtest::allFinite(out));
    CHECK(dgtest::peakAbs(out) <= rampAt(2430));
    CHECK(dgtest::peakAbs(out, 2400) >= rampAt(2380));
}

TEST_CASE("retrigger in the middle of the crossfade stays smooth", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 0.75f, 25.0f); // Zone 3300..3600
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    auto out = renderN(v, ctx, 3450);
    v.start(ctx, 2);
    const auto after = renderN(v, ctx, 600);
    out.insert(out.end(), after.begin(), after.end());
    CHECK(dgtest::allFinite(out));
    CHECK(maxStep(out, 3400, 4050) < 0.005f);
    CHECK(dgtest::peakAbs(out) < 0.4f); // keine Pegelüberhöhung
}

TEST_CASE("a loop keeps running during the release and then ends", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 0.75f);
    p.releaseS = 0.05f; // 2400 Samples
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 3500);
    v.release(ctx);
    const auto tail = renderN(v, ctx, 1200); // läuft über das Bereichsende hinweg
    CHECK(v.isActive());
    CHECK(dgtest::peakAbs(tail, 600) > 0.0f);
    renderN(v, ctx, 2400);
    CHECK_FALSE(v.isActive());
}
```

- [ ] **Step 2: Build und Tests ausführen, das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\Release\DubgefahrenTests.exe "[loop]"`
Expected: FAIL in den Schleifen- und Live-Tests (die Stimme endet am Bereichsende bzw. bricht hart ab). „one shot ignores the loop“, „moving the start marker past the position keeps playing“ und „switching reverse while playing …“ können schon bestehen.

- [ ] **Step 3: Schleife, X-Fade und Live-Sprünge in `render` einbauen**

`engine/SamplePlayer.cpp`: im anonymen Namespace nach `cubicAt` ergänzen:

```cpp
// Die Stimme loopt nur, wenn Loop an ist und der Slot nicht als One Shot spielt.
bool loops(const SlotParams& p) { return p.loop && p.trigMode != TriggerMode::OneShot; }
```

In `render` nach der Zeile `dir_ = p.reverse ? -1.0 : 1.0;` ergänzen:

```cpp
    const bool looping = loops(p);
    const LoopSegment seg = resolveLoopSegment(region, p.reverse, x.size());
    const double segLen = seg.hi - seg.lo;
    // X-Fade: Anteil der Segmentlänge, begrenzt auf das Material, das in Laufrichtung vor dem
    // Schleifenanfang liegt (vorwärts unterhalb von lo, rückwärts oberhalb von hi).
    const double n = static_cast<double>(x.size());
    const double xfWanted = looping ? std::clamp(static_cast<double>(p.loopXfadePct), 0.0, 50.0) / 100.0 * segLen : 0.0;
    const double xf = std::min(xfWanted, dir_ > 0.0 ? seg.lo : n - seg.hi);
```

Den Block `if (past && !overrun_) { stop(); … return; }` ersetzen durch:

```cpp
        if (past && !overrun_)
        {
            // Mehr als einen Schritt hinter dem Ende liegt die Position nur, wenn ein Marker
            // unter der Stimme verschoben wurde.
            const double over = dir_ > 0.0 ? pos_ - region.end : region.start - pos_;
            const bool far = over > step + 1.0;
            if (looping)
            {
                if (far)
                {
                    // Sprung an den Schleifenanfang; die alte Position blendet als zweiter Lesekopf aus.
                    fadePos_ = pos_;
                    fadeDir_ = dir_;
                    fadeGain_ = 1.0f;
                    pos_ = dir_ > 0.0 ? seg.lo : std::max(seg.lo, seg.hi - 1.0);
                }
                else if (dir_ > 0.0)
                    pos_ = seg.lo + std::fmod(pos_ - seg.lo, segLen); // auch wenn der Schritt länger als das Segment ist
                else
                    pos_ = seg.hi - std::fmod(seg.hi - pos_, segLen);
            }
            else if (far)
            {
                env_.kill(); // kurz ausblenden statt hart abbrechen
                overrun_ = true;
            }
            else
            {
                stop();
                std::fill(out + i, out + numSamples, 0.0f);
                return;
            }
        }
```

In der Lambda `endGain` die Bedingung `if (overrun_)` ersetzen durch `if (looping || overrun_)`.

Die Zeile `float y = cubicAt(x, pos_) * endGain(pos_, dir_) * (1.0f - fadeGain_);` ersetzen durch:

```cpp
        float y = cubicAt(x, pos_);
        if (xf > 0.0)
        {
            // Kurz vor dem Rücksprung in das Material überblenden, das eine Segmentlänge entfernt
            // liegt; am Sprung ist das Signal dadurch stetig.
            if (dir_ > 0.0 && pos_ >= seg.hi - xf)
            {
                const float g = static_cast<float>(std::clamp((pos_ - (seg.hi - xf)) / xf, 0.0, 1.0));
                y += (cubicAt(x, pos_ - segLen) - y) * g;
            }
            else if (dir_ < 0.0 && pos_ < seg.lo + xf)
            {
                const float g = static_cast<float>(std::clamp((seg.lo + xf - pos_) / xf, 0.0, 1.0));
                y += (cubicAt(x, pos_ + segLen) - y) * g;
            }
        }
        // Beim Retrigger und beim Sprung Überblendung: neuer Kopf blendet ein, alter aus (Summe der Gewichte = 1).
        y *= endGain(pos_, dir_) * (1.0f - fadeGain_);
```

- [ ] **Step 4: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\Release\DubgefahrenTests.exe "[sampler]"`
Expected: PASS, alle Testfälle (bestehende, Task 2 und Task 3).

Schlägt „a loop shorter than one step …“ an der unteren Pegelgrenze fehl, weil das Segment an seiner Oberkante genau `seg.hi` liest: die Grenze `rampAt(2380)` beibehalten und die Ursache prüfen (die Position muss nach dem Rücksprung in `[seg.lo, seg.hi]` liegen); nicht die Schranke lockern.

- [ ] **Step 5: Risiken der Spec prüfen**

Die zwei Risiken aus Abschnitt 8 der Spec sind durch Tests abgedeckt; ihre Ergebnisse ausdrücklich ansehen:

Run: `build\tests\Release\DubgefahrenTests.exe "moving the end marker behind the position jumps to the loop start without a click"`
Run: `build\tests\Release\DubgefahrenTests.exe "retrigger in the middle of the crossfade stays smooth"`
Expected: PASS. Schlägt einer fehl und lässt sich nicht innerhalb der Spec beheben: **stoppen und melden**.

- [ ] **Step 6: Alle Tests, dann Commit**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

```bash
git add engine/SamplePlayer.cpp tests/test_SamplePlayer.cpp
git commit -m "feat(engine): sample loop with adjustable crossfade and live marker changes (#11)"
```

---

### Task 4: Latch für geloopte Sample-Slots

**Files:**
- Modify: `engine/Engine.cpp:99-108`, `engine/PadRouter.cpp` (`advance`)
- Test: `tests/test_Engine.cpp`, `tests/test_PadRouter.cpp`

**Interfaces:**
- Consumes: `SlotParams::loop` (Task 1); loopender `SamplePlayer` (Task 3).
- Produces: `TriggerSettings::mode == Latch` für Sample-Slots mit `loop`; `PadRouter::advance` vergisst den Latch eines Slots ohne aktive Stimme.

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

An `tests/test_PadRouter.cpp` anhängen:

```cpp
TEST_CASE("a latch whose voice has ended is forgotten", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Latch);
    r.noteOn(36, s, v);
    REQUIRE(r.isLatched(0));
    r.advance(64, v);
    CHECK(r.isLatched(0)); // Stimme läuft: Latch bleibt

    v.active[0] = false; // die Stimme hat von selbst geendet
    r.advance(64, v);
    CHECK_FALSE(r.isLatched(0));
    CHECK(r.latchedMask() == 0u);

    // Die nächste Note startet neu, statt einen vergessenen Latch zu lösen.
    r.noteOn(36, s, v);
    CHECK(v.log.back() == "start 0");
    CHECK(r.isLatched(0));
}
```

An `tests/test_Engine.cpp` anhängen:

```cpp
TEST_CASE("a looped sample slot can be latched", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    const auto d = testSample(0.1); // 4800 Samples
    makeSampleSlot(p, 1, &d, TriggerMode::Latch);
    p.slots[1].loop = true;

    run(e, p, 4800, { noteOn(37) });
    CHECK(e.activeMask() == 2u);
    CHECK(e.latchedMask() == 2u);
    const auto held = run(e, p, 9600, { noteOff(37) }); // Loslassen ändert nichts, die Schleife läuft weiter
    CHECK(e.activeMask() == 2u);
    CHECK(dgtest::peakAbs(held.l, 4800) > 0.1f); // klingt über die Sample-Länge hinaus

    run(e, p, 4800, { noteOn(37) }); // zweite Note stoppt
    CHECK(e.latchedMask() == 0u);
    run(e, p, 4800);
    CHECK(e.activeMask() == 0u);
}

TEST_CASE("switching the loop off while latched lets the sample end and clears the latch", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    const auto d = testSample(0.1);
    makeSampleSlot(p, 1, &d, TriggerMode::Latch);
    p.slots[1].loop = true;
    run(e, p, 9600, { noteOn(37) });
    REQUIRE(e.latchedMask() == 2u);

    p.slots[1].loop = false;
    run(e, p, 9600); // mehr als eine Sample-Länge
    CHECK(e.activeMask() == 0u);
    CHECK(e.latchedMask() == 0u);

    // Ohne Loop wirkt Latch wieder wie Gate.
    run(e, p, 2400, { noteOn(37) });
    CHECK(e.latchedMask() == 0u);
    run(e, p, 2400, { noteOff(37) });
    CHECK(e.activeMask() == 0u);
}

TEST_CASE("changing the sample data stops a latched looped slot and clears the latch", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    const auto d = testSample(0.1);
    const auto other = testSample(0.1, 220.0f);
    makeSampleSlot(p, 1, &d, TriggerMode::Latch);
    p.slots[1].loop = true;
    run(e, p, 9600, { noteOn(37) });
    REQUIRE(e.latchedMask() == 2u);

    p.samples[1] = &other;
    run(e, p, 512);
    CHECK(e.activeMask() == 0u);
    CHECK(e.latchedMask() == 0u);

    p.samples[1] = nullptr;
    const auto o = run(e, p, 512, { noteOn(37) });
    CHECK(e.activeMask() == 0u);
    CHECK(dgtest::peakAbs(o.l) == 0.0f);
}

TEST_CASE("a looped sample one-shot still plays once to the end of its region", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    const auto d = testSample(0.1);
    makeSampleSlot(p, 0, &d, TriggerMode::OneShot);
    p.slots[0].loop = true;
    p.slots[0].sampleEnd = 0.5f; // 2400 Samples
    run(e, p, 1200, { noteOn(36) });
    CHECK(e.activeMask() == 1u);
    run(e, p, 2400);
    CHECK(e.activeMask() == 0u);
}
```

- [ ] **Step 2: Build und Tests ausführen, das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\Release\DubgefahrenTests.exe "[router],[engine]"`
Expected: FAIL in „a latch whose voice has ended is forgotten“, „a looped sample slot can be latched“ und „switching the loop off while latched …“. Die anderen zwei neuen Fälle können schon bestehen.

- [ ] **Step 3: Engine lässt Latch für geloopte Samples zu**

`engine/Engine.cpp`, in `process` den Block für Sample-Slots ersetzen:

```cpp
        if (sp.source == SourceType::Sample)
        {
            // Sample: One Shot spielt bis zum Ende des Bereichs. Latch gilt nur für geloopte
            // Samples, sonst wirkt es wie Gate.
            const bool oneShot = sp.trigMode == TriggerMode::OneShot;
            const bool latch = sp.trigMode == TriggerMode::Latch && sp.loop;
            const TriggerMode mode = oneShot ? TriggerMode::OneShot : latch ? TriggerMode::Latch : TriggerMode::Gate;
            trig_[s] = { mode, sp.oneShotS, sp.chokeGroup, oneShot };
        }
```

- [ ] **Step 4: Router vergisst den Latch einer beendeten Stimme**

`engine/PadRouter.cpp`, `advance` ersetzen:

```cpp
void PadRouter::advance(int numSamples, VoiceControl& voices)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        // Eine gelatchte Stimme, die von selbst geendet hat (Sample ohne Loop), ist nicht mehr gelatcht.
        if (latched_[s] && !voices.isVoiceActive(s))
            latched_[s] = false;

        if (oneShotRemaining_[s] < 0)
            continue;
        oneShotRemaining_[s] -= numSamples;
        if (oneShotRemaining_[s] <= 0)
        {
            oneShotRemaining_[s] = -1;
            if (voices.isVoiceActive(s) && !voices.isVoiceReleasing(s))
                voices.releaseVoice(s);
        }
    }
}
```

- [ ] **Step 5: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\Release\DubgefahrenTests.exe "[router],[engine]"`
Expected: PASS, auch der bestehende Fall „a sample gate releases on note off and latch acts like gate“.

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

- [ ] **Step 6: Commit**

```bash
git add engine/Engine.cpp engine/PadRouter.cpp tests/test_Engine.cpp tests/test_PadRouter.cpp
git commit -m "feat(engine): latch for looped sample slots (#11)"
```

---

### Task 5: Kit-Format Version 4 und Processor

**Files:**
- Modify: `plugin/KitFile.cpp:8`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp` (`setSlotSample`)
- Test: `tests/plugin/test_KitFile.cpp`, `tests/plugin/test_Samples.cpp`

**Interfaces:**
- Consumes: `resetSampleRegionFields` (Task 1).
- Produces:
  - `std::shared_ptr<const SampleData> DubgefahrenProcessor::slotSampleData(int slot) const`
  - `setSlotSample` setzt die sechs Bereichsfelder zurück, wenn ein anderes Sample gewählt wird.

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

In `tests/plugin/test_KitFile.cpp` den Testfall „sample slots survive a version 3 round-trip“ umbenennen in „sample slots survive a version 4 round-trip“, darin `CHECK(static_cast<int>(v["version"]) == 3);` auf `== 4` ändern und vor `const auto v = parsed(k);` ergänzen:

```cpp
    k.slots[3].sampleStart = 0.25f;
    k.slots[3].loopStart = 0.5f;
    k.slots[3].sampleEnd = 0.75f;
    k.slots[3].loop = true;
    k.slots[3].reverse = true;
    k.slots[3].loopXfadePct = 12.5f;
```

und nach der Zeile mit `["params"]["tune"]`:

```cpp
    CHECK(static_cast<double>(v["slots"][3]["params"]["smpStart"]) == 0.25);
    CHECK(static_cast<double>(v["slots"][3]["params"]["loopStart"]) == 0.5);
    CHECK(static_cast<double>(v["slots"][3]["params"]["smpEnd"]) == 0.75);
    CHECK(static_cast<double>(v["slots"][3]["params"]["loop"]) == 1.0);
    CHECK(static_cast<double>(v["slots"][3]["params"]["reverse"]) == 1.0);
    CHECK(static_cast<double>(v["slots"][3]["params"]["xfade"]) == 12.5);
```

Am Dateiende anhängen:

```cpp
TEST_CASE("version 3 kits load with default region fields", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[3].source = SourceType::Sample;
    k.slots[3].tuneSemis = 5.0f;
    k.samples[3] = "horn.wav";

    auto v = parsed(k);
    v.getDynamicObject()->setProperty("version", 3);
    for (int s = 0; s < kNumSlots; ++s)
        if (auto* params = v["slots"][s]["params"].getDynamicObject())
            for (const char* key : { "smpStart", "loopStart", "smpEnd", "loop", "reverse", "xfade" })
                params->removeProperty(key);

    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->slots == k.slots); // k trägt die Standardwerte der neuen Felder
    CHECK(r.kit->slots[3].sampleEnd == 1.0f);
    CHECK_FALSE(r.kit->slots[3].loop);
    CHECK(r.kit->samples == k.samples);
}

TEST_CASE("version 5 kits are rejected as too new", "[kitfile]")
{
    auto v = parsed(makeFactoryKit());
    v.getDynamicObject()->setProperty("version", 5);
    const auto r = reparse(v);
    CHECK_FALSE(r.kit.has_value());
    CHECK(r.error.contains("newer version"));
}
```

An `tests/plugin/test_Samples.cpp` anhängen:

```cpp
TEST_CASE("choosing another sample resets the region fields, choosing the same one keeps them", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    juce::WavAudioFormat wav;
    dgtest::writeSine(wav, kit.folder.getChildFile("bell.wav"), 880.0f, 48000.0, 24000);
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(3, "horn.wav");

    SlotParams sp = readSlotFromParameters(p.state(), 3);
    CHECK(sp.sampleStart == 0.0f); // frisch gewählt: Standardwerte
    CHECK(sp.sampleEnd == 1.0f);
    sp.sampleStart = 0.25f;
    sp.loopStart = 0.5f;
    sp.sampleEnd = 0.75f;
    sp.loop = true;
    sp.reverse = true;
    sp.loopXfadePct = 20.0f;
    sp.tuneSemis = 3.0f;
    p.setSlot(3, sp, "horn", "horn.wav");

    p.setSlotSample(3, "horn.wav"); // dasselbe Sample: alles bleibt
    sp = readSlotFromParameters(p.state(), 3);
    CHECK_THAT(sp.sampleStart, WithinAbs(0.25, 1e-4));
    CHECK(sp.loop);

    p.setSlotSample(3, "bell.wav"); // anderes Sample: Bereich zurück, der Rest bleibt
    sp = readSlotFromParameters(p.state(), 3);
    CHECK(sp.sampleStart == 0.0f);
    CHECK(sp.loopStart == 0.0f);
    CHECK(sp.sampleEnd == 1.0f);
    CHECK_FALSE(sp.loop);
    CHECK_FALSE(sp.reverse);
    CHECK_THAT(sp.loopXfadePct, WithinAbs(5.0, 1e-4));
    CHECK_THAT(sp.tuneSemis, WithinAbs(3.0, 1e-4));
}

TEST_CASE("the processor hands out the loaded sample data of a slot", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    CHECK(p.slotSampleData(3) == nullptr);
    p.setSlotSample(3, "horn.wav");
    p.waitForSampleLoads();
    const auto data = p.slotSampleData(3);
    REQUIRE(data != nullptr);
    CHECK(data->samples.size() == 48000u);
    CHECK(p.slotSampleData(0) == nullptr); // Synth-Slot
    p.resetSlotToFactory(3);
    CHECK(p.slotSampleData(3) == nullptr);
    CHECK(data->samples.size() == 48000u); // die herausgegebenen Daten bleiben gültig
}

TEST_CASE("a host state without the region parameters restores their defaults", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    juce::MemoryBlock block;
    a.getStateInformation(block);

    // Den Zustand so zurechtschneiden, wie ihn Version 0.4.0 geschrieben hat: ohne die neuen Parameter.
    auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(xml != nullptr);
    int removed = 0;
    for (const char* key : { "smpStart", "loopStart", "smpEnd", "loop", "reverse", "xfade" })
        for (int s = 0; s < kNumSlots; ++s)
        {
            const auto id = juce::String::formatted("s%02d_", s + 1) + key;
            for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
                if (child->getStringAttribute("id") == id)
                {
                    xml->removeChildElement(child, true);
                    ++removed;
                    break;
                }
        }
    REQUIRE(removed == 6 * kNumSlots);
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml, old);

    DubgefahrenProcessor b;
    SlotParams sp = readSlotFromParameters(b.state(), 0);
    sp.sampleEnd = 0.5f;
    sp.loop = true;
    sp.loopXfadePct = 30.0f;
    b.setSlot(0, sp, "x");
    b.setStateInformation(old.getData(), static_cast<int>(old.getSize()));

    sp = readSlotFromParameters(b.state(), 0);
    CHECK(sp.sampleStart == 0.0f);
    CHECK(sp.sampleEnd == 1.0f);
    CHECK_FALSE(sp.loop);
    CHECK_THAT(sp.loopXfadePct, WithinAbs(5.0, 1e-4));
}
```

- [ ] **Step 2: Build ausführen und das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL, `slotSampleData` ist kein Member von `DubgefahrenProcessor`.

- [ ] **Step 3: Kit-Version und Processor anpassen**

`plugin/KitFile.cpp`: `constexpr int kVersion = 3;` ändern in `constexpr int kVersion = 4;`.

`plugin/PluginProcessor.h`, nach `bool isSampleMissing(int slot) const;`:

```cpp
    // Geladene Daten eines Sample-Slots für die Anzeige (nullptr: keine). Nur im Message-Thread.
    std::shared_ptr<const SampleData> slotSampleData(int slot) const;
```

`plugin/PluginProcessor.cpp`: Include `#include "engine/SlotFields.h"` ergänzen, falls nicht vorhanden. Nach `isSampleLoaded`:

```cpp
std::shared_ptr<const SampleData> DubgefahrenProcessor::slotSampleData(int slot) const
{
    JUCE_ASSERT_MESSAGE_THREAD
    return sampleData_[static_cast<std::size_t>(slot)];
}
```

`setSlotSample` ersetzen:

```cpp
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
```

- [ ] **Step 4: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[kitfile],[samples]"`
Expected: PASS.

Heißt der Attributname der Parameter-Kinder im State nicht `id`, schlägt `REQUIRE(removed == 6 * kNumSlots)` fehl: dann `xml->toString()` einmal ausgeben, den tatsächlichen Attributnamen übernehmen und die Ausgabe wieder entfernen.

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/KitFile.cpp plugin/PluginProcessor.h plugin/PluginProcessor.cpp tests/plugin/test_KitFile.cpp tests/plugin/test_Samples.cpp
git commit -m "feat(plugin): kit format version 4 with sample region fields (#11)"
```

---

### Task 6: Wellenform-Anzeige

**Files:**
- Create: `plugin/ui/WaveformView.h`, `plugin/ui/WaveformView.cpp`, `tests/plugin/test_Waveform.cpp`
- Modify: `plugin/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `resolveSampleRegion`, `resolveLoopSegment`, `kMinRegionSamples` (Task 1); `DubgefahrenProcessor::slotSampleData`, `isSampleMissing` (Task 5); `slotParamId`; `ui::font`, `colours::padEmpty`, `padBase`, `latched`, `text`, `textDim`.
- Produces:
  - `struct ui::PeakColumn { float min, max; }`
  - `std::vector<ui::PeakColumn> ui::computePeaks(const std::vector<float>& samples, int columns)`
  - `class ui::WaveformView : juce::Component` mit `enum class Marker { Start, Loop, End }`, `setSlot(int)`, `refresh()`, `hasData()`, `hintText()`, `isMarkerVisible(Marker)`, `markerX(Marker)`, `pressAt(float)`, `dragTo(float)`, `release()`

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

`tests/plugin/test_Waveform.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <vector>
#include "engine/Kit.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/SampleFiles.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/WaveformView.h"
#include "RenderTestHelpers.h"
#include "SampleTestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;
using Marker = ui::WaveformView::Marker;

namespace {
// Processor mit einem geladenen Sample (4800 Samples) in Slot 3.
struct Fixture
{
    dgtest::TempDir tmp;
    juce::File kitFile = tmp.dir.getChildFile("Dub.dgkit");
    DubgefahrenProcessor p;
    Fixture()
    {
        sampleFolderFor(kitFile).createDirectory();
        juce::WavAudioFormat wav;
        dgtest::writeSine(wav, sampleFolderFor(kitFile).getChildFile("horn.wav"), 441.0f, 48000.0, 4800);
        p.applyKit(makeFactoryKit(), kitFile);
        p.setSlotSample(2, "horn.wav");
        p.waitForSampleLoads();
    }
    float value(SlotField f) { return p.state().getRawParameterValue(slotParamId(2, f))->load(); }
    void set(SlotField f, float v)
    {
        auto* param = p.state().getParameter(slotParamId(2, f));
        param->setValueNotifyingHost(param->convertTo0to1(v));
    }
};

struct GestureCounter final : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0;
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override { starting ? ++begins : ++ends; }
};
} // namespace

TEST_CASE("peaks for empty, tiny and long data", "[waveform]")
{
    CHECK(ui::computePeaks({}, 100).empty());
    CHECK(ui::computePeaks({ 0.5f }, 0).empty());
    CHECK(ui::computePeaks({ 0.5f }, -3).empty());

    const auto one = ui::computePeaks({ 0.5f }, 4); // weniger Samples als Spalten
    REQUIRE(one.size() == 4u);
    for (const auto& c : one)
    {
        CHECK(c.min == 0.5f);
        CHECK(c.max == 0.5f);
    }

    std::vector<float> data(1000, 0.0f);
    data[10] = 0.8f;
    data[20] = -0.6f;
    data[990] = 0.3f;
    const auto peaks = ui::computePeaks(data, 10); // 100 Samples pro Spalte
    REQUIRE(peaks.size() == 10u);
    CHECK(peaks[0].max == 0.8f);
    CHECK(peaks[0].min == -0.6f);
    CHECK(peaks[5].max == 0.0f);
    CHECK(peaks[9].max == 0.3f);

    const auto bad = ui::computePeaks({ std::numeric_limits<float>::quiet_NaN(), 5.0f, -5.0f }, 1);
    REQUIRE(bad.size() == 1u);
    CHECK(bad[0].max == 1.0f); // begrenzt auf -1..1, NaN ignoriert
    CHECK(bad[0].min == -1.0f);
}

TEST_CASE("the waveform shows start and end markers, the loop marker only with loop on", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    REQUIRE(view.hasData());
    CHECK(view.hintText().isEmpty());
    CHECK(view.isMarkerVisible(Marker::Start));
    CHECK(view.isMarkerVisible(Marker::End));
    CHECK_FALSE(view.isMarkerVisible(Marker::Loop));
    CHECK(view.markerX(Marker::Start) == 0.0f);
    CHECK(view.markerX(Marker::End) == 580.0f);

    f.set(SlotField::Loop, 1.0f);
    f.set(SlotField::LoopStart, 0.5f);
    CHECK(view.isMarkerVisible(Marker::Loop));
    CHECK_THAT(view.markerX(Marker::Loop), WithinAbs(290.0, 0.5));
}

TEST_CASE("dragging a marker writes its parameter as one gesture", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    GestureCounter counter;
    auto* end = f.p.state().getParameter(slotParamId(2, SlotField::SampleEnd));
    end->addListener(&counter);

    view.pressAt(578.0f);
    view.dragTo(400.0f);
    view.dragTo(290.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::SampleEnd), WithinAbs(0.5, 0.002));
    CHECK(counter.begins == 1);
    CHECK(counter.ends == 1);
    CHECK_THAT(view.markerX(Marker::End), WithinAbs(290.0, 0.5));

    view.pressAt(100.0f); // kein Marker in der Nähe
    view.dragTo(50.0f);
    view.release();
    CHECK(counter.begins == 1);
    CHECK_THAT(f.value(SlotField::SampleEnd), WithinAbs(0.5, 0.002));
    end->removeListener(&counter);
}

TEST_CASE("markers stop at their neighbours", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    f.set(SlotField::SampleEnd, 0.5f); // 2400 Samples, x = 290
    const float minFrac = static_cast<float>(kMinRegionSamples) / 4800.0f;

    // Start bleibt die Mindestlänge vor End stehen.
    view.pressAt(1.0f);
    view.dragTo(560.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::SampleStart), WithinAbs(0.5 - minFrac, 1e-4));

    // End bleibt die Mindestlänge hinter Start stehen.
    view.pressAt(view.markerX(Marker::End));
    view.dragTo(0.0f);
    view.release();
    CHECK(f.value(SlotField::SampleEnd) >= f.value(SlotField::SampleStart) + minFrac - 1e-4f);

    // Mit Loop begrenzt L beide Nachbarn und bleibt selbst zwischen ihnen.
    f.set(SlotField::SampleStart, 0.2f);
    f.set(SlotField::SampleEnd, 0.8f);
    f.set(SlotField::Loop, 1.0f);
    f.set(SlotField::LoopStart, 0.5f);
    view.pressAt(view.markerX(Marker::Start));
    view.dragTo(500.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::SampleStart), WithinAbs(0.5, 1e-3));
    view.pressAt(view.markerX(Marker::End));
    view.dragTo(10.0f);
    view.release();
    CHECK(f.value(SlotField::SampleEnd) >= 0.5f - 1e-3f);
    f.set(SlotField::SampleStart, 0.2f);
    f.set(SlotField::SampleEnd, 0.8f);
    view.pressAt(view.markerX(Marker::Loop));
    view.dragTo(580.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::LoopStart), WithinAbs(0.8, 1e-3));
    view.pressAt(view.markerX(Marker::Loop));
    view.dragTo(0.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::LoopStart), WithinAbs(0.2, 1e-3));
}

TEST_CASE("with overlapping handles the drag direction picks the marker that can move", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    f.set(SlotField::Loop, 1.0f); // S und L liegen beide bei 0

    view.pressAt(2.0f);
    view.dragTo(200.0f); // nach rechts: L kann, S müsste L überholen
    view.release();
    CHECK(f.value(SlotField::LoopStart) > 0.3f);
    CHECK(f.value(SlotField::SampleStart) == 0.0f);

    // L auf E: nach links kann L, nach rechts keiner.
    f.set(SlotField::LoopStart, 1.0f);
    view.pressAt(579.0f);
    view.dragTo(300.0f);
    view.release();
    CHECK(f.value(SlotField::LoopStart) < 0.6f);
    CHECK(f.value(SlotField::SampleEnd) == 1.0f);
}

TEST_CASE("hidden loop marker does not block start and end", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    f.set(SlotField::LoopStart, 0.1f); // Loop ist aus: L unsichtbar
    view.pressAt(1.0f);
    view.dragTo(290.0f);
    view.release();
    CHECK_THAT(f.value(SlotField::SampleStart), WithinAbs(0.5, 0.002));
    CHECK_THAT(f.value(SlotField::LoopStart), WithinAbs(0.1, 1e-4)); // unangetastet
}

TEST_CASE("without data nothing can be dragged", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    f.p.setSlotSample(4, "gone.wav");
    f.p.waitForSampleLoads();
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(4);
    CHECK_FALSE(view.hasData());
    CHECK(view.hintText() == "Sample missing");
    CHECK_FALSE(view.isMarkerVisible(Marker::Start));
    const float before = f.p.state().getRawParameterValue(slotParamId(4, SlotField::SampleEnd))->load();
    view.pressAt(578.0f);
    view.dragTo(100.0f);
    view.release();
    CHECK(f.p.state().getRawParameterValue(slotParamId(4, SlotField::SampleEnd))->load() == before);

    view.setSlot(0); // Synth-Slot
    CHECK(view.hintText() == "No sample loaded");

    ui::WaveformView empty(f.p); // Breite 0, kein Slot
    CHECK_NOTHROW(empty.pressAt(0.0f));
    CHECK_NOTHROW(empty.dragTo(10.0f));
    CHECK_NOTHROW(empty.release());
    CHECK_NOTHROW(dgtest::snapshot(view));
}

TEST_CASE("switching the slot ends an open drag", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::WaveformView view(f.p);
    view.setSize(580, 76);
    view.setSlot(2);
    GestureCounter counter;
    auto* end = f.p.state().getParameter(slotParamId(2, SlotField::SampleEnd));
    end->addListener(&counter);
    view.pressAt(578.0f);
    view.dragTo(400.0f);
    REQUIRE(counter.begins == 1);
    view.setSlot(5);
    CHECK(counter.ends == 1);
    const float value = f.value(SlotField::SampleEnd);
    view.dragTo(100.0f); // nach dem Wechsel wirkungslos
    view.release();
    CHECK(f.value(SlotField::SampleEnd) == value);
    CHECK(counter.ends == 1);
    end->removeListener(&counter);
}

TEST_CASE("the waveform draws the curve, the dimmed outside and the loop tint", "[waveform]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    ui::DgLookAndFeel lnf;
    ui::WaveformView view(f.p);
    view.setLookAndFeel(&lnf);
    view.setSize(580, 76);
    view.setSlot(2);
    f.set(SlotField::SampleStart, 0.25f);
    f.set(SlotField::SampleEnd, 0.75f);

    const auto plain = dgtest::snapshot(view);
    // Links vom Start abgedunkelt: dort ist die Kurve dunkler als im Bereich.
    const auto brightest = [&](const juce::Image& img, int x0, int x1) {
        float b = 0.0f;
        for (int x = x0; x < x1; ++x)
            for (int y = 16; y < 76; ++y)
                b = std::max(b, img.getPixelAt(x, y).getBrightness());
        return b;
    };
    CHECK(brightest(plain, 200, 380) > brightest(plain, 20, 120) + 0.2f);

    f.set(SlotField::Loop, 1.0f);
    f.set(SlotField::LoopStart, 0.5f);
    const auto looped = dgtest::snapshot(view);
    // Blaue Tönung zwischen L (290) und E (435), nicht zwischen S (145) und L.
    // Zeile 66: unterhalb der Kurve (Sinus mit halber Aussteuerung) und außerhalb des oberen Innenschattens.
    const auto blueAt = [&](const juce::Image& img, int x) { return static_cast<int>(img.getPixelAt(x, 66).getBlue()); };
    CHECK(blueAt(looped, 360) > blueAt(plain, 360) + 10);
    CHECK(std::abs(blueAt(looped, 220) - blueAt(plain, 220)) < 4);

    f.set(SlotField::Reverse, 1.0f); // gespiegelt: Tönung zwischen S und L
    const auto reversed = dgtest::snapshot(view);
    CHECK(blueAt(reversed, 220) > blueAt(plain, 220) + 10);
    CHECK(std::abs(blueAt(reversed, 360) - blueAt(plain, 360)) < 4);
    dgtest::savePng(looped, "waveform-loop");
    view.setLookAndFeel(nullptr);
}
```

In `tests/CMakeLists.txt` die Quellenliste von `DubgefahrenPluginTests` um `plugin/test_Waveform.cpp` erweitern.

- [ ] **Step 2: Build ausführen und das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL, `plugin/ui/WaveformView.h` nicht gefunden.

- [ ] **Step 3: `plugin/ui/WaveformView.h` schreiben**

```cpp
#pragma once
#include <memory>
#include <optional>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "melatonin_blur/melatonin_blur.h"
#include "engine/SampleData.h"
#include "engine/SampleRegion.h"
#include "engine/SlotFields.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

struct PeakColumn
{
    float min = 0.0f;
    float max = 0.0f;
};

// Minimum und Maximum pro Pixelspalte, begrenzt auf -1..1. Leere Daten oder columns <= 0
// ergeben einen leeren Vektor.
std::vector<PeakColumn> computePeaks(const std::vector<float>& samples, int columns);

// Wellenform eines Sample-Slots mit den ziehbaren Markern Start (S), Loop-Start (L) und End (E).
// Gezeichnet und begrenzt wird mit den aufgelösten Positionen aus resolveSampleRegion, damit
// die Anzeige genau das zeigt, was die Engine spielt.
class WaveformView final : public juce::Component, private juce::Timer
{
public:
    enum class Marker { Start, Loop, End };

    explicit WaveformView(DubgefahrenProcessor& proc);
    ~WaveformView() override;

    void setSlot(int slot);
    // Holt die Sample-Daten des Slots neu (nach dem Laden, einem Sample- oder Slot-Wechsel).
    void refresh();

    bool hasData() const { return data_ != nullptr && !data_->samples.empty(); }
    // Text statt der Kurve, wenn keine Daten da sind; sonst leer.
    juce::String hintText() const;
    bool isMarkerVisible(Marker m) const;
    float markerX(Marker m) const;

    // Bedienung in lokalen x-Koordinaten; die Maus-Callbacks leiten hierher, Tests rufen sie direkt auf.
    void pressAt(float x);
    void dragTo(float x);
    void release();

    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;
    void mouseDown(const juce::MouseEvent& e) override { pressAt(e.position.x); }
    void mouseDrag(const juce::MouseEvent& e) override { dragTo(e.position.x); }
    void mouseUp(const juce::MouseEvent&) override { release(); }

private:
    void timerCallback() override;
    float value(SlotField f) const;
    bool loopOn() const { return value(SlotField::Loop) > 0.5f; }
    bool reverseOn() const { return value(SlotField::Reverse) > 0.5f; }
    SampleRegion region() const;
    float xFor(double position) const;
    juce::RangedAudioParameter* parameter(Marker m) const;
    bool canMove(Marker m, float direction) const;
    void begin(Marker m);
    void moveTo(Marker m, float x);
    void drawMarker(juce::Graphics& g, float x, juce::Colour colour, const char* letter) const;

    DubgefahrenProcessor& proc_;
    int slot_ = -1;
    std::shared_ptr<const SampleData> data_;
    std::vector<PeakColumn> peaks_;
    const SampleData* peaksData_ = nullptr;
    int peaksWidth_ = 0;
    std::vector<Marker> candidates_; // Marker unter dem Mausklick, solange noch keiner gewählt ist
    std::optional<Marker> active_;
    float pressX_ = 0.0f;
    float seen_[5] = {}; // zuletzt gezeichnete Parameterwerte, für das Nachführen bei Automation
    melatonin::InnerShadow inset_ { { juce::Colours::black.withAlpha(0.8f), 6, { 0, 2 } } };
};

} // namespace dg::ui
```

- [ ] **Step 4: `plugin/ui/WaveformView.cpp` schreiben**

```cpp
#include "plugin/ui/WaveformView.h"
#include <algorithm>
#include <cmath>
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"

namespace dg::ui {

namespace {
constexpr float kCorner = 6.0f;
constexpr float kGrab = 8.0f;    // Fangbereich eines Markers links und rechts
constexpr float kHandle = 14.0f; // Kantenlänge des Griffs
constexpr SlotField kWatched[5] = { SlotField::SampleStart, SlotField::LoopStart, SlotField::SampleEnd, SlotField::Loop,
                                    SlotField::Reverse };

SlotField fieldFor(WaveformView::Marker m)
{
    switch (m)
    {
        case WaveformView::Marker::Start: return SlotField::SampleStart;
        case WaveformView::Marker::Loop:  return SlotField::LoopStart;
        case WaveformView::Marker::End:   return SlotField::SampleEnd;
    }
    return SlotField::SampleStart;
}
} // namespace

std::vector<PeakColumn> computePeaks(const std::vector<float>& samples, int columns)
{
    std::vector<PeakColumn> out;
    if (samples.empty() || columns <= 0)
        return out;
    out.resize(static_cast<std::size_t>(columns));
    const std::size_t n = samples.size();
    const auto cols = static_cast<std::size_t>(columns);
    for (std::size_t c = 0; c < cols; ++c)
    {
        const std::size_t from = std::min(n - 1, n * c / cols);
        const std::size_t to = std::max(from + 1, std::min(n, n * (c + 1) / cols));
        float lo = 1.0f, hi = -1.0f;
        bool any = false;
        for (std::size_t i = from; i < to; ++i)
        {
            if (!std::isfinite(samples[i]))
                continue;
            const float v = std::clamp(samples[i], -1.0f, 1.0f);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
            any = true;
        }
        out[c] = any ? PeakColumn { lo, hi } : PeakColumn {};
    }
    return out;
}

WaveformView::WaveformView(DubgefahrenProcessor& proc) : proc_(proc) {}

WaveformView::~WaveformView()
{
    stopTimer();
    release();
}

void WaveformView::setSlot(int slot)
{
    release(); // eine offene Geste gehört zum alten Slot
    slot_ = slot;
    refresh();
}

void WaveformView::refresh()
{
    data_ = slot_ >= 0 ? proc_.slotSampleData(slot_) : nullptr;
    repaint();
}

juce::String WaveformView::hintText() const
{
    if (hasData())
        return {};
    return slot_ >= 0 && proc_.isSampleMissing(slot_) ? "Sample missing" : "No sample loaded";
}

float WaveformView::value(SlotField f) const
{
    if (slot_ < 0)
        return fieldSpec(f).def;
    const auto* v = proc_.state().getRawParameterValue(slotParamId(slot_, f));
    return v != nullptr ? v->load() : fieldSpec(f).def;
}

SampleRegion WaveformView::region() const
{
    return resolveSampleRegion(value(SlotField::SampleStart), value(SlotField::LoopStart), value(SlotField::SampleEnd),
                               hasData() ? data_->samples.size() : 0);
}

float WaveformView::xFor(double position) const
{
    if (!hasData())
        return 0.0f;
    return static_cast<float>(position / static_cast<double>(data_->samples.size()) * getWidth());
}

bool WaveformView::isMarkerVisible(Marker m) const { return hasData() && (m != Marker::Loop || loopOn()); }

float WaveformView::markerX(Marker m) const
{
    const auto r = region();
    return xFor(m == Marker::Start ? r.start : m == Marker::Loop ? r.loopStart : r.end);
}

juce::RangedAudioParameter* WaveformView::parameter(Marker m) const
{
    return slot_ >= 0 ? proc_.state().getParameter(slotParamId(slot_, fieldFor(m))) : nullptr;
}

bool WaveformView::canMove(Marker m, float direction) const
{
    const auto r = region();
    const double n = static_cast<double>(data_->samples.size());
    const double minLen = std::min(kMinRegionSamples, n);
    const bool loop = loopOn();
    switch (m)
    {
        case Marker::Start:
            return direction < 0.0f ? r.start > 0.0 : r.start < std::min(r.end - minLen, loop ? r.loopStart : n);
        case Marker::Loop:
            return direction < 0.0f ? r.loopStart > r.start : r.loopStart < r.end;
        case Marker::End:
            return direction > 0.0f ? r.end < n : r.end > std::max(r.start + minLen, loop ? r.loopStart : 0.0);
    }
    return false;
}

void WaveformView::begin(Marker m)
{
    if (auto* p = parameter(m))
    {
        p->beginChangeGesture();
        active_ = m;
    }
    candidates_.clear();
}

void WaveformView::pressAt(float x)
{
    release();
    if (!hasData() || getWidth() <= 0)
        return;
    pressX_ = x;
    for (const auto m : { Marker::Loop, Marker::End, Marker::Start })
        if (isMarkerVisible(m) && std::abs(markerX(m) - x) <= kGrab)
            candidates_.push_back(m);
    if (candidates_.size() == 1)
        begin(candidates_.front());
}

void WaveformView::dragTo(float x)
{
    if (!hasData() || getWidth() <= 0)
        return;
    if (!active_)
    {
        if (candidates_.empty() || x == pressX_)
            return;
        // Mehrere Griffe übereinander: der Marker, der sich in Zugrichtung noch bewegen lässt
        // (Reihenfolge L, E, S, siehe pressAt).
        const float direction = x - pressX_;
        Marker pick = candidates_.front();
        for (const auto m : candidates_)
            if (canMove(m, direction))
            {
                pick = m;
                break;
            }
        begin(pick);
    }
    if (active_)
        moveTo(*active_, x);
}

void WaveformView::moveTo(Marker m, float x)
{
    auto* p = parameter(m);
    if (p == nullptr)
        return;
    const auto r = region();
    const double n = static_cast<double>(data_->samples.size());
    const double minLen = std::min(kMinRegionSamples, n);
    const bool loop = loopOn();
    double pos = std::clamp(static_cast<double>(x) / getWidth(), 0.0, 1.0) * n;
    switch (m)
    {
        case Marker::Start: pos = std::clamp(pos, 0.0, std::min(r.end - minLen, loop ? r.loopStart : n)); break;
        case Marker::Loop:  pos = std::clamp(pos, r.start, r.end); break;
        case Marker::End:   pos = std::clamp(pos, std::max(r.start + minLen, loop ? r.loopStart : 0.0), n); break;
    }
    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(pos / n)));
    repaint();
}

void WaveformView::release()
{
    if (active_)
        if (auto* p = parameter(*active_))
            p->endChangeGesture();
    active_.reset();
    candidates_.clear();
}

void WaveformView::visibilityChanged()
{
    if (isVisible())
        startTimerHz(30);
    else
        stopTimer();
}

void WaveformView::timerCallback()
{
    // Nachführen bei Automation, Slot-Wechsel und fertig geladenen Daten.
    const auto latest = slot_ >= 0 ? proc_.slotSampleData(slot_) : nullptr;
    bool changed = latest != data_;
    data_ = latest;
    for (int i = 0; i < 5; ++i)
    {
        const float v = value(kWatched[i]);
        changed = changed || v != seen_[i];
        seen_[i] = v;
    }
    if (changed)
        repaint();
}

void WaveformView::drawMarker(juce::Graphics& g, float x, juce::Colour colour, const char* letter) const
{
    const float h = static_cast<float>(getHeight());
    const float w = static_cast<float>(getWidth());
    g.setColour(colour);
    g.fillRect(juce::Rectangle<float>(std::clamp(x - 0.75f, 0.0f, std::max(0.0f, w - 1.5f)), 0.0f, 1.5f, h));
    const juce::Rectangle<float> handle(std::clamp(x - kHandle * 0.5f, 0.0f, std::max(0.0f, w - kHandle)), 0.0f, kHandle, kHandle);
    g.fillRoundedRectangle(handle, 3.0f);
    g.setColour(colours::padEmpty);
    g.setFont(font(10.0f, true));
    g.drawText(letter, handle, juce::Justification::centred);
}

void WaveformView::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
        return;
    juce::Path body;
    body.addRoundedRectangle(bounds, kCorner);
    g.setColour(colours::padEmpty);
    g.fillPath(body);

    if (!hasData())
    {
        inset_.render(g, body);
        g.setColour(colours::textDim);
        g.setFont(font(13.0f));
        g.drawText(hintText(), bounds, juce::Justification::centred);
        return;
    }

    // Spitzenwerte einmal pro Daten und Breite berechnen.
    if (peaksData_ != data_.get() || peaksWidth_ != getWidth())
    {
        peaks_ = computePeaks(data_->samples, getWidth());
        peaksData_ = data_.get();
        peaksWidth_ = getWidth();
    }

    const auto r = region();
    const float xStart = xFor(r.start);
    const float xEnd = xFor(r.end);
    const float h = bounds.getHeight();
    {
        const juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(body);

        if (loopOn())
        {
            const auto seg = resolveLoopSegment(r, reverseOn(), data_->samples.size());
            g.setColour(colours::latched.withAlpha(0.13f));
            g.fillRect(juce::Rectangle<float>(xFor(seg.lo), 0.0f, xFor(seg.hi) - xFor(seg.lo), h));
        }

        const float mid = h * 0.5f;
        const float half = std::max(1.0f, mid - 4.0f);
        g.setColour(colours::padBase);
        for (std::size_t c = 0; c < peaks_.size(); ++c)
        {
            const float top = mid - peaks_[c].max * half;
            const float bottom = mid - peaks_[c].min * half;
            g.fillRect(juce::Rectangle<float>(static_cast<float>(c), top, 1.0f, std::max(1.0f, bottom - top)));
        }

        // Der nicht gespielte Teil ist abgedunkelt.
        g.setColour(colours::padEmpty.withAlpha(0.72f));
        g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, xStart, h));
        g.fillRect(juce::Rectangle<float>(xEnd, 0.0f, bounds.getWidth() - xEnd, h));
    }
    inset_.render(g, body);

    drawMarker(g, xStart, colours::text, "S");
    drawMarker(g, xEnd, colours::text, "E");
    if (loopOn())
        drawMarker(g, xFor(r.loopStart), colours::latched, "L");
}

} // namespace dg::ui
```

`plugin/CMakeLists.txt`: in `target_sources` nach den `SlotEditor`-Zeilen ergänzen:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/WaveformView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/WaveformView.cpp
```

- [ ] **Step 5: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[waveform]"`
Expected: PASS, 10 Testfälle.

- [ ] **Step 6: Aussehen prüfen**

```powershell
$env:DG_SNAPSHOT_DIR = Join-Path $env:TEMP 'dg-snapshots'
build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[waveform]"
Remove-Item Env:DG_SNAPSHOT_DIR
```

`waveform-loop.png` in `%TEMP%\dg-snapshots` ansehen: vertiefte dunkle Fläche, grüne Kurve, links von S und rechts von E abgedunkelt, blaue Tönung zwischen L und E, drei Griffe mit den Buchstaben S, L, E.

- [ ] **Step 7: Alle Tests, dann Commit**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

```bash
git add plugin/ui/WaveformView.h plugin/ui/WaveformView.cpp plugin/CMakeLists.txt tests/CMakeLists.txt tests/plugin/test_Waveform.cpp
git commit -m "feat(ui): waveform view with draggable start, loop and end markers (#11)"
```

---

### Task 7: Slot-Editor im Sample-Modus

**Files:**
- Modify: `plugin/ui/SlotEditor.h`, `plugin/ui/SlotEditor.cpp`, `plugin/PluginEditor.h`
- Test: `tests/plugin/test_Editor.cpp`, `tests/plugin/test_Look.cpp`

**Interfaces:**
- Consumes: `ui::WaveformView` (Task 6); `SlotField::Loop`, `Reverse`, `LoopXfade` (Task 1); `ui::Toggle`, `ui::Knob`.
- Produces:
  - `SlotEditor::waveform()` (const und nicht const), `SlotEditor::showsLoopControls() const`
  - `DubgefahrenEditor::slotEditor()` (const und nicht const) für Tests

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

An `tests/plugin/test_Editor.cpp` anhängen (Include `#include "plugin/ParameterLayout.h"` ergänzen, falls nicht vorhanden):

```cpp
namespace {
// Processor mit Kit-Ordner und horn.wav in Slot 3 (Index 2).
struct SampleSlotFixture
{
    dgtest::TempDir tmp;
    juce::File kitFile = tmp.dir.getChildFile("Dub.dgkit");
    DubgefahrenProcessor p;
    SampleSlotFixture()
    {
        sampleFolderFor(kitFile).createDirectory();
        juce::WavAudioFormat wav;
        dgtest::writeSine(wav, sampleFolderFor(kitFile).getChildFile("horn.wav"), 441.0f, 48000.0, 4800);
        p.applyKit(makeFactoryKit(), kitFile);
        p.setSlotSample(2, "horn.wav");
        p.waitForSampleLoads();
    }
    void set(SlotField f, float v)
    {
        auto* param = p.state().getParameter(slotParamId(2, f));
        param->setValueNotifyingHost(param->convertTo0to1(v));
    }
};
} // namespace

TEST_CASE("a sample slot shows the waveform and the loop controls", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleSlotFixture f;
    std::unique_ptr<juce::AudioProcessorEditor> editor(f.p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    e->selectSlot(2);
    auto& slotEditor = e->slotEditor();
    CHECK(slotEditor.waveform().isVisible());
    CHECK(slotEditor.waveform().hasData());
    CHECK(slotEditor.showsLoopControls());
    // Streifen über die volle Breite, eine Reihe hoch, unter der Kopfzeile.
    const auto w = slotEditor.waveform().getBounds();
    CHECK(w.getX() == 12);
    CHECK(w.getWidth() == slotEditor.getWidth() - 24);
    CHECK(w.getY() >= 40);
    CHECK(w.getBottom() <= 40 + 84);

    e->selectSlot(0); // Synth-Slot: unverändert
    CHECK_FALSE(slotEditor.waveform().isVisible());
    CHECK_FALSE(slotEditor.showsLoopControls());

    f.p.clearSlot(2);
    e->pollProcessorState();
    e->selectSlot(2); // leerer Slot
    CHECK_FALSE(slotEditor.waveform().isVisible());
    CHECK_FALSE(slotEditor.showsLoopControls());
}

TEST_CASE("latch becomes selectable for a sample slot when loop is on", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleSlotFixture f;
    std::unique_ptr<juce::AudioProcessorEditor> editor(f.p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    e->selectSlot(2);
    CHECK_FALSE(e->slotEditorLatchSelectable());

    f.set(SlotField::Loop, 1.0f); // wie Automation oder ein Klick auf den Schalter
    CHECK(e->slotEditorLatchSelectable());
    CHECK(e->slotEditor().waveform().isMarkerVisible(ui::WaveformView::Marker::Loop));

    f.set(SlotField::Loop, 0.0f);
    CHECK_FALSE(e->slotEditorLatchSelectable());

    // Ein anderer Sample-Slot mit Loop an: der Zustand folgt dem Slot.
    f.p.setSlotSample(4, "horn.wav");
    auto* loop5 = f.p.state().getParameter(slotParamId(4, SlotField::Loop));
    loop5->setValueNotifyingHost(1.0f);
    e->pollProcessorState();
    e->selectSlot(4);
    CHECK(e->slotEditorLatchSelectable());
    e->selectSlot(2);
    CHECK_FALSE(e->slotEditorLatchSelectable());
    e->selectSlot(0); // Synth: Latch immer wählbar
    CHECK(e->slotEditorLatchSelectable());
}

TEST_CASE("the waveform follows a sample that finishes loading or goes missing", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleSlotFixture f;
    std::unique_ptr<juce::AudioProcessorEditor> editor(f.p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    e->selectSlot(2);
    REQUIRE(e->slotEditor().waveform().hasData());

    f.p.setSlotSample(2, "gone.wav");
    f.p.waitForSampleLoads();
    e->pollProcessorState();
    CHECK_FALSE(e->slotEditor().waveform().hasData());
    CHECK(e->slotEditor().waveform().hintText() == "Sample missing");

    f.p.setSlotSample(2, "horn.wav");
    f.p.waitForSampleLoads();
    e->pollProcessorState();
    CHECK(e->slotEditor().waveform().hasData());
}
```

An `tests/plugin/test_Look.cpp` anhängen (Includes `#include "plugin/SampleFiles.h"`, `#include "engine/Kit.h"` und `#include "SampleTestHelpers.h"` ergänzen, falls nicht vorhanden):

```cpp
TEST_CASE("the editor paints a sample slot at every window scale", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    const auto kitFile = tmp.dir.getChildFile("Dub.dgkit");
    sampleFolderFor(kitFile).createDirectory();
    juce::WavAudioFormat wav;
    dgtest::writeSine(wav, sampleFolderFor(kitFile).getChildFile("horn.wav"), 441.0f, 48000.0, 4800);

    for (const float scale : { 0.75f, 1.0f, 2.0f })
    {
        DubgefahrenProcessor p;
        p.applyKit(makeFactoryKit(), kitFile);
        p.setSlotSample(2, "horn.wav");
        p.setSlotSample(3, "gone.wav");
        p.waitForSampleLoads();
        SlotParams sp = readSlotFromParameters(p.state(), 2);
        sp.sampleStart = 0.1f;
        sp.loopStart = 0.4f;
        sp.sampleEnd = 0.8f;
        sp.loop = true;
        p.setSlot(2, sp, "horn", "horn.wav");
        p.waitForSampleLoads();
        p.setUiScale(scale);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* e = static_cast<DubgefahrenEditor*>(editor.get());

        e->selectSlot(2);
        const auto img = dgtest::snapshot(*editor);
        REQUIRE(img.isValid());
        dgtest::savePng(img, "editor-sample-" + juce::String(scale, 2));

        e->selectSlot(3); // fehlendes Sample: Hinweis statt Kurve
        const auto missing = dgtest::snapshot(*editor);
        REQUIRE(missing.isValid());
        dgtest::savePng(missing, "editor-sample-missing-" + juce::String(scale, 2));
    }
}
```

- [ ] **Step 2: Build ausführen und das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL, `slotEditor` ist kein Member von `DubgefahrenEditor`.

- [ ] **Step 3: `SlotEditor.h` erweitern**

Include `#include "plugin/ui/WaveformView.h"` ergänzen. Im öffentlichen Teil nach `juce::Component& sampleButton() { return sampleButton_; }`:

```cpp
    WaveformView& waveform() { return waveform_; }
    const WaveformView& waveform() const { return waveform_; }
    bool showsLoopControls() const { return loop_.isVisible() && reverse_.isVisible() && xfade_.isVisible(); }
```

Im privaten Teil vor `Mode mode_ = Mode::Synth;`:

```cpp
    void updateLatchItem();
```

und nach `Knob tune_ { u8("Tune") };`:

```cpp
    Toggle loop_ { u8("Loop") };
    Toggle reverse_ { u8("Reverse") };
    Knob xfade_ { u8("X-Fade") };
    WaveformView waveform_;
```

- [ ] **Step 4: `SlotEditor.cpp` anpassen**

Die Konstanten für den Sample-Modus ersetzen:

```cpp
// Sample-Modus: Reihe 0 ist die Wellenform, darunter drei Reihen Regler.
constexpr int kNumSampleRows = 3;
const char* const kSampleRowNames[kNumSampleRows] = { "SAMPLE\nAMP", "LOOP\nTRIG", "MIX" };
constexpr int kWaveformInset = 4; // Abstand der Wellenform zum oberen und unteren Rand ihrer Reihe
```

Konstruktor: die Initialisierungsliste wird `SlotEditor::SlotEditor(DubgefahrenProcessor& proc) : proc_(proc), waveform_(proc)`. Da `waveform_` nach `proc_` deklariert ist, stimmt die Reihenfolge. Die Zeilen ab `sampleControls_ = …` bis `addChildComponent(tune_);` ersetzen durch:

```cpp
    sampleControls_ = { &waveform_, &tune_, &attack_, &release_, &loop_, &reverse_, &xfade_, &trigMode_, &choke_, &vol_, &pan_, &send_ };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &waveform_, &tune_, &loop_, &reverse_, &xfade_ })
        addChildComponent(*c);
    // Der Latch-Eintrag und die Wellenform folgen dem Loop-Schalter, auch bei Automation.
    loop_.button.onStateChange = [this] {
        updateLatchItem();
        waveform_.repaint();
    };
    reverse_.button.onStateChange = [this] { waveform_.repaint(); };
```

`setSlot`: nach `tune_.attach(...)` ergänzen:

```cpp
    loop_.attach(s, slotParamId(slot, SlotField::Loop));
    reverse_.attach(s, slotParamId(slot, SlotField::Reverse));
    xfade_.attach(s, slotParamId(slot, SlotField::LoopXfade));
    waveform_.setSlot(slot);
```

`refresh`: die Zeile `tune_.setVisible(false);` ersetzen durch:

```cpp
    for (auto* c : sampleControls_)
        if (std::find(controls_.begin(), controls_.end(), c) == controls_.end())
            c->setVisible(false); // nur im Sample-Modus sichtbare Controls
```

und die Zeile `trigMode_.box.setItemEnabled(kLatchItemId, mode_ != Mode::Sample); // …` ersetzen durch:

```cpp
    waveform_.refresh();
    updateLatchItem();
```

`#include <algorithm>` ergänzen. Neue Funktion nach `refresh`:

```cpp
void SlotEditor::updateLatchItem()
{
    // Latch gilt bei Samples nur für geloopte Slots; sonst wirkt es wie Gate und ist nicht wählbar.
    trigMode_.box.setItemEnabled(kLatchItemId, mode_ != Mode::Sample || loop_.button.getToggleState());
}
```

`paint`: die Schleife über die Zeilenbeschriftungen ersetzen durch:

```cpp
    const bool sample = mode_ == Mode::Sample;
    const int rows = sample ? kNumSampleRows : kNumRows;
    const int firstRow = sample ? 1 : 0; // im Sample-Modus gehört Reihe 0 der Wellenform
    for (int row = 0; row < rows; ++row)
        g.drawFittedText(sample ? kSampleRowNames[row] : kRowNames[row], 12, kTopOffset + (row + firstRow) * kRowHeight,
                         kRowLabelWidth - 12, kRowHeight, juce::Justification::centredLeft, 2);
```

(Die bisherige Zeile `const bool sample = mode_ == Mode::Sample;` und `const int rows = …` entfallen dabei, sie stehen jetzt im Ersatz.)

`resized`: den Zweig `if (mode_ == Mode::Sample) { … }` ersetzen durch:

```cpp
    if (mode_ == Mode::Sample)
    {
        waveform_.setBounds(12, kTopOffset + kWaveformInset, getWidth() - 24, kRowHeight - 2 * kWaveformInset);
        place(1, 0, tune_);  place(1, 1, attack_);  place(1, 2, release_);
        place(2, 0, loop_);  place(2, 1, reverse_); place(2, 2, xfade_); place(2, 3, trigMode_); place(2, 4, choke_);
        place(3, 0, vol_);   place(3, 1, pan_);     place(3, 2, send_);
    }
```

- [ ] **Step 5: Zugriff für Tests im Editor**

`plugin/PluginEditor.h`, nach `juce::String lastMessage() const { return lastMessage_; }`:

```cpp
    // Für Tests: der Slot-Editor mit Wellenform und Loop-Controls.
    ui::SlotEditor& slotEditor() { return slotEditor_; }
    const ui::SlotEditor& slotEditor() const { return slotEditor_; }
```

- [ ] **Step 6: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[editor],[look]"`
Expected: PASS, auch der bestehende Fall „a sample slot shows the sample controls, its file and no latch“.

Folgt der Latch-Eintrag einer Parameteränderung nicht (Test „latch becomes selectable …“), feuert `onStateChange` des Schalters bei einer Änderung durch die Attachment nicht: dann zusätzlich in `SlotEditor` einen `juce::AudioProcessorValueTreeState::Listener` auf den Loop-Parameter des aktuellen Slots registrieren, der `updateLatchItem()` und `waveform_.repaint()` über `juce::MessageManager::callAsync` mit einem `SafePointer` aufruft; der Test ruft danach `e->pollProcessorState()` nicht, deshalb im Listener bei Aufruf auf dem Message-Thread direkt handeln.

- [ ] **Step 7: Aussehen prüfen**

```powershell
$env:DG_SNAPSHOT_DIR = Join-Path $env:TEMP 'dg-snapshots'
build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "the editor paints a sample slot at every window scale"
Remove-Item Env:DG_SNAPSHOT_DIR
```

`editor-sample-0.75.png`, `editor-sample-1.00.png`, `editor-sample-2.00.png` und `editor-sample-missing-1.00.png` ansehen:
- Wellenform als Streifen über die volle Breite unter der Kopfzeile; S, L, E sichtbar, L blau; Tönung zwischen L und E.
- Drei Reihen darunter: `SAMPLE AMP` (Tune, Attack, Release), `LOOP TRIG` (Loop, Reverse, X-Fade, Mode, Choke), `MIX` (Volume, Pan, FX-Send). Nichts überlappt, nichts ragt unten aus dem Panel.
- X-Fade zeigt zwei Nachkommastellen („5.00“).
- Beim fehlenden Sample steht „Sample missing“ im Streifen.
- Ein Synth-Slot (`editor-1.00.png` aus dem bestehenden Test) sieht aus wie vorher.

Ragt die unterste Reihe aus dem Panel: **stoppen und melden**; die vier Reihen sind dieselben 4 × 84 px wie im Synth-Modus und müssen passen.

- [ ] **Step 8: Alle Tests, dann Commit**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

```bash
git add plugin/ui/SlotEditor.h plugin/ui/SlotEditor.cpp plugin/PluginEditor.h tests/plugin/test_Editor.cpp tests/plugin/test_Look.cpp
git commit -m "feat(ui): sample slot editor with waveform strip, loop, reverse and x-fade (#11)"
```

---

### Task 8: Gesamtprüfung und Übergabe zur Abnahme

**Files:**
- Modify: `docs/superpowers/specs/2026-09-30-sample-regions-design.md` (nur Status)

**Interfaces:**
- Consumes: alles aus Task 1 bis 7.

- [ ] **Step 1: Aufräum-Prüfungen**

Run: `git grep -n "juce" -- engine`
Expected: keine Treffer (die Engine bleibt JUCE-frei).

Run: `git grep -n "FontOptions(" -- plugin`
Expected: nur `plugin/ui/Fonts.cpp`.

Run: `git diff main -- engine/PadRouter.cpp | findstr /C:"previewOn" /C:"previewOff"`
Expected: keine Treffer (die Maus-Vorschau ist unverändert).

Run: `git diff main --stat -- plugin/ui/PadGrid.cpp plugin/ui/FxPanel.cpp plugin/ui/PerformancePanel.cpp plugin/ui/DgLookAndFeel.cpp`
Expected: leer.

- [ ] **Step 2: Alle Tests und Validator**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
Expected: VST3-Validator 47/47 bestanden; pluginval bestanden oder als „nicht gefunden – übersprungen“ gemeldet.

- [ ] **Step 3: Spec-Status setzen und committen**

In `docs/superpowers/specs/2026-09-30-sample-regions-design.md` die Zeile `**Status:** Entwurf zur Freigabe` ersetzen durch `**Status:** Umgesetzt, Abnahme in Ableton offen`.

```bash
git add docs/superpowers/specs/2026-09-30-sample-regions-design.md
git commit -m "docs: mark sample regions spec as implemented (#11)"
```

- [ ] **Step 4: Übergabe zur Abnahme von Hand**

Das Bundle liegt unter `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3`. Installation durch Kopieren des Bundles nach `%CommonProgramFiles%\VST3` (Adminrechte); `build.ps1 install` nicht verwenden.

Checkliste für Ableton, dem Nutzer vorlegen:

1. Ein Sample-Slot zeigt die Wellenform; S und E lassen sich ziehen, der abgedunkelte Teil wird nicht gespielt.
2. Loop an: L erscheint, der geloopte Abschnitt ist blau getönt; die Schleife läuft bei gehaltener Note (Gate).
3. X-Fade: bei 0 % ist der Schnitt hörbar, bei höheren Werten läuft die Schleife ohne Knacken.
4. Reverse ohne Loop spielt den Bereich einmal rückwärts; mit Loop kreist der Abschnitt zwischen S und L, und die Tönung zeigt ihn.
5. Latch: Bei Loop an ist „Latch“ wählbar; erste Note startet, zweite stoppt; der blaue Punkt auf dem Pad erscheint und erlischt.
6. One Shot spielt den Bereich einmal, auch wenn Loop an ist.
7. Marker während des Spielens ziehen: kein hartes Knacken.
8. Automation von Start, Loop-Start, End und X-Fade wird aufgezeichnet und abgespielt; die Marker folgen.
9. Ein anderes Sample im Slot wählen: die Marker stehen wieder auf der ganzen Datei, Loop und Reverse sind aus.
10. Ein Projekt und ein Kit aus Version 0.4.0 laden und klingen unverändert.
11. Kit exportieren, Plugin neu laden, Kit importieren: Marker, Loop, Reverse und X-Fade sind erhalten.
