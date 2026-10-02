# Tape-Delay-Charakter (RE-201) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `engine/TapeDelay` klingt wie ein RE-201-artiges Bandecho: kubisches Lesen, Tape-Loop (Hochpass, Bass-Bump, Höhenverlust, asymmetrische Sättigung), leises Bandrauschen und unregelmäßiges Wow/Flutter.

**Architecture:** Die bestehende Klasse `TapeDelay` wird in vier Schritten ausgebaut, ohne ihre Schnittstelle zu ändern (`prepare`, `reset`, `setParams(time, feedback, tone, wow)`, `process`). Alle Klangkonstanten stehen als benannte `constexpr`-Werte oben in `TapeDelay.cpp`, damit sie nach dem Hörtest leicht nachjustiert werden können. Es gibt keine neuen Parameter, keine UI- und keine Preset-Änderung.

**Tech Stack:** C++17, Catch2 (`DubgefahrenTests`), CMake über `build.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-02-tape-delay-character-design.md`

## Global Constraints

- Schnittstelle von `TapeDelay` bleibt: `void setParams(float timeSeconds, float feedback, float tone, float wow)`, `void process(float inL, float inR, float& wetL, float& wetR)`.
- `engine/FxChain.cpp`, `engine/FxParams.h`, UI, Kits und Presets bleiben unverändert.
- Feedback bis 1.1 muss endlich und begrenzt bleiben (Soft-Clipper im Loop).
- Bandrauschen im Loop: ca. −75 dBFS; bei Feedback 0.45 kein hörbares Dauerzischen.
- Wow/Flutter-Gesamttiefe überschreitet das heutige Maximum nicht (0.0029 s bei Wow 1).
- Zufallsgenerator: fester Startwert pro Instanz, wird bei `reset()` nicht zurückgesetzt.
- Nicht enthalten: Mehrfach-Köpfe, Benidub-Modus, Ping-Pong, Feedback-Filter aus PR #21.
- Tests laufen auf dem Branch `feature/tape-delay-character`. Build und Test: `powershell -File build.ps1 test`. Nur Delay-Tests: `powershell -File build.ps1 build`, danach `build\tests\Release\DubgefahrenTests.exe "[delay]"`.
- Commit-Messages enden mit der Zeile `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

1. Kürzeste Delay-Zeit (1 ms) bei Wow 1: Lesepunkt darf nie unter 2 Samples fallen, Ausgabe bleibt endlich (Task 4).
2. Zeitsprünge (0.1 s → 5 s → 1 ms) während der Modulation läuft: endlich, kein Absturz (Task 4).
3. Andere Abtastraten (44.1 kHz, 96 kHz): Koeffizienten skalieren richtig, Feedback 1.1 bleibt begrenzt (Task 2).
4. Extrem laute Eingabe (Pegel 100) bei Feedback 1.1: endlich, Loop läuft nicht weg (Task 2).
5. `reset()` mitten im Betrieb: danach Stille bis zur ersten Wiederholung, auch das Rauschen ist weg (Task 3).

---

### Task 1: Kubische Interpolation beim Lesen

**Files:**
- Modify: `engine/DspMath.h` (nach `onePoleCoeff`)
- Modify: `engine/TapeDelay.cpp` (`read`, Mindest-Delay in `process`)
- Test: `tests/test_DspMath.cpp`

**Interfaces:**
- Produces: `inline float dg::cubicInterp(float xm1, float x0, float x1, float x2, float t)`. Catmull-Rom zwischen `x0` und `x1`, `t` in 0..1, `xm1` liegt vor `x0`, `x2` hinter `x1`.

- [ ] **Step 1: Failing tests schreiben** (an `tests/test_DspMath.cpp` anhängen)

```cpp
TEST_CASE("cubicInterp hits its end points and is exact on a line", "[math]")
{
    CHECK_THAT(dg::cubicInterp(-1.0f, 0.3f, 0.9f, 2.0f, 0.0f), WithinAbs(0.3, 1e-6));
    CHECK_THAT(dg::cubicInterp(-1.0f, 0.3f, 0.9f, 2.0f, 1.0f), WithinAbs(0.9, 1e-6));
    CHECK_THAT(dg::cubicInterp(0.0f, 1.0f, 2.0f, 3.0f, 0.25f), WithinAbs(1.25, 1e-6));
}

