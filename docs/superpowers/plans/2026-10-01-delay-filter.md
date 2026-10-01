# Delay-Filter im Feedback mit LFO – Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Der globale Filter und der Delay-Tone-Regler entfallen; stattdessen sitzt ein Filter (LP/BP/HP/Notch) mit LFO auf Cutoff und Resonanz im Feedback-Weg des Tape-Delays (Issue #20).

**Architecture:**
- **engine (JUCE-frei):** `SvFilter` wählt einen von vier Typen mit weichem Wechsel; `TapeDelay` bekommt Filter und LFO im Loop (Koeffizienten alle 8 Abtastwerte); `FxChain` verliert die zwei globalen Filter; `FxParams` trägt die neuen Felder.
- **plugin:** vier Parameter entfallen, neun kommen hinzu (`dly…`); `ParamCache` liest sie.
- **UI:** Das FX-Panel wird von 13 auf 16 Zellen à 60 px umgebaut, die Rate-Zelle wechselt zwischen Hz und Teilung.

**Tech Stack:** C++20, JUCE 8.0.15, Catch2 v3, MSVC x64, CMake.

**Spec:** `docs/superpowers/specs/2026-10-01-delay-filter-design.md`

## Global Constraints

- `engine/` darf **keine** JUCE-Header einbinden.
- Im Audio-Thread: keine Allokation, keine Locks, kein Datei-I/O.
- Entfallende Parameter-IDs: `fltCutoff`, `fltRes`, `fltType`, `dlyTone`. Neue Parameter-IDs und Standardwerte (ParameterID-Versionshinweis **5**):

  | `pid::` | ID | Anzeigename | Art | Bereich | Standard | Einheit |
  |---|---|---|---|---|---|---|
  | `delayFltType` | `dlyFltType` | `Delay Filter Type` | Choice `LP, BP, HP, Notch` | 0–3 | LP | – |
  | `delayFltCutoff` | `dlyFltCutoff` | `Delay Filter Cutoff` | Float, Skew-Mitte 1000 | 20–20000 | 2400 | `Hz` |
  | `delayFltRes` | `dlyFltRes` | `Delay Filter Resonance` | Float | 0–1 | 0,1 | – |
  | `delayLfoShape` | `dlyLfoShape` | `Delay LFO Shape` | Choice (Auswahl des Slot-LFO) | 0–4 | Triangle | – |
  | `delayLfoRate` | `dlyLfoRate` | `Delay LFO Rate` | Float, Skew-Mitte 2 | 0,05–40 | 0,5 | `Hz` |
  | `delayLfoSync` | `dlyLfoSync` | `Delay LFO Sync` | Bool | – | aus | – |
  | `delayLfoSyncDiv` | `dlyLfoSyncDiv` | `Delay LFO Sync Rate` | Choice (Auswahl des Slot-LFO) | 0–10 | 1/4 | – |
  | `delayLfoCutDepth` | `dlyLfoCutDepth` | `Delay LFO Cutoff Depth` | Float | 0–4 | 0 | `oct` |
  | `delayLfoResDepth` | `dlyLfoResDepth` | `Delay LFO Resonance Depth` | Float | 0–1 | 0 | – |

- Host-Parameter insgesamt: **441** (436 − 4 + 9). Zwischenstände: nach Task 1 433, nach Task 4 441.
- Drive, Wow, Feedback, Mix, Delay-Zeit und der Reverb bleiben unverändert. Kit-Format bleibt Version 4 (Kits enthalten keine FX-Parameter).
- Der LFO-Wert (0…1) wird zentriert (`2 * v - 1`); Cutoff: `cutoff * 2^(depthOct * c)`, Resonanz: `res + depth * c`, beides begrenzt (Cutoff 20 Hz … 0,49 · Abtastrate, Resonanz 0…1).
- Filterkoeffizienten werden **alle 8 Abtastwerte exakt** berechnet (`kControlInterval = 8`); Task 6 gleicht den Spec-Text („günstige Näherung“) daran an.
- Typwechsel: Mischgewichte gleiten in etwa 10 ms (`onePoleCoeff(0.01f, sr)`); Cutoff und Resonanz werden über die Kontrolltakte mit 5 ms geglättet (Cutoff im Log-Bereich).
- FX-Panel: 16 Zellen à 60 px (Abstand 6 px), Start bei x = 12, Gruppen `DRIVE 0/1`, `DELAY 1/4`, `FILTER 5/3`, `LFO 8/4`, `REVERB 12/3`, `MASTER 15/1`; die Panelhöhe (96 px) ändert sich nicht.
- UI im Look von #15: Schrift nur über `ui::font(...)`; Farben aus `colours` in `plugin/ui/DgLookAndFeel.h`. Quelltexte UTF-8; Kommentare Deutsch, UI-Texte Englisch.
- Befehle aus dem Repo-Root:
  - Build: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
  - Neu konfigurieren (nach CMake-Änderungen): `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
  - Engine-Tests: `build\tests\Release\DubgefahrenTests.exe "<tag>"`
  - Plugin-Tests: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "<tag>"`
  - Alles: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
  - Validator: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
- Bilder zur Sichtprüfung: Ist `DG_SNAPSHOT_DIR` gesetzt, schreiben die Render-Tests PNG-Dateien dorthin (`dgtest::savePng`).
- Jeder Task endet mit einem grünen Build und grünen Tests; Commits enden mit `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

- **Cutoff-Modulation an den Bereichsenden (20 Hz mit −4 Oktaven, 20 kHz mit +4 Oktaven) bei 44,1, 96 und 192 kHz.** Erwartet: endlich, begrenzt, keine Ausnahme. → Test in Task 3 („extreme cutoff and depth stay finite at every sample rate“).
- **Host-Tempo 0 oder sehr klein bei eingeschaltetem Sync.** Erwartet: Der LFO bleibt nicht auf einer Extremstellung stehen; das Tempo wird wie bei der Delay-Zeit auf mindestens 30 bpm geklemmt. → Test in Task 3 („the delay LFO rate follows the free setting or the tempo“).
- **Typwechsel, während Echos mit Feedback 1,1 kreisen.** Erwartet: endlich, begrenzt, kein Knacken. → Tests in Task 2 („switching the filter type glides without a click“) und Task 3 („changing the type while echoes circulate stays bounded“).
- **Ein alter Host-Zustand (0.5.0) mit den entfernten Parametern und ohne die neuen.** Erwartet: lädt ohne Fehler, neue Parameter auf Standardwerten, alte werden ignoriert. → Test in Task 4 („a host state from 0.5.0 still loads“).
- **Schmales Panel und Rate-Zelle im Sync-Modus.** Erwartet: alle 16 Zellen innerhalb des Panels, keine Überlappung außer dem Sync-Schalter über der Rate-Zelle; im Sync-Modus erscheint die Teilung statt des Hz-Reglers. → Tests in Task 5 („the FX panel fits all controls without overlap“, „the rate cell shows the division when sync is on“).

---

### Task 1: Globalen Filter entfernen

Der Filter im Main- und Send-Weg und seine drei Parameter fallen weg. `SvFilter` bleibt in der Engine (Task 2 baut ihn um), nur `FxChain` benutzt ihn nicht mehr. Delay Tone bleibt bis Task 3/4 unverändert.

**Files:**
- Modify: `engine/FxParams.h`, `engine/FxChain.h`, `engine/FxChain.cpp`
- Modify: `plugin/ParameterLayout.h`, `plugin/ParameterLayout.cpp`
- Modify: `plugin/ui/FxPanel.h`, `plugin/ui/FxPanel.cpp`
- Test: `tests/test_FxChain.cpp`, `tests/test_Engine.cpp`, `tests/plugin/test_PluginProcessor.cpp`

**Interfaces:**
- Consumes: bestehende `FxChain::process`, `driveSample`.
- Produces: `FxParams` ohne `cutoffHz`, `resonance`, `filterType`; `FxChain` ohne Filter. Host-Parameter: 433.

- [ ] **Step 1: Failing test schreiben**

In `tests/test_FxChain.cpp` nach dem Test `neutral settings pass the main bus through` einfügen:

```cpp
TEST_CASE("the main bus is not filtered", "[fxchain]")
{
    FxChain fx;
    fx.prepare(kSr);
    Buses b(48000);
    b.ml = dgtest::sine(15000.0f, kSr, 48000, 0.5f);
    b.mr = b.ml;
    run(fx, b, neutral());
    CHECK_THAT(dgtest::peakAbs(b.ml, 24000), WithinAbs(0.5, 0.01));
}
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und danach `build\tests\Release\DubgefahrenTests.exe "the main bus is not filtered"`
Expected: FAIL (der Tiefpass bei 20 kHz dämpft 15 kHz auf etwa 0,44).

- [ ] **Step 3: Engine umbauen**

`engine/FxParams.h`: die drei Zeilen `cutoffHz`, `resonance`, `filterType` aus `struct FxParams` löschen (die Zeilen mit `// 20 .. 20000`, `// 0 .. 1` für `resonance` und `// 0 = LP, 0.5 = BP, 1 = HP`).

