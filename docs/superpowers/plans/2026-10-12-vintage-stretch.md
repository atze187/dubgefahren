# Vintage-Stretch für Samples Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ein Sample-Slot lässt sich per zyklischem Grain-Stretch (S950-Charakter) in der Zeit dehnen oder stauchen, ohne die Tonhöhe zu ändern; wahlweise auf den ganzen Bereich oder nur auf die Loop-Region.

**Architecture:** Eine neue Klasse `GrainStretch` (`engine/GrainStretch.h/.cpp`) hält zwei überblendete Stücke und erzeugt pro Ausgabesample einen Wert. `SamplePlayer` behält `pos_` als Quellposition, lässt sie bei aktivem Stretch mit `Quellrate / Faktor` laufen und ruft `GrainStretch` statt des einfachen Lesens auf; Ende, Loop-Umbruch und Ausblendung des Bereichs laufen unverändert über `pos_`. Drei neue Slot-Felder (`StretchMode`, `StretchFactor`, `StretchGrain`) laufen über die Feld-Tabelle in Host-Parameter und Kit; der Slot-Editor bekommt eine Stretch-Gruppe.

**Tech Stack:** C++20, JUCE (Plugin-Teil), Catch2 (`DubgefahrenTests`, `DubgefahrenPluginTests`), CMake über `build.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-12-vintage-stretch-design.md`

## Global Constraints

- Neue Slot-Felder, am Ende von `SlotField` angefügt (die Reihenfolge ist Teil des Host-States), `ParameterID`-Versionshinweis **5**:
  - `StretchMode`, Schlüssel `stretch`, Choice `Off`, `All`, `Loop`, Standard `Off` (Index 0)
  - `StretchFactor`, Schlüssel `stretchPct`, Float 50 … 200, Standard 100, `skewCentre` 100, Einheit `%`
  - `StretchGrain`, Schlüssel `grainMs`, Float 5 … 100, Standard 20, `skewCentre` 20, Einheit `ms`