TEST_CASE("cubicInterp follows a sine better than linear interpolation", "[math]")
{
    const auto s = [](int k) { return std::sin(2.0f * dg::kPi * 0.1f * static_cast<float>(k)); };
    const float truth = std::sin(2.0f * dg::kPi * 0.1f * 0.5f);
    const float cubic = dg::cubicInterp(s(-1), s(0), s(1), s(2), 0.5f);
    const float linear = 0.5f * (s(0) + s(1));
    CHECK(std::abs(cubic - truth) < 0.01f);
    CHECK(std::abs(cubic - truth) < std::abs(linear - truth));
}
```

Außerdem oben in `tests/test_DspMath.cpp` `#include <cmath>` ergänzen.

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

Run: `powershell -File build.ps1 build`
Expected: Build-Fehler, `cubicInterp` ist nicht definiert.

- [ ] **Step 3: Implementieren**

In `engine/DspMath.h` nach `onePoleCoeff` einfügen:

```cpp
// Catmull-Rom zwischen x0 und x1 (t = 0..1); xm1 liegt vor x0, x2 hinter x1.
inline float cubicInterp(float xm1, float x0, float x1, float x2, float t)
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}
```

In `engine/TapeDelay.cpp` `read` ersetzen:

```cpp
float TapeDelay::read(const std::vector<float>& buf, float delaySamples) const
{
    const std::size_t size = buf.size();
    const auto di = static_cast<std::size_t>(delaySamples);
    const float frac = delaySamples - static_cast<float>(di);
    const std::size_t i0 = (write_ + size - (di % size)) % size;      // Sample bei ganzzahliger Verzögerung
    const float xm1 = buf[(i0 + 1) % size];                           // ein Sample neuer
    const float x0 = buf[i0];
    const float x1 = buf[(i0 + size - 1) % size];                     // ein Sample älter
    const float x2 = buf[(i0 + size - 2) % size];
    return cubicInterp(xm1, x0, x1, x2, frac);
}
```

In `process` die Zeile `const float delay = std::max(1.0f, current_ + mod);` ändern zu `const float delay = std::max(2.0f, current_ + mod);` (der neueste Nachbar `xm1` darf nicht der noch nicht überschriebene Schreibplatz sein).

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -File build.ps1 test`
Expected: alle Tests grün, auch die bestehenden `[delay]`-Tests (an ganzzahligen Verzögerungen liefert die kubische Interpolation exakt das Sample).

- [ ] **Step 5: Commit**

```bash
git add engine/DspMath.h engine/TapeDelay.cpp tests/test_DspMath.cpp
git commit -m "feat(engine): cubic interpolation when reading the tape delay"
```

---

### Task 2: Tape-Loop (Hochpass, Bass-Bump, Höhenverlust, Sättigung, DC-Blocker)

**Files:**
- Modify: `engine/TapeDelay.h`
- Modify: `engine/TapeDelay.cpp`
- Test: `tests/test_TapeDelay.cpp`

**Interfaces:**
- Consumes: `dg::cubicInterp` (Task 1), `dbToGain`, `kTwoPi` aus `engine/DspMath.h`.
- Produces: in `TapeDelay` privat `struct ChannelState { float hp, bump, tone, head, dcX, dcY; }` und `std::array<ChannelState, 2> ch_`. Die Task 3 und 4 bauen darauf auf; `lp_` entfällt.

- [ ] **Step 1: Failing tests schreiben**

In `tests/test_TapeDelay.cpp` im anonymen Namespace (nach `impulseResponse`) ergänzen:

```cpp
// RMS-Verhältnis zweite zu erster Wiederholung eines 50-ms-Sinusburst bei Feedback 0.5.
float echoRatio(float freq)
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.5f, 1.0f, 0.0f);
    const auto burst = dgtest::sine(freq, kSr, 2400, 0.5f);
    std::vector<float> y(48000);
    for (int i = 0; i < 48000; ++i)
    {
        const float in = i < 2400 ? burst[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        y[static_cast<std::size_t>(i)] = l;
    }
    return dgtest::rms(y, 24200, 26200) / dgtest::rms(y, 12200, 14200);
}
} // namespace
```

(Die schließende Klammer `} // namespace` ersetzt die bisherige.) Am Dateiende anhängen:

```cpp
TEST_CASE("every repeat loses more highs than lows", "[delay]")
{
    const float low = echoRatio(200.0f);
    const float high = echoRatio(8000.0f);
    CHECK(low > 0.35f);
    CHECK(high < 0.8f * low);
}

TEST_CASE("asymmetric saturation leaves no DC in the loop", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.05f, 0.9f, 1.0f, 0.0f);
    const auto x = dgtest::sine(200.0f, kSr, 96000, 0.4f);
    std::vector<float> y(96000);
    for (int i = 0; i < 96000; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(x[static_cast<std::size_t>(i)], x[static_cast<std::size_t>(i)], l, r);
        y[static_cast<std::size_t>(i)] = l;
    }
    double sum = 0.0;
    for (std::size_t i = 48000; i < 96000; ++i)
        sum += y[i];
    CHECK(std::abs(sum / 48000.0) < 0.005);
}

TEST_CASE("loop stays bounded at 44.1 and 96 kHz", "[delay]")
{
    for (const double sr : { 44100.0, 96000.0 })
    {
        TapeDelay d;
        d.prepare(sr);
        d.setParams(0.1f, 1.1f, 1.0f, 1.0f);
        const int burstLen = static_cast<int>(sr / 2);
        const auto burst = dgtest::noise(burstLen, 0.5f);
        float peak = 0.0f;
        bool finite = true;
        for (int i = 0; i < static_cast<int>(sr * 10); ++i)
        {
            const float in = i < burstLen ? burst[static_cast<std::size_t>(i)] : 0.0f;
            float l = 0.0f, r = 0.0f;
            d.process(in, in, l, r);
            finite = finite && std::isfinite(l) && std::isfinite(r);
            peak = std::max({ peak, std::abs(l), std::abs(r) });
        }
        CHECK(finite);
        CHECK(peak < 4.0f);
    }
}

TEST_CASE("extreme input level stays finite", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.1f, 1.1f, 1.0f, 1.0f);
    const auto loud = dgtest::noise(48000, 100.0f);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 48000 * 10; ++i)
    {
        const float in = i < 48000 ? loud[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(finite);
    CHECK(peak < 200.0f);
}
```

Außerdem in dieser Datei die bestehenden Erwartungen an die neue Filterung anpassen (der Impuls geht nun durch zwei Tiefpässe und wird breiter):
- `single echo at the delay time without feedback`: `CHECK(*peakIt > 0.5f);` → `CHECK(*peakIt > 0.4f);`
- `feedback produces a quieter second echo`: `CHECK(second / first > 0.3f);` → `CHECK(second / first > 0.2f);`

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[delay]"`
Expected: `every repeat loses more highs than lows` schlägt fehl (der alte Loop hat nur den Tone-Tiefpass: Verhältnis ca. 0.87 statt unter 0.8). Die übrigen neuen Tests dürfen schon grün sein. Sie sichern die neue Struktur ab (siehe Step 5).

- [ ] **Step 3: Header anpassen**

In `engine/TapeDelay.h` `#include <cstdint>` ergänzen (wird in Task 3 gebraucht), im privaten Teil `lp_` ersetzen und Koeffizienten ergänzen. Ergebnis der betroffenen Zeilen:

```cpp
private:
    struct ChannelState
    {
        float hp = 0.0f;     // Tiefpass-Zustand für den Loop-Hochpass
        float bump = 0.0f;   // Tiefpass-Zustand für den Bass-Bump
        float tone = 0.0f;   // Tone-Tiefpass
        float head = 0.0f;   // feste Höhenabsenkung (Kopfverlust)
        float dcX = 0.0f;    // DC-Blocker
        float dcY = 0.0f;
    };

    float read(const std::vector<float>& buf, float delaySamples) const;

    double sampleRate_ = 44100.0;
    std::array<std::vector<float>, 2> buf_;
    std::size_t write_ = 0;
    float target_ = 1.0f;
    float current_ = 1.0f;
    bool snapped_ = false;
    float glideCoeff_ = 0.001f;
    float feedback_ = 0.0f;
    float wow_ = 0.0f;
    float lpCoeff_ = 1.0f;
    float hpCoeff_ = 0.0f;
    float bumpCoeff_ = 0.0f;
    float bumpGain_ = 0.0f;
    float headCoeff_ = 1.0f;
    float dcR_ = 0.999f;
    std::array<ChannelState, 2> ch_ {};
    float wowPhase1_ = 0.0f;
    float wowPhase2_ = 0.0f;
};
```

