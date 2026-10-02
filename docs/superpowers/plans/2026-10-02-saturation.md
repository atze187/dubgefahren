# Saturation (weich bis hart, ADAA) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Das einfache `tanh`-Drive wird durch eine neue Klasse `Saturator` ersetzt: weich und warm bei wenig Drive, ab der Mitte zunehmend hart, mit ADAA gegen Aliasing, DC-Blocker und teilweisem Pegelausgleich (ca. +6 dB bei vollem Drive).

**Architecture:** `Saturator` (`engine/Saturator.h/.cpp`) hält pro Signalweg den ADAA- und DC-Zustand. `FxChain` besitzt vier Instanzen (MainL, MainR, SendL, SendR) an der Stelle der bisherigen `driveSample`-Aufrufe. `engine/Drive.h` entfällt. Die Parameter, die UI und die Drive-Glättung bleiben unverändert.

**Tech Stack:** C++20, Catch2 (`DubgefahrenTests`), CMake über `build.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-02-saturation-design.md`

## Global Constraints

- Schnittstelle: `void Saturator::prepare(double sampleRate)`, `void Saturator::reset()`, `float Saturator::process(float x, float drive)` mit `drive` 0..1 (bereits geglättet).
- Bei `drive <= 0` ist `process` bit-genau `x`.
- ADAA 1. Ordnung, Rechnung in `double`; Schwelle für kleine Schritte `1e-6`.
- Konstanten laut Spec: `g = 10^(1,4·d)`, Härte `m = smoothstep(0,3 … 1,0 | d)`, Bias `b = 0,1`, DC-Blocker ca. 10 Hz, Mischung `x + min(1, 10·d)·(y − x)`.
- Pegel: ca. +6 dB (± 1,5 dB) für einen Sinus bei −12 dBFS zwischen `d = 0` und `d = 1`. Startwert `β = 0,32`, wird per Test abgestimmt.
- Alle Klangkonstanten stehen als benannte Werte oben in `Saturator.cpp`.
- Nicht enthalten: UI, Grit-Knob, Oversampling, Tone-Regler, neue Parameter.
- Build und Test: `powershell -ExecutionPolicy Bypass -File build.ps1 test`. Nur Saturator-Tests: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, danach `build\tests\Release\DubgefahrenTests.exe "[saturator]"`.
- Commit-Messages enden mit der Zeile `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

1. Stille und konstantes Eingangssignal bei Drive über 0: keine NaN, der Ausgang kehrt auf 0 zurück (Task 1).
2. Drive-Sweep 0 → 1 über die Zeit: keine Sprünge im Ausgang (Task 1).
3. Erstes Sample nach `reset()` und nach dem Aufdrehen aus 0: keine Spitze (Task 1).
4. Andere Abtastraten (44,1 und 96 kHz): stabil, begrenzt (Task 1).
5. NaN im Eingang der Kette: `FxChain` setzt zurück und läuft danach sauber weiter; fehlt der Reset der Sättiger, bleibt der Zustand vergiftet (Task 4).

---

### Task 1: `Saturator`-Kern (Kennlinien, ADAA, DC-Blocker) mit Grundtests

**Files:**
- Create: `engine/Saturator.h`, `engine/Saturator.cpp`
- Create: `tests/test_Saturator.cpp`
- Modify: `engine/CMakeLists.txt` (Quellen), `tests/CMakeLists.txt` (Testdatei)

**Interfaces:**
- Produces: `dg::Saturator` mit `prepare(double)`, `reset()`, `float process(float x, float drive)`. Task 2 bis 4 bauen darauf auf.

- [ ] **Step 1: Failing tests schreiben**

`tests/test_Saturator.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/Saturator.h"
#include "TestHelpers.h"

using namespace dg;

namespace {
constexpr double kSr = 48000.0;

std::vector<float> saturate(const std::vector<float>& x, float drive, double sr = kSr)
{
    Saturator s;
    s.prepare(sr);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        y[i] = s.process(x[i], drive);
    return y;
}
} // namespace

TEST_CASE("drive 0 is bit-exact transparent", "[saturator]")
{
    Saturator s;
    s.prepare(kSr);
    for (float x : { -1.5f, -0.3f, 0.0f, 0.2f, 0.9f, 3.0f })
        CHECK(s.process(x, 0.0f) == x);
}