- `SlotParams`: `StretchMode stretchMode = StretchMode::Off;` (Enum `StretchMode { Off, All, Loop }` in `engine/SlotParams.h`), `float stretchPct = 100.0f;`, `float grainMs = 20.0f;`.
- Danach: `kNumSlotFields == 28`; Host-Parameter `16 * 29 + 26 = 490` (jeder Slot 28 Felder + Quelle, dazu 26 globale).
- Kit-Format **Version 5** (`kVersion = 5`). Kits der Versionen 1 bis 4 laden mit den Standardwerten der neuen Felder (das geschieht schon über `makeDefaultSlotParams()` in `kitFromJsonString`). Ein Kit mit Version 6 wird weiterhin als „newer version“ abgelehnt.
- Der Faktor ist die Dauer relativ zum Original (200 % = doppelt so lang). Bei aktivem Stretch bestimmt allein der Faktor die Dauer; Tune und Performance-Pitch wirken nur auf die Lesegeschwindigkeit der Stücke.
- Stückgröße in Ausgabesamples: `P = max(4, round(grainMs * 0.001 * sampleRate))`; Crossfade-Länge `X = max(1, P / 4)`, linear. Ein Stück lebt `P + X` Ausgabesamples, alle `P` Samples startet ein neues.
- Stretch **Aus** rendert bitgleich wie bisher.
- Modus **Loop** wirkt nur mit eingeschaltetem Loop. Ohne Loop verhält sich der Slot wie bei Aus.
- Bei Stretch ungleich Aus und eingeschaltetem Loop gilt der Loop-X-Fade-Regler nicht (die Stück-Crossfades übernehmen).
- Der Faktor wird mit der bestehenden Pitch-Glättung (`kPerfSmoothingS`, 20 ms, `perfCoeff_`) geglättet.
- Keine Speicherzuteilung im Audio-Thread.
- Build und Test: `powershell -ExecutionPolicy Bypass -File build.ps1 test`. Nur bauen: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, danach `build\tests\Release\DubgefahrenTests.exe "[stretch]"` (Plugin-Tests: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe`).
- Nach grünen Tests auf diesem Branch: die Zip des VST3-Ordners nach `C:\Users\atzet\OneDrive\Dokumente\musik\dubgefahren\dubgefahren.vst3.feature-vintage-stretch.zip` legen (`Compress-Archive -Path E:\Repos\dubgefahren\build\plugin\Dubgefahren_artefacts\Release\VST3 -DestinationPath <Zip> -Force`); das macht der Controller im letzten Schritt.
- Commit-Messages enden mit der Zeile `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Bekannte, bewusst akzeptierte Vereinfachungen: (a) Wechselt der Stretch während des Spielens von aktiv auf inaktiv (Modus live auf Aus gestellt, Marker unter der Stimme verschoben), schaltet der Hauptkopf ohne Überblendung auf einfaches Lesen zurück (seltener Fall, kurzer Klick möglich); (b) der Retrigger lässt den alten Kopf als einfaches Lesen ohne Stretch auslaufen; (c) ohne Loop halten die Stücke an den Bereichsgrenzen den letzten Wert, bis die Ausblendung am Ende greift.

## Review Focus

1. Modus Aus: bitgleich zum bisherigen Verhalten, auch mit beliebigen Werten in den Stretch-Feldern (Task 3).
2. Dauer und Tonhöhe: Faktor 50 %, 100 %, 200 %; Tune ändert nur die Tonhöhe, nicht die Dauer (Task 3).
3. Reverse mit Stretch klingt rückwärts (Stücke werden rückwärts gelesen) und endet am Bereichsanfang (Task 3).
4. Loop mit Stretch endet nie von selbst, bleibt im Loop-Segment und erzeugt keine Werte außerhalb (Task 3).
5. Sehr kurzes Sample (kürzer als eine Stückgröße), Grain- und Faktor-Wechsel beim Spielen, Retrigger mitten im Stretch bleiben endlich und ohne große Sprünge (Task 3).
6. Alte Plugin-Zustände und Kits (Version 4 und älter) laden mit Stretch Aus (Task 4).
7. Oberfläche: Faktor und Grain sind bei Aus (und bei Modus Loop ohne Loop) deaktiviert, X-Fade bei Stretch mit Loop (Task 5).

---

### Task 1: Gemeinsame Interpolation und die Klasse `GrainStretch`

**Files:**
- Create: `engine/SampleInterp.h`, `engine/GrainStretch.h`, `engine/GrainStretch.cpp`
- Create: `tests/test_GrainStretch.cpp`
- Modify: `engine/SamplePlayer.cpp` (die private Funktion `cubicAt` entfällt, `#include "engine/SampleInterp.h"`)
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `float dg::cubicAt(const std::vector<float>& x, double pos)` in `engine/SampleInterp.h` (Catmull-Rom, Werte außerhalb des Puffers gelten als 0; inline).
  - `struct dg::GrainFrame { double step; double dir; bool cycle; double lo; double hi; };` Bedeutung: `step` = Lesegeschwindigkeit der Stücke in Quellsamples pro Ausgabesample (inklusive Tune und Pitch), `dir` = +1 oder -1, `cycle` = Lesepositionen werden in `[lo, hi)` umgebrochen, sonst auf `[lo, hi - 1]` begrenzt.
  - `class dg::GrainStretch` mit `void reset()`, `void start(double srcPos, double dir, int grainLen)`, `float process(const std::vector<float>& x, double srcPos, int grainLen, const GrainFrame& f)`.

- [ ] **Step 1: Interpolation in eine eigene Datei verschieben**

`engine/SampleInterp.h`:

```cpp
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

namespace dg {

// Catmull-Rom-Interpolation; Werte außerhalb des Puffers gelten als 0.
inline float cubicAt(const std::vector<float>& x, double pos)
{
    const auto n = static_cast<std::int64_t>(x.size());
    const double fl = std::floor(pos); // auch für negative Positionen (Rückwärtslauf) abrunden
    const auto i = static_cast<std::int64_t>(fl);
    const float t = static_cast<float>(pos - fl);
    const auto at = [&](std::int64_t k) { return (k < 0 || k >= n) ? 0.0f : x[static_cast<std::size_t>(k)]; };
    const float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
    return y1 + 0.5f * t * (y2 - y0 + t * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + t * (3.0f * (y1 - y2) + y3 - y0)));
}

} // namespace dg
```

In `engine/SamplePlayer.cpp`: die Funktion `cubicAt` im anonymen Namespace löschen, `#include "engine/SampleInterp.h"` ergänzen. Der Aufruf `cubicAt(x, pos)` bleibt (jetzt `dg::cubicAt`, im Namespace `dg` sichtbar).

- [ ] **Step 2: Failing tests schreiben**

`tests/test_GrainStretch.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/GrainStretch.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr float kUnit = 1.0e-4f;

std::vector<float> ramp(int n)
{
    std::vector<float> x(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        x[static_cast<std::size_t>(i)] = static_cast<float>(i) * kUnit;
    return x;
}

float rampAt(double pos) { return static_cast<float>(pos) * kUnit; }
} // namespace

TEST_CASE("the first grain reads from its start position without a fade-in", "[stretch]")
{
    const auto x = ramp(48000);
    GrainStretch g;
    g.start(1000.0, 1.0, 960);
    const GrainFrame f { 1.0, 1.0, false, 0.0, 48000.0 };
    CHECK_THAT(g.process(x, 1000.0, 960, f), WithinAbs(rampAt(1000), 1e-5));
    CHECK_THAT(g.process(x, 1000.5, 960, f), WithinAbs(rampAt(1001), 1e-5));
}

TEST_CASE("a new grain starts at the source position every grain length", "[stretch]")
{
    const auto x = ramp(48000);
    GrainStretch g;
    g.start(0.0, 1.0, 960);
    const GrainFrame f { 1.0, 1.0, false, 0.0, 48000.0 };
    float last = 0.0f;
    // Quellposition läuft mit 0,5 pro Sample (Faktor 2): nach 960 Samples bei 480.
    for (int i = 0; i < 1261; ++i)
        last = g.process(x, i * 0.5, 960, f);
    // Neues Stück startete bei Aufruf 960 an der Quellposition 480; 300 Samples später (Aufruf 1260)
    // liest es bei 780, das alte Stück ist verklungen.
    CHECK_THAT(last, WithinAbs(rampAt(780), 1e-4));
}

TEST_CASE("the crossfade between two grains sums to one", "[stretch]")
{
    std::vector<float> ones(48000, 1.0f);
    GrainStretch g;
    g.start(100.0, 1.0, 400);
    const GrainFrame f { 1.0, 1.0, false, 0.0, 48000.0 };
    for (int i = 0; i < 2000; ++i)
        CHECK_THAT(g.process(ones, 100.0 + i * 0.5, 400, f), WithinAbs(1.0, 1e-4));
}

TEST_CASE("reading wraps inside the segment when cycling", "[stretch]")
{
    const auto x = ramp(4000);
    GrainStretch g;
    g.start(1900.0, 1.0, 960);
    const GrainFrame f { 1.0, 1.0, true, 1000.0, 2000.0 };
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 6000; ++i)
    {
        const float v = g.process(x, 1000.0 + std::fmod(900.0 + i * 0.5, 1000.0), 960, f);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    CHECK(lo >= rampAt(1000) - 1e-4f);
    CHECK(hi <= rampAt(2000) + 1e-4f);
}

TEST_CASE("without cycling the read position is held inside the bounds", "[stretch]")
{
    const auto x = ramp(4000);
    GrainStretch g;
    g.start(3990.0, 1.0, 960);
    const GrainFrame f { 1.0, 1.0, false, 0.0, 4000.0 };
    float v = 0.0f;
    for (int i = 0; i < 100; ++i)
        v = g.process(x, 3990.0, 960, f);
    CHECK_THAT(v, WithinAbs(rampAt(3999), 1e-5)); // gehalten am letzten Sample
}

TEST_CASE("a backwards grain reads backwards", "[stretch]")
{
    const auto x = ramp(48000);
    GrainStretch g;
    g.start(5000.0, -1.0, 960);
    const GrainFrame f { 1.0, -1.0, false, 0.0, 48000.0 };
    CHECK_THAT(g.process(x, 5000.0, 960, f), WithinAbs(rampAt(5000), 1e-5));
    CHECK_THAT(g.process(x, 4999.5, 960, f), WithinAbs(rampAt(4999), 1e-5));
}

TEST_CASE("reset silences the grains", "[stretch]")
{
    const auto x = ramp(48000);
    GrainStretch g;
    g.start(1000.0, 1.0, 960);
    g.reset();
    // Nach reset startet das erste Stück erst bei Aufruf 0 des nächsten process() mit Einblendung.
    const GrainFrame f { 1.0, 1.0, false, 0.0, 48000.0 };
    CHECK_THAT(g.process(x, 1000.0, 960, f), WithinAbs(0.0, 1e-6));
}
```

In `tests/CMakeLists.txt` `test_GrainStretch.cpp` zu `DubgefahrenTests` ergänzen; in `engine/CMakeLists.txt` `SampleInterp.h GrainStretch.h GrainStretch.cpp` zur Quellliste.

- [ ] **Step 3: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL beim Kompilieren (`engine/GrainStretch.h` fehlt).

- [ ] **Step 4: Minimal implementation**

`engine/GrainStretch.h`:

```cpp
#pragma once
#include <array>
#include <vector>

namespace dg {

// Lesebereich und Geschwindigkeit der Stücke für einen Aufruf von GrainStretch::process.
struct GrainFrame
{
    double step = 1.0;  // Quellsamples pro Ausgabesample (inklusive Tune und Pitch)
    double dir = 1.0;   // +1 vorwärts, -1 rückwärts
    bool cycle = false; // true: Lesepositionen in [lo, hi) umbrechen; false: auf [lo, hi - 1] halten
    double lo = 0.0;
    double hi = 0.0;
};

// Zyklischer Stretch im Stil des Akai S950: Alle grainLen Ausgabesamples startet an der Quellposition
// ein neues Stück, das alte läuft ein Viertel der Stückdauer weiter und wird linear übergeblendet.
// Die Tonhöhe kommt allein aus GrainFrame::step; die Quellposition liefert der Aufrufer.
class GrainStretch
{
public:
    void reset();
    // Beginnt das erste Stück sofort bei srcPos, ohne Einblendung (der Aufrufer überblendet selbst).
    void start(double srcPos, double dir, int grainLen);
    float process(const std::vector<float>& x, double srcPos, int grainLen, const GrainFrame& f);

private:
    struct Grain
    {
        bool active = false;
        bool fadeIn = false;
        double pos = 0.0;
        double dir = 1.0;
        int age = 0;
        int len = 0;
        int fade = 1;
    };

    void launch(double srcPos, double dir, int grainLen, bool fadeIn);

    std::array<Grain, 2> grains_ {};
    int untilNext_ = 0;
};

} // namespace dg
```

`engine/GrainStretch.cpp`:

```cpp
#include "engine/GrainStretch.h"
#include <algorithm>
#include <cmath>
#include "engine/SampleInterp.h"

namespace dg {

namespace {
float readGrain(const std::vector<float>& x, double pos, const GrainFrame& f)
{
    const double span = f.hi - f.lo;
    if (f.cycle && span > 0.0)
    {
        pos = f.lo + std::fmod(pos - f.lo, span);
        if (pos < f.lo)
            pos += span;
    }
    else
        pos = std::clamp(pos, f.lo, std::max(f.lo, f.hi - 1.0));
    return cubicAt(x, pos);
}
} // namespace

void GrainStretch::reset()
{
    for (auto& g : grains_)
        g.active = false;
    untilNext_ = 0;
}

void GrainStretch::launch(double srcPos, double dir, int grainLen, bool fadeIn)
{
    // Freien Platz nehmen, sonst das älteste Stück ersetzen (nur bei plötzlich kleinerer Stückgröße).
    Grain* slot = &grains_[0];
    for (auto& g : grains_)
        if (!g.active)
        {
            slot = &g;
            break;
        }
        else if (g.age > slot->age)
            slot = &g;
    slot->active = true;
    slot->fadeIn = fadeIn;
    slot->pos = srcPos;
    slot->dir = dir;
    slot->age = 0;
    slot->len = grainLen;
    slot->fade = std::max(1, grainLen / 4);
    untilNext_ = grainLen;
}

void GrainStretch::start(double srcPos, double dir, int grainLen)
{
    reset();
    launch(srcPos, dir, grainLen, false);
}

float GrainStretch::process(const std::vector<float>& x, double srcPos, int grainLen, const GrainFrame& f)
{
    if (untilNext_ <= 0)
        launch(srcPos, f.dir, grainLen, true);
    --untilNext_;

    float y = 0.0f;
    for (auto& g : grains_)
    {
        if (!g.active)
            continue;
        float gain = 1.0f;
        if (g.fadeIn && g.age < g.fade)
            gain = static_cast<float>(g.age) / static_cast<float>(g.fade);
        if (g.age >= g.len)
            gain *= static_cast<float>(g.len + g.fade - g.age) / static_cast<float>(g.fade);
        y += gain * readGrain(x, g.pos, f);
        g.pos += g.dir * f.step;
        if (++g.age >= g.len + g.fade)
            g.active = false;
    }
    return y;
}

} // namespace dg
```

Prüfpunkt: Im Test „reset silences“ startet `process` nach `reset()` (`untilNext_ == 0`) ein Stück mit Einblendung, dessen erster Wert bei Alter 0 Gain 0 hat – deshalb 0.

- [ ] **Step 5: Run test to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[stretch]"` und `build\tests\Release\DubgefahrenTests.exe "[sampler]"` (die Interpolation wurde verschoben, die Sampler-Tests müssen unverändert grün bleiben).
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add engine/SampleInterp.h engine/GrainStretch.h engine/GrainStretch.cpp engine/SamplePlayer.cpp engine/CMakeLists.txt tests/test_GrainStretch.cpp tests/CMakeLists.txt
git commit -m "feat(engine): grain stretch unit with crossfaded grains

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Slot-Felder `StretchMode`, `StretchFactor`, `StretchGrain`

**Files:**
- Modify: `engine/SlotParams.h`, `engine/SlotFields.h`, `engine/SlotFields.cpp`, `plugin/PluginProcessor.cpp:312`
- Test: `tests/test_SlotFields.cpp`

**Interfaces:**
- Consumes: nichts aus Task 1.
- Produces:
  - `enum class dg::StretchMode { Off, All, Loop };` in `engine/SlotParams.h`; `SlotParams::stretchMode`, `stretchPct`, `grainMs`.
  - `SlotField::StretchMode`, `SlotField::StretchFactor`, `SlotField::StretchGrain` (nach `LoopXfade`).
  - `void dg::resetSampleFields(SlotParams& p)` (umbenannt aus `resetSampleRegionFields`); setzt die sechs Bereichsfelder und die drei Stretch-Felder auf ihre Standardwerte.

- [ ] **Step 1: Failing tests schreiben**

In `tests/test_SlotFields.cpp`: `kNumSlotFields == 25` und `keys.size() == 25` auf **28** ändern, den Testtitel „there are 25 slot fields“ auf „there are 28 slot fields“. Neue Tests anhängen und den bestehenden Test `resetSampleRegionFields restores only the six region fields` ersetzen:

```cpp
TEST_CASE("the stretch fields default to off, 100 percent and 20 ms", "[fields][stretch]")
{
    const SlotParams p = makeDefaultSlotParams();
    CHECK(p.stretchMode == StretchMode::Off);
    CHECK(p.stretchPct == 100.0f);
    CHECK(p.grainMs == 20.0f);
    CHECK(SlotParams {} == p);

    for (const auto f : { SlotField::StretchMode, SlotField::StretchFactor, SlotField::StretchGrain })
        CHECK(fieldSpec(f).versionHint == 5);
    CHECK(std::string(fieldSpec(SlotField::StretchMode).key) == "stretch");
    CHECK(std::string(fieldSpec(SlotField::StretchFactor).key) == "stretchPct");
    CHECK(std::string(fieldSpec(SlotField::StretchGrain).key) == "grainMs");
    CHECK(fieldSpec(SlotField::StretchMode).kind == FieldKind::Choice);
    CHECK(fieldSpec(SlotField::StretchMode).choices.size() == 3);
    CHECK(fieldSpec(SlotField::StretchFactor).min == 50.0f);
    CHECK(fieldSpec(SlotField::StretchFactor).max == 200.0f);
    CHECK(fieldSpec(SlotField::StretchGrain).min == 5.0f);
    CHECK(fieldSpec(SlotField::StretchGrain).max == 100.0f);
    CHECK(fieldSpec(SlotField::LoopXfade).versionHint == 4); // bestehende Hinweise bleiben
}

TEST_CASE("stretch fields clamp and convert", "[fields][stretch]")
{
    SlotParams p = makeDefaultSlotParams();
    setSlotField(p, SlotField::StretchMode, 2.0f);
    CHECK(p.stretchMode == StretchMode::Loop);
    setSlotField(p, SlotField::StretchMode, 9.0f);
    CHECK(p.stretchMode == StretchMode::Loop); // auf das Maximum begrenzt
    setSlotField(p, SlotField::StretchFactor, 1000.0f);
    CHECK(p.stretchPct == 200.0f);
    setSlotField(p, SlotField::StretchGrain, 0.0f);
    CHECK(p.grainMs == 5.0f);
    CHECK(getSlotField(p, SlotField::StretchMode) == 2.0f);
}

TEST_CASE("resetSampleFields restores the region and the stretch fields only", "[fields]")
{
    SlotParams p = makeDefaultSlotParams();
    p.sampleStart = 0.3f;
    p.loopStart = 0.4f;
    p.sampleEnd = 0.6f;
    p.loop = true;
    p.reverse = true;
    p.loopXfadePct = 20.0f;
    p.stretchMode = StretchMode::All;
    p.stretchPct = 150.0f;
    p.grainMs = 60.0f;
    p.tuneSemis = 7.0f;
    p.volumeDb = -12.0f;
    resetSampleFields(p);
    SlotParams expected = makeDefaultSlotParams();
    expected.tuneSemis = 7.0f;
    expected.volumeDb = -12.0f;
    CHECK(p == expected);
}
```

Außerdem in dem bestehenden Test `the sample region fields default to the whole file without loop` die Zeile mit der Schleife über `versionHint == 4` unverändert lassen.

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL beim Kompilieren (`StretchMode`, `resetSampleFields` unbekannt).

- [ ] **Step 3: Implementierung**

`engine/SlotParams.h`: nach `enum class TriggerMode` ergänzen

```cpp
// Stretch eines Sample-Slots: aus, ganzer Bereich oder nur die Loop-Region. Reihenfolge ist Teil des Host-States.
enum class StretchMode { Off, All, Loop };
```

und in `SlotParams` nach `loopXfadePct`:

```cpp
    StretchMode stretchMode = StretchMode::Off;
    float stretchPct = 100.0f;     // 50 .. 200 % der Dauer
    float grainMs = 20.0f;         // 5 .. 100 ms Stückgröße
```

`engine/SlotFields.h`: Enum erweitern `SampleStart, LoopStart, SampleEnd, Loop, Reverse, LoopXfade, StretchMode, StretchFactor, StretchGrain,`; die Deklaration `void resetSampleRegionFields(SlotParams& p);` ersetzen durch

```cpp
// Setzt die sechs Bereichsfelder und die drei Stretch-Felder eines Sample-Slots auf ihre Standardwerte
// (ganze Datei, kein Loop, kein Stretch).
void resetSampleFields(SlotParams& p);
```

(Kommentar darüber entsprechend anpassen.) `engine/SlotFields.cpp`: oben `constexpr const char* kStretchChoices[] = { "Off", "All", "Loop" };`, in `kSpecs` nach `xfade`:

```cpp
    { "stretch",    "Stretch",        FieldKind::Choice, 0.0f, 2.0f, 0.0f, 0.0f, "", C(kStretchChoices), 5 },
    { "stretchPct", "Stretch Factor", FieldKind::Float, 50.0f, 200.0f, 100.0f, 100.0f, "%", {}, 5 },
    { "grainMs",    "Stretch Grain",  FieldKind::Float, 5.0f, 100.0f, 20.0f, 20.0f, "ms", {}, 5 },
```

in `getSlotField`:

```cpp
        case SlotField::StretchMode:   return static_cast<float>(p.stretchMode);
        case SlotField::StretchFactor: return p.stretchPct;
        case SlotField::StretchGrain:  return p.grainMs;
```

in `setSlotField`:

```cpp
        case SlotField::StretchMode:   p.stretchMode = static_cast<StretchMode>(i); break;
        case SlotField::StretchFactor: p.stretchPct = value; break;
        case SlotField::StretchGrain:  p.grainMs = value; break;
```

und die Funktion ersetzen:

```cpp
void resetSampleFields(SlotParams& p)
{
    for (const auto f : { SlotField::SampleStart, SlotField::LoopStart, SlotField::SampleEnd, SlotField::Loop, SlotField::Reverse,
                          SlotField::LoopXfade, SlotField::StretchMode, SlotField::StretchFactor, SlotField::StretchGrain })
        setSlotField(p, f, fieldSpec(f).def);
}
```

`plugin/PluginProcessor.cpp` (Zeile ~312): `resetSampleRegionFields(p)` → `resetSampleFields(p)`; den Kommentar darüber auf „Marker, Loop, Reverse und Stretch gehören zu einer bestimmten Datei“ anpassen.

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[fields]"`.
Expected: PASS. (Plugin-Tests, die Parameterzahlen prüfen, folgen in Task 4 und schlagen bis dahin fehl.)

- [ ] **Step 5: Commit**

```bash
git add engine/SlotParams.h engine/SlotFields.h engine/SlotFields.cpp plugin/PluginProcessor.cpp tests/test_SlotFields.cpp
git commit -m "feat(engine): stretch mode, factor and grain slot fields

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Stretch im `SamplePlayer`

**Files:**
- Modify: `engine/SamplePlayer.h`, `engine/SamplePlayer.cpp`
- Test: `tests/test_SamplePlayer.cpp`

**Interfaces:**
- Consumes: `GrainStretch`, `GrainFrame` (Task 1); `SlotParams::stretchMode/stretchPct/grainMs`, `StretchMode` (Task 2).
- Produces: nichts Neues nach außen; das Verhalten ist über `SlotParams` gesteuert.

- [ ] **Step 1: Failing tests schreiben**

In `tests/test_SamplePlayer.cpp` anhängen (Helfer `rampData`, `rampAt`, `sampleParams`, `renderN`, `sineData`, `kSr` existieren oben in der Datei):

```cpp
namespace {
// Rendert, bis die Stimme endet (höchstens maxN Samples), und liefert die Ausgabe.
std::vector<float> renderUntilDone(SamplePlayer& v, const VoiceContext& ctx, int maxN)
{
    std::vector<float> all;
    std::vector<float> block(256);
    while (static_cast<int>(all.size()) < maxN && v.isActive())
    {
        v.render(block.data(), 256, ctx);
        all.insert(all.end(), block.begin(), block.end());
    }
    return all;
}
} // namespace

TEST_CASE("stretch off renders exactly like before whatever the other stretch fields say", "[sampler][stretch]")
{
    const auto d = sineData(440.0f, kSr, 0.5);
    SlotParams a = sampleParams();
    a.loop = true;
    a.loopXfadePct = 10.0f;
    SlotParams b = a;
    b.stretchMode = StretchMode::Off;
    b.stretchPct = 170.0f;
    b.grainMs = 7.0f;
    SamplePlayer va, vb;
    va.prepare(kSr);
    vb.prepare(kSr);
    VoiceContext ca { &a, 120.0, {}, &d };
    VoiceContext cb { &b, 120.0, {}, &d };
    va.start(ca, 1);
    vb.start(cb, 1);
    CHECK(renderN(va, ca, 30000) == renderN(vb, cb, 30000));
}

TEST_CASE("the stretch factor scales the duration", "[sampler][stretch]")
{
    const auto d = rampData(24000); // 0,5 s bei 48 kHz
    struct Case { float pct; int expected; } cases[] = { { 100.0f, 24000 }, { 200.0f, 48000 }, { 50.0f, 12000 } };
    for (const auto& c : cases)
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = sampleParams();
        p.stretchMode = StretchMode::All;
        p.stretchPct = c.pct;
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderUntilDone(v, ctx, 100000);
        INFO("factor " << c.pct);
        CHECK(static_cast<int>(out.size()) >= c.expected - 300);
        CHECK(static_cast<int>(out.size()) <= c.expected + 300);
    }
}