- [ ] **Step 4: Implementierung in `engine/TapeDelay.cpp`**

Oben nach den Includes und `namespace dg {` einfügen:

```cpp
namespace {
// Klangkonstanten des Tape-Loops. Hier nachjustieren, nicht im Code verteilt.
constexpr float kLoopHighpassHz = 100.0f; // verhindert Bass-Aufstau im Loop
constexpr float kBumpHz = 120.0f;         // Kopf-Bump: sanftes Low-Shelf
constexpr float kBumpDb = 2.0f;
constexpr float kHeadLossHz = 9500.0f;    // feste leichte Höhenabsenkung
constexpr float kSatBias = 0.1f;          // Asymmetrie der Sättigung (gerade Obertöne)
constexpr float kDcBlockHz = 10.0f;
const float kSatBiasOffset = std::tanh(kSatBias);

float onePoleHz(float hz, float sampleRate) { return 1.0f - std::exp(-kTwoPi * hz / sampleRate); }
} // namespace
```

`prepare` ergänzen (nach `glideCoeff_ = ...`):

```cpp
    const float sr = static_cast<float>(sampleRate);
    hpCoeff_ = onePoleHz(kLoopHighpassHz, sr);
    bumpCoeff_ = onePoleHz(kBumpHz, sr);
    bumpGain_ = dbToGain(kBumpDb) - 1.0f;
    headCoeff_ = onePoleHz(kHeadLossHz, sr);
    dcR_ = 1.0f - kTwoPi * kDcBlockHz / sr;
```

In `reset` die Zeile `lp_ = { 0.0f, 0.0f };` ersetzen durch `ch_.fill(ChannelState {});`.

In `setParams` bleibt die Berechnung von `lpCoeff_` (Tone) unverändert.

In `process` die Kanalschleife ersetzen durch:

```cpp
    const float delay = std::max(2.0f, current_ + mod);
    const float in[2] = { inL, inR };
    float wet[2] = {};
    for (std::size_t ch = 0; ch < 2; ++ch)
    {
        auto& s = ch_[ch];
        float x = read(buf_[ch], delay);
        s.hp += hpCoeff_ * (x - s.hp); // Hochpass = Eingang minus Tiefpass
        x -= s.hp;
        s.bump += bumpCoeff_ * (x - s.bump);
        x += bumpGain_ * s.bump;
        s.tone += lpCoeff_ * (x - s.tone);
        s.head += headCoeff_ * (s.tone - s.head);
        wet[ch] = s.head;

        // Weicher, leicht asymmetrischer Clipper im Feedback-Weg: auch bei 110 % bleibt alles begrenzt.
        const float sat = std::tanh(feedback_ * wet[ch] + kSatBias) - kSatBiasOffset;
        const float y = sat - s.dcX + dcR_ * s.dcY;
        s.dcX = sat;
        s.dcY = y;
        buf_[ch][write_] = in[ch] + y;
    }
```

(Die Zeile `const float delay = ...` aus Task 1 steht schon da; nicht doppelt einfügen.)

- [ ] **Step 5: Tests laufen lassen und Mutation prüfen**

Run: `powershell -File build.ps1 test`
Expected: alle Tests grün. Wenn eine Zahl nicht stimmt (z. B. `first`-Verhältnis oder Peak), nicht blind lockern: die gemessenen Werte notieren und mir melden.

Mutation: in `process` kurz `const float y = sat - s.dcX + dcR_ * s.dcY;` ersetzen durch `const float y = sat;`, bauen, `[delay]` laufen lassen. Erwartet: `asymmetric saturation leaves no DC in the loop` schlägt fehl. Danach die Änderung zurücknehmen und Tests erneut laufen lassen.

