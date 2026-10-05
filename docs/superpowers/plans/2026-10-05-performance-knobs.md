# Performance-Knobs Space, Grit, Throw und Live-Ansicht Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Drei Performance-Knobs (Space, Grit, Throw) koppeln die Effekt-Einzelwerte als Aufschlag; ein Umschalter im Header wechselt zwischen einer kompakten Live-Ansicht und der vollen Edit-Ansicht mit aufklappbarem Advanced-Bereich.

**Architecture:** Eine reine Funktion `applyMacros` (`engine/Macros.h/.cpp`) rechnet die Einzelwerte plus Aufschläge zu wirksamen Werten um; `FxChain` glättet die Knobs und wendet sie pro Block an, `Engine` addiert den Throw-Aufschlag auf den FX-Send. Das Plugin bekommt drei Host-Parameter und zwei nicht-automatierbare Zustände (Live/Edit, Advanced auf/zu). Die Effektleiste wird in `LivePanel` (Knobs, Filter, Delay-Zeit, Master) und `AdvancedPanel` (übrige Einzelregler) geteilt, `PerformancePanel` passt seine Anordnung an, der Editor kennt drei Layouts.

**Tech Stack:** C++20, JUCE (Plugin-Teil), Catch2 (`DubgefahrenTests`, `DubgefahrenPluginTests`), CMake über `build.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-05-performance-knobs-design.md`

## Global Constraints