TEST_CASE("tune changes the pitch but not the duration while stretching", "[sampler][stretch]")
{
    const auto d = sineData(440.0f, kSr, 1.0);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.stretchMode = StretchMode::All;
    p.stretchPct = 200.0f;
    p.tuneSemis = 12.0f;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderUntilDone(v, ctx, 200000);
    CHECK(static_cast<int>(out.size()) >= 96000 - 300); // 1 s * 2
    CHECK(static_cast<int>(out.size()) <= 96000 + 300);
    CHECK_THAT(dgtest::estimateFrequency(out, kSr, 12000, 84000), WithinRel(880.0f, 0.08f));
}

TEST_CASE("stretching keeps the pitch at 50 and 200 percent", "[sampler][stretch]")
{
    for (const float pct : { 50.0f, 200.0f })
    {
        const auto d = sineData(440.0f, kSr, 1.0);
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = sampleParams();
        p.stretchMode = StretchMode::All;
        p.stretchPct = pct;
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderUntilDone(v, ctx, 200000);
        const auto to = static_cast<std::size_t>(out.size() * 9 / 10);
        INFO("factor " << pct);
        CHECK_THAT(dgtest::estimateFrequency(out, kSr, 3000, to), WithinRel(440.0f, 0.08f));
    }
}

TEST_CASE("grains read from the source position with the pitch step", "[sampler][stretch]")
{
    const auto d = rampData(24000);
    SlotParams p = sampleParams();
    p.stretchMode = StretchMode::All;
    p.grainMs = 20.0f; // 960 Samples
    {
        p.stretchPct = 200.0f; // Quellposition 0,5 pro Sample
        SamplePlayer v;
        v.prepare(kSr);
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 1300);
        CHECK_THAT(out[400], WithinAbs(rampAt(400), 1e-3));
        // Das zweite Stück startet bei Aufruf 960 an der Quellposition 480 und liest mit Schritt 1.
        CHECK_THAT(out[1260], WithinAbs(rampAt(480 + 300), 1e-3));
    }
    {
        p.stretchPct = 50.0f; // Quellposition 2 pro Sample
        SamplePlayer v;
        v.prepare(kSr);
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 1300);
        CHECK_THAT(out[1260], WithinAbs(rampAt(1920 + 300), 1e-3)); // Material wird übersprungen
    }
}