- [ ] **Step 6: Commit**

```bash
git add engine/TapeDelay.h engine/TapeDelay.cpp tests/test_TapeDelay.cpp
git commit -m "feat(engine): tape loop with head bump, head loss and asymmetric saturation"
```

---

### Task 3: Bandrauschen im Loop

**Files:**
- Modify: `engine/TapeDelay.h`
- Modify: `engine/TapeDelay.cpp`
- Test: `tests/test_TapeDelay.cpp`

**Interfaces:**
- Consumes: `ChannelState`, `ch_`, `process` aus Task 2.
- Produces: `float TapeDelay::nextNoise()` (privat) liefert gleichverteilt −1..1 aus einem xorshift-Generator `std::uint32_t rng_`. Task 4 nutzt ihn weiter.

- [ ] **Step 1: Failing tests schreiben** (an `tests/test_TapeDelay.cpp` anhängen)

```cpp
TEST_CASE("tape noise is inaudible at moderate feedback but audible when the loop rings", "[delay]")
{
    {
        TapeDelay d;
        d.prepare(kSr);
        d.setParams(0.05f, 0.45f, 0.5f, 0.0f);
        float peak = 0.0f;
        for (int i = 0; i < 48000 * 3; ++i)
        {
            float l = 0.0f, r = 0.0f;
            d.process(0.0f, 0.0f, l, r);
            peak = std::max({ peak, std::abs(l), std::abs(r) });
        }
        CHECK(peak > 1.0e-5f);  // das Rauschen ist da
        CHECK(peak < 1.0e-3f);  // aber unter -60 dBFS
    }
    {
        TapeDelay d;
        d.prepare(kSr);
        d.setParams(0.05f, 1.1f, 0.5f, 0.0f);
        float peak = 0.0f;
        bool finite = true;
        for (int i = 0; i < 48000 * 30; ++i)
        {
            float l = 0.0f, r = 0.0f;
            d.process(0.0f, 0.0f, l, r);
            finite = finite && std::isfinite(l) && std::isfinite(r);
            peak = std::max({ peak, std::abs(l), std::abs(r) });
        }
        CHECK(finite);
        CHECK(peak > 0.01f);    // Selbstoszillation aus dem Rauschen
        CHECK(peak < 4.0f);
    }
}

TEST_CASE("reset silences the loop including the noise", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.9f, 0.5f, 0.5f);
    const auto burst = dgtest::noise(48000, 0.5f);
    for (int i = 0; i < 48000; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(burst[static_cast<std::size_t>(i)], burst[static_cast<std::size_t>(i)], l, r);
    }
    d.reset();
    float peak = 0.0f;
    for (int i = 0; i < 11990; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(0.0f, 0.0f, l, r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(peak == 0.0f);
}
```

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[delay]"`
Expected: `tape noise ...` schlägt fehl (`peak > 1.0e-5f`, ohne Rauschen ist der Ausgang exakt 0). Der `reset`-Test darf schon grün sein, er sichert das neue Verhalten ab.

- [ ] **Step 3: Implementieren**

In `engine/TapeDelay.h` im privaten Teil ergänzen:

```cpp
    float nextNoise();
    ...
    std::uint32_t rng_ = 0x9E3779B9u; // fester Startwert, wird bei reset() nicht zurückgesetzt
```

In `engine/TapeDelay.cpp` zur Konstanten-Gruppe ergänzen: `constexpr float kNoiseGain = 1.78e-4f; // ca. -75 dBFS Spitze`, und die Funktion hinzufügen:

```cpp
float TapeDelay::nextNoise()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_) / 2147483648.0f - 1.0f; // -1 .. 1
}
```

In `process` die Schreibzeile ersetzen:

```cpp
        buf_[ch][write_] = in[ch] + y + kNoiseGain * nextNoise();
```

- [ ] **Step 4: Tests laufen lassen**

Run: `powershell -File build.ps1 test`
Expected: alle grün. Bei Abweichung der Schwellen Werte notieren und melden, nicht lockern.

- [ ] **Step 5: Commit**

```bash
git add engine/TapeDelay.h engine/TapeDelay.cpp tests/test_TapeDelay.cpp
git commit -m "feat(engine): tape noise in the delay loop"
```