- Neue Host-Parameter (automatierbar, 0 bis 1, Standard 0): `Space` (ID `fxSpace`), `Grit` (`fxGrit`), `Throw` (`fxThrow`), Versionshinweis **5**. Danach 26 globale und 442 Parameter insgesamt.
- Aufschläge bei Knob = 1 (jeweils linear mit dem Knob, begrenzt): Space: Delay Mix +0,35 (bis 1), Delay Feedback +0,30 (bis 0,95), Reverb Mix +0,40 (bis 1), Reverb Decay +0,30 (bis 1). Grit: Drive +0,70 (bis 1), Delay Wow +0,50 (bis 1), Delay Tone −0,30 (nicht unter 0), Phaser Mix +0,60 (bis 1), Phaser Depth +0,30 (bis 1). Throw: FX-Send aller Slots +1 (bis 1), Delay Feedback +0,50 (bis 1,10), Delay Mix +0,50 (bis 1).
- Regel „nie unter dem Grundwert“: wirksam = `max(Grundwert, min(Grundwert + Aufschlag, Grenze))`; nur Tone wird gesenkt, aber nie unter 0.
- Bei allen Knobs auf 0 ist alles bit-genau wie vorher. Die Einzelparameter werden nie verändert.
- Knobs werden in `FxChain` mit 20 ms geglättet; der FX-Send-Aufschlag läuft über die bestehende Send-Glättung.
- Alle Faktoren und Obergrenzen stehen als benannte Werte oben in `engine/Macros.cpp`.
- Layouts (Basisgröße): Edit mit Advanced 1200 × 744, Edit ohne Advanced 1200 × 640, Live 880 × 460. Skalierung 75 bis 200 % und das feste Seitenverhältnis gelten pro Layout. Alte Projekte öffnen in Edit mit Advanced.
- Ansicht und Advanced-Zustand liegen im Plugin-Zustand (ValueTree-Eigenschaften `liveView` und `advancedOpen`, keine Host-Parameter).
- Nicht enthalten: einstellbare Kurven, MIDI-Learn, tempo-synchroner Throw, Hochformat, Throw-Taster.
- Build und Test: `powershell -ExecutionPolicy Bypass -File build.ps1 test`. Nur Engine-Tests: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, danach `build\tests\Release\DubgefahrenTests.exe "[macros]"` (Plugin-Tests: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe`).
- Nach grünen Tests auf diesem Branch: die Zip des VST3-Ordners nach `C:\Users\atzet\OneDrive\Dokumente\musik\dubgefahren\dubgefahren.vst3.feature-performance-knobs.zip` legen (`Compress-Archive -Path E:\Repos\dubgefahren\build\plugin\Dubgefahren_artefacts\Release\VST3 -DestinationPath <Zip> -Force`); das macht der Controller im letzten Schritt.
- Commit-Messages enden mit der Zeile `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

1. Knobs auf 0: `applyMacros` ist die Identität, und die Kette klingt bit-genau wie ohne Knobs (Task 1 und 2).
2. „Nie unter dem Grundwert“: ein Grundwert über der Obergrenze (zum Beispiel Feedback 1,05 bei Space) wird nicht abgesenkt; Tone bleibt nie unter 0; ungültige Knobwerte (negativ, über 1, NaN) werden begrenzt (Task 1).
3. Extremwerte: alle Knobs auf 1 mit Extremeinstellungen (Feedback 1,1, Drive 1, Mix 1) bleiben endlich und unter dem Limiter-Pegel (Task 2).
4. Layouts: in jedem Layout liegen alle sichtbaren Regler vollständig im Fenster und überlappen nicht; ausgeblendete Panels sind weder sichtbar noch bedienbar; das Umschalten erhält die Skalierung; stellt der Host den Zustand wieder her, während der Editor offen ist, folgt das Layout (Task 4 und 5).
5. Alte Plugin-Zustände ohne die neuen Eigenschaften und Parameter öffnen in Edit mit Advanced und Knobs auf 0 (Task 3).

---

### Task 1: `applyMacros` in der Engine

**Files:**
- Create: `engine/Macros.h`, `engine/Macros.cpp`
- Create: `tests/test_Macros.cpp`
- Modify: `engine/CMakeLists.txt` (Quellen), `tests/CMakeLists.txt` (Testdatei)

**Interfaces:**
- Produces: `struct dg::MacroParams { float space = 0.0f; float grit = 0.0f; float throwAmount = 0.0f; };`, `dg::FxParams dg::applyMacros(const FxParams& base, const MacroParams& m)`, `float dg::throwSendBoost(const MacroParams& m)` (Aufschlag auf den FX-Send, 0 bis 1). Task 2 bis 3 bauen darauf auf.

- [ ] **Step 1: Failing tests schreiben**

`tests/test_Macros.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include "engine/FxParams.h"
#include "engine/Macros.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
FxParams sampleBase()
{
    FxParams p;
    p.drive = 0.2f;
    p.cutoffHz = 900.0f;
    p.resonance = 0.3f;
    p.filterType = 0.5f;
    p.delayDiv = DelayDivision::D1_4;
    p.delayFeedback = 0.45f;
    p.delayTone = 0.5f;
    p.delayWow = 0.2f;
    p.delayMix = 0.35f;
    p.reverbDecay = 0.5f;
    p.reverbTone = 0.4f;
    p.reverbMix = 0.25f;
    p.phaserRate = 0.7f;
    p.phaserDepth = 0.5f;
    p.phaserMix = 0.0f;
    p.masterDb = -3.0f;
    return p;
}

bool same(const FxParams& a, const FxParams& b)
{
    return a.drive == b.drive && a.cutoffHz == b.cutoffHz && a.resonance == b.resonance &&
           a.filterType == b.filterType && a.delayDiv == b.delayDiv && a.delayFeedback == b.delayFeedback &&
           a.delayTone == b.delayTone && a.delayWow == b.delayWow && a.delayMix == b.delayMix &&
           a.reverbDecay == b.reverbDecay && a.reverbTone == b.reverbTone && a.reverbMix == b.reverbMix &&
           a.phaserRate == b.phaserRate && a.phaserDepth == b.phaserDepth && a.phaserMix == b.phaserMix &&
           a.masterDb == b.masterDb;
}
} // namespace

TEST_CASE("knobs at zero leave every parameter untouched", "[macros]")
{
    const auto base = sampleBase();
    CHECK(same(applyMacros(base, MacroParams {}), base));
    CHECK(throwSendBoost(MacroParams {}) == 0.0f);
}

TEST_CASE("space opens delay and reverb but only up to feedback 0.95", "[macros]")
{
    const auto base = sampleBase();
    const auto e = applyMacros(base, MacroParams { 1.0f, 0.0f, 0.0f });
    CHECK_THAT(e.delayMix, WithinAbs(0.70, 1e-5));
    CHECK_THAT(e.delayFeedback, WithinAbs(0.75, 1e-5));
    CHECK_THAT(e.reverbMix, WithinAbs(0.65, 1e-5));
    CHECK_THAT(e.reverbDecay, WithinAbs(0.80, 1e-5));
    CHECK(e.drive == base.drive);
    CHECK(e.phaserMix == base.phaserMix);
    CHECK(e.masterDb == base.masterDb);

    const auto half = applyMacros(base, MacroParams { 0.5f, 0.0f, 0.0f });
    CHECK_THAT(half.delayMix, WithinAbs(0.525, 1e-5));
    CHECK_THAT(half.reverbMix, WithinAbs(0.45, 1e-5));

    auto high = base;
    high.delayFeedback = 0.9f;
    CHECK_THAT(applyMacros(high, MacroParams { 1.0f, 0.0f, 0.0f }).delayFeedback, WithinAbs(0.95, 1e-5));
}

TEST_CASE("grit adds saturation, wobble, darker tone and phaser", "[macros]")
{
    const auto base = sampleBase();
    const auto e = applyMacros(base, MacroParams { 0.0f, 1.0f, 0.0f });
    CHECK_THAT(e.drive, WithinAbs(0.90, 1e-5));
    CHECK_THAT(e.delayWow, WithinAbs(0.70, 1e-5));
    CHECK_THAT(e.delayTone, WithinAbs(0.20, 1e-5));
    CHECK_THAT(e.phaserMix, WithinAbs(0.60, 1e-5));
    CHECK_THAT(e.phaserDepth, WithinAbs(0.80, 1e-5));
    CHECK(e.delayMix == base.delayMix);
    CHECK(e.reverbMix == base.reverbMix);

    auto loud = base;
    loud.drive = 0.6f;
    loud.delayTone = 0.1f;
    const auto c = applyMacros(loud, MacroParams { 0.0f, 1.0f, 0.0f });
    CHECK_THAT(c.drive, WithinAbs(1.0, 1e-6));  // begrenzt
    CHECK_THAT(c.delayTone, WithinAbs(0.0, 1e-6)); // nie unter 0
}

TEST_CASE("throw pushes feedback up to self-oscillation and the send boost to 1", "[macros]")
{
    const auto base = sampleBase();
    const auto e = applyMacros(base, MacroParams { 0.0f, 0.0f, 1.0f });
    CHECK_THAT(e.delayFeedback, WithinAbs(0.95, 1e-5));
    CHECK_THAT(e.delayMix, WithinAbs(0.85, 1e-5));
    CHECK(e.reverbMix == base.reverbMix);
    CHECK_THAT(throwSendBoost(MacroParams { 0.0f, 0.0f, 1.0f }), WithinAbs(1.0, 1e-6));
    CHECK_THAT(throwSendBoost(MacroParams { 0.0f, 0.0f, 0.5f }), WithinAbs(0.5, 1e-6));

    auto fb = base;
    fb.delayFeedback = 0.8f;
    fb.delayMix = 0.8f;
    const auto c = applyMacros(fb, MacroParams { 0.0f, 0.0f, 1.0f });
    CHECK_THAT(c.delayFeedback, WithinAbs(1.10, 1e-5)); // Obergrenze
    CHECK_THAT(c.delayMix, WithinAbs(1.0, 1e-6));

    // Space und Throw zusammen: Space bis 0,95, dann Throw bis 1,10.
    const auto both = applyMacros(base, MacroParams { 1.0f, 0.0f, 1.0f });
    CHECK_THAT(both.delayFeedback, WithinAbs(1.10, 1e-5));
}

TEST_CASE("a knob never lowers a value that is already above its ceiling", "[macros]")
{
    auto base = sampleBase();
    base.delayFeedback = 1.05f;
    CHECK_THAT(applyMacros(base, MacroParams { 1.0f, 0.0f, 0.0f }).delayFeedback, WithinAbs(1.05, 1e-6));
    base.delayFeedback = 1.10f;
    CHECK_THAT(applyMacros(base, MacroParams { 1.0f, 0.0f, 0.0f }).delayFeedback, WithinAbs(1.10, 1e-6));

    // Über ein Raster: außer Tone wird nichts gesenkt, Tone wird nie angehoben.
    for (const float fb : { 0.0f, 0.45f, 0.95f, 1.1f })
        for (const float s : { 0.0f, 0.25f, 0.5f, 1.0f })
            for (const float g : { 0.0f, 0.25f, 0.5f, 1.0f })
                for (const float t : { 0.0f, 0.25f, 0.5f, 1.0f })
                {
                    auto b = sampleBase();
                    b.delayFeedback = fb;
                    const auto e = applyMacros(b, MacroParams { s, g, t });
                    CHECK(e.delayFeedback >= b.delayFeedback);
                    CHECK(e.delayMix >= b.delayMix);
                    CHECK(e.reverbMix >= b.reverbMix);
                    CHECK(e.reverbDecay >= b.reverbDecay);
                    CHECK(e.drive >= b.drive);
                    CHECK(e.delayWow >= b.delayWow);
                    CHECK(e.phaserMix >= b.phaserMix);
                    CHECK(e.phaserDepth >= b.phaserDepth);
                    CHECK(e.delayTone <= b.delayTone);
                    CHECK(e.delayTone >= 0.0f);
                    CHECK(e.delayFeedback <= std::max(b.delayFeedback, 1.10f));
                }
}

TEST_CASE("invalid knob values are clamped", "[macros]")
{
    const auto base = sampleBase();
    const auto one = applyMacros(base, MacroParams { 1.0f, 1.0f, 1.0f });
    CHECK(same(applyMacros(base, MacroParams { 5.0f, 5.0f, 5.0f }), one));
    CHECK(same(applyMacros(base, MacroParams { -1.0f, -2.0f, -3.0f }), base));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(same(applyMacros(base, MacroParams { nan, nan, nan }), base));
    CHECK(throwSendBoost(MacroParams { 0.0f, 0.0f, nan }) == 0.0f);
    CHECK(throwSendBoost(MacroParams { 0.0f, 0.0f, 7.0f }) == 1.0f);
}
```

In `tests/CMakeLists.txt` nach `test_Phaser.cpp` die Zeile `    test_Macros.cpp` einfügen. In `engine/CMakeLists.txt` in der Zeile mit `FxParams.h FxParams.cpp Phaser.h Phaser.cpp TapeDelay.h TapeDelay.cpp` die Einträge `Macros.h Macros.cpp` ergänzen.

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Fehler, `engine/Macros.h` existiert nicht.

- [ ] **Step 3: Implementieren**

`engine/Macros.h`:

```cpp
#pragma once
#include "engine/FxParams.h"

namespace dg {

// Die drei Performance-Knobs (je 0 bis 1). Sie sind ein Aufschlag auf die Einzelwerte der Effekte:
// bei 0 ändert sich nichts, die Einzelparameter selbst werden nie verändert.
struct MacroParams
{
    float space = 0.0f;       // Delay und Reverb weiter und tiefer
    float grit = 0.0f;        // Sättigung, Wow, dunklere Echos, Phaser
    float throwAmount = 0.0f; // Dub-Wurf: mehr FX-Send, Delay bis zur Selbstoszillation
};

// Wirksame Effektwerte: Einzelwerte plus Aufschläge, jeweils begrenzt. Ein Einzelwert, der schon über
// der Obergrenze eines Knobs liegt, wird nicht abgesenkt.
FxParams applyMacros(const FxParams& base, const MacroParams& m);

// Aufschlag (0 bis 1), den Throw zum FX-Send jedes Slots addiert.
float throwSendBoost(const MacroParams& m);

} // namespace dg
```

`engine/Macros.cpp`:

```cpp
#include "engine/Macros.h"
#include <algorithm>
#include <cmath>

namespace dg {

namespace {
// Aufschläge bei Knob = 1 und Obergrenzen. Hier nachjustieren, nicht im Code verteilt.
constexpr float kSpaceDelayMix = 0.35f;
constexpr float kSpaceFeedback = 0.30f;
constexpr float kSpaceFeedbackCap = 0.95f; // Space schwingt nie von selbst
constexpr float kSpaceReverbMix = 0.40f;
constexpr float kSpaceReverbDecay = 0.30f;

constexpr float kGritDrive = 0.70f;
constexpr float kGritWow = 0.50f;
constexpr float kGritToneDarker = 0.30f;
constexpr float kGritPhaserMix = 0.60f;
constexpr float kGritPhaserDepth = 0.30f;

constexpr float kThrowSend = 1.0f;
constexpr float kThrowFeedback = 0.50f;
constexpr float kThrowFeedbackCap = 1.10f; // nur Throw darf bis zur Selbstoszillation
constexpr float kThrowDelayMix = 0.50f;

float unit(float x) { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f; }

// Hebt base um add an, höchstens bis cap; ein Grundwert über cap bleibt unverändert.
float raise(float base, float add, float cap) { return std::max(base, std::min(base + add, cap)); }
} // namespace

FxParams applyMacros(const FxParams& base, const MacroParams& m)
{
    const float space = unit(m.space);
    const float grit = unit(m.grit);
    const float thr = unit(m.throwAmount);

    FxParams e = base;

    e.delayMix = raise(e.delayMix, kSpaceDelayMix * space, 1.0f);
    e.delayFeedback = raise(e.delayFeedback, kSpaceFeedback * space, kSpaceFeedbackCap);
    e.reverbMix = raise(e.reverbMix, kSpaceReverbMix * space, 1.0f);
    e.reverbDecay = raise(e.reverbDecay, kSpaceReverbDecay * space, 1.0f);

    e.drive = raise(e.drive, kGritDrive * grit, 1.0f);
    e.delayWow = raise(e.delayWow, kGritWow * grit, 1.0f);
    e.delayTone = std::max(0.0f, e.delayTone - kGritToneDarker * grit);
    e.phaserMix = raise(e.phaserMix, kGritPhaserMix * grit, 1.0f);
    e.phaserDepth = raise(e.phaserDepth, kGritPhaserDepth * grit, 1.0f);

    e.delayFeedback = raise(e.delayFeedback, kThrowFeedback * thr, kThrowFeedbackCap);
    e.delayMix = raise(e.delayMix, kThrowDelayMix * thr, 1.0f);
    return e;
}

float throwSendBoost(const MacroParams& m) { return kThrowSend * unit(m.throwAmount); }

} // namespace dg
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[macros]"`
Expected: alle Tests grün. Bei Abweichung die gemessenen Werte notieren und melden, nicht blind lockern.

- [ ] **Step 5: Mutation prüfen**

Kurz in `Macros.cpp` `raise` ersetzen durch `return std::min(base + add, cap);` (ohne `max(base, …)`), bauen, `[macros]` laufen lassen. Erwartet: `a knob never lowers a value ...` schlägt fehl (Feedback 1,05 wird auf 0,95 gesenkt). Danach zurücksetzen (`git checkout engine/Macros.cpp` nach dem Commit).

- [ ] **Step 6: Commit**

```bash
git add engine/Macros.h engine/Macros.cpp engine/CMakeLists.txt tests/test_Macros.cpp tests/CMakeLists.txt
git commit -m "feat(engine): performance macros Space, Grit and Throw as an additive layer"
```

(Mutationsprüfung aus Step 5 nach dem Commit ausführen und mit `git checkout engine/Macros.cpp` zurücksetzen.)

---

### Task 2: `FxChain` und `Engine` wenden die Knobs an

**Files:**
- Modify: `engine/Engine.h` (`GlobalParams`), `engine/Engine.cpp` (FX-Aufruf, Send-Aufschlag), `engine/FxChain.h`, `engine/FxChain.cpp`
- Test: `tests/test_FxChain.cpp`, `tests/test_Engine.cpp`

**Interfaces:**
- Consumes: `MacroParams`, `applyMacros`, `throwSendBoost` (Task 1).
- Produces: `GlobalParams::macros` (`MacroParams`); `FxChain::process(..., const FxParams& p, double bpm, const MacroParams& macros = {})`. Task 3 füllt `GlobalParams::macros` aus den Host-Parametern.

- [ ] **Step 1: Failing tests schreiben**

An `tests/test_FxChain.cpp` anhängen (`neutral()`, `Buses`, `run` mit Standardmakros gibt es; hier ein eigener Lauf mit Knobs):

```cpp
namespace {
void runWithMacros(FxChain& fx, Buses& b, const FxParams& p, const MacroParams& m, int block = 512)
{
    const int n = static_cast<int>(b.ml.size());
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min(block, n - pos);
        fx.process(b.ml.data() + pos, b.mr.data() + pos, b.sl.data() + pos, b.sr.data() + pos, len, p, 120.0, m);
    }
}
} // namespace

TEST_CASE("knobs at zero sound exactly like no knobs", "[fxchain][macros]")
{
    auto p = neutral();
    p.delayMix = 0.5f;
    p.reverbMix = 0.3f;
    p.drive = 0.3f;
    p.phaserMix = 0.4f;

    FxChain a, b;
    a.prepare(kSr);
    b.prepare(kSr);
    Buses ba(24000), bb(24000);
    ba.ml = dgtest::sine(440.0f, kSr, 24000, 0.5f);
    ba.mr = ba.ml;
    ba.sl = dgtest::noise(24000, 0.3f, 7);
    ba.sr = ba.sl;
    bb = ba;
    run(a, ba, p);
    runWithMacros(b, bb, p, MacroParams {});
    CHECK(ba.ml == bb.ml);
    CHECK(ba.mr == bb.mr);
}

TEST_CASE("space makes the echo louder, throw makes the repeats stronger", "[fxchain][macros]")
{
    auto p = neutral();
    p.delayMix = 0.2f;
    p.delayFeedback = 0.3f;
    p.delayTone = 1.0f;
    p.delayWow = 0.0f;
    p.delayDiv = DelayDivision::D1_16T; // 4000 Samples bei 120 bpm

    auto echoes = [&](const MacroParams& m) {
        FxChain fx;
        fx.prepare(kSr);
        Buses b(16000);
        b.sl[0] = b.sr[0] = 1.0f;
        runWithMacros(fx, b, p, m);
        return std::vector<float> { dgtest::peakAbs(b.ml, 3990, 4300), dgtest::peakAbs(b.ml, 7990, 8300) };
    };
    const auto base = echoes(MacroParams {});
    const auto space = echoes(MacroParams { 1.0f, 0.0f, 0.0f });
    const auto thr = echoes(MacroParams { 0.0f, 0.0f, 1.0f });
    CHECK(space[0] > 2.0f * base[0]);                 // erstes Echo lauter
    CHECK(thr[1] / thr[0] > 2.0f * (base[1] / base[0])); // zweites Echo relativ viel stärker
}

TEST_CASE("all knobs at maximum with extreme settings stay finite and below the ceiling", "[fxchain][macros]")
{
    FxParams p;
    p.drive = 1.0f;
    p.cutoffHz = 1000.0f;
    p.resonance = 1.0f;
    p.delayFeedback = 1.1f;
    p.delayWow = 1.0f;
    p.delayMix = 1.0f;
    p.reverbDecay = 1.0f;
    p.reverbMix = 1.0f;
    p.phaserMix = 1.0f;
    p.phaserDepth = 1.0f;
    p.masterDb = 6.0f;

    FxChain fx;
    fx.prepare(kSr);
    Buses b(48000 * 20);
    b.ml = dgtest::noise(48000 * 20, 0.5f, 3);
    b.mr = dgtest::noise(48000 * 20, 0.5f, 4);
    b.sl = dgtest::noise(48000 * 20, 0.5f, 5);
    b.sr = dgtest::noise(48000 * 20, 0.5f, 6);
    runWithMacros(fx, b, p, MacroParams { 1.0f, 1.0f, 1.0f });
    CHECK(dgtest::allFinite(b.ml));
    CHECK(dgtest::allFinite(b.mr));
    CHECK(dgtest::peakAbs(b.ml) <= kLimiterCeiling + 1.0e-6f);
    CHECK(dgtest::peakAbs(b.mr) <= kLimiterCeiling + 1.0e-6f);
}

TEST_CASE("a knob jump does not click", "[fxchain][macros]")
{
    auto p = neutral();
    p.delayMix = 0.3f;
    p.delayFeedback = 0.5f;
    p.delayDiv = DelayDivision::D1_8;

    auto run1 = [&](bool jump) {
        FxChain fx;
        fx.prepare(kSr);
        Buses b(48000);
        b.sl = dgtest::sine(300.0f, kSr, 48000, 0.4f);
        b.sr = b.sl;
        b.ml = dgtest::sine(200.0f, kSr, 48000, 0.3f);
        b.mr = b.ml;
        // zwei Hälften: in der zweiten springen alle Knobs auf 1
        Buses first(24000), second(24000);
        std::copy(b.ml.begin(), b.ml.begin() + 24000, first.ml.begin());
        std::copy(b.mr.begin(), b.mr.begin() + 24000, first.mr.begin());
        std::copy(b.sl.begin(), b.sl.begin() + 24000, first.sl.begin());
        std::copy(b.sr.begin(), b.sr.begin() + 24000, first.sr.begin());
        std::copy(b.ml.begin() + 24000, b.ml.end(), second.ml.begin());
        std::copy(b.mr.begin() + 24000, b.mr.end(), second.mr.begin());
        std::copy(b.sl.begin() + 24000, b.sl.end(), second.sl.begin());
        std::copy(b.sr.begin() + 24000, b.sr.end(), second.sr.begin());
        runWithMacros(fx, first, p, MacroParams {});
        runWithMacros(fx, second, p, jump ? MacroParams { 1.0f, 1.0f, 1.0f } : MacroParams {});
        std::vector<float> all = first.ml;
        all.insert(all.end(), second.ml.begin(), second.ml.end());
        return all;
    };
    const auto steady = run1(false);
    const auto jumped = run1(true);
    // Der Übergang um Sample 24000 darf keinen deutlich größeren Schritt erzeugen als der Dauerton.
    CHECK(dgtest::maxStep(jumped, 23900, 25000) < 2.5f * dgtest::maxStep(steady, 23900, 25000) + 0.05f);
}
```

An `tests/test_Engine.cpp` anhängen:

```cpp
TEST_CASE("throw sends every slot into the delay even with FX send 0", "[engine][macros]")
{
    auto makeParams = [] {
        auto p = testParams();
        p.global.fx.delayMix = 1.0f;
        p.global.fx.delayFeedback = 0.0f;
        p.global.fx.delayTone = 1.0f;
        p.global.fx.delayWow = 0.0f;
        p.global.fx.delayDiv = DelayDivision::D1_4; // 24000 Samples bei 120 bpm
        return p;
    };
    auto echo = [&](float throwAmount) {
        Engine e;
        e.prepare(kSr, 512);
        auto p = makeParams();
        p.global.macros.throwAmount = throwAmount;
        const auto o = run(e, p, 48000, { noteOn(36), noteOff(36, 2400) });
        return dgtest::peakAbs(o.l, 24500, 26500);
    };
    CHECK(echo(0.0f) < 1.0e-3f);  // FX-Send 0: nichts im Send-Bus, kein Echo
    CHECK(echo(1.0f) > 0.1f);     // Throw: der Slot geht in den Send-Bus
}
```

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Build-Fehler, `process` kennt kein `MacroParams`-Argument und `GlobalParams` hat kein `macros`.

- [ ] **Step 3: Implementieren**

`engine/Engine.h`: `#include "engine/Macros.h"` ergänzen (bei den anderen Includes) und in `GlobalParams` nach `FxParams fx {};` einfügen: `MacroParams macros {};`.

`engine/FxChain.h`: `#include "engine/Macros.h"` ergänzen; die Deklaration von `process` ändern zu:

```cpp
    void process(float* mainL, float* mainR, float* sendL, float* sendR, int numSamples,
                 const FxParams& p, double bpm, const MacroParams& macros = {});
```

und im privaten Teil nach `bool smoothInit_ = false;` einfügen:

```cpp
    // Die Knobs werden pro Block mit 20 ms geglättet; die Einzelwerte glätten die Stufen darunter wie bisher.
    MacroParams smMacros_ {};
    bool macrosInit_ = false;
```

`engine/FxChain.cpp`: In `reset()` nach `smoothInit_ = false;` einfügen `macrosInit_ = false;`. Die Signatur von `process` anpassen und den Anfang so ändern: statt `const FxParams& p` heißt der Parameter `pIn`, und direkt nach dem `needsReset_`-Block einfügen:

```cpp
    // Knobs glätten (pro Block, 20 ms) und als Aufschlag auf die Einzelwerte anwenden.
    if (!macrosInit_)
    {
        smMacros_ = macros;
        macrosInit_ = true;
    }
    const float macroA = 1.0f - std::exp(-static_cast<float>(numSamples) / (0.02f * static_cast<float>(sampleRate_)));
    smMacros_.space += macroA * (macros.space - smMacros_.space);
    smMacros_.grit += macroA * (macros.grit - smMacros_.grit);
    smMacros_.throwAmount += macroA * (macros.throwAmount - smMacros_.throwAmount);
    const FxParams p = applyMacros(pIn, smMacros_);
```

Der restliche Funktionskörper benutzt weiter `p` (jetzt die wirksamen Werte). Die Definition lautet damit:

```cpp
void FxChain::process(float* mainL, float* mainR, float* sendL, float* sendR, int numSamples,
                      const FxParams& pIn, double bpm, const MacroParams& macros)
```

Sind die geglätteten Knobs nicht endlich (NaN), setzt `unit()` in `applyMacros` sie auf 0; der Glätter selbst bleibt dann NaN. Deshalb in `reset()` zusätzlich `smMacros_ = MacroParams {};` einfügen.

`engine/Engine.cpp`: Den Aufruf `fx_.process(..., params_->global.fx, bpm_);` ändern zu `fx_.process(..., params_->global.fx, bpm_, params_->global.macros);`. In der Slot-Schleife die Zeile

```cpp
        const float targetSend = std::clamp(sp.fxSend, 0.0f, 1.0f);
```

ersetzen durch

```cpp
        const float targetSend = std::clamp(sp.fxSend + throwSendBoost(params_->global.macros), 0.0f, 1.0f);
```

- [ ] **Step 4: Alles laufen lassen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests grün. Bei Abweichung der Schwellen (`2.0f * base[0]`, die Klick-Grenze 2,5) die gemessenen Werte notieren und melden, nicht blind lockern.

- [ ] **Step 5: Commit**

```bash
git add engine/Engine.h engine/Engine.cpp engine/FxChain.h engine/FxChain.cpp tests/test_FxChain.cpp tests/test_Engine.cpp
git commit -m "feat(engine): apply the performance knobs in the FX chain and boost the FX send with Throw"
```

---

### Task 3: Plugin-Parameter und Zustand für Live/Edit

**Files:**
- Modify: `plugin/ParameterLayout.h`, `plugin/ParameterLayout.cpp`
- Modify: `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`
- Test: `tests/plugin/test_PluginProcessor.cpp`

**Interfaces:**
- Consumes: `GlobalParams::macros` (Task 2).
- Produces: Parameter-IDs `pid::space = "fxSpace"`, `pid::grit = "fxGrit"`, `pid::throwAmount = "fxThrow"`; `bool DubgefahrenProcessor::liveView() const`, `void setLiveView(bool)`, `bool advancedOpen() const`, `void setAdvancedOpen(bool)` (Standard: `liveView` false, `advancedOpen` true). Task 4 und 5 benutzen sie.

- [ ] **Step 1: Failing tests schreiben**

In `tests/plugin/test_PluginProcessor.cpp`:
- Die Parameterzahl `CHECK(p.getParameters().size() == 16 * 26 + 23);` ändern zu `16 * 26 + 26`.
- Die Prüfung der eindeutigen IDs (`ids.size() == 439`, plus Testtitel mit `439`) ändern auf `442`.
- Im Testfall `state round-trip keeps parameters, names with umlauts and UI settings` ergänzen: bei `a` nach `a.state().getParameter(pid::phaserMix)->setValueNotifyingHost(0.6f);` die Zeilen `a.state().getParameter(pid::space)->setValueNotifyingHost(0.4f);`, `a.setLiveView(true);`, `a.setAdvancedOpen(false);`; bei `b` nach der Phaser-Prüfung `CHECK_THAT(b.state().getParameter(pid::space)->getValue(), WithinAbs(0.4, 1e-4));`, `CHECK(b.liveView());`, `CHECK_FALSE(b.advancedOpen());`.

Neuer Testfall (am Dateiende):

```cpp
TEST_CASE("the performance knobs default to zero and a fresh processor opens in Edit with Advanced", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    for (const char* id : { pid::space, pid::grit, pid::throwAmount })
    {
        auto* param = p.state().getParameter(id);
        REQUIRE(param != nullptr);
        CHECK(param->isAutomatable());
        CHECK_THAT(param->convertFrom0to1(param->getValue()), WithinAbs(0.0, 1e-6));
    }
    CHECK_FALSE(p.liveView());
    CHECK(p.advancedOpen());

    ParamCache cache(p.state());
    EngineParams params;
    cache.read(params);
    CHECK(params.global.macros.space == 0.0f);
    CHECK(params.global.macros.grit == 0.0f);
    CHECK(params.global.macros.throwAmount == 0.0f);

    p.state().getParameter(pid::throwAmount)->setValueNotifyingHost(1.0f);
    cache.read(params);
    CHECK_THAT(params.global.macros.throwAmount, WithinAbs(1.0, 1e-5));
}
```

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Build-Fehler, `pid::space` und `liveView()` sind nicht definiert.

- [ ] **Step 3: Implementieren**

`plugin/ParameterLayout.h`: In `namespace dg::pid` nach `phaserMix` einfügen:

```cpp
inline constexpr const char* space = "fxSpace";
inline constexpr const char* grit = "fxGrit";
inline constexpr const char* throwAmount = "fxThrow";
```

nach `constexpr int kPhaserParameterVersion = 3;` einfügen: `// Versionshinweis für die Performance-Knobs Space, Grit und Throw.` und `constexpr int kMacroParameterVersion = 5;`; in `ParamCache` nach `Ptr phaserRate_, phaserDepth_, phaserMix_;` einfügen `Ptr macroSpace_, macroGrit_, macroThrow_;`.

`plugin/ParameterLayout.cpp`: nach der Zeile `layout.add(makeFloat(pid::phaserMix, ...));` einfügen:

```cpp
    layout.add(makeFloat(pid::space, "Space", 0.0f, 1.0f, 0.0f, 0.0f, "", kMacroParameterVersion));
    layout.add(makeFloat(pid::grit, "Grit", 0.0f, 1.0f, 0.0f, 0.0f, "", kMacroParameterVersion));
    layout.add(makeFloat(pid::throwAmount, "Throw", 0.0f, 1.0f, 0.0f, 0.0f, "", kMacroParameterVersion));
```

Im `ParamCache`-Konstruktor nach `phaserMix_(raw(apvts, pid::phaserMix)),` einfügen `macroSpace_(raw(apvts, pid::space)), macroGrit_(raw(apvts, pid::grit)), macroThrow_(raw(apvts, pid::throwAmount)),` (Reihenfolge wie die Member in der Klasse). In `ParamCache::read` nach `fx.phaserMix = load(phaserMix_);` einfügen:

```cpp
    out.global.macros = MacroParams { load(macroSpace_), load(macroGrit_), load(macroThrow_) };
```

`plugin/PluginProcessor.h`: nach `void setEditorFollowsFocus(bool follow);` einfügen:

```cpp
    // Ansicht des Editors (Live = kompakt, Edit = voll) und ob der Advanced-Streifen aufgeklappt ist.
    // Beides liegt im Plugin-Zustand, keine Host-Parameter. Standard: Edit mit Advanced.
    bool liveView() const;
    void setLiveView(bool live);
    bool advancedOpen() const;
    void setAdvancedOpen(bool open);
```

`plugin/PluginProcessor.cpp`: bei den Identifiern nach `kFollowFocusId` einfügen `const juce::Identifier kLiveViewId { "liveView" };` und `const juce::Identifier kAdvancedOpenId { "advancedOpen" };`; nach den Funktionen `editorFollowsFocus`/`setEditorFollowsFocus` einfügen:

```cpp
bool DubgefahrenProcessor::liveView() const
{
    return static_cast<bool>(apvts_.state.getProperty(kLiveViewId, false));
}

void DubgefahrenProcessor::setLiveView(bool live)
{
    apvts_.state.setProperty(kLiveViewId, live, nullptr);
}

bool DubgefahrenProcessor::advancedOpen() const
{
    return static_cast<bool>(apvts_.state.getProperty(kAdvancedOpenId, true));
}

void DubgefahrenProcessor::setAdvancedOpen(bool open)
{
    apvts_.state.setProperty(kAdvancedOpenId, open, nullptr);
}
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests grün. Schlägt ein weiterer Test wegen der neuen Parameterzahl fehl, nur die abgeleiteten Zählwerte anpassen und im Report nennen.

- [ ] **Step 5: Commit**

```bash
git add plugin/ParameterLayout.h plugin/ParameterLayout.cpp plugin/PluginProcessor.h plugin/PluginProcessor.cpp tests/plugin/test_PluginProcessor.cpp
git commit -m "feat(plugin): Space, Grit and Throw parameters and the live view state"
```

---

### Task 4: Effektleiste teilen (LivePanel und AdvancedPanel), Edit-Layout 1200 × 744

**Files:**
- Create: `plugin/ui/LivePanel.h`, `plugin/ui/LivePanel.cpp`
- Rename: `plugin/ui/FxPanel.h` → `plugin/ui/AdvancedPanel.h`, `plugin/ui/FxPanel.cpp` → `plugin/ui/AdvancedPanel.cpp` (Klasse `FxPanel` → `AdvancedPanel`, nur noch die Advanced-Regler)
- Modify: `plugin/ui/PerformancePanel.h/.cpp` (Anordnung nach Breite), `plugin/CMakeLists.txt`, `plugin/PluginEditor.h/.cpp`
- Test: `tests/plugin/test_Editor.cpp`, `tests/plugin/test_Look.cpp`

**Interfaces:**
- Consumes: `pid::space`, `pid::grit`, `pid::throwAmount` (Task 3).
- Produces: `ui::LivePanel`, `ui::AdvancedPanel`, `std::vector<juce::Rectangle<int>> controlBounds() const` an `LivePanel`, `AdvancedPanel` und `PerformancePanel` (Rechtecke aller Regler im Panel-Koordinatensystem); im Editor die Basisgröße 1200 × 744 für das einzige Layout dieses Tasks (Edit mit Advanced). Task 5 baut darauf die weiteren Layouts.

- [ ] **Step 1: Tests anpassen und schreiben**

`tests/plugin/test_Editor.cpp` im Testfall `editor opens with the stored scale and follows focus`: `CHECK(editor->getHeight() == 960);` ändern zu `CHECK(editor->getHeight() == 1116);` (744 × 1,5; die Breite 1800 bleibt). Neuer Testfall am Dateiende:

```cpp
TEST_CASE("the effects bar is split into the live strip and the advanced strip without overlaps", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.setUiScale(1.0f);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    REQUIRE(e != nullptr);
    CHECK(e->getWidth() == 1200);
    CHECK(e->getHeight() == 744);

    const auto check = [](const juce::Rectangle<int>& panel, const std::vector<juce::Rectangle<int>>& controls) {
        const auto local = panel.withZeroOrigin();
        for (std::size_t i = 0; i < controls.size(); ++i)
        {
            CHECK(local.contains(controls[i]));
            for (std::size_t j = i + 1; j < controls.size(); ++j)
                CHECK_FALSE(controls[i].intersects(controls[j]));
        }
    };
    check(e->livePanel().getBounds(), e->livePanel().controlBounds());
    check(e->advancedPanel().getBounds(), e->advancedPanel().controlBounds());
    check(e->performancePanel().getBounds(), e->performancePanel().controlBounds());
    CHECK(e->livePanel().controlBounds().size() == 8);     // Space, Grit, Throw, Cutoff, Reso, Typ, Zeit, Master
    CHECK(e->advancedPanel().controlBounds().size() == 11); // Drive, 4x Delay, 3x Phaser, 3x Reverb
}
```

In `tests/plugin/test_Look.cpp`: die Render-Tests laufen weiter mit der Breite `1200.0f * scale`; die Pixelprobe `(1170, 420)` bleibt gültig, weil der Slot-Editor seine Höhe behält (nur das Fenster wächst um die neue Zeile). Falls ein Test die Fensterhöhe `640` verwendet, auf `744` ändern.

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Build-Fehler, `livePanel()`, `advancedPanel()`, `performancePanel()` und `controlBounds()` gibt es nicht.

- [ ] **Step 3: Implementieren**

1. `git mv plugin/ui/FxPanel.h plugin/ui/AdvancedPanel.h` und `git mv plugin/ui/FxPanel.cpp plugin/ui/AdvancedPanel.cpp`; in `plugin/CMakeLists.txt` die beiden Pfade umbenennen und `LivePanel.h`/`LivePanel.cpp` danach einfügen.

2. `plugin/ui/AdvancedPanel.h/.cpp`: Klasse in `AdvancedPanel` umbenennen und auf die Advanced-Regler reduzieren: Knobs `drive_` (Label "Drive"), `delayFeedback_`, `delayTone_`, `delayWow_`, `delayMix_`, `phaserRate_`, `phaserDepth_`, `phaserMix_`, `reverbDecay_`, `reverbTone_`, `reverbMix_`. Gruppen `{ "DRIVE", 0, 1 }, { "DELAY", 1, 4 }, { "PHASER", 5, 3 }, { "REVERB", 8, 3 }`; `kCell = 72`; der Rest (Konstruktor mit `attach`, `paint` mit Titeln und Trennern, `resized` mit `setBounds(12 + i * kCell, 20, kCell - 6, getHeight() - 24)`) bleibt wie in der bisherigen `FxPanel`, ohne Cutoff, Resonance, Filter-Typ, Delay-Zeit und Master. Dazu in beiden Klassen:

```cpp
    std::vector<juce::Rectangle<int>> controlBounds() const
    {
        std::vector<juce::Rectangle<int>> out;
        for (auto* c : getChildren())
            out.push_back(c->getBounds());
        return out;
    }
```

(als inline-Methode im Header; `#include <vector>` ergänzen).

3. `plugin/ui/LivePanel.h/.cpp` (neu): Klasse `ui::LivePanel : juce::Component` mit `explicit LivePanel(DubgefahrenProcessor& proc)`, `paint`, `resized`, `controlBounds()` wie oben und den Reglern `Knob space_ { u8("Space") }`, `grit_ { u8("Grit") }`, `throw_ { u8("Throw") }`, `cutoff_ { u8("Cutoff") }`, `resonance_ { u8("Reso") }`, `filterType_ { u8("LP·BP·HP") }`, `Choice delayTime_ { u8("Time") }`, `Knob master_ { u8("Master") }`. Konstruktor: `attach(s, pid::space)`, `pid::grit`, `pid::throwAmount`, `pid::cutoff`, `pid::resonance`, `pid::filterType`, `pid::delayTime`, `pid::masterVol`, danach `addAndMakeVisible` für alle acht. `resized()` ordnet nach der Breite an:

```cpp
void LivePanel::resized()
{
    constexpr int kCell = 72;
    if (getWidth() >= 900)
    {
        // Eine Zeile (Edit): DUB FX | FILTER | DELAY | MASTER
        int x = 12;
        const int h = getHeight() - 24;
        for (auto* k : { &space_, &grit_, &throw_ })
        {
            k->setBounds(x, 20, 90 - 6, h);
            x += 90;
        }
        x += 12;
        for (auto* k : { &cutoff_, &resonance_, &filterType_ })
        {
            k->setBounds(x, 20, kCell - 6, h);
            x += kCell;
        }
        x += 12;
        delayTime_.setBounds(x, 20, 110, h);
        x += 122;
        master_.setBounds(x, 20, kCell - 6, h);
    }
    else
    {
        // Kompakt (Live): oben die drei großen Knobs, darunter Filter, Zeit und Master
        const int colW = (getWidth() - 24) / 3;
        int x = 12;
        for (auto* k : { &space_, &grit_, &throw_ })
        {
            k->setBounds(x, 20, colW - 6, 100);
            x += colW;
        }
        x = 12;
        for (auto* k : { &cutoff_, &resonance_, &filterType_ })
        {
            k->setBounds(x, 140, kCell - 6, 88);
            x += kCell;
        }
        delayTime_.setBounds(x + 4, 150, 110, 70);
        x += 126;
        master_.setBounds(x, 140, kCell - 6, 88);
    }
}
```

`paint`: `drawPanelBody`, danach Gruppentitel (Schrift `font(11.0f, true)`, Farbe `colours::textDim`): in der breiten Variante "DUB FX" über den drei Knobs (x = 12), "FILTER" (x = 12 + 270 + 12), "DELAY" (über der Zeit), "MASTER"; in der kompakten Variante "DUB FX" oben und "FILTER · DELAY · MASTER" über der zweiten Zeile (y = 122). Dividers wie in der alten `FxPanel` (`drawDivider`). Die exakten Pixelmaße sind Richtwerte: nach dem Bauen mit `DG_SNAPSHOT_DIR` einen Snapshot rendern, ansehen und Maße so anpassen, dass nichts abgeschnitten wird oder überlappt (der Test oben prüft das).

4. `plugin/ui/PerformancePanel.h/.cpp`: Anordnung nach der Breite. `resized()` erhält zwei Zweige: Breite ≥ 900 wie bisher (eine Zeile); sonst kompakt:

```cpp
    else
    {
        // Kompakt (Live): oben vier Knobs, darunter die beiden Auswahlfelder
        int x = 12;
        for (auto* c : std::initializer_list<juce::Component*> { &pitch_, &rate_, &depth_, &sweep_ })
        {
            c->setBounds(x, 22, 72, 84);
            x += 76;
        }
        target_.setBounds(12, 108, 130, 40);
        latchStop_.setBounds(154, 108, 170, 40);
    }
```

und `paint` zeichnet "PERFORMANCE" in der kompakten Variante oben links (y = 4, Höhe 16) statt vertikal zentriert. `controlBounds()` wie oben (Header, `#include <vector>`).

5. `plugin/PluginEditor.h`: `#include "plugin/ui/FxPanel.h"` ersetzen durch `#include "plugin/ui/AdvancedPanel.h"` und `#include "plugin/ui/LivePanel.h"`; `static constexpr int kBaseWidth = 1200; static constexpr int kBaseHeight = 640;` ersetzen durch `kBaseWidth = 1200` und `kBaseHeight = 744` (Edit mit Advanced; Task 5 macht sie layoutabhängig); Member `ui::FxPanel fx_;` ersetzen durch `ui::LivePanel live_; ui::AdvancedPanel advanced_;`; `std::array<ui::PanelShadow, 3> panelShadows_;` ändern zu `std::array<ui::PanelShadow, 4>`; neue öffentliche Test-Accessoren:

```cpp
    const ui::LivePanel& livePanel() const { return live_; }
    const ui::AdvancedPanel& advancedPanel() const { return advanced_; }
    const ui::PerformancePanel& performancePanel() const { return perf_; }
```

`plugin/PluginEditor.cpp`: Konstruktor-Initialisierung `fx_(proc)` ersetzen durch `live_(proc), advanced_(proc)`; in der `addAndMakeVisible`-Liste `&fx_` durch `&live_, &advanced_` ersetzen; in `paint` die Schatten rendern für `slotEditor_`, `live_`, `advanced_`, `perf_` (Indizes 0 bis 3); in `layoutContent()` ersetzen:

```cpp
    perf_.setBounds(r.removeFromBottom(84));
    r.removeFromBottom(8);
    advanced_.setBounds(r.removeFromBottom(96));
    r.removeFromBottom(8);
    live_.setBounds(r.removeFromBottom(96));
    r.removeFromBottom(8);
```

(statt der beiden alten Zeilen für `perf_` und `fx_`; der Rest bleibt).

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests grün. Schlägt ein bestehender Test wegen der neuen Höhe fehl, nur die abgeleiteten Zahlen anpassen und jede Änderung mit Grund im Report nennen.

- [ ] **Step 5: Commit**

```bash
git add -A plugin/ui plugin/CMakeLists.txt plugin/PluginEditor.h plugin/PluginEditor.cpp tests/plugin/test_Editor.cpp tests/plugin/test_Look.cpp
git commit -m "feat(ui): split the effects bar into a live strip and an advanced strip"
```

---

### Task 5: Live/Edit-Umschalter, Advanced aufklappen und die drei Layouts

**Files:**
- Modify: `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`
- Test: `tests/plugin/test_Editor.cpp`, `tests/plugin/test_Look.cpp`

**Interfaces:**
- Consumes: `liveView()`, `advancedOpen()` und ihre Setter (Task 3), `LivePanel`, `AdvancedPanel`, `PerformancePanel` mit `controlBounds()` (Task 4).
- Produces: `enum class DubgefahrenEditor::Layout { EditAdvanced, Edit, Live }`, `static juce::Point<int> baseSize(Layout)`, `Layout layout() const`, Test-Accessoren `bool slotEditorVisible() const`, `bool advancedPanelVisible() const`.

- [ ] **Step 1: Failing tests schreiben**

An `tests/plugin/test_Editor.cpp` anhängen:

```cpp
TEST_CASE("the editor has three layouts with fixed base sizes", "[editor]")
{
    using Layout = DubgefahrenEditor::Layout;
    CHECK(DubgefahrenEditor::baseSize(Layout::EditAdvanced) == juce::Point<int>(1200, 744));
    CHECK(DubgefahrenEditor::baseSize(Layout::Edit) == juce::Point<int>(1200, 640));
    CHECK(DubgefahrenEditor::baseSize(Layout::Live) == juce::Point<int>(880, 460));
}

TEST_CASE("switching the view resizes the window, keeps the scale and hides what Live does not show", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.setUiScale(1.5f);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    REQUIRE(e != nullptr);
    CHECK(e->layout() == DubgefahrenEditor::Layout::EditAdvanced);
    CHECK(e->getWidth() == 1800);
    CHECK(e->slotEditorVisible());
    CHECK(e->advancedPanelVisible());

    e->setLiveView(true);
    CHECK(e->layout() == DubgefahrenEditor::Layout::Live);
    CHECK(e->getWidth() == 1320);   // 880 * 1.5
    CHECK(e->getHeight() == 690);   // 460 * 1.5
    CHECK(p.liveView());
    CHECK_FALSE(e->slotEditorVisible());
    CHECK_FALSE(e->advancedPanelVisible());
    CHECK_THAT(p.uiScale(), Catch::Matchers::WithinAbs(1.5, 1e-3));

    e->setLiveView(false);
    CHECK(e->layout() == DubgefahrenEditor::Layout::EditAdvanced);
    CHECK(e->getWidth() == 1800);
    CHECK_THAT(p.uiScale(), Catch::Matchers::WithinAbs(1.5, 1e-3));

    e->setAdvancedOpen(false);
    CHECK(e->layout() == DubgefahrenEditor::Layout::Edit);
    CHECK(e->getHeight() == 960);   // 640 * 1.5
    CHECK_FALSE(e->advancedPanelVisible());
    CHECK(e->slotEditorVisible());
}

TEST_CASE("in every layout all visible controls lie inside their panel without overlaps", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const auto mode : { 0, 1, 2 })
    {
        DubgefahrenProcessor p;
        p.setUiScale(1.0f);
        p.setLiveView(mode == 2);
        p.setAdvancedOpen(mode == 0);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* e = static_cast<DubgefahrenEditor*>(editor.get());
        REQUIRE(e != nullptr);
        const auto base = DubgefahrenEditor::baseSize(e->layout());
        CHECK(e->getWidth() == base.x);
        CHECK(e->getHeight() == base.y);

        const auto check = [&](const juce::Rectangle<int>& panel, const std::vector<juce::Rectangle<int>>& controls) {
            const auto local = panel.withZeroOrigin();
            for (std::size_t i = 0; i < controls.size(); ++i)
            {
                CHECK(local.contains(controls[i]));
                for (std::size_t j = i + 1; j < controls.size(); ++j)
                    CHECK_FALSE(controls[i].intersects(controls[j]));
            }
        };
        check(e->livePanel().getBounds(), e->livePanel().controlBounds());
        check(e->performancePanel().getBounds(), e->performancePanel().controlBounds());
        if (e->advancedPanelVisible())
            check(e->advancedPanel().getBounds(), e->advancedPanel().controlBounds());

        // Alle Panels liegen im Inhaltsbereich der Basisgröße und überlappen sich nicht.
        const juce::Rectangle<int> content(0, 0, base.x, base.y);
        CHECK(content.contains(e->livePanel().getBounds()));
        CHECK(content.contains(e->performancePanel().getBounds()));
        CHECK_FALSE(e->livePanel().getBounds().intersects(e->performancePanel().getBounds()));
    }
}

TEST_CASE("the layout follows the processor state restored by the host", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    a.setLiveView(true);
    a.setAdvancedOpen(false);
    juce::MemoryBlock mb;
    a.getStateInformation(mb);

    DubgefahrenProcessor b;
    std::unique_ptr<juce::AudioProcessorEditor> editor(b.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    REQUIRE(e != nullptr);
    CHECK(e->layout() == DubgefahrenEditor::Layout::EditAdvanced);
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    e->pollProcessorState();
    CHECK(e->layout() == DubgefahrenEditor::Layout::Live);
    CHECK_FALSE(e->slotEditorVisible());
}
```

(`#include <catch2/matchers/catch_matchers_floating_point.hpp>` oben ergänzen, falls es fehlt.) In `tests/plugin/test_Look.cpp` für die Live-Ansicht einen Render-Test ergänzen: für Skalierung 0,75, 1 und 2 `p.setLiveView(true)`, Editor erzeugen, `dgtest::snapshot(*editor)` rendern, `img.getWidth() == roundToInt(880 * scale)` und `img.getHeight() == roundToInt(460 * scale)`, und eine Pixelprobe in der Mitte des Live-Panels, die nicht der Fensterhintergrund ist (`livePanel().getBounds().getCentre()` skaliert).

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Build-Fehler, `Layout`, `baseSize`, `layout()`, `setLiveView`, `slotEditorVisible` fehlen im Editor.

- [ ] **Step 3: Implementieren**

`plugin/PluginEditor.h`:
- `kBaseWidth` und `kBaseHeight` entfernen und ersetzen durch:

```cpp
    enum class Layout { EditAdvanced, Edit, Live };
    // Basisgröße (vor der Skalierung) pro Layout.
    static juce::Point<int> baseSize(Layout l)
    {
        switch (l)
        {
            case Layout::Edit: return { 1200, 640 };
            case Layout::Live: return { 880, 460 };
            case Layout::EditAdvanced: break;
        }
        return { 1200, 744 };
    }
    Layout layout() const { return layout_; }
    // Für Tests und die Header-Taster: wechselt die Ansicht bzw. klappt Advanced auf/zu.
    void setLiveView(bool live);
    void setAdvancedOpen(bool open);
    bool slotEditorVisible() const { return slotEditor_.isVisible(); }
    bool advancedPanelVisible() const { return advanced_.isVisible(); }
```

- Private Member ergänzen: `juce::TextButton viewButton_ { "Live" };`, `juce::TextButton advancedButton_ { juce::String::fromUTF8("Advanced ▾") };`, `Layout layout_ = Layout::EditAdvanced;`, Methode `void applyLayout();`.

`plugin/PluginEditor.cpp`:
- Konstruktor: nach den bestehenden Button-Handlern `viewButton_.onClick = [this] { setLiveView(!proc_.liveView()); };` und `advancedButton_.onClick = [this] { setAdvancedOpen(!proc_.advancedOpen()); };`; beide Taster in die `addAndMakeVisible`-Liste aufnehmen. Statt `content_.setSize(kBaseWidth, kBaseHeight)`, `setResizeLimits(...)`, `setFixedAspectRatio(...)` und `setSize(...)` danach (Reihenfolge und der Kommentar zu `uiScale()` bleiben): zuerst `layout_` aus dem Zustand bestimmen, dann `const float scale = proc_.uiScale();` lesen, dann `applyLayout()` aufrufen, das die Größenlogik übernimmt:

```cpp
void DubgefahrenEditor::applyLayout()
{
    layout_ = proc_.liveView() ? Layout::Live : proc_.advancedOpen() ? Layout::EditAdvanced : Layout::Edit;
    const bool live = layout_ == Layout::Live;
    const auto base = baseSize(layout_);

    slotEditor_.setVisible(!live);
    advanced_.setVisible(layout_ == Layout::EditAdvanced);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &followFocus_, &midiButton_, &importButton_, &exportButton_, &advancedButton_ })
        c->setVisible(!live);
    viewButton_.setButtonText(live ? "Edit" : "Live");
    advancedButton_.setButtonText(juce::String::fromUTF8(proc_.advancedOpen() ? "Advanced ▾" : "Advanced ▸"));

    content_.setSize(base.x, base.y);
    layoutContent();

    // uiScale() vorab lesen: setResizeLimits() zwingt die Editor-Bounds sofort auf die Mindestgröße,
    // was über resized() einen Zwischenwert in den Processor zurückschreibt.
    const float scale = proc_.uiScale();
    setResizeLimits(base.x * 3 / 4, base.y * 3 / 4, base.x * 2, base.y * 2);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(base.x) / base.y);
    setSize(juce::roundToInt(base.x * scale), juce::roundToInt(base.y * scale));
    repaint();
}

void DubgefahrenEditor::setLiveView(bool live)
{
    proc_.setLiveView(live);
    applyLayout();
}

void DubgefahrenEditor::setAdvancedOpen(bool open)
{
    proc_.setAdvancedOpen(open);
    applyLayout();
}
```

- `resized()`: `const auto base = baseSize(layout_); const float scale = static_cast<float>(getWidth()) / static_cast<float>(base.x); content_.setBounds(0, 0, base.x, base.y);` (Rest unverändert).
- `paint`: Schatten nur für sichtbare Panels rendern (`if (slotEditor_.isVisible()) …`, `if (advanced_.isVisible()) …`, `live_` und `perf_` immer).
- `layoutContent()`: an das Layout anpassen. Header: im Edit-Layout wie bisher plus `advancedButton_` (110) und `viewButton_` (80) rechts neben PANIC; im Live-Layout nur Titel, CPU, PANIC, `viewButton_` und `kitButton_`. Danach:

```cpp
    const auto base = baseSize(layout_);
    auto r = juce::Rectangle<int>(0, 0, base.x, base.y).reduced(12);
    auto header = r.removeFromTop(36);
    title_.setBounds(header.removeFromLeft(220));
    cpuMeter_.setBounds(header.removeFromLeft(90));
    panicButton_.setBounds(header.removeFromRight(90).reduced(2));
    header.removeFromRight(12);
    viewButton_.setBounds(header.removeFromRight(80).reduced(2));
    if (layout_ == Layout::Live)
    {
        kitButton_.setBounds(header.removeFromRight(110).reduced(2));
        r.removeFromTop(8);
        pads_.setBounds(r.removeFromLeft(360));
        r.removeFromLeft(12);
        live_.setBounds(r.removeFromTop(232));
        r.removeFromTop(8);
        perf_.setBounds(r);
        return;
    }
    advancedButton_.setBounds(header.removeFromRight(110).reduced(2));
    exportButton_.setBounds(header.removeFromRight(80).reduced(2));
    importButton_.setBounds(header.removeFromRight(80).reduced(2));
    kitButton_.setBounds(header.removeFromRight(110).reduced(2));
    midiButton_.setBounds(header.removeFromRight(80).reduced(2));
    followFocus_.setBounds(header.removeFromRight(170));

    perf_.setBounds(r.removeFromBottom(84));
    r.removeFromBottom(8);
    if (layout_ == Layout::EditAdvanced)
    {
        advanced_.setBounds(r.removeFromBottom(96));
        r.removeFromBottom(8);
    }
    live_.setBounds(r.removeFromBottom(96));
    r.removeFromBottom(8);
    pads_.setBounds(r.removeFromLeft(360));
    r.removeFromLeft(12);
    slotEditor_.setBounds(r);
```

  Im Live-Layout liegt `perf_` im Rest der rechten Spalte (Höhe 460 − 24 − 36 − 8 − 232 − 8 = 152). Die Pixelmaße sind Richtwerte: nach dem Bauen einen Snapshot der Live-Ansicht rendern (`DG_SNAPSHOT_DIR`), ansehen und bei Bedarf anpassen; die Tests prüfen, dass nichts überlappt oder herausragt.
- `pollProcessorState()`: am Anfang einfügen:

```cpp
    // Ansicht und Advanced-Zustand kommen auch vom Host (Projekt laden, Undo): dem Layout folgen.
    const Layout wanted = proc_.liveView() ? Layout::Live : proc_.advancedOpen() ? Layout::EditAdvanced : Layout::Edit;
    if (wanted != layout_)
        applyLayout();
```

- Der Header-Taster `advancedButton_` zeigt "Advanced ▾" (offen) oder "Advanced ▸" (zu).

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests grün. Bei Abweichungen (Überlappungen, Maße) erst den Snapshot ansehen, Maße anpassen und im Report nennen; die Prüfungen nicht lockern.

- [ ] **Step 5: Commit**

```bash
git add plugin/PluginEditor.h plugin/PluginEditor.cpp tests/plugin/test_Editor.cpp tests/plugin/test_Look.cpp
git commit -m "feat(ui): live view, edit view and a collapsible advanced strip"
```

---

### Task 6: Gesamtprüfung, README und Abschluss

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/specs/2026-10-05-performance-knobs-design.md` (Status, Fenstergrößen)

- [ ] **Step 1: Gesamte Suite**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Engine- und Plugin-Tests grün.

- [ ] **Step 2: VST3-Validator**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 validate`
Expected: Validator läuft durch.

- [ ] **Step 3: README**

In `README.md` im Abschnitt **Effects** vor den Listenpunkten einen Absatz ergänzen und die Beschreibung anpassen, zum Beispiel:

```
Three performance knobs sit on top of the effects: **Space** (longer, wider echoes and reverb), **Grit**
(saturation, tape wobble, darker echoes, phaser) and **Throw** (the dub throw: more FX send and a delay that
can run into self-oscillation). They add to the values of the individual controls: at 0 the sound is exactly
what the individual controls say. The individual controls live in the collapsible *Advanced* strip.
```

Außerdem einen neuen Abschnitt **Live and Edit view** (nach Effects): Der Taster *Live* im Header schaltet in eine kompakte Ansicht mit Pads, Space, Grit, Throw, Filter, Delay-Zeit, Master und den Performance-Controls (Fenster etwa 880 × 460); *Edit* zeigt alles inklusive Slot-Editor und dem *Advanced*-Streifen. Die Wahl wird im Projekt gespeichert. In der Abnahme-Checkliste Einträge ergänzen: `- [ ] Space, Grit and Throw: at 0 nothing changes; each knob adds its part (Space: bigger room and echoes, Grit: dirtier, Throw: the delay opens up to self-oscillation).` und `- [ ] Live view: the window shrinks, the pads, the three knobs, filter, delay time, master and the performance controls are all reachable; Edit brings everything back; the choice survives saving and reloading the project.`

- [ ] **Step 4: Spec-Status und Maße aktualisieren und committen**

In der Spec `**Status:** Spec, Review und Plan offen` ändern zu `**Status:** Umgesetzt, Abnahme in Ableton offen`, und in der Tabelle der Fenstergrößen `1200 × 736` durch `1200 × 744` und `etwa 800 × 540` durch `880 × 460` ersetzen (der Plan hat die endgültigen Maße festgelegt).

```bash
git add README.md docs/superpowers/specs/2026-10-05-performance-knobs-design.md
git commit -m "docs: describe the performance knobs and the live view"
```

- [ ] **Step 5: Hörtest und Spieltest (manuell, du)**

Das Plugin liegt nach dem Build unter `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3`; die Zip legt der Controller ins OneDrive. In Ableton prüfen: Space, Grit und Throw einzeln und zusammen (bei 0 ändert sich nichts), Gefühl und Stärke der Aufschläge, Live-Ansicht (Fenstergröße, Erreichbarkeit aller Regler, Umschalten, Speichern und Laden). Danach die Faktoren oben in `Macros.cpp` nachjustieren.