TEST_CASE("reverse stretches backwards from the end of the region", "[sampler][stretch]")
{
    const auto d = rampData(24000);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.stretchMode = StretchMode::All;
    p.stretchPct = 200.0f;
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 1300);
    CHECK_THAT(out[200], WithinAbs(rampAt(23999 - 200), 1e-3));
    CHECK_THAT(out[1260], WithinAbs(rampAt(23999 - 480 - 300), 1e-3));
    const auto all = [&] {
        SamplePlayer w;
        w.prepare(kSr);
        w.start(ctx, 1);
        return renderUntilDone(w, ctx, 100000);
    }();
    CHECK(static_cast<int>(all.size()) >= 48000 - 300);
    CHECK(static_cast<int>(all.size()) <= 48000 + 300);
}

TEST_CASE("a stretched loop never ends and stays inside the loop segment", "[sampler][stretch]")
{
    const auto d = rampData(24000);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.stretchMode = StretchMode::All;
    p.stretchPct = 150.0f;
    p.loop = true;
    p.trigMode = TriggerMode::Gate;
    p.loopStart = 0.5f; // 12000
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 200000);
    CHECK(v.isActive());
    CHECK(dgtest::allFinite(out));
    // Nach dem ersten Durchlauf liest nichts außerhalb des Loop-Segments [12000, 24000).
    float lo = 1e9f, hi = -1e9f;
    for (std::size_t i = 100000; i < out.size(); ++i)
    {
        lo = std::min(lo, out[i]);
        hi = std::max(hi, out[i]);
    }
    CHECK(lo >= rampAt(12000) - 1e-3f);
    CHECK(hi <= rampAt(24000) + 1e-3f);
}