---

### Task 4: Unregelmäßiges Wow und Flutter

**Files:**
- Modify: `engine/TapeDelay.h`
- Modify: `engine/TapeDelay.cpp`
- Test: `tests/test_TapeDelay.cpp`

**Interfaces:**
- Consumes: `nextNoise()` (Task 3), `ch_` und die Kanalschleife (Task 2).
- Produces: Die Modulation ist pro Kanal getrennt (`delay` wird in der Schleife pro Kanal berechnet). `wowPhase1_`/`wowPhase2_` entfallen.

- [ ] **Step 1: Failing tests schreiben**

Im anonymen Namespace von `tests/test_TapeDelay.cpp` ergänzen:

```cpp
// Mittlere Frequenz über interpolierte steigende Nulldurchgänge.
double meanFrequency(const std::vector<float>& y, std::size_t from, std::size_t to)
{
    double first = 0.0, last = 0.0;
    int n = 0;
    for (std::size_t i = from + 1; i < to; ++i)
    {
        if (y[i - 1] < 0.0f && y[i] >= 0.0f)
        {
            const double t = static_cast<double>(i - 1) + static_cast<double>(-y[i - 1]) / static_cast<double>(y[i] - y[i - 1]);
            if (n == 0)
                first = t;
            last = t;
            ++n;
        }
    }
    return n < 2 ? 0.0 : (n - 1) * kSr / (last - first);
}

// Pitch-Streuung (max - min) und größte Abweichung von 1 kHz über acht 0,5-s-Fenster der ersten Wiederholung.
struct PitchStats { double spread; double maxDeviation; float maxLeftRightDiff; };

PitchStats measurePitch(float wow)
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.0f, 1.0f, wow);
    const auto x = dgtest::sine(1000.0f, kSr, 240000, 0.5f);
    std::vector<float> yl(x.size()), yr(x.size());
    float diff = 0.0f;
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(x[i], x[i], l, r);
        yl[i] = l;
        yr[i] = r;
        if (i > 24000)
            diff = std::max(diff, std::abs(l - r));
    }
    double lo = 1.0e9, hi = -1.0e9, dev = 0.0;
    for (std::size_t w = 0; w < 8; ++w)
    {
        const double f = meanFrequency(yl, 24000 + w * 24000, 48000 + w * 24000);
        lo = std::min(lo, f);
        hi = std::max(hi, f);
        dev = std::max(dev, std::abs(f - 1000.0));
    }
    return { hi - lo, dev, diff };
}
```

(Platzieren vor der schließenden `} // namespace`.) Am Dateiende anhängen:

```cpp
TEST_CASE("without wow the pitch is constant and both channels agree", "[delay]")
{
    const auto s = measurePitch(0.0f);
    CHECK(s.spread < 0.05);
    CHECK(s.maxDeviation < 0.05);
    CHECK(s.maxLeftRightDiff < 1.0e-6f);
}

TEST_CASE("with full wow the pitch drifts within limits and the channels differ slightly", "[delay]")
{
    const auto s = measurePitch(1.0f);
    CHECK(s.spread > 1.0);          // hörbare Tonhöhenschwankung
    CHECK(s.maxDeviation < 20.0);   // unter 2 %
    CHECK(s.maxLeftRightDiff > 1.0e-3f);
    CHECK(s.maxLeftRightDiff < 0.5f);
}

TEST_CASE("shortest delay with full wow stays finite", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.001f, 0.9f, 1.0f, 1.0f);
    const auto x = dgtest::noise(48000 * 5, 0.5f);
    bool finite = true;
    for (float v : x)
    {
        float l = 0.0f, r = 0.0f;
        d.process(v, v, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
    }
    CHECK(finite);
}

TEST_CASE("time jumps while the tape is wobbling stay finite", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    const auto x = dgtest::noise(48000 * 4, 0.5f);
    bool finite = true;
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        if (i == 0)
            d.setParams(0.1f, 0.8f, 0.5f, 1.0f);
        else if (i == 48000)
            d.setParams(5.0f, 0.8f, 0.5f, 1.0f);
        else if (i == 48000 * 3)
            d.setParams(0.001f, 0.8f, 0.5f, 1.0f);
        float l = 0.0f, r = 0.0f;
        d.process(x[i], x[i], l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
    }
    CHECK(finite);
}
```