`engine/FxChain.h`: `#include "engine/SvFilter.h"` löschen; die Member `SvFilter filterMain_; SvFilter filterSend_;`, `float cutoff_ = 20000.0f; bool cutoffInit_ = false;` und `float smFilterType_ = 0.0f;` löschen.

`engine/FxChain.cpp` – ersetzen:

```cpp
void FxChain::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    delay_.prepare(sampleRate);
    reverb_.prepare(sampleRate);
    limiter_.prepare(sampleRate);
    reset();
}

void FxChain::reset()
{
    delay_.reset();
    reverb_.reset();
    limiter_.reset();
    smoothInit_ = false;
}
```

In `FxChain::process` den Block von `const float target = std::clamp(p.cutoffHz …` bis einschließlich `filterSend_.setParams(…)` ersetzen durch:

```cpp
    delay_.setParams(static_cast<float>(delayDivisionSeconds(p.delayDiv, bpm)), p.delayFeedback, p.delayTone, p.delayWow);
    reverb_.setParams(p.reverbDecay, p.reverbTone);
```

Die Zeile `const float targetFilterType = …;` und `smFilterType_ = targetFilterType;` löschen; in der Sample-Schleife die Zeilen `smFilterType_ += …;`, `filterMain_.setTypeWeights(…);`, `filterSend_.setTypeWeights(…);` löschen und die vier Filterzeilen ersetzen durch:

```cpp
        const float ml = driveSample(mainL[i], smDrive_);
        const float mr = driveSample(mainR[i], smDrive_);
        float sl = driveSample(sendL[i], smDrive_);
        float sr = driveSample(sendR[i], smDrive_);
```

- [ ] **Step 4: Tests und Plugin anpassen**

`tests/test_FxChain.cpp`: in `neutral()` die drei Zeilen `p.cutoffHz = 20000.0f;`, `p.resonance = 0.1f;`, `p.filterType = 0.0f;` löschen; im Test `extreme settings stay finite …` die Zeilen `p.cutoffHz = 1000.0f;` und `p.resonance = 1.0f;` löschen.

`tests/test_Engine.cpp` Zeile 34: `p.global.fx.cutoffHz = 20000.0f;` löschen.

`plugin/ParameterLayout.h`: in `namespace dg::pid` die Zeilen `cutoff`, `resonance`, `filterType` löschen; in `ParamCache` die Zeile `Ptr drive_, cutoff_, resonance_, filterType_;` ersetzen durch `Ptr drive_;`.

`plugin/ParameterLayout.cpp`:
- die drei `layout.add(makeFloat(pid::cutoff …` / `pid::resonance` / `pid::filterType` Zeilen löschen;
- im `ParamCache`-Konstruktor `drive_(raw(apvts, pid::drive)), cutoff_(…), resonance_(…),` und `filterType_(raw(apvts, pid::filterType)), ` entfernen, sodass die Liste mit `drive_(raw(apvts, pid::drive)), delayTime_(raw(apvts, pid::delayTime)),` beginnt;
- in `ParamCache::read` die drei Zeilen `fx.cutoffHz = …`, `fx.resonance = …`, `fx.filterType = …` löschen.

`plugin/ui/FxPanel.h` – die drei Member `cutoff_`, `resonance_`, `filterType_` löschen.

`plugin/ui/FxPanel.cpp` – ersetzen:

```cpp
constexpr Group kGroups[] = { { "DRIVE", 0, 1 }, { "DELAY", 1, 5 }, { "REVERB", 6, 3 }, { "MASTER", 9, 1 } };
```

Im Konstruktor die drei `attach`-Zeilen für `cutoff_`, `resonance_`, `filterType_` löschen und die Liste in `addAndMakeVisible` auf `&drive_, &delayTime_, &delayFeedback_, &delayTone_, &delayWow_, &delayMix_, &reverbDecay_, &reverbTone_, &reverbMix_, &master_` kürzen. In `resized()`:

```cpp
    juce::Component* order[] = { &drive_, &delayTime_, &delayFeedback_, &delayTone_, &delayWow_, &delayMix_,
                                 &reverbDecay_, &reverbTone_, &reverbMix_, &master_ };
    for (int i = 0; i < 10; ++i)
        order[i]->setBounds(12 + i * kCell, 20, kCell - 6, getHeight() - 24);
```

`tests/plugin/test_PluginProcessor.cpp`: `16 * 26 + 20` → `16 * 26 + 17`, `ids.size() == 436` → `== 433`, Testname „436“ → „433“.

- [ ] **Step 5: Alles bauen und testen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS, auch `the main bus is not filtered`; keine Verweise mehr auf `cutoffHz`, `filterType` (Prüfung: `Grep` nach `cutoffHz|pid::cutoff|filterMain_` ohne Treffer in `engine/`, `plugin/`, `tests/`).

- [ ] **Step 6: Commit**

```bash
git add engine plugin tests
git commit -m "feat(fx): remove the global filter (#20)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 2: SvFilter mit vier Typen

**Files:**
- Modify: `engine/SvFilter.h`, `engine/SvFilter.cpp`
- Test: `tests/test_FxUnits.cpp`

**Interfaces:**
- Consumes: `onePoleCoeff`, `kPi` aus `engine/DspMath.h`.
- Produces: `enum class FilterType { Lowpass, Bandpass, Highpass, Notch }`, `constexpr int kNumFilterTypes = 4`; `SvFilter::setParams(float cutoffHz, float resonance)`, `SvFilter::setType(FilterType, bool immediate = false)`, `SvFilter::tick()` (einmal pro Abtastwert vor `process()` beider Kanäle), `SvFilter::process(float x, int channel)`. Entfallen: `setParams` mit drei Argumenten, `setTypeWeights`.

- [ ] **Step 1: Tests umschreiben und ergänzen**

In `tests/test_FxUnits.cpp` die Hilfsfunktion `filterGain` ersetzen:

```cpp
float filterGain(float freq, float cutoff, float res, FilterType type)
{
    SvFilter f;
    f.prepare(kSr);
    f.setParams(cutoff, res);
    f.setType(type, true);
    const auto x = dgtest::sine(freq, kSr, 48000);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        f.tick();
        y[i] = f.process(x[i], 0);
    }
    return dgtest::peakAbs(y, 24000);
}
```

Den Test `filter low pass, band pass and high pass responses` ersetzen:

```cpp
TEST_CASE("filter low pass, band pass and high pass responses", "[fx]")
{
    CHECK_THAT(filterGain(100.0f, 1000.0f, 0.0f, FilterType::Lowpass), WithinAbs(1.0, 0.05));
    CHECK(filterGain(10000.0f, 1000.0f, 0.0f, FilterType::Lowpass) < 0.05f);

    CHECK_THAT(filterGain(10000.0f, 1000.0f, 0.0f, FilterType::Highpass), WithinAbs(1.0, 0.05));
    CHECK(filterGain(100.0f, 1000.0f, 0.0f, FilterType::Highpass) < 0.05f);

    CHECK_THAT(filterGain(1000.0f, 1000.0f, 0.0f, FilterType::Bandpass), WithinAbs(1.0, 0.05));
    CHECK(filterGain(100.0f, 1000.0f, 0.0f, FilterType::Bandpass) < 0.3f);
    CHECK(filterGain(10000.0f, 1000.0f, 0.0f, FilterType::Bandpass) < 0.3f);
}

TEST_CASE("filter notch removes the cutoff and keeps the rest", "[fx]")
{
    CHECK(filterGain(1000.0f, 1000.0f, 0.0f, FilterType::Notch) < 0.05f);
    CHECK(filterGain(100.0f, 1000.0f, 0.0f, FilterType::Notch) > 0.9f);
    CHECK(filterGain(10000.0f, 1000.0f, 0.0f, FilterType::Notch) > 0.9f);
}

TEST_CASE("switching the filter type glides without a click", "[fx]")
{
    // Sinus 1 kHz, Amplitude 0,5: ohne Wechsel ändert er sich pro Abtastwert um höchstens ~0,07.
    SvFilter f;
    f.prepare(kSr);
    f.setParams(1000.0f, 0.0f);
    f.setType(FilterType::Lowpass, true);
    const auto x = dgtest::sine(1000.0f, kSr, 48000, 0.5f);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        if (i == 24000)
            f.setType(FilterType::Highpass);
        f.tick();
        y[i] = f.process(x[i], 0);
    }
    CHECK(dgtest::allFinite(y));
    CHECK(dgtest::maxStep(y, 24000, 24000 + 2400) < 0.15f);

    // Ohne immediate gleiten die Gewichte: nach 0,5 s arbeitet der Hochpass.
    SvFilter g;
    g.prepare(kSr);
    g.setParams(1000.0f, 0.0f);
    g.setType(FilterType::Lowpass, true);
    g.setType(FilterType::Highpass);
    const auto low = dgtest::sine(100.0f, kSr, 48000);
    std::vector<float> z(low.size());
    for (std::size_t i = 0; i < low.size(); ++i)
    {
        g.tick();
        z[i] = g.process(low[i], 0);
    }
    CHECK(dgtest::peakAbs(z, 24000) < 0.1f);
}
```

Den Test `filter stays stable at maximum resonance and extreme cutoff` ersetzen:

```cpp
TEST_CASE("filter stays stable at maximum resonance and extreme cutoff", "[fx]")
{
    for (const auto type : { FilterType::Lowpass, FilterType::Bandpass, FilterType::Highpass, FilterType::Notch })
        for (const float cutoff : { 20.0f, 1000.0f, 30000.0f })
        {
            SvFilter f;
            f.prepare(kSr);
            f.setParams(cutoff, 1.0f);
            f.setType(type, true);
            const auto x = dgtest::noise(48000 * 30, 0.5f);
            std::vector<float> y(x.size());
            for (std::size_t i = 0; i < x.size(); ++i)
            {
                f.tick();
                y[i] = f.process(x[i], 1);
            }
            CHECK(dgtest::allFinite(y));
            CHECK(dgtest::peakAbs(y) < 50.0f);
        }
}
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Build-FEHLER (`FilterType` unbekannt, `setType`/`tick` fehlen).