TEST_CASE("loop mode plays normally up to the loop start and stretches the loop region", "[sampler][stretch]")
{
    const auto d = rampData(24000);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.stretchMode = StretchMode::Loop;
    p.stretchPct = 200.0f;
    p.loop = true;
    p.trigMode = TriggerMode::Gate;
    p.loopStart = 0.5f; // 12000
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 60000);
    CHECK_THAT(out[5000], WithinAbs(rampAt(5000), 1e-3)); // vor dem Loop: normal, ungestretcht
    CHECK(v.isActive());
    float lo = 1e9f;
    for (std::size_t i = 14000; i < out.size(); ++i)
        lo = std::min(lo, out[i]);
    CHECK(lo >= rampAt(12000) - 1e-3f); // danach nur noch Material der Loop-Region
}

TEST_CASE("loop mode without loop behaves like stretch off", "[sampler][stretch]")
{
    const auto d = sineData(440.0f, kSr, 0.5);
    SlotParams a = sampleParams();
    SlotParams b = a;
    b.stretchMode = StretchMode::Loop;
    b.stretchPct = 170.0f;
    SamplePlayer va, vb;
    va.prepare(kSr);
    vb.prepare(kSr);
    VoiceContext ca { &a, 120.0, {}, &d };
    VoiceContext cb { &b, 120.0, {}, &d };
    va.start(ca, 1);
    vb.start(cb, 1);
    CHECK(renderN(va, ca, 20000) == renderN(vb, cb, 20000));
}

TEST_CASE("a sample shorter than one grain stretches cleanly", "[sampler][stretch]")
{
    const auto d = rampData(100);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.stretchMode = StretchMode::All;
    p.stretchPct = 150.0f;
    p.grainMs = 100.0f;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderUntilDone(v, ctx, 20000);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::allFinite(out));
    CHECK(dgtest::peakAbs(out) <= rampAt(100) + 1e-3f);
}

TEST_CASE("changing grain size and factor while playing stays continuous", "[sampler][stretch]")
{
    const auto d = sineData(220.0f, kSr, 2.0);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.stretchMode = StretchMode::All;
    p.stretchPct = 120.0f;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    std::vector<float> out;
    for (int block = 0; block < 60; ++block)
    {
        if (block == 10) p.grainMs = 60.0f;
        if (block == 20) p.stretchPct = 70.0f;
        if (block == 30) p.grainMs = 5.0f;
        if (block == 40) p.stretchPct = 190.0f;
        const auto b = renderN(v, ctx, 480);
        out.insert(out.end(), b.begin(), b.end());
    }
    CHECK(dgtest::allFinite(out));
    float maxJump = 0.0f;
    for (std::size_t i = 1; i < out.size(); ++i)
        maxJump = std::max(maxJump, std::abs(out[i] - out[i - 1]));
    CHECK(maxJump < 0.25f);
}

TEST_CASE("a retrigger in the middle of a stretch stays continuous", "[sampler][stretch]")
{
    const auto d = sineData(220.0f, kSr, 2.0);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.stretchMode = StretchMode::All;
    p.stretchPct = 150.0f;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    auto out = renderN(v, ctx, 5000);
    v.start(ctx, 2);
    const auto after = renderN(v, ctx, 5000);
    out.insert(out.end(), after.begin(), after.end());
    CHECK(dgtest::allFinite(out));
    float maxJump = 0.0f;
    for (std::size_t i = 1; i < out.size(); ++i)
        maxJump = std::max(maxJump, std::abs(out[i] - out[i - 1]));
    CHECK(maxJump < 0.25f);
}

TEST_CASE("pitch from the performance control does not change the stretched duration", "[sampler][stretch]")
{
    const auto d = rampData(24000);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.stretchMode = StretchMode::All;
    p.stretchPct = 200.0f;
    VoiceContext ctx { &p, 120.0, {}, &d };
    ctx.perf.pitchSemis = 12.0f;
    v.start(ctx, 1);
    const auto out = renderUntilDone(v, ctx, 100000);
    CHECK(static_cast<int>(out.size()) >= 48000 - 300);
    CHECK(static_cast<int>(out.size()) <= 48000 + 300);
}
```

`#include <catch2/matchers/catch_matchers_floating_point.hpp>` ist schon oben; `WithinRel` kommt aus derselben Datei (`using Catch::Matchers::WithinRel;` oben ergänzen).

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[stretch]"`
Expected: FAIL (die Faktoren wirken noch nicht: die Dauer-Tests liefern 24000 statt 48000 / 12000, die Lesepositions-Tests treffen nicht).