TEST_CASE("drive 1 stays finite and bounded for huge inputs", "[saturator]")
{
    Saturator s;
    s.prepare(kSr);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 2000; ++i)
    {
        const float x = (i % 2 == 0 ? 1.0e6f : -1.0e6f) * (i % 7 == 0 ? 0.01f : 1.0f);
        const float y = s.process(x, 1.0f);
        finite = finite && std::isfinite(y);
        peak = std::max(peak, std::abs(y));
    }
    CHECK(finite);
    CHECK(peak < 2.0f);
}

TEST_CASE("louder input gives louder output", "[saturator]")
{
    Saturator a, b;
    a.prepare(kSr);
    b.prepare(kSr);
    CHECK(a.process(0.5f, 0.7f) > b.process(0.2f, 0.7f));
}

TEST_CASE("asymmetric saturation leaves no DC", "[saturator]")
{
    const auto y = saturate(dgtest::sine(200.0f, kSr, 48000, 0.5f), 1.0f);
    double sum = 0.0;
    for (std::size_t i = 24000; i < y.size(); ++i)
        sum += y[i];
    CHECK(std::abs(sum / 24000.0) < 0.01);
}

TEST_CASE("silence and constant input never produce NaN and settle at zero", "[saturator]")
{
    const std::vector<float> silence(48000, 0.0f);
    const auto ys = saturate(silence, 0.8f);
    CHECK(dgtest::allFinite(ys));
    CHECK(dgtest::peakAbs(ys) < 1.0e-6f);

    const std::vector<float> dc(48000, 0.3f);
    const auto yd = saturate(dc, 0.8f);
    CHECK(dgtest::allFinite(yd));
    CHECK(dgtest::peakAbs(yd, 40000) < 0.01f); // der DC-Blocker nimmt den Gleichanteil weg
}