- [ ] **Step 2: Fehlschlag prüfen**

Run: `powershell -File build.ps1 build`, dann `build\tests\Release\DubgefahrenTests.exe "[delay]"`
Expected: `with full wow ...` schlägt fehl (`maxLeftRightDiff` ist 0, weil beide Kanäle heute identisch modulieren; die Spreizung ist mit den alten festen Sinus-Schwingungen schon über 1 Hz). Die anderen neuen Tests dürfen schon grün sein.

- [ ] **Step 3: Header anpassen**

In `engine/TapeDelay.h` `wowPhase1_` und `wowPhase2_` ersetzen durch:

```cpp
    float wowPhase_ = 0.0f;
    float flutterPhase_ = 0.0f;
    float rwSlow_ = 0.0f;       // Random Walk für das langsame Wow
    float rwJit_ = 0.0f;        // gefiltertes Rauschen für den Flutter-Jitter
    float rwSlowCoeff_ = 0.0f;
    float rwSlowGain_ = 1.0f;
    float rwJitCoeff_ = 0.0f;
    float rwJitGain_ = 1.0f;
```

- [ ] **Step 4: Implementieren**

In `engine/TapeDelay.cpp` zur Konstanten-Gruppe ergänzen (Summe der Maxima: 0.0015 + 0.0009 + 0.00036 + 0.0001 = 0.00286 s, unter dem bisherigen Maximum von 0.0029 s):

```cpp
// Modulationstiefen in Sekunden bei Wow = 1.
constexpr float kWowSinSec = 0.0015f;   // langsamer Sinus (0,55 Hz)
constexpr float kWowRandSec = 0.0009f;  // Random Walk (ca. 0,5 Hz)
constexpr float kFlutterSec = 0.0003f;  // schneller Sinus (6 bis 9 Hz)
constexpr float kJitterSec = 0.0001f;   // feiner Jitter
constexpr float kWowHz = 0.55f;
constexpr float kFlutterHz = 7.5f;
constexpr float kFlutterHzSpread = 1.5f;
constexpr float kRandWalkHz = 0.5f;
constexpr float kJitterHz = 25.0f;
constexpr float kRightOffsetRad = 0.3f; // fester Versatz des rechten Kanals

// Verstärkung, die gefiltertes Gleichverteilungs-Rauschen (Std 0,577) auf Std 0,5 bringt.
float randomWalkGain(float coeff) { return 0.5f / (0.5774f * std::sqrt(coeff / (2.0f - coeff))); }
```

`prepare` ergänzen:

```cpp
    rwSlowCoeff_ = onePoleHz(kRandWalkHz, sr);
    rwSlowGain_ = randomWalkGain(rwSlowCoeff_);
    rwJitCoeff_ = onePoleHz(kJitterHz, sr);
    rwJitGain_ = randomWalkGain(rwJitCoeff_);
```

`reset` anpassen: `wowPhase1_ = wowPhase2_ = 0.0f;` ersetzen durch `wowPhase_ = flutterPhase_ = 0.0f; rwSlow_ = rwJit_ = 0.0f;` (`rng_` bleibt unberührt).

In `process` den Block `const float mod = ...` bis einschließlich der beiden Phasen-Wraps ersetzen und die Kanalschleife auf pro-Kanal-Delay umstellen:

```cpp
void TapeDelay::process(float inL, float inR, float& wetL, float& wetR)
{
    const float sr = static_cast<float>(sampleRate_);
    current_ += (target_ - current_) * glideCoeff_; // Zeitänderung gleitet wie beim Band

    rwSlow_ += rwSlowCoeff_ * (nextNoise() - rwSlow_);
    rwJit_ += rwJitCoeff_ * (nextNoise() - rwJit_);
    const float slow = std::clamp(rwSlow_ * rwSlowGain_, -1.0f, 1.0f);
    const float jit = std::clamp(rwJit_ * rwJitGain_, -1.0f, 1.0f);
    wowPhase_ += kTwoPi * kWowHz / sr;
    flutterPhase_ += kTwoPi * (kFlutterHz + kFlutterHzSpread * slow) / sr;
    if (wowPhase_ >= kTwoPi) wowPhase_ -= kTwoPi;
    if (flutterPhase_ >= kTwoPi) flutterPhase_ -= kTwoPi;

    const float in[2] = { inL, inR };
    float wet[2] = {};
    for (std::size_t ch = 0; ch < 2; ++ch)
    {
        const float off = ch == 0 ? 0.0f : kRightOffsetRad;
        const float modSeconds = kWowSinSec * std::sin(wowPhase_ + off)
                               + kWowRandSec * slow
                               + kFlutterSec * (1.0f + 0.2f * jit) * std::sin(flutterPhase_ + 2.0f * off)
                               + kJitterSec * jit;
        const float delay = std::max(2.0f, current_ + wow_ * sr * modSeconds);

        auto& s = ch_[ch];
        float x = read(buf_[ch], delay);
        s.hp += hpCoeff_ * (x - s.hp);
        x -= s.hp;
        s.bump += bumpCoeff_ * (x - s.bump);
        x += bumpGain_ * s.bump;
        s.tone += lpCoeff_ * (x - s.tone);
        s.head += headCoeff_ * (s.tone - s.head);
        wet[ch] = s.head;

        const float sat = std::tanh(feedback_ * wet[ch] + kSatBias) - kSatBiasOffset;
        const float y = sat - s.dcX + dcR_ * s.dcY;
        s.dcX = sat;
        s.dcY = y;
        buf_[ch][write_] = in[ch] + y + kNoiseGain * nextNoise();
    }
    write_ = (write_ + 1) % buf_[0].size();
    wetL = wet[0];
    wetR = wet[1];
}
```

Damit ersetzt dieser Block die gesamte bisherige `process`-Funktion (inklusive der alten Zeilen `const float mod = ...`, `const float delay = ...` und `const float in[2] = ...`).

Hinweis zur Spec: Die Aperiodizität der Modulation (Spec-Test 5, zweiter Teil) wird nicht automatisch geprüft, weil ein stabiler Test dafür fehleranfällig wäre. Sie ergibt sich aus dem Random Walk und wird im Hörtest beurteilt.

- [ ] **Step 5: Tests laufen lassen**

Run: `powershell -File build.ps1 test`
Expected: alle grün. Bei Abweichung der Schwellen (`spread > 1.0`, `maxDeviation < 20.0`) die gemessenen Werte notieren und melden.

- [ ] **Step 6: Commit**

```bash
git add engine/TapeDelay.h engine/TapeDelay.cpp tests/test_TapeDelay.cpp
git commit -m "feat(engine): irregular wow and flutter with a small stereo offset"
```

---

### Task 5: Gesamtprüfung und Abschluss

**Files:**
- Modify: `docs/superpowers/specs/2026-10-02-tape-delay-character-design.md` (Status)

- [ ] **Step 1: Gesamte Suite und Plugin-Build**

Run: `powershell -File build.ps1 test`
Expected: alle Engine- und Plugin-Tests grün (`FxChain`-Tests mit Feedback 1.1 und Limiter-Grenze eingeschlossen).

- [ ] **Step 2: VST3-Validator**

Run: `powershell -File build.ps1 validate`
Expected: Validator läuft durch (pluginval wird übersprungen, wenn `tools\bin\pluginval.exe` fehlt).

- [ ] **Step 3: Spec-Status aktualisieren**

In der Spec die Zeile `**Status:** Spec, Review und Plan offen` ändern zu `**Status:** Umgesetzt, Abnahme in Ableton offen`.

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/specs/2026-10-02-tape-delay-character-design.md
git commit -m "docs: mark the tape delay character spec as implemented"
```

- [ ] **Step 5: Hörtest in Ableton (manuell, du)**

Plugin installieren (`powershell -File build.ps1 install`), in Ableton laden und prüfen: Wärme, Abdunkeln pro Wiederholung, Wow/Flutter bei Wow 0.2 und 1, kein Zischen bei Feedback 0.45, Selbstoszillation bei Feedback 1.1 ohne Clipping. Danach die Konstanten oben in `TapeDelay.cpp` nach Gehör nachjustieren.