- [ ] **Step 3: Implementierung**

`engine/SamplePlayer.h`: `#include "engine/GrainStretch.h"`; Member ergänzen

```cpp
    // Stretch: pos_ ist dann die Quellposition und läuft mit Quellrate / Faktor; die Stücke lesen daneben.
    GrainStretch grains_;
    bool stretching_ = false;
    float smFactor_ = 1.0f;
```

`engine/SamplePlayer.cpp`, oben im anonymen Namespace zwei Helfer:

```cpp
float stretchTarget(const SlotParams& p) { return std::clamp(p.stretchPct, 50.0f, 200.0f) / 100.0f; }
int grainSamples(const SlotParams& p, double sampleRate)
{
    return std::max(4, static_cast<int>(std::lround(std::clamp(p.grainMs, 5.0f, 100.0f) * 0.001 * sampleRate)));
}
```

`prepare`: `grains_.reset(); stretching_ = false; smFactor_ = 1.0f;` ergänzen.

`start`: nach `startedAsOneShot_ = ...;` einfügen

```cpp
    smFactor_ = stretchTarget(p);
    stretching_ = p.stretchMode == StretchMode::All;
    if (stretching_)
        grains_.start(pos_, dir_, grainSamples(p, sampleRate_));
    else
        grains_.reset();
```

`stop()`: `stretching_ = false; grains_.reset();` ergänzen.

`render`, nach `const LoopSegment seg = ...;` (vor `segLen`):

```cpp
    const bool stretchWanted = p.stretchMode != StretchMode::Off;
    const float factorTarget = stretchTarget(p);
    const int grainLen = grainSamples(p, sampleRate_);
```

Die Zeile `const double xfWanted = looping ? ... : 0.0;` ändern in `const double xfWanted = (looping && !stretchWanted) ? ... : 0.0;` (X-Fade gilt bei Stretch nicht).

Im Schleifenkopf, nach der Zeile `const double step = baseStep * semitonesToRatio(...)`:

```cpp
        smFactor_ += perfCoeff_ * (factorTarget - smFactor_);

        // Stretch ein- oder ausschalten. Im Modus Loop beginnt er, sobald die Position die Loop-Region erreicht.
        const bool inLoopRegion = looping && (dir_ > 0.0 ? pos_ >= seg.lo : pos_ < seg.hi);
        const bool wantStretch = p.stretchMode == StretchMode::All || (p.stretchMode == StretchMode::Loop && inLoopRegion);
        if (wantStretch && !stretching_)
        {
            // Der einfache Kopf läuft als zweiter Lesekopf aus, das erste Stück beginnt an der Quellposition.
            fadePos_ = pos_;
            fadeDir_ = dir_;
            fadeGain_ = 1.0f;
            fadeRaw_ = false;
            grains_.start(pos_, dir_, grainLen);
        }
        else if (!wantStretch && stretching_)
            grains_.reset();
        stretching_ = wantStretch;
        // Quellposition pro Ausgabesample: mit Stretch Quellrate / Faktor, sonst der Tonhöhenschritt.
        const double adv = stretching_ ? baseStep / static_cast<double>(smFactor_) : step;
```

Im Rest der Schleife an drei Stellen `step` durch `adv` ersetzen, soweit es die Hauptposition `pos_` betrifft:
- `const bool far = over > step + 1.0;` → `over > adv + 1.0`
- im Lambda `endGain` einen Parameter `rate` ergänzen: `const auto endGain = [&](double pos, double dir, double rate) {...}` mit `const double remaining = (...) / rate;`; Aufruf für den Hauptkopf `endGain(pos_, dir_, adv)`, für den Fade-Kopf `endGain(fadePos_, fadeDir_, step)`.
- `pos_ += dir_ * step;` → `pos_ += dir_ * adv;`

Den Hauptwert berechnen:

```cpp
        const GrainFrame frame { step, dir_, looping, looping ? seg.lo : region.start, looping ? seg.hi : region.end };
        const float mainValue = stretching_ ? grains_.process(x, pos_, grainLen, frame) : read(pos_, dir_, false);
        float y = mainValue * endGain(pos_, dir_, adv) * (1.0f - fadeGain_);
```

(ersetzt die bisherige Zeile `float y = read(pos_, dir_, false) * endGain(pos_, dir_) * (1.0f - fadeGain_);`). Der Fade-Kopf bleibt: `y += read(fadePos_, fadeDir_, fadeRaw_) * endGain(fadePos_, fadeDir_, step) * fadeGain_;`.

Hinweis: In `read(...)` steht `xf`, das bei Stretch 0 ist; das einfache Lesen des Fade-Kopfs nutzt dasselbe.

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[stretch]"` und `build\tests\Release\DubgefahrenTests.exe "[sampler]"`.
Expected: PASS, alle bestehenden Sampler-Tests bleiben grün. Schlägt `the stretch factor scales the duration` knapp fehl (Dauer um ein paar Samples neben der Toleranz), prüfe die Ausblendung am Ende (`endGain` mit `adv`) und ob `stop()` bei `pos_ >= region.end` greift.

- [ ] **Step 5: Commit**

```bash
git add engine/SamplePlayer.h engine/SamplePlayer.cpp tests/test_SamplePlayer.cpp
git commit -m "feat(engine): stretch samples with crossfaded grains in the sample player

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Plugin-Parameter, Kit-Format Version 5, Zustand

**Files:**
- Modify: `plugin/KitFile.cpp` (`kVersion = 5`)
- Test: `tests/plugin/test_PluginProcessor.cpp`, `tests/plugin/test_KitFile.cpp`, `tests/plugin/test_Samples.cpp`

**Interfaces:**
- Consumes: die Felder aus Task 2 (die Parameter-Erzeugung läuft in `plugin/ParameterLayout.cpp` generisch über `kNumSlotFields`; dort ist nichts zu ändern).
- Produces: Kit-Dateien mit `"version": 5`.

- [ ] **Step 1: Failing tests schreiben**

`tests/plugin/test_PluginProcessor.cpp`, im Test `the plugin exposes 442 uniquely named parameters`: Titel auf „490“ ändern, `16 * 26 + 26` → `16 * 29 + 26`, `ids.size() == 442` → `490`, und anhängen:

```cpp
    CHECK(slotParamId(0, SlotField::StretchMode) == "s01_stretch");
    CHECK(slotParamId(15, SlotField::StretchFactor) == "s16_stretchPct");
    CHECK(slotParamId(3, SlotField::StretchGrain) == "s04_grainMs");
    CHECK(p.state().getParameter("s01_stretch")->isAutomatable());
```

`tests/plugin/test_PluginProcessor.cpp`: der Test `loading a state without the newer parameters resets them to their defaults` bekommt zusätzlich die Stretch-Parameter in `removed` und prüft sie. Die Zeile `const juce::StringArray removed { ... }` erweitern um `"s01_stretch", "s01_stretchPct", "s01_grainMs"`; vor `b.setLiveView(true)` ergänzen

```cpp
    b.state().getParameter("s01_stretch")->setValueNotifyingHost(1.0f);
    b.state().getParameter("s01_stretchPct")->setValueNotifyingHost(1.0f);
    b.state().getParameter("s01_grainMs")->setValueNotifyingHost(1.0f);