TEST_CASE("a drive sweep does not click", "[saturator]")
{
    const auto x = dgtest::sine(500.0f, kSr, 48000, 0.3f);
    Saturator sweep;
    sweep.prepare(kSr);
    std::vector<float> ys(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        ys[i] = sweep.process(x[i], static_cast<float>(i) / static_cast<float>(x.size()));
    const auto fixed = saturate(x, 1.0f);
    CHECK(dgtest::allFinite(ys));
    CHECK(dgtest::maxStep(ys) <= 1.2f * dgtest::maxStep(fixed) + 0.02f);
}

TEST_CASE("no spike right after reset or when drive comes up from zero", "[saturator]")
{
    Saturator s;
    s.prepare(kSr);
    for (int i = 0; i < 1000; ++i)
        s.process(0.5f * std::sin(0.05f * static_cast<float>(i)), 1.0f);
    s.reset();
    CHECK(std::abs(s.process(0.0f, 1.0f)) < 0.05f);

    Saturator t;
    t.prepare(kSr);
    for (int i = 0; i < 1000; ++i)
        t.process(0.2f, 0.0f);
    CHECK(std::abs(t.process(0.2f, 0.5f)) < 1.5f);
}

TEST_CASE("stable at 44.1 and 96 kHz", "[saturator]")
{
    for (const double sr : { 44100.0, 96000.0 })
    {
        const auto y = saturate(dgtest::sine(1000.0f, sr, static_cast<int>(sr), 0.5f), 1.0f, sr);
        CHECK(dgtest::allFinite(y));
        CHECK(dgtest::peakAbs(y) < 2.0f);
    }
}
```

In `tests/CMakeLists.txt` nach `test_FxUnits.cpp` die Zeile `    test_Saturator.cpp` einfügen. In `engine/CMakeLists.txt` in der Zeile mit `Drive.h SvFilter.h ...` nach `Drive.h` die Einträge `Saturator.h Saturator.cpp` einfügen (`Drive.h` bleibt bis Task 4).

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Fehler, `engine/Saturator.h` existiert nicht.

- [ ] **Step 3: Implementieren**

`engine/Saturator.h`:

```cpp
#pragma once

namespace dg {

// Sättigung für Drive: weich (tanh) bei wenig Drive, ab der Mitte zunehmend hart (Clip), jeweils mit
// ADAA gegen Aliasing, kleinem Bias für gerade Obertöne, DC-Blocker und teilweisem Pegelausgleich.
// drive 0..1 (bereits geglättet); 0 ist bit-genau transparent.
class Saturator
{
public:
    void prepare(double sampleRate);
    void reset();
    float process(float x, float drive);

private:
    double uPrev_ = 0.0;
    double fSoftPrev_ = 0.0;
    double fHardPrev_ = 0.0;
    bool havePrev_ = false;
    double dcX_ = 0.0;
    double dcY_ = 0.0;
    double dcR_ = 0.999;
};

} // namespace dg
```

`engine/Saturator.cpp`:

```cpp
#include "engine/Saturator.h"
#include <algorithm>
#include <cmath>

namespace dg {

namespace {
// Klangkonstanten. Hier nachjustieren, nicht im Code verteilt.
constexpr double kGainExponent = 1.4;   // g = 10^(1,4 d): bis ca. +28 dB
constexpr double kHardStart = 0.3;      // ab hier wird die Kennlinie zunehmend hart
constexpr double kBias = 0.1;           // Asymmetrie für gerade Obertöne
constexpr double kBeta = 0.32;          // Pegelausgleich g^(-beta): teilweise ausgeglichen
constexpr double kDcBlockHz = 10.0;
constexpr double kAdaaEps = 1.0e-6;
constexpr double kLn2 = 0.6931471805599453;
constexpr double kTwoPi = 6.283185307179586;

double smoothstep(double a, double b, double x)
{
    const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Stammfunktion von tanh, numerisch stabil für große |u|.
double logCosh(double u)
{
    const double a = std::abs(u);
    return a + std::log1p(std::exp(-2.0 * a)) - kLn2;
}

// Stammfunktion von clamp(u, -1, 1).
double hardAntiderivative(double u)
{
    const double a = std::abs(u);
    return a <= 1.0 ? 0.5 * u * u : a - 0.5;
}
} // namespace

void Saturator::prepare(double sampleRate)
{
    dcR_ = 1.0 - kTwoPi * kDcBlockHz / sampleRate;
    reset();
}

void Saturator::reset()
{
    uPrev_ = fSoftPrev_ = fHardPrev_ = 0.0;
    havePrev_ = false;
    dcX_ = dcY_ = 0.0;
}

float Saturator::process(float x, float drive)
{
    if (drive <= 0.0f)
    {
        reset(); // der erste Sample nach dem Aufdrehen beginnt ohne Altlasten
        return x;
    }

    const double d = std::min(static_cast<double>(drive), 1.0);
    const double g = std::pow(10.0, kGainExponent * d);
    const double m = smoothstep(kHardStart, 1.0, d);
    const double u = g * static_cast<double>(x) + kBias;

    const double fSoft = logCosh(u);
    const double fHard = hardAntiderivative(u);
    const double du = u - uPrev_;
    double ySoft, yHard;
    if (havePrev_ && std::abs(du) >= kAdaaEps)
    {
        ySoft = (fSoft - fSoftPrev_) / du;
        yHard = (fHard - fHardPrev_) / du;
    }
    else
    {
        const double um = havePrev_ ? 0.5 * (u + uPrev_) : u;
        ySoft = std::tanh(um);
        yHard = std::clamp(um, -1.0, 1.0);
    }
    uPrev_ = u;
    fSoftPrev_ = fSoft;
    fHardPrev_ = fHard;
    havePrev_ = true;

    // Offsets entfernen den Ruhe-Arbeitspunkt: bei Stille kommt 0 heraus.
    double y = (1.0 - m) * (ySoft - std::tanh(kBias)) + m * (yHard - std::clamp(kBias, -1.0, 1.0));
    y *= std::pow(g, -kBeta);

    const double blocked = y - dcX_ + dcR_ * dcY_;
    dcX_ = y;
    dcY_ = blocked;

    const double mix = std::min(1.0, 10.0 * d);
    return static_cast<float>(x + mix * (blocked - x));
}

} // namespace dg
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[saturator]"`
Expected: alle Tests grün. Bei Abweichung die gemessenen Werte notieren und melden, nicht blind lockern.

- [ ] **Step 5: Commit**

```bash
git add engine/Saturator.h engine/Saturator.cpp engine/CMakeLists.txt tests/test_Saturator.cpp tests/CMakeLists.txt
git commit -m "feat(engine): saturator with soft and hard curves, ADAA and DC blocker"
```

---

### Task 2: Pegel (+6 dB) und Charakter (gerade und ungerade Obertöne)

**Files:**
- Modify: `engine/Saturator.cpp` (`kBeta` nur falls nötig)
- Test: `tests/test_Saturator.cpp`

**Interfaces:**
- Consumes: `Saturator` aus Task 1, `saturate`-Helfer in `tests/test_Saturator.cpp`.

- [ ] **Step 1: Tests schreiben**

Im anonymen Namespace von `tests/test_Saturator.cpp` ergänzen (vor `} // namespace`):

```cpp
// Amplitude der k-ten Harmonischen von 1 kHz über die zweiten 0,5 s (ganze Perioden, daher exakt).
double harmonicAmp(const std::vector<float>& y, int k)
{
    constexpr std::size_t kFrom = 24000;
    double re = 0.0, im = 0.0;
    for (std::size_t i = kFrom; i < y.size(); ++i)
    {
        const double ph = 6.283185307179586 * 1000.0 * k * static_cast<double>(i) / kSr;
        re += y[i] * std::cos(ph);
        im -= y[i] * std::sin(ph);
    }
    return 2.0 * std::sqrt(re * re + im * im) / static_cast<double>(y.size() - kFrom);
}
```

Am Dateiende anhängen:

```cpp
TEST_CASE("full drive is about 6 dB louder than no drive for a -12 dBFS sine", "[saturator]")
{
    const auto x = dgtest::sine(1000.0f, kSr, 48000, 0.251f);
    const float off = dgtest::rms(saturate(x, 0.0f), 24000);
    const float on = dgtest::rms(saturate(x, 1.0f), 24000);
    const double gainDb = 20.0 * std::log10(static_cast<double>(on) / static_cast<double>(off));
    CHECK(gainDb > 4.5);
    CHECK(gainDb < 7.5);
}

TEST_CASE("low drive adds even harmonics, high drive gets harder", "[saturator]")
{
    const auto x = dgtest::sine(1000.0f, kSr, 48000, 0.5f);
    const auto soft = saturate(x, 0.3f);
    const auto hard = saturate(x, 1.0f);
    const double soft1 = harmonicAmp(soft, 1), hard1 = harmonicAmp(hard, 1);
    CHECK(harmonicAmp(soft, 2) / soft1 > 0.01);                          // gerade Obertöne (Bias)
    CHECK(harmonicAmp(hard, 3) / hard1 > harmonicAmp(soft, 3) / soft1);  // härter: stärkere 3. Harmonische
}
```

- [ ] **Step 2: Laufen lassen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[saturator]"`
Expected: erwartet grün, weil Task 1 schon die Zielwerte erzeugt (rechnerisch ca. +5,7 dB bei `β = 0,32`). Ist der Pegel-Test rot, liegt `gainDb` außerhalb 4,5 bis 7,5: `kBeta` in `Saturator.cpp` so ändern, dass der Wert nahe 6 liegt (größeres `β` macht leiser), und die Messung im Commit notieren. Sind die Charakter-Tests rot, Werte melden, nicht lockern.

- [ ] **Step 3: Mutation prüfen**

Kurz in `Saturator.cpp` `kBias` auf `0.0` setzen, bauen, `[saturator]` laufen lassen. Erwartet: der Charakter-Test schlägt fehl (keine gerade Harmonische). Danach zurücksetzen.

- [ ] **Step 4: Commit**

```bash
git add engine/Saturator.cpp tests/test_Saturator.cpp
git commit -m "test(engine): saturator level gain and harmonic character"
```

---

### Task 3: Aliasing-Test gegen naive Kennlinie

**Files:**
- Test: `tests/test_Saturator.cpp`

**Interfaces:**
- Consumes: `Saturator` aus Task 1.

- [ ] **Step 1: Test schreiben**

Im anonymen Namespace ergänzen (vor `} // namespace`):

```cpp
// Leistung bei Frequenz f über 1 s (ganze Perioden bei ganzzahligem f).
double powerAt(const std::vector<float>& y, double f)
{
    double re = 0.0, im = 0.0;
    for (std::size_t i = 0; i < y.size(); ++i)
    {
        const double ph = 6.283185307179586 * f * static_cast<double>(i) / kSr;
        re += y[i] * std::cos(ph);
        im -= y[i] * std::sin(ph);
    }
    return re * re + im * im;
}

// Alias-Energie relativ zur Energie der echten Harmonischen (9 kHz, 18 kHz) in dB.
// Bei 9 kHz Grundton und 48 kHz Abtastrate landen die Alias-Produkte auf 3, 6, 12, 15 und 21 kHz.
double aliasDb(const std::vector<float>& y)
{
    const double wanted = powerAt(y, 9000.0) + powerAt(y, 18000.0);
    double alias = 0.0;
    for (const double f : { 3000.0, 6000.0, 12000.0, 15000.0, 21000.0 })
        alias += powerAt(y, f);
    return 10.0 * std::log10(alias / wanted);
}

// Dieselbe Mischkennlinie wie Saturator bei drive 1 (rein hart), aber ohne ADAA.
std::vector<float> naiveHard(const std::vector<float>& x)
{
    const double g = std::pow(10.0, 1.4);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        y[i] = static_cast<float>(std::clamp(g * x[i] + 0.1, -1.0, 1.0));
    return y;
}
```

Am Dateiende anhängen:

```cpp
TEST_CASE("ADAA cuts alias energy by at least 6 dB compared with naive clipping", "[saturator]")
{
    const auto x = dgtest::sine(9000.0f, kSr, 48000, 0.5f);
    const double naive = aliasDb(naiveHard(x));
    const double adaa = aliasDb(saturate(x, 1.0f));
    CAPTURE(naive, adaa);
    CHECK(adaa < naive - 6.0);
}
```

- [ ] **Step 2: Laufen lassen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[saturator]"`
Expected: grün. Ist der Unterschied kleiner als 6 dB, die gemessenen `naive` und `adaa` melden, nicht lockern.

- [ ] **Step 3: Mutation prüfen**

In `Saturator.cpp` kurz die ADAA-Bedingung `if (havePrev_ && std::abs(du) >= kAdaaEps)` ersetzen durch `if (false)`, bauen, laufen lassen. Erwartet: der Aliasing-Test schlägt fehl. Danach zurücksetzen.

- [ ] **Step 4: Commit**

```bash
git add tests/test_Saturator.cpp
git commit -m "test(engine): ADAA reduces aliasing against naive clipping"
```

---

### Task 4: Einbau in `FxChain`, `Drive.h` entfällt

**Files:**
- Modify: `engine/FxChain.h`, `engine/FxChain.cpp`
- Modify: `engine/CMakeLists.txt` (`Drive.h` entfernen)
- Delete: `engine/Drive.h`
- Modify: `tests/test_FxUnits.cpp` (alten Drive-Test und `#include "engine/Drive.h"` entfernen)
- Test: `tests/test_FxChain.cpp`

**Interfaces:**
- Consumes: `Saturator` (Task 1 bis 3).

- [ ] **Step 1: Failing test schreiben** (an `tests/test_FxChain.cpp` anhängen)

Zuerst die vorhandenen Hilfen der Datei ansehen (`neutral()`, `Buses`, `run`), die der Test benutzt.

```cpp
TEST_CASE("NaN in the input resets the saturators and the chain recovers", "[fxchain]")
{
    auto p = neutral();
    p.drive = 0.5f;

    FxChain fx;
    fx.prepare(kSr);

    Buses bad(4800);
    bad.ml = dgtest::sine(440.0f, kSr, 4800, 0.5f);
    bad.mr = bad.ml;
    bad.ml[10] = std::numeric_limits<float>::quiet_NaN();
    run(fx, bad, p);

    Buses clean(48000);
    clean.ml = dgtest::sine(440.0f, kSr, 48000, 0.5f);
    clean.mr = clean.ml;
    run(fx, clean, p);
    CHECK(dgtest::allFinite(clean.ml));
    CHECK(dgtest::peakAbs(clean.ml, 24000) > 0.1f);
}
```

(`#include <limits>` oben ergänzen, falls es fehlt.) Dieser Test läuft auch heute grün, weil `driveSample` zustandslos ist. Er schützt die neue Umstellung: fehlt der Reset der Sättiger, vergiftet das NaN deren Zustand dauerhaft (Step 5, Mutation).

- [ ] **Step 2: Umbau**

`engine/FxChain.h`: `#include "engine/Saturator.h"` ergänzen, im privaten Teil nach `SvFilter filterSend_;` einfügen:

```cpp
    Saturator satMainL_, satMainR_, satSendL_, satSendR_;
```

`engine/FxChain.cpp`: `#include "engine/Drive.h"` entfernen. In `prepare` nach `filterSend_.prepare(sampleRate);` einfügen:

```cpp
    satMainL_.prepare(sampleRate);
    satMainR_.prepare(sampleRate);
    satSendL_.prepare(sampleRate);
    satSendR_.prepare(sampleRate);
```

In `reset` nach `filterSend_.reset();` einfügen:

```cpp
    satMainL_.reset();
    satMainR_.reset();
    satSendL_.reset();
    satSendR_.reset();
```

In `process` die vier Zeilen ersetzen:

```cpp
        const float ml = filterMain_.process(satMainL_.process(mainL[i], smDrive_), 0);
        const float mr = filterMain_.process(satMainR_.process(mainR[i], smDrive_), 1);
        float sl = filterSend_.process(satSendL_.process(sendL[i], smDrive_), 0);
        float sr = filterSend_.process(satSendR_.process(sendR[i], smDrive_), 1);
```

`engine/CMakeLists.txt`: in der Zeile mit `Drive.h Saturator.h ...` den Eintrag `Drive.h` entfernen. `engine/Drive.h` löschen (`git rm engine/Drive.h`). In `tests/test_FxUnits.cpp` die Zeile `#include "engine/Drive.h"` und den kompletten Testfall `drive 0 is transparent, drive 1 saturates and stays bounded` entfernen (die Inhalte sind in `test_Saturator.cpp` abgedeckt).

- [ ] **Step 3: Alles laufen lassen**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests grün, auch `extreme settings stay finite and below the ceiling`. Schlägt ein bestehender FxChain-Test fehl, Werte notieren und melden.

- [ ] **Step 4: Mutation prüfen**

In `FxChain::reset` kurz die vier `sat*_.reset();`-Zeilen auskommentieren, bauen, `[fxchain]` laufen lassen. Erwartet: der neue NaN-Test schlägt fehl. Danach zurücksetzen und erneut laufen lassen.

- [ ] **Step 5: Commit**

```bash
git add engine/FxChain.h engine/FxChain.cpp engine/CMakeLists.txt engine/Drive.h tests/test_FxUnits.cpp tests/test_FxChain.cpp
git commit -m "feat(engine): use the saturator for drive in the FX chain"
```

---

### Task 5: Gesamtprüfung und Abschluss

**Files:**
- Modify: `docs/superpowers/specs/2026-10-02-saturation-design.md` (Status)

- [ ] **Step 1: Gesamte Suite**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Engine- und Plugin-Tests grün.

- [ ] **Step 2: VST3-Validator**

Run: `powershell -ExecutionPolicy Bypass -File build.ps1 validate`
Expected: Validator läuft durch.

- [ ] **Step 3: Spec-Status aktualisieren und committen**

In der Spec `**Status:** Spec, Review und Plan offen` ändern zu `**Status:** Umgesetzt, Abnahme in Ableton offen`.

```bash
git add docs/superpowers/specs/2026-10-02-saturation-design.md
git commit -m "docs: mark the saturation spec as implemented"
```

- [ ] **Step 4: Hörtest (manuell, du)**

Das Plugin liegt nach dem Build unter `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3`. In Ableton prüfen: Drive niedrig (warm), Drive mittel bis hoch (härter, bissig), Pegelanstieg, kein Aliasing-Zischeln, Zusammenspiel mit Filter und Resonanz. Danach die Konstanten oben in `Saturator.cpp` (`kBeta`, `kBias`, `kHardStart`, `kGainExponent`) nach Gehör nachjustieren.