- [ ] **Step 3: SvFilter umbauen**

`engine/SvFilter.h` ersetzen:

```cpp
#pragma once

namespace dg {

enum class FilterType { Lowpass, Bandpass, Highpass, Notch };
constexpr int kNumFilterTypes = 4;

// Zustandsvariabler Filter (TPT, Zavalishin), 12 dB. Der Typ wird gewählt; bei einem Wechsel
// gleiten die Mischgewichte in etwa 10 ms zum neuen Typ, damit es nicht knackt.
class SvFilter
{
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(float cutoffHz, float resonance);
    // immediate: die Gewichte springen sofort (erster Aufruf, Reset); sonst gleiten sie über tick().
    void setType(FilterType type, bool immediate = false);
    // Einmal pro Abtastwert aufrufen, vor process() beider Kanäle: rückt die Typ-Gewichte vor.
    void tick();
    float process(float x, int channel);

private:
    double sampleRate_ = 44100.0;
    float g_ = 0.0f, k_ = 2.0f, a1_ = 1.0f, a2_ = 0.0f, a3_ = 0.0f;
    float wLp_ = 1.0f, wBp_ = 0.0f, wHp_ = 0.0f; // aktuelle Gewichte
    float tLp_ = 1.0f, tBp_ = 0.0f, tHp_ = 0.0f; // Zielgewichte
    float weightCoeff_ = 0.01f;
    float ic1_[2] {};
    float ic2_[2] {};
};

} // namespace dg
```

`engine/SvFilter.cpp` ersetzen:

```cpp
#include "engine/SvFilter.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

void SvFilter::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    weightCoeff_ = onePoleCoeff(0.01f, sampleRate);
    reset();
    setParams(20000.0f, 0.0f);
    setType(FilterType::Lowpass, true);
}

void SvFilter::reset()
{
    ic1_[0] = ic1_[1] = 0.0f;
    ic2_[0] = ic2_[1] = 0.0f;
}

void SvFilter::setParams(float cutoffHz, float resonance)
{
    const float sr = static_cast<float>(sampleRate_);
    const float fc = std::clamp(cutoffHz, 20.0f, 0.49f * sr);
    g_ = std::tan(kPi * fc / sr);
    const float q = 0.5f * std::pow(40.0f, std::clamp(resonance, 0.0f, 1.0f));
    k_ = 1.0f / q;
    a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
    a2_ = g_ * a1_;
    a3_ = g_ * a2_;
}

void SvFilter::setType(FilterType type, bool immediate)
{
    // Notch ist die Summe aus Tief- und Hochpass.
    switch (type)
    {
        case FilterType::Lowpass:  tLp_ = 1.0f; tBp_ = 0.0f; tHp_ = 0.0f; break;
        case FilterType::Bandpass: tLp_ = 0.0f; tBp_ = 1.0f; tHp_ = 0.0f; break;
        case FilterType::Highpass: tLp_ = 0.0f; tBp_ = 0.0f; tHp_ = 1.0f; break;
        case FilterType::Notch:    tLp_ = 1.0f; tBp_ = 0.0f; tHp_ = 1.0f; break;
    }
    if (immediate)
    {
        wLp_ = tLp_;
        wBp_ = tBp_;
        wHp_ = tHp_;
    }
}

void SvFilter::tick()
{
    wLp_ += weightCoeff_ * (tLp_ - wLp_);
    wBp_ += weightCoeff_ * (tBp_ - wBp_);
    wHp_ += weightCoeff_ * (tHp_ - wHp_);
}

float SvFilter::process(float x, int channel)
{
    float& ic1 = ic1_[channel];
    float& ic2 = ic2_[channel];
    const float v3 = x - ic2;
    const float v1 = a1_ * ic1 + a2_ * v3;
    const float v2 = ic2 + a2_ * ic1 + a3_ * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;

    const float lp = v2;
    const float bp = k_ * v1; // normiert: Spitzenverstärkung 1
    const float hp = x - k_ * v1 - v2;
    return wLp_ * lp + wBp_ * bp + wHp_ * hp;
}

} // namespace dg
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\Release\DubgefahrenTests.exe "[fx]"`
Expected: PASS. Weicht nur eine Schwelle knapp ab (z. B. die Notch-Dämpfung), den gemessenen Wert ausgeben und die Schwelle nur dann anpassen, wenn die Abweichung aus der Filtermathematik folgt und nicht aus einem Fehler; Begründung in die Commit-Nachricht.

- [ ] **Step 5: Alles testen und committen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

```bash
git add engine/SvFilter.h engine/SvFilter.cpp tests/test_FxUnits.cpp
git commit -m "feat(engine): state variable filter with four types and a gliding type switch (#20)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Filter und LFO im Delay-Loop

**Files:**
- Modify: `engine/FxParams.h`, `engine/FxParams.cpp`, `engine/TapeDelay.h`, `engine/TapeDelay.cpp`, `engine/FxChain.cpp`
- Test: `tests/test_TapeDelay.cpp`, `tests/test_FxChain.cpp`

**Interfaces:**
- Consumes: `SvFilter` (Task 2), `Lfo`, `lfoRateFromSync`, `LfoShape`, `SyncDivision`, `onePoleCoeff`.
- Produces: `struct DelayFilterParams { FilterType type; float cutoffHz, resonance; LfoShape lfoShape; float lfoRateHz, cutDepthOct, resDepth; }` mit den Standardwerten aus den Global Constraints; `TapeDelay::setParams(float timeSeconds, float feedback, float wow, const DelayFilterParams& filter)` (ersetzt die Fassung mit `tone`); `FxParams` bekommt `delayFilterType`, `delayFilterCutoffHz`, `delayFilterRes`, `delayLfoShape`, `delayLfoRateHz`, `delayLfoSync`, `delayLfoSyncDiv`, `delayLfoCutDepthOct`, `delayLfoResDepth`; `float resolveDelayLfoRate(const FxParams&, double bpm)`. `FxParams::delayTone` bleibt bis Task 4 (die Engine ignoriert es).

- [ ] **Step 1: Failing tests schreiben**

`tests/test_TapeDelay.cpp`: `#include <cmath>`, `#include "engine/Lfo.h"` ergänzen und im anonymen Namespace nach `impulseResponse` einfügen:

```cpp
// Praktisch durchlässig: Hochpass bei 20 Hz, keine Modulation.
DelayFilterParams openFilter()
{
    DelayFilterParams f;
    f.type = FilterType::Highpass;
    f.cutoffHz = 20.0f;
    f.resonance = 0.0f;
    f.cutDepthOct = 0.0f;
    f.resDepth = 0.0f;
    return f;
}

std::vector<float> runDelay(TapeDelay& d, const std::vector<float>& in, int n)
{
    std::vector<float> out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        const float v = static_cast<std::size_t>(i) < in.size() ? in[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(v, v, l, r);
        out[static_cast<std::size_t>(i)] = l;
    }
    return out;
}
```

Die bestehenden Aufrufe von `setParams` anpassen (neue Reihenfolge `time, feedback, wow, filter`):
- `d.setParams(0.25f, 0.0f, 1.0f, 0.0f);` → `d.setParams(0.25f, 0.0f, 0.0f, openFilter());`
- `d.setParams(0.25f, 0.5f, 1.0f, 0.0f);` → `d.setParams(0.25f, 0.5f, 0.0f, openFilter());`
- `d.setParams(0.1f, 1.1f, 1.0f, 1.0f);` → `d.setParams(0.1f, 1.1f, 1.0f, openFilter());`
- `d.setParams(0.01f, 0.3f, 0.5f, 0.0f);` → `d.setParams(0.01f, 0.3f, 0.0f, openFilter());`
- `d.setParams(0.250000089f, 0.3f, 0.5f, 0.0f);` → `d.setParams(0.250000089f, 0.3f, 0.0f, openFilter());`

Am Dateiende ergänzen:

```cpp
TEST_CASE("the first echo is already filtered", "[delay][filter]")
{
    DelayFilterParams lp = openFilter();
    lp.type = FilterType::Lowpass;
    lp.cutoffHz = 500.0f;
    const auto tone = dgtest::sine(8000.0f, kSr, 4800, 0.5f);

    TapeDelay open, closed;
    open.prepare(kSr);
    closed.prepare(kSr);
    open.setParams(0.25f, 0.0f, 0.0f, openFilter());
    closed.setParams(0.25f, 0.0f, 0.0f, lp);
    const auto a = runDelay(open, tone, 24000);
    const auto b = runDelay(closed, tone, 24000);
    const float rmsOpen = dgtest::rms(a, 12000, 16800);
    CHECK(rmsOpen > 0.3f);
    CHECK(dgtest::rms(b, 12000, 16800) < 0.05f * rmsOpen);
}

TEST_CASE("every repeat passes the filter again", "[delay][filter]")
{
    DelayFilterParams lp = openFilter();
    lp.type = FilterType::Lowpass;
    lp.cutoffHz = 2000.0f;
    const auto burst = dgtest::sine(4000.0f, kSr, 2400, 0.5f);

    const auto ratio = [&](const DelayFilterParams& f) {
        TapeDelay d;
        d.prepare(kSr);
        d.setParams(0.1f, 0.8f, 0.0f, f);
        const auto y = runDelay(d, burst, 14400);
        return dgtest::rms(y, 9600, 12000) / dgtest::rms(y, 4800, 7200);
    };
    CHECK(ratio(openFilter()) > 0.6f);
    CHECK(ratio(lp) < 0.3f);
}

TEST_CASE("the filter starts at the first set cutoff", "[delay][filter]")
{
    DelayFilterParams lp = openFilter();
    lp.type = FilterType::Lowpass;
    lp.cutoffHz = 200.0f;
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.001f, 0.0f, 0.0f, lp);
    const auto y = runDelay(d, dgtest::sine(8000.0f, kSr, 4800, 0.5f), 4800);
    CHECK(dgtest::rms(y, 96, 4800) < 0.002f); // kein Gleiten von einem früheren Wert herab
}

TEST_CASE("an LFO with zero depth leaves the filter static", "[delay][filter]")
{
    DelayFilterParams a = openFilter();
    a.type = FilterType::Lowpass;
    a.cutoffHz = 1500.0f;
    a.resonance = 0.3f;
    a.lfoShape = LfoShape::Square;
    a.lfoRateHz = 7.0f;
    DelayFilterParams b = a;
    b.lfoShape = LfoShape::Triangle;
    b.lfoRateHz = 0.3f;

    TapeDelay da, db;
    da.prepare(kSr);
    db.prepare(kSr);
    da.setParams(0.05f, 0.5f, 0.0f, a);
    db.setParams(0.05f, 0.5f, 0.0f, b);
    const auto in = dgtest::noise(24000, 0.5f);
    const auto ya = runDelay(da, in, 24000);
    const auto yb = runDelay(db, in, 24000);
    float maxDiff = 0.0f;
    for (std::size_t i = 0; i < ya.size(); ++i)
        maxDiff = std::max(maxDiff, std::abs(ya[i] - yb[i]));
    CHECK(maxDiff < 1.0e-6f);
}

TEST_CASE("the LFO moves the cutoff up and down around the setting", "[delay][filter]")
{
    const auto in = dgtest::noise(48000, 0.5f);
    const auto run = [&](float depthOct) {
        DelayFilterParams f = openFilter();
        f.type = FilterType::Lowpass;
        f.cutoffHz = 1000.0f;
        f.lfoShape = LfoShape::Square;
        f.lfoRateHz = 1.0f;
        f.cutDepthOct = depthOct;
        TapeDelay d;
        d.prepare(kSr);
        d.setParams(0.001f, 0.0f, 0.0f, f);
        return runDelay(d, in, 48000);
    };
    const auto moved = run(3.0f);
    const auto still = run(0.0f);
    // Square beginnt in der unteren Hälfte (c = -1): Cutoff 125 Hz, danach 8 kHz.
    const float low = dgtest::rms(moved, 2400, 21600);
    const float high = dgtest::rms(moved, 26400, 45600);
    const float middle = dgtest::rms(still, 2400, 45600);
    CHECK(low < middle);
    CHECK(middle < high);
    CHECK(high > 2.0f * low);
}

TEST_CASE("extreme cutoff and depth stay finite at every sample rate", "[delay][filter]")
{
    for (const double sr : { 44100.0, 96000.0, 192000.0 })
        for (const float cutoff : { 20.0f, 20000.0f })
        {
            DelayFilterParams f = openFilter();
            f.type = FilterType::Bandpass;
            f.cutoffHz = cutoff;
            f.resonance = 1.0f;
            f.lfoShape = LfoShape::SampleHold;
            f.lfoRateHz = 40.0f;
            f.cutDepthOct = 4.0f;
            f.resDepth = 1.0f;
            TapeDelay d;
            d.prepare(sr);
            d.setParams(0.05f, 0.9f, 1.0f, f);
            const auto in = dgtest::noise(static_cast<int>(sr), 0.5f);
            const auto y = runDelay(d, in, static_cast<int>(sr) * 2);
            CHECK(dgtest::allFinite(y));
            CHECK(dgtest::peakAbs(y) < 4.0f);
        }
}

TEST_CASE("changing the type while echoes circulate stays bounded", "[delay][filter]")
{
    DelayFilterParams f = openFilter();
    f.cutoffHz = 800.0f;
    f.resonance = 1.0f;
    f.lfoShape = LfoShape::Triangle;
    f.lfoRateHz = 3.0f;
    f.cutDepthOct = 2.0f;
    f.resDepth = 1.0f;
    TapeDelay d;
    d.prepare(kSr);
    const auto burst = dgtest::noise(24000, 0.5f);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 48000 * 20; ++i)
    {
        if (i % 4800 == 0)
        {
            f.type = static_cast<FilterType>((i / 4800) % kNumFilterTypes);
            d.setParams(0.1f, 1.1f, 1.0f, f);
        }
        const float in = i < 24000 ? burst[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(finite);
    CHECK(peak < 4.0f);
}

TEST_CASE("the delay LFO rate follows the free setting or the tempo", "[delay][filter]")
{
    FxParams p;
    p.delayLfoRateHz = 3.0f;
    CHECK_THAT(resolveDelayLfoRate(p, 120.0), WithinAbs(3.0, 1e-6));
    p.delayLfoSync = true;
    p.delayLfoSyncDiv = SyncDivision::D1_4;
    CHECK_THAT(resolveDelayLfoRate(p, 120.0), WithinAbs(2.0, 1e-6));
    CHECK_THAT(resolveDelayLfoRate(p, 0.0), WithinAbs(0.5, 1e-6)); // bpm auf 30 geklemmt
}
```

`tests/test_FxChain.cpp`: im Test `only the send bus reaches the delay` die Zeile `p.delayTone = 1.0f;` ersetzen durch `p.delayFilterType = FilterType::Highpass; p.delayFilterCutoffHz = 20.0f; p.delayFilterRes = 0.0f;`; im Test `extreme settings stay finite …` nach `p.delayWow = 1.0f;` einfügen:

```cpp
    p.delayFilterType = FilterType::Bandpass;
    p.delayFilterCutoffHz = 300.0f;
    p.delayFilterRes = 1.0f;
    p.delayLfoShape = LfoShape::SampleHold;
    p.delayLfoRateHz = 40.0f;
    p.delayLfoCutDepthOct = 4.0f;
    p.delayLfoResDepth = 1.0f;
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Build-FEHLER (`DelayFilterParams`, `resolveDelayLfoRate`, neue `FxParams`-Felder fehlen).

- [ ] **Step 3: FxParams**

`engine/FxParams.h`: nach `#pragma once` einfügen `#include "engine/SlotParams.h"` und `#include "engine/SvFilter.h"`; in `struct FxParams` nach `delayMix` ergänzen:

```cpp
    FilterType delayFilterType = FilterType::Lowpass;
    float delayFilterCutoffHz = 2400.0f; // 20 .. 20000
    float delayFilterRes = 0.1f;         // 0 .. 1
    LfoShape delayLfoShape = LfoShape::Triangle;
    float delayLfoRateHz = 0.5f;         // 0.05 .. 40
    bool delayLfoSync = false;
    SyncDivision delayLfoSyncDiv = SyncDivision::D1_4;
    float delayLfoCutDepthOct = 0.0f;    // 0 .. 4
    float delayLfoResDepth = 0.0f;       // 0 .. 1
```

Hinter dem `struct FxParams` (vor dem Ende von `namespace dg`) einfügen:

```cpp
// Rate des Delay-LFOs in Hz: frei oder aus der Teilung; das Tempo wird wie bei der Delay-Zeit auf
// mindestens 30 bpm geklemmt, damit der LFO nie auf einer Extremstellung stehen bleibt.
float resolveDelayLfoRate(const FxParams& p, double bpm);
```

`engine/FxParams.cpp`: `#include <algorithm>` und `#include "engine/Lfo.h"` ergänzen und anfügen:

```cpp
float resolveDelayLfoRate(const FxParams& p, double bpm)
{
    return p.delayLfoSync ? lfoRateFromSync(p.delayLfoSyncDiv, std::max(30.0, bpm)) : p.delayLfoRateHz;
}
```

- [ ] **Step 4: TapeDelay**

`engine/TapeDelay.h` ersetzen:

```cpp
#pragma once
#include <array>
#include <vector>
#include "engine/Lfo.h"
#include "engine/SlotParams.h"
#include "engine/SvFilter.h"

namespace dg {

// Filter im Feedback-Weg samt LFO; die Rate liegt bereits in Hz vor.
struct DelayFilterParams
{
    FilterType type = FilterType::Lowpass;
    float cutoffHz = 2400.0f;   // 20 .. 20000
    float resonance = 0.1f;     // 0 .. 1
    LfoShape lfoShape = LfoShape::Triangle;
    float lfoRateHz = 0.5f;     // 0 .. 40
    float cutDepthOct = 0.0f;   // 0 .. 4
    float resDepth = 0.0f;      // 0 .. 1
};

class TapeDelay
{
public:
    static constexpr float kMaxDelaySeconds = 10.0f;

    void prepare(double sampleRate);
    void reset();
    void setParams(float timeSeconds, float feedback, float wow, const DelayFilterParams& filter);
    // Liefert nur das Wet-Signal; es ist bereits gefiltert und speist das Feedback.
    void process(float inL, float inR, float& wetL, float& wetR);

private:
    float read(const std::vector<float>& buf, float delaySamples) const;
    void updateFilter(float lfo);

    static constexpr int kControlInterval = 8; // Filterkoeffizienten alle 8 Abtastwerte

    double sampleRate_ = 44100.0;
    std::array<std::vector<float>, 2> buf_;
    std::size_t write_ = 0;
    float target_ = 1.0f;
    float current_ = 1.0f;
    bool snapped_ = false;
    float glideCoeff_ = 0.001f;
    float feedback_ = 0.0f;
    float wow_ = 0.0f;
    float wowPhase1_ = 0.0f;
    float wowPhase2_ = 0.0f;

    SvFilter filter_;
    Lfo lfo_;
    DelayFilterParams filterParams_;
    bool filterSnapped_ = false;
    int controlCounter_ = 0;
    float controlCoeff_ = 0.1f;
    float cutLog2_ = 11.23f; // log2(2400)
    float resBase_ = 0.1f;
};

} // namespace dg
```

`engine/TapeDelay.cpp` – drei Änderungen:

1. `prepare` nach `glideCoeff_ = …;` ergänzen:

```cpp
    filter_.prepare(sampleRate);
    lfo_.prepare(sampleRate);
    controlCoeff_ = onePoleCoeff(0.005f, sampleRate / kControlInterval);
```

2. `reset` – die Zeile `lp_ = { 0.0f, 0.0f };` ersetzen durch:

```cpp
    filter_.reset();
    lfo_.reset(1);
    controlCounter_ = 0;
    filterSnapped_ = false;
```

3. `setParams`, `updateFilter` und `process` ersetzen (`read` bleibt):

```cpp
void TapeDelay::setParams(float timeSeconds, float feedback, float wow, const DelayFilterParams& filter)
{
    const float sr = static_cast<float>(sampleRate_);
    target_ = std::clamp(timeSeconds, 0.001f, kMaxDelaySeconds - 0.1f) * sr;
    if (!snapped_)
    {
        current_ = target_;
        snapped_ = true;
    }
    feedback_ = std::clamp(feedback, 0.0f, 1.1f);
    wow_ = std::clamp(wow, 0.0f, 1.0f);

    filterParams_ = filter;
    filterParams_.cutoffHz = std::clamp(filter.cutoffHz, 20.0f, 20000.0f);
    filterParams_.resonance = std::clamp(filter.resonance, 0.0f, 1.0f);
    filterParams_.lfoRateHz = std::clamp(filter.lfoRateHz, 0.0f, 40.0f);
    filterParams_.cutDepthOct = std::clamp(filter.cutDepthOct, 0.0f, 4.0f);
    filterParams_.resDepth = std::clamp(filter.resDepth, 0.0f, 1.0f);
    filter_.setType(filterParams_.type, !filterSnapped_);
    if (!filterSnapped_)
    {
        cutLog2_ = std::log2(filterParams_.cutoffHz);
        resBase_ = filterParams_.resonance;
        filterSnapped_ = true;
        controlCounter_ = 0;
    }
}

void TapeDelay::updateFilter(float lfo)
{
    const float sr = static_cast<float>(sampleRate_);
    cutLog2_ += controlCoeff_ * (std::log2(filterParams_.cutoffHz) - cutLog2_);
    resBase_ += controlCoeff_ * (filterParams_.resonance - resBase_);
    const float cutoff = std::clamp(std::exp2(cutLog2_ + filterParams_.cutDepthOct * lfo), 20.0f, 0.49f * sr);
    const float res = std::clamp(resBase_ + filterParams_.resDepth * lfo, 0.0f, 1.0f);
    filter_.setParams(cutoff, res);
}

void TapeDelay::process(float inL, float inR, float& wetL, float& wetR)
{
    const float sr = static_cast<float>(sampleRate_);
    current_ += (target_ - current_) * glideCoeff_; // Zeitänderung gleitet wie beim Band

    const float mod = wow_ * sr * (0.0025f * std::sin(wowPhase1_) + 0.0004f * std::sin(wowPhase2_));
    wowPhase1_ += kTwoPi * 0.55f / sr;
    wowPhase2_ += kTwoPi * 6.5f / sr;
    if (wowPhase1_ >= kTwoPi) wowPhase1_ -= kTwoPi;
    if (wowPhase2_ >= kTwoPi) wowPhase2_ -= kTwoPi;

    // LFO zentriert um den eingestellten Wert (c in [-1, 1]).
    const float lfo = 2.0f * lfo_.process(filterParams_.lfoShape, filterParams_.lfoRateHz) - 1.0f;
    if (controlCounter_ == 0)
        updateFilter(lfo);
    controlCounter_ = (controlCounter_ + 1) % kControlInterval;
    filter_.tick();

    const float delay = std::max(1.0f, current_ + mod);
    const float in[2] = { inL, inR };
    float wet[2] = {};
    for (int ch = 0; ch < 2; ++ch)
    {
        const float r = read(buf_[static_cast<std::size_t>(ch)], delay);
        wet[ch] = filter_.process(r, ch);
        // Soft-Clipper im Feedback-Weg: auch bei 110 % bleibt alles begrenzt.
        buf_[static_cast<std::size_t>(ch)][write_] = in[ch] + std::tanh(feedback_ * wet[ch]);
    }
    write_ = (write_ + 1) % buf_[0].size();
    wetL = wet[0];
    wetR = wet[1];
}
```

- [ ] **Step 5: FxChain verdrahten**

`engine/FxChain.cpp` – die Zeile mit `delay_.setParams(` ersetzen durch:

```cpp
    DelayFilterParams filter;
    filter.type = p.delayFilterType;
    filter.cutoffHz = p.delayFilterCutoffHz;
    filter.resonance = p.delayFilterRes;
    filter.lfoShape = p.delayLfoShape;
    filter.lfoRateHz = resolveDelayLfoRate(p, bpm);
    filter.cutDepthOct = p.delayLfoCutDepthOct;
    filter.resDepth = p.delayLfoResDepth;
    delay_.setParams(static_cast<float>(delayDivisionSeconds(p.delayDiv, bpm)), p.delayFeedback, p.delayWow, filter);
```

- [ ] **Step 6: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\Release\DubgefahrenTests.exe "[delay],[fxchain]"`
Expected: PASS. Schwellen, die knapp reißen, wie in Task 2 behandeln (Messwert ausgeben, nur bei mathematisch begründeter Abweichung anpassen, Begründung in die Commit-Nachricht).

- [ ] **Step 7: Alles testen und committen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

```bash
git add engine tests
git commit -m "feat(engine): filter and LFO in the delay feedback loop (#20)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Parameter

Vier Parameter entfallen, neun kommen hinzu; `delayTone` verschwindet endgültig aus `FxParams`.

**Files:**
- Modify: `engine/FxParams.h`, `plugin/ParameterLayout.h`, `plugin/ParameterLayout.cpp`, `plugin/ui/FxPanel.h`, `plugin/ui/FxPanel.cpp`
- Test: `tests/plugin/test_PluginProcessor.cpp`

**Interfaces:**
- Consumes: `FxParams` (Task 3), `fieldSpec(SlotField::LfoShape/LfoSyncDiv).choices`, `kNumFilterTypes`.
- Produces: die neun `pid::delay…`-Konstanten; `ParamCache::read` füllt die neuen `FxParams`-Felder; `kDelayFilterParameterVersion = 5`; Host-Parameter: 441.

- [ ] **Step 1: Failing tests schreiben**

In `tests/plugin/test_PluginProcessor.cpp`: den ersten Test auf 441 umstellen (`16 * 26 + 25`, `ids.size() == 441`, Testname „441“) und danach ergänzen:

```cpp
TEST_CASE("the delay filter parameters replace the global filter and tone", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    for (const char* gone : { "fltCutoff", "fltRes", "fltType", "dlyTone" })
        CHECK(p.state().getParameter(gone) == nullptr);
    for (const char* added : { "dlyFltType", "dlyFltCutoff", "dlyFltRes", "dlyLfoShape", "dlyLfoRate", "dlyLfoSync",
                               "dlyLfoSyncDiv", "dlyLfoCutDepth", "dlyLfoResDepth" })
        CHECK(p.state().getParameter(added) != nullptr);

    auto* cutoff = p.state().getParameter(pid::delayFltCutoff);
    CHECK_THAT(cutoff->convertFrom0to1(cutoff->getDefaultValue()), WithinAbs(2400.0, 1.0));
    auto* rate = p.state().getParameter(pid::delayLfoRate);
    CHECK_THAT(rate->convertFrom0to1(rate->getDefaultValue()), WithinAbs(0.5, 1e-3));
}

TEST_CASE("the parameter cache reads the delay filter into the engine parameters", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    setParam(p, pid::delayFltType, 3.0f);      // Notch
    setParam(p, pid::delayFltCutoff, 500.0f);
    setParam(p, pid::delayFltRes, 0.7f);
    setParam(p, pid::delayLfoShape, 2.0f);     // SawUp
    setParam(p, pid::delayLfoRate, 4.0f);
    setParam(p, pid::delayLfoSync, 1.0f);
    setParam(p, pid::delayLfoSyncDiv, 6.0f);   // 1/4
    setParam(p, pid::delayLfoCutDepth, 2.5f);
    setParam(p, pid::delayLfoResDepth, 0.4f);

    ParamCache cache(p.state());
    EngineParams ep;
    cache.read(ep);
    const FxParams& fx = ep.global.fx;
    CHECK(fx.delayFilterType == FilterType::Notch);
    CHECK_THAT(fx.delayFilterCutoffHz, WithinAbs(500.0, 1.0));
    CHECK_THAT(fx.delayFilterRes, WithinAbs(0.7, 1e-3));
    CHECK(fx.delayLfoShape == LfoShape::SawUp);
    CHECK_THAT(fx.delayLfoRateHz, WithinAbs(4.0, 1e-2));
    CHECK(fx.delayLfoSync);
    CHECK(fx.delayLfoSyncDiv == SyncDivision::D1_4);
    CHECK_THAT(fx.delayLfoCutDepthOct, WithinAbs(2.5, 1e-3));
    CHECK_THAT(fx.delayLfoResDepth, WithinAbs(0.4, 1e-3));
}

TEST_CASE("a host state from 0.5.0 still loads", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    setParam(a, pid::delayFltCutoff, 500.0f);
    setParam(a, pid::delayLfoCutDepth, 3.0f);
    setParam(a, pid::delayMix, 0.8f);
    juce::MemoryBlock mb;
    a.getStateInformation(mb);

    // Den Zustand so zurechtschneiden, wie ihn 0.5.0 schrieb: alte Filterparameter drin, neue fehlen.
    auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), static_cast<int>(mb.getSize()));
    REQUIRE(xml != nullptr);
    std::vector<juce::XmlElement*> remove;
    for (auto* e : xml->getChildIterator())
    {
        const auto id = e->getStringAttribute("id");
        if (id.startsWith("dlyFlt") || id.startsWith("dlyLfo"))
            remove.push_back(e);
    }
    for (auto* e : remove)
        xml->removeChildElement(e, true);
    for (const char* oldId : { "fltCutoff", "fltRes", "fltType", "dlyTone" })
    {
        auto* e = xml->createNewChildElement("PARAM");
        e->setAttribute("id", oldId);
        e->setAttribute("value", 0.5);
    }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml, old);

    DubgefahrenProcessor b;
    b.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    auto* cutoff = b.state().getParameter(pid::delayFltCutoff);
    auto* depth = b.state().getParameter(pid::delayLfoCutDepth);
    CHECK_THAT(cutoff->convertFrom0to1(cutoff->getValue()), WithinAbs(2400.0, 1.0));
    CHECK_THAT(depth->convertFrom0to1(depth->getValue()), WithinAbs(0.0, 1e-3));
    CHECK_THAT(b.state().getParameter(pid::delayMix)->getValue(), WithinAbs(0.8, 1e-4));

    prepare(b);
    processBlocks(b, 4);
}
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Build-FEHLER (`pid::delayFltType` … fehlen).

- [ ] **Step 3: Parameter anlegen**

`engine/FxParams.h`: die Zeile `float delayTone = 0.5f;          // 0 .. 1` löschen.

`plugin/ParameterLayout.h`:
- in `namespace dg::pid` die Zeile `delayTone` löschen und nach `delayMix` ergänzen:

```cpp
inline constexpr const char* delayFltType = "dlyFltType";
inline constexpr const char* delayFltCutoff = "dlyFltCutoff";
inline constexpr const char* delayFltRes = "dlyFltRes";
inline constexpr const char* delayLfoShape = "dlyLfoShape";
inline constexpr const char* delayLfoRate = "dlyLfoRate";
inline constexpr const char* delayLfoSync = "dlyLfoSync";
inline constexpr const char* delayLfoSyncDiv = "dlyLfoSyncDiv";
inline constexpr const char* delayLfoCutDepth = "dlyLfoCutDepth";
inline constexpr const char* delayLfoResDepth = "dlyLfoResDepth";
```

- nach `kSourceParameterVersion` ergänzen:

```cpp
// Versionshinweis für die Delay-Filter-Parameter (#20).
constexpr int kDelayFilterParameterVersion = 5;
```

- in `ParamCache` die Zeile `Ptr delayTime_, delayFeedback_, delayTone_, delayWow_, delayMix_;` ersetzen durch:

```cpp
    Ptr delayTime_, delayFeedback_, delayWow_, delayMix_;
    Ptr delayFltType_, delayFltCutoff_, delayFltRes_, delayLfoShape_, delayLfoRate_, delayLfoSync_,
        delayLfoSyncDiv_, delayLfoCutDepth_, delayLfoResDepth_;
```

`plugin/ParameterLayout.cpp`:
- die Zeile `layout.add(makeFloat(pid::delayTone, "Delay Tone" …` löschen;
- nach der Zeile mit `pid::delayMix` einfügen:

```cpp
    const auto fv = kDelayFilterParameterVersion;
    layout.add(makeChoice(pid::delayFltType, "Delay Filter Type", { "LP", "BP", "HP", "Notch" },
                          static_cast<int>(fx.delayFilterType), fv));
    layout.add(makeFloat(pid::delayFltCutoff, "Delay Filter Cutoff", 20.0f, 20000.0f, fx.delayFilterCutoffHz, 1000.0f, "Hz", fv));
    layout.add(makeFloat(pid::delayFltRes, "Delay Filter Resonance", 0.0f, 1.0f, fx.delayFilterRes, 0.0f, "", fv));
    layout.add(makeChoice(pid::delayLfoShape, "Delay LFO Shape", toStringArray(fieldSpec(SlotField::LfoShape).choices),
                          static_cast<int>(fx.delayLfoShape), fv));
    layout.add(makeFloat(pid::delayLfoRate, "Delay LFO Rate", 0.05f, 40.0f, fx.delayLfoRateHz, 2.0f, "Hz", fv));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { pid::delayLfoSync, fv }, "Delay LFO Sync",
                                                          fx.delayLfoSync));
    layout.add(makeChoice(pid::delayLfoSyncDiv, "Delay LFO Sync Rate", toStringArray(fieldSpec(SlotField::LfoSyncDiv).choices),
                          static_cast<int>(fx.delayLfoSyncDiv), fv));
    layout.add(makeFloat(pid::delayLfoCutDepth, "Delay LFO Cutoff Depth", 0.0f, 4.0f, fx.delayLfoCutDepthOct, 0.0f, "oct", fv));
    layout.add(makeFloat(pid::delayLfoResDepth, "Delay LFO Resonance Depth", 0.0f, 1.0f, fx.delayLfoResDepth, 0.0f, "", fv));
```

- im `ParamCache`-Konstruktor: `delayFeedback_(raw(apvts, pid::delayFeedback)), delayTone_(raw(apvts, pid::delayTone)),` ersetzen durch `delayFeedback_(raw(apvts, pid::delayFeedback)),` und nach `delayMix_(raw(apvts, pid::delayMix)),` einfügen:

```cpp
      delayFltType_(raw(apvts, pid::delayFltType)), delayFltCutoff_(raw(apvts, pid::delayFltCutoff)),
      delayFltRes_(raw(apvts, pid::delayFltRes)), delayLfoShape_(raw(apvts, pid::delayLfoShape)),
      delayLfoRate_(raw(apvts, pid::delayLfoRate)), delayLfoSync_(raw(apvts, pid::delayLfoSync)),
      delayLfoSyncDiv_(raw(apvts, pid::delayLfoSyncDiv)), delayLfoCutDepth_(raw(apvts, pid::delayLfoCutDepth)),
      delayLfoResDepth_(raw(apvts, pid::delayLfoResDepth)),
```

(Die Initialisierungsreihenfolge folgt der Deklaration: erst `delayTime_, delayFeedback_, delayWow_, delayMix_`, dann die neun neuen, dann `reverbDecay_ …`. Ist die Liste im Konstruktor anders sortiert, entsprechend umstellen.)

- in `ParamCache::read`: die Zeile `fx.delayTone = load(delayTone_);` löschen und nach `fx.delayMix = load(delayMix_);` einfügen:

```cpp
    fx.delayFilterType = static_cast<FilterType>(loadIndex(delayFltType_, kNumFilterTypes - 1));
    fx.delayFilterCutoffHz = load(delayFltCutoff_);
    fx.delayFilterRes = load(delayFltRes_);
    fx.delayLfoShape = static_cast<LfoShape>(loadIndex(delayLfoShape_, 4));
    fx.delayLfoRateHz = load(delayLfoRate_);
    fx.delayLfoSync = load(delayLfoSync_) > 0.5f;
    fx.delayLfoSyncDiv = static_cast<SyncDivision>(loadIndex(delayLfoSyncDiv_, 10));
    fx.delayLfoCutDepthOct = load(delayLfoCutDepth_);
    fx.delayLfoResDepth = load(delayLfoResDepth_);
```

`plugin/ui/FxPanel.h` und `plugin/ui/FxPanel.cpp`: den Delay-Tone-Regler entfernen (Member `delayTone_`, sein `attach`, seinen Eintrag in `addAndMakeVisible` und in `order[]`; `order` hat danach 9 Einträge, die Schleife läuft bis 9, die Gruppen werden `{ "DRIVE", 0, 1 }, { "DELAY", 1, 4 }, { "REVERB", 5, 3 }, { "MASTER", 8, 1 }`). Task 5 ersetzt das Panel ohnehin vollständig.

- [ ] **Step 4: Alles testen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS (Engine- und Plugin-Tests); keine Treffer mehr für `delayTone|pid::delayTone|dlyTone` außerhalb der Tests und der Spec.

- [ ] **Step 5: Commit**

```bash
git add engine plugin tests
git commit -m "feat(plugin): delay filter parameters replace the global filter and delay tone (#20)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 5: FX-Panel mit 16 Zellen

**Files:**
- Modify: `plugin/ui/FxPanel.h`, `plugin/ui/FxPanel.cpp`, `plugin/ui/Controls.cpp`
- Create: `tests/plugin/test_FxPanel.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Knob`, `Choice`, `Toggle` aus `plugin/ui/Controls.h`, die `pid::delay…`-Konstanten (Task 4), `RenderTestHelpers.h`.
- Produces: `FxPanel::showsSyncedRate() const` (true, wenn statt des Hz-Reglers die Teilung sichtbar ist); `Toggle::resized` begrenzt die Höhe auf `std::min(24, getHeight())`.

- [ ] **Step 1: Failing tests schreiben**

`tests/plugin/test_FxPanel.cpp` anlegen:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/FxPanel.h"
#include "RenderTestHelpers.h"

using namespace dg;

TEST_CASE("the FX panel fits all controls without overlap", "[fxpanel]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor proc;
    ui::FxPanel panel(proc);
    panel.setSize(976, 96);

    // Der Sync-Schalter liegt bewusst über der Beschriftungszeile der Rate-Zelle und wird beim
    // Überlappungstest ausgelassen; die 15 übrigen sichtbaren Zellen dürfen sich nicht berühren.
    const auto panelBounds = panel.getLocalBounds();
    std::vector<juce::Rectangle<int>> cells;
    for (auto* c : panel.getChildren())
    {
        if (!c->isVisible())
            continue;
        CHECK(panelBounds.contains(c->getBounds()));
        if (dynamic_cast<juce::ToggleButton*>(c) == nullptr && c->getName() != "sync")
            cells.push_back(c->getBounds());
    }
    CHECK(cells.size() == 16);
    for (std::size_t i = 0; i < cells.size(); ++i)
        for (std::size_t j = i + 1; j < cells.size(); ++j)
            CHECK_FALSE(cells[i].intersects(cells[j]));

    for (const float scale : { 0.75f, 1.0f, 2.0f })
    {
        const auto img = dgtest::snapshot(panel, scale);
        CHECK(dgtest::hasVisiblePixel(img));
        dgtest::savePng(img, "fxpanel_" + juce::String(static_cast<int>(scale * 100)));
    }
}

TEST_CASE("the rate cell shows the division when sync is on", "[fxpanel]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor proc;
    ui::FxPanel panel(proc);
    panel.setSize(976, 96);
    CHECK_FALSE(panel.showsSyncedRate());

    auto* sync = proc.state().getParameter(pid::delayLfoSync);
    sync->setValueNotifyingHost(1.0f);
    CHECK(panel.showsSyncedRate());
    sync->setValueNotifyingHost(0.0f);
    CHECK_FALSE(panel.showsSyncedRate());
}
```

`tests/CMakeLists.txt`: in der Liste der `DubgefahrenPluginTests`-Quellen `plugin/test_FxPanel.cpp` ergänzen.

Hinweis: Die Zellen sind `Knob`/`Choice` (je eine Komponente); mit 16 sichtbaren Zellen (ohne den Sync-Schalter, der ein `Toggle` ist) und der Rate-Zelle als genau einer sichtbaren Komponente (Knob **oder** Choice) ergibt das 16. Der Schalter ist ein `ui::Toggle`; der Test lässt ihn über `c->getName() == "sync"` aus (Schritt 3 setzt diesen Namen).

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure` und `build`
Expected: Build-FEHLER (`showsSyncedRate` fehlt).

- [ ] **Step 3: Panel bauen**

`plugin/ui/Controls.cpp`: `Toggle::resized` ersetzen:

```cpp
void Toggle::resized() { button.setBounds(getLocalBounds().withSizeKeepingCentre(getWidth(), std::min(24, getHeight()))); }
```

(`#include <algorithm>` ergänzen, falls es fehlt.)

`plugin/ui/FxPanel.h` ersetzen:

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/Controls.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

class FxPanel final : public juce::Component
{
public:
    explicit FxPanel(DubgefahrenProcessor& proc);
    void paint(juce::Graphics& g) override;
    void resized() override;

    // true, wenn die Rate-Zelle die Teilung statt der Frequenz zeigt (Sync an).
    bool showsSyncedRate() const { return lfoDiv_.isVisible(); }

private:
    void updateRateCell();

    Knob drive_ { u8("Drive") };
    Choice delayTime_ { u8("Time") };
    Knob delayFeedback_ { u8("Feedback") };
    Knob delayWow_ { u8("Wow") };
    Knob delayMix_ { u8("Mix") };
    Choice fltType_ { u8("Type") };
    Knob fltCutoff_ { u8("Cutoff") };
    Knob fltRes_ { u8("Reso") };
    Choice lfoShape_ { u8("Shape") };
    Knob lfoRate_ { u8("Rate") };
    Choice lfoDiv_ { u8("Rate") };
    Toggle lfoSync_ { u8("Sync") };
    Knob lfoCutDepth_ { u8("Cut Amt") };
    Knob lfoResDepth_ { u8("Res Amt") };
    Knob reverbDecay_ { u8("Decay") };
    Knob reverbTone_ { u8("Tone") };
    Knob reverbMix_ { u8("Mix") };
    Knob master_ { u8("Master") };
};

} // namespace dg::ui
```

`plugin/ui/FxPanel.cpp` ersetzen:

```cpp
#include "plugin/ui/FxPanel.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/Surfaces.h"

namespace dg::ui {

namespace {
constexpr int kCell = 60;
constexpr int kCells = 16;
constexpr int kRateCell = 9;       // Rate-Zelle: Hz-Regler oder Teilung, darüber der Sync-Schalter
constexpr int kToggleRow = 14;     // Höhe der Beschriftungszeile, in der der Sync-Schalter sitzt
struct Group { const char* title; int firstCell; int cells; };
constexpr Group kGroups[] = { { "DRIVE", 0, 1 }, { "DELAY", 1, 4 }, { "FILTER", 5, 3 },
                              { "LFO", 8, 4 },   { "REVERB", 12, 3 }, { "MASTER", 15, 1 } };
} // namespace

FxPanel::FxPanel(DubgefahrenProcessor& proc)
{
    auto& s = proc.state();
    drive_.attach(s, pid::drive);
    delayTime_.attach(s, pid::delayTime);
    delayFeedback_.attach(s, pid::delayFeedback);
    delayWow_.attach(s, pid::delayWow);
    delayMix_.attach(s, pid::delayMix);
    fltType_.attach(s, pid::delayFltType);
    fltCutoff_.attach(s, pid::delayFltCutoff);
    fltRes_.attach(s, pid::delayFltRes);
    lfoShape_.attach(s, pid::delayLfoShape);
    lfoRate_.attach(s, pid::delayLfoRate);
    lfoDiv_.attach(s, pid::delayLfoSyncDiv);
    lfoSync_.attach(s, pid::delayLfoSync);
    lfoCutDepth_.attach(s, pid::delayLfoCutDepth);
    lfoResDepth_.attach(s, pid::delayLfoResDepth);
    reverbDecay_.attach(s, pid::reverbDecay);
    reverbTone_.attach(s, pid::reverbTone);
    reverbMix_.attach(s, pid::reverbMix);
    master_.attach(s, pid::masterVol);

    // Der Sync-Schalter ersetzt die Beschriftungszeile der Rate-Zelle.
    lfoRate_.label.setVisible(false);
    lfoDiv_.label.setVisible(false);
    lfoSync_.setName("sync");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &drive_, &delayTime_, &delayFeedback_, &delayWow_, &delayMix_, &fltType_, &fltCutoff_, &fltRes_,
             &lfoShape_, &lfoRate_, &lfoDiv_, &lfoSync_, &lfoCutDepth_, &lfoResDepth_, &reverbDecay_, &reverbTone_,
             &reverbMix_, &master_ })
        addAndMakeVisible(*c);

    // Auch bei Automation: Der Schalter folgt dem Parameter, die Zelle zeigt Hz oder Teilung.
    lfoSync_.button.onStateChange = [this] { updateRateCell(); };
    updateRateCell();
}