```

und nach den bestehenden Prüfungen

```cpp
    CHECK_THAT(b.state().getParameter("s01_stretch")->getValue(), WithinAbs(0.0, 1e-6));
    CHECK_THAT(b.state().getParameter("s01_stretchPct")->convertFrom0to1(b.state().getParameter("s01_stretchPct")->getValue()),
               WithinAbs(100.0, 1e-3));
    CHECK_THAT(b.state().getParameter("s01_grainMs")->convertFrom0to1(b.state().getParameter("s01_grainMs")->getValue()),
               WithinAbs(20.0, 1e-3));
```

`tests/plugin/test_KitFile.cpp`: in den Tests `sample slots survive a version 4 round-trip` (Titel → „version 5“) und `kits with empty slots survive a round-trip` `== 4` → `== 5`; im Test mit `newer`: `setProperty("version", 5)` → `6`. Im Round-trip-Test vor `const auto v = parsed(k);` ergänzen

```cpp
    k.slots[3].stretchMode = StretchMode::Loop;
    k.slots[3].stretchPct = 140.0f;
    k.slots[3].grainMs = 35.0f;
```

und nach der `xfade`-Zeile

```cpp
    CHECK(static_cast<double>(v["slots"][3]["params"]["stretch"]) == 2.0);
    CHECK(static_cast<double>(v["slots"][3]["params"]["stretchPct"]) == 140.0);
    CHECK(static_cast<double>(v["slots"][3]["params"]["grainMs"]) == 35.0);
```

Neuer Test am Ende der Datei:

```cpp
TEST_CASE("a version 4 kit without stretch keys loads with stretch off", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[3].source = SourceType::Sample;
    k.samples[3] = "horn.wav";
    k.names[3] = "Horn";
    auto v = parsed(k);
    v.getDynamicObject()->setProperty("version", 4);
    auto* params = v["slots"][3]["params"].getDynamicObject();
    params->removeProperty("stretch");
    params->removeProperty("stretchPct");
    params->removeProperty("grainMs");
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->slots[3].stretchMode == StretchMode::Off);
    CHECK(r.kit->slots[3].stretchPct == 100.0f);
    CHECK(r.kit->slots[3].grainMs == 20.0f);
}
```

`tests/plugin/test_Samples.cpp`: im Test, der `setSlotSample` mit anderem Sample prüft (um Zeile 324–341), vor `p.setSlot(3, sp, "horn", "horn.wav");` ergänzen `sp.stretchMode = StretchMode::All; sp.stretchPct = 150.0f; sp.grainMs = 60.0f;`, nach dem „dasselbe Sample“-Block `CHECK(sp.stretchMode == StretchMode::All);` und nach dem „anderes Sample“-Block

```cpp
    CHECK(sp.stretchMode == StretchMode::Off);
    CHECK_THAT(sp.stretchPct, WithinAbs(100.0, 1e-3));
    CHECK_THAT(sp.grainMs, WithinAbs(20.0, 1e-3));
```

Der Schlüssel-Test bei Zeile 373 (`for (const char* key : { "smpStart", ... "xfade" })`): die drei neuen Schlüssel `"stretch", "stretchPct", "grainMs"` zur Liste ergänzen (gleiche Änderung bei Zeile 230 in `test_KitFile.cpp`), falls diese Tests prüfen, dass Synth- und Sample-Kits die Schlüssel enthalten.

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, dann `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe`
Expected: FAIL (Version steht noch auf 4).

- [ ] **Step 3: Implementierung**

`plugin/KitFile.cpp`: `constexpr int kVersion = 4;` → `5`. Sonst keine Änderung: Schreiben und Lesen laufen über `kNumSlotFields`, fehlende Schlüssel behalten die Standardwerte aus `makeDefaultSlotParams()`.

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS (alle Tests, Engine und Plugin).

- [ ] **Step 5: Commit**

```bash
git add plugin/KitFile.cpp tests/plugin/test_PluginProcessor.cpp tests/plugin/test_KitFile.cpp tests/plugin/test_Samples.cpp
git commit -m "feat(plugin): kit format version 5 with the stretch fields

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Stretch-Gruppe im Slot-Editor

**Files:**
- Modify: `plugin/ui/SlotEditor.h`, `plugin/ui/SlotEditor.cpp`
- Test: `tests/plugin/test_Editor.cpp`

**Interfaces:**
- Consumes: `SlotField::StretchMode/StretchFactor/StretchGrain` (Task 2).
- Produces: `SlotEditor::showsStretchControls() const`, `bool stretchFactorEnabled() const`, `bool xfadeEnabled() const`, `bool showsStretchHint() const`, `juce::Component& stretchModeControl()`.

Neue Anordnung des Sample-Modus (die Zeilen sind 5 Zellen breit, Reihe 0 ist die Wellenform):

| Reihe | Beschriftung | Zellen 0 bis 4 |
|---|---|---|
| 1 | `SAMPLE\nSTRETCH` | Tune, Stretch, Factor, Grain, Hinweis |
| 2 | `LOOP\nTRIG` | Loop, Reverse, X-Fade, Mode, Choke |
| 3 | `AMP\nMIX` | Attack, Release, Volume, Pan, FX-Send |

- [ ] **Step 1: Failing test schreiben**

`tests/plugin/test_Editor.cpp` anhängen (nutzt `SampleSlotFixture` mit `f.set(...)` wie der Latch-Test):

```cpp
TEST_CASE("the stretch controls follow the stretch mode and the loop switch", "[editor][stretch]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleSlotFixture f;
    std::unique_ptr<juce::AudioProcessorEditor> editor(f.p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    e->selectSlot(2);
    auto& se = e->slotEditor();

    CHECK(se.showsStretchControls());
    CHECK_FALSE(se.stretchFactorEnabled()); // Aus
    CHECK(se.xfadeEnabled());
    CHECK_FALSE(se.showsStretchHint());

    f.set(SlotField::StretchMode, 1.0f); // Alles
    CHECK(se.stretchFactorEnabled());
    CHECK(se.xfadeEnabled()); // ohne Loop gilt der X-Fade-Regler nicht, wird aber nicht gesperrt
    f.set(SlotField::Loop, 1.0f);
    CHECK_FALSE(se.xfadeEnabled());

    f.set(SlotField::Loop, 0.0f);
    f.set(SlotField::StretchMode, 2.0f); // Loop ohne Loop
    CHECK_FALSE(se.stretchFactorEnabled());
    CHECK(se.showsStretchHint());
    f.set(SlotField::Loop, 1.0f);
    CHECK(se.stretchFactorEnabled());
    CHECK_FALSE(se.showsStretchHint());
    CHECK_FALSE(se.xfadeEnabled());

    e->selectSlot(0); // Synth-Slot: keine Stretch-Controls
    CHECK_FALSE(se.showsStretchControls());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL beim Kompilieren (`showsStretchControls` fehlt).

- [ ] **Step 3: Implementierung**

`plugin/ui/SlotEditor.h`: öffentliche Methoden

```cpp
    bool showsStretchControls() const { return stretch_.isVisible() && stretchPct_.isVisible() && grain_.isVisible(); }
    bool stretchFactorEnabled() const { return stretchPct_.isEnabled() && grain_.isEnabled(); }
    bool xfadeEnabled() const { return xfade_.isEnabled(); }
    bool showsStretchHint() const { return stretchHint_.isVisible(); }
```

private: `void updateStretchEnabled();` und Member

```cpp
    Choice stretch_ { u8("Stretch") };
    Knob stretchPct_ { u8("Factor") };
    Knob grain_ { u8("Grain") };
    juce::Label stretchHint_;
```

`plugin/ui/SlotEditor.cpp`:
- `kSampleRowNames` → `{ "SAMPLE\nSTRETCH", "LOOP\nTRIG", "AMP\nMIX" }`.
- Konstruktor: `sampleControls_` um `&stretch_, &stretchPct_, &grain_` erweitern; in der Liste der `addChildComponent`-Aufrufe ergänzen; `stretchHint_` mit `setText(u8("needs Loop"), dontSendNotification)`, `setFont(font(12.0f))`, `setColour(juce::Label::textColourId, colours::textDim)`, `setJustificationType(centredLeft)` und `addChildComponent(stretchHint_)`.
- Der bestehende Handler `loop_.button.onStateChange` ruft zusätzlich `updateStretchEnabled();` auf. `stretch_.box.onChange = [this] { updateStretchEnabled(); };`
- `setSlot`: nach den bestehenden `attach`-Aufrufen

```cpp
    stretch_.attach(s, slotParamId(slot, SlotField::StretchMode));
    stretchPct_.attach(s, slotParamId(slot, SlotField::StretchFactor));
    grain_.attach(s, slotParamId(slot, SlotField::StretchGrain));
```

- `refresh()`: vor `resized()` `updateStretchEnabled();`; `stretchHint_` wird in `updateStretchEnabled` nur im Sample-Modus sichtbar gemacht, außerhalb des Sample-Modus unsichtbar gelassen.
- Neue Funktion:

```cpp
void SlotEditor::updateStretchEnabled()
{
    const int mode = stretch_.box.getSelectedItemIndex(); // 0 Off, 1 All, 2 Loop
    const bool loopOn = loop_.button.getToggleState();
    const bool active = mode > 0 && (mode != 2 || loopOn);
    stretchPct_.setEnabled(active);
    grain_.setEnabled(active);
    xfade_.setEnabled(!(mode > 0 && loopOn)); // bei Stretch mit Loop übernehmen die Stück-Crossfades
    stretchHint_.setVisible(mode_ == Mode::Sample && mode == 2 && !loopOn);
}
```

- `resized()`, Sample-Modus: Platzierung ersetzen durch

```cpp
        place(1, 0, tune_);    place(1, 1, stretch_); place(1, 2, stretchPct_); place(1, 3, grain_);
        stretchHint_.setBounds(kRowLabelWidth + 4 * kCellWidth, kTopOffset + kRowHeight, kCellWidth - 8, kRowHeight - 6);
        place(2, 0, loop_);    place(2, 1, reverse_); place(2, 2, xfade_);     place(2, 3, trigMode_); place(2, 4, choke_);
        place(3, 0, attack_);  place(3, 1, release_); place(3, 2, vol_);       place(3, 3, pan_);      place(3, 4, send_);
```

Prüfpunkt Step 4: Falls `setEnabled(false)` an den zusammengesetzten Reglern keine sichtbare Abdunklung bewirkt, `Knob` und `Choice` in `plugin/ui/Controls.cpp` ergänzen: bei `enabledStateChanged()` `setAlpha(isEnabled() ? 1.0f : 0.4f)`.

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS (alle Tests). Außerdem die Editor-Tests `[editor]` im Blick behalten, weil sich Positionen im Sample-Modus geändert haben (sie prüfen nur die Wellenform und Sichtbarkeit).

- [ ] **Step 5: Commit**

```bash
git add plugin/ui/SlotEditor.h plugin/ui/SlotEditor.cpp plugin/ui/Controls.cpp tests/plugin/test_Editor.cpp
git commit -m "feat(ui): stretch controls in the sample slot editor

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

(`plugin/ui/Controls.cpp` nur ins `git add`, wenn dort etwas geändert wurde.)

---

### Task 6: Dokumentation und Abnahmeliste

**Files:**
- Modify: `README.md`, `docs/superpowers/specs/2026-10-12-vintage-stretch-design.md`

- [ ] **Step 1: README ergänzen**

Im Abschnitt `## Kits` nach dem Absatz zu den Samples einen Absatz anhängen:

```markdown
Sample pads have a vintage time-stretch in the style of the Akai S950: the sample is cut into short grains that are repeated or skipped and joined with short crossfades. Stretch can be Off, All (everything between start and end) or Loop (the beginning plays normally, only the loop region is stretched; needs Loop on). Factor sets the duration (50–200 %, pitch stays the same; Tune and the performance pitch only change the pitch), Grain sets the grain size (5–100 ms): short grains sound metallic and rough, long ones softer with audible repeats. With Stretch on and Loop on, the Loop X-Fade has no effect.
```

In der Abnahmeliste ans Ende anhängen:

```markdown
- [ ] Stretch: Factor 100 % with Stretch All sounds like the plain sample; 200 % takes twice as long without changing the pitch, 50 % half as long.
- [ ] Stretch: Grain 10 ms is metallic and rough, 60 ms softer with audible repeats; a break from 130 to 170 BPM (about 76 %) still grooves.
- [ ] Stretch with Loop and Reverse: loop regions cycle without clicks; Loop mode plays the beginning normally; Tune only changes the pitch.
- [ ] Stretch Off sounds exactly like before; old sets and kits load with Stretch Off.
```

- [ ] **Step 2: Spec an die Umsetzung angleichen**

In `docs/superpowers/specs/2026-10-12-vintage-stretch-design.md`:
- Abschnitt 5, Zeile zum Loop-X-Fade ersetzen: „**Loop-X-Fade:** Bei Stretch (Alles oder Loop) mit eingeschaltetem Loop gilt der Regler nicht; die Stück-Crossfades übernehmen seine Rolle.“
- Abschnitt 8, die Punkte zu den Hinweisen ersetzen: Im Modus Loop ohne Loop sind Faktor und Grain deaktiviert und neben ihnen steht der Hinweis „needs Loop“; der X-Fade-Regler ist bei Stretch (Alles oder Loop) mit Loop deaktiviert. Die Anordnung im Sample-Modus: Reihe `SAMPLE/STRETCH` mit Tune, Stretch, Factor, Grain und Hinweis, Reihe `LOOP/TRIG`, Reihe `AMP/MIX`.
- Abschnitt 6.2: ergänzen „Ohne Loop halten die Stücke an den Bereichsgrenzen ihren letzten Wert, bis die Ausblendung am Ende greift. Schaltet der Stretch während des Spielens von aktiv auf inaktiv (Modus live auf Aus gestellt, Marker unter der Stimme verschoben), wechselt der Hauptkopf ohne Überblendung auf einfaches Lesen.“
- Status auf „Plan geschrieben, Umsetzung offen“.

- [ ] **Step 3: Gesamtlauf**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 all`
Expected: Build, alle Tests und die Validierung (VST3 Validator, pluginval) grün.

- [ ] **Step 4: Commit**

```bash
git add README.md docs/superpowers/specs/2026-10-12-vintage-stretch-design.md
git commit -m "docs: describe the vintage stretch and its acceptance checks

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 5 (Controller): Zip und PR**

Nach grünen Tests das Zip laut Global Constraints erzeugen und den PR (Englisch, mit den zurückgestellten Punkten: Tempo-Sync mit dem Host, Faktoren über 200 %, Stückgrenzen in der Wellenform, Stretch für Synth-Slots, die drei bekannten Vereinfachungen) öffnen. Die Abnahme in Ableton erfolgt durch den Nutzer.