void FxPanel::updateRateCell()
{
    const bool sync = lfoSync_.button.getToggleState();
    lfoRate_.setVisible(!sync);
    lfoDiv_.setVisible(sync);
}

void FxPanel::paint(juce::Graphics& g)
{
    drawPanelBody(g, getLocalBounds().toFloat());
    g.setFont(font(11.0f, true));
    for (const auto& grp : kGroups)
    {
        const int x = 12 + grp.firstCell * kCell;
        g.setColour(colours::textDim);
        g.drawText(grp.title, x, 4, grp.cells * kCell, 16, juce::Justification::centredLeft);
        drawDivider(g, static_cast<float>(x - 5), 6.0f, static_cast<float>(getHeight() - 6));
    }
}

void FxPanel::resized()
{
    const auto cell = [this](int i) {
        return juce::Rectangle<int>(12 + i * kCell, 20, kCell - 6, getHeight() - 24);
    };
    jassert(cell(kCells - 1).getRight() <= getWidth());

    drive_.setBounds(cell(0));
    delayTime_.setBounds(cell(1));
    delayFeedback_.setBounds(cell(2));
    delayWow_.setBounds(cell(3));
    delayMix_.setBounds(cell(4));
    fltType_.setBounds(cell(5));
    fltCutoff_.setBounds(cell(6));
    fltRes_.setBounds(cell(7));
    lfoShape_.setBounds(cell(8));
    lfoRate_.setBounds(cell(kRateCell));
    lfoDiv_.setBounds(cell(kRateCell));
    lfoSync_.setBounds(cell(kRateCell).withHeight(kToggleRow));
    lfoCutDepth_.setBounds(cell(10));
    lfoResDepth_.setBounds(cell(11));
    reverbDecay_.setBounds(cell(12));
    reverbTone_.setBounds(cell(13));
    reverbMix_.setBounds(cell(14));
    master_.setBounds(cell(15));
}

} // namespace dg::ui
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[fxpanel]"`
Expected: PASS. Die Zählung `cells.size() == 16` ist erfüllt, wenn die Rate-Zelle genau eine sichtbare Komponente beiträgt (Knob oder Choice).

- [ ] **Step 5: Sichtprüfung**

Mit gesetztem `DG_SNAPSHOT_DIR` (z. B. dem Scratchpad) die Plugin-Tests `"[fxpanel]"` erneut starten und die drei PNGs `fxpanel_75.png`, `fxpanel_100.png`, `fxpanel_200.png` mit dem Read-Werkzeug ansehen. Prüfen: Alle Beschriftungen lesbar, nichts abgeschnitten, der Sync-Schalter in der Rate-Zelle mit sichtbarem Text. Ist der Schalter-Text zu klein oder abgeschnitten, `kToggleRow` auf 16 erhöhen (die Knob-Beschriftung ist dort ausgeblendet, es gibt keinen Konflikt) und erneut prüfen.

- [ ] **Step 6: Alles testen und committen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

```bash
git add plugin tests
git commit -m "feat(ui): FX panel with delay filter and LFO controls (#20)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

---

### Task 6: CPU-Messung, Doku und Abschluss

**Files:**
- Modify: `tests/test_FxChain.cpp`, `README.md`, `docs/superpowers/specs/2026-10-01-delay-filter-design.md`

**Interfaces:**
- Consumes: alles aus Task 1 bis 5.
- Produces: ein Last-Test mit der Schwelle „10 s Audio in unter 2 s“ (nur in Release), aktualisierte Doku.

- [ ] **Step 1: Last-Test schreiben**

`tests/test_FxChain.cpp`: `#include <chrono>` ergänzen und am Dateiende:

```cpp
#ifdef NDEBUG
TEST_CASE("the FX chain with the delay filter LFO runs far faster than real time", "[fxchain][perf]")
{
    FxParams p;
    p.delayMix = 0.5f;
    p.delayFeedback = 0.8f;
    p.delayLfoShape = LfoShape::Triangle;
    p.delayLfoRateHz = 5.0f;
    p.delayLfoCutDepthOct = 3.0f;
    p.delayLfoResDepth = 0.5f;
    p.reverbMix = 0.3f;

    constexpr int kSeconds = 10;
    FxChain fx;
    fx.prepare(kSr);
    Buses b(48000 * kSeconds);
    b.ml = dgtest::noise(48000 * kSeconds, 0.3f, 1);
    b.mr = dgtest::noise(48000 * kSeconds, 0.3f, 2);
    b.sl = dgtest::noise(48000 * kSeconds, 0.3f, 3);
    b.sr = dgtest::noise(48000 * kSeconds, 0.3f, 4);

    const auto start = std::chrono::steady_clock::now();
    run(fx, b, p);
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    INFO("elapsed " << elapsed << " s for " << kSeconds << " s of audio");
    CHECK(elapsed < 2.0); // unter 20 % der Echtzeit
}
#endif
```

- [ ] **Step 2: Test laufen lassen und Messwert notieren**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build` und `build\tests\Release\DubgefahrenTests.exe "[perf]" -s`
Expected: PASS. Die gemessene Zeit (`elapsed …`, bei erfolgreichen Checks mit `-s` sichtbar) in den PR-Text übernehmen. Liegt sie über 0,5 s, vor dem Weitermachen melden: Dann reicht das Kontrollintervall von 8 Abtastwerten nicht, und die Spec wird mit der Messung angepasst.

- [ ] **Step 3: Doku anpassen**

`README.md` Zeile 4: `(drive, filter, tape delay, spring reverb)` → `(drive, tape delay with a filter and LFO in its feedback loop, spring reverb)`.

`docs/superpowers/specs/2026-10-01-delay-filter-design.md`:
- Kopfzeile `**Status:** Entwurf, Review offen` → `**Status:** Umgesetzt, Abnahme in Ableton offen`.
- Abschnitt 4: Den Satz „Die Koeffizienten werden pro Sample aus einer günstigen Näherung statt `tan()` berechnet; die CPU-Last wird gemessen (Abschnitt 8).“ ersetzen durch „Die Koeffizienten werden alle 8 Abtastwerte exakt (mit `tan()`) berechnet; Cutoff und Resonanz werden dazwischen nicht interpoliert. Die CPU-Last wird gemessen (Abschnitt 8).“
- Abschnitt 8, Stichpunkt Plugin: „CPU-Last des Delays mit LFO unter einer festgelegten Schwelle (wird im Plan konkretisiert)“ → „CPU-Last: 10 s Audio durch die FX-Kette mit allen Modulationen in unter 2 s (Release)“.
- Abschnitt 9, letzter Punkt: „Filter pro Sample zu aktualisieren kostet CPU; gemessen und notfalls auf eine Update-Rate von wenigen Samples reduziert.“ → „Die Koeffizienten werden nur alle 8 Abtastwerte neu berechnet; die CPU-Last ist gemessen (Task 6 des Plans).“

- [ ] **Step 4: Gesamtlauf mit Validator**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test` und danach `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
Expected: alle Tests grün; VST3-Validator ohne Fehler, pluginval SUCCESS.

- [ ] **Step 5: Commit**

```bash
git add tests/test_FxChain.cpp README.md docs/superpowers/specs/2026-10-01-delay-filter-design.md
git commit -m "test: CPU check for the delay filter LFO, docs for the delay filter (#20)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Abnahme-Checkliste für Ableton**

Für den PR-Text, vom Nutzer abzuhaken (Release-Build installieren, Ableton schließen, `.vst3` kopieren):
1. Standardzustand: Der Delay klingt als dunkler werdendes Echo, der Dry-Weg ist unverändert hell.
2. Typ LP, BP, HP, Notch durchschalten: Jedes Echo klingt anders; der Wechsel während laufender Echos knackt nicht.
3. Cutoff und Resonanz fahren: Die Echos folgen hörbar, keine Zipper-Geräusche.
4. LFO-Tiefe Cutoff aufdrehen, Form wechseln: Sweep ist zu hören; Square springt, Triangle gleitet, S&H zufällig.
5. Sync an: Die Rate-Zelle zeigt die Teilung, der Sweep folgt dem Tempo; Tempo ändern.
6. Resonanz 1, Feedback 1,1: kontrollierte Selbstoszillation, kein Verzerren über den Limiter hinaus.
7. Altes Projekt aus 0.5.0 laden: lädt ohne Fehlermeldung, Delay klingt etwas anders (ohne Tone).
8. FX-Panel bei verkleinertem und vergrößertem Fenster: nichts abgeschnitten.
9. Parameter-Automation für Cutoff und Typ in Ableton aufzeichnen und abspielen.
