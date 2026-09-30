# Sample-Player als Klangquelle pro Slot – Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ein Slot kann statt der Sirene ein Sample (WAV/AIFF/MP3/FLAC) aus dem Ordner `<Kit>/` neben der `.dgkit`-Datei abspielen – mit Tune, Attack, Release, Gate/One Shot, Choke, Volume, Pan und FX-Send (Issue #8).

**Architecture:**
- **engine (JUCE-frei):** `SampleData` (Mono-Puffer plus Samplerate) und ein `SamplePlayer : SoundSource`. Die Engine hält pro Slot eine Sirene und einen Player und startet je nach Quellentyp den passenden.
- **plugin:** Der `SampleLoader` dekodiert im Hintergrund. Der Processor besitzt die Daten, veröffentlicht sie per Atomic-Zeiger an die Engine und gibt ersetzte Daten erst nach einem vollständigen Audioblock frei.
- **Kit-Datei und State:** Der Kit-Dateipfad und die Sample-Referenzen stehen im Host-State. Das Kit-Format steigt auf Version 3.
- **UI:** Quellen-Popup mit Sample-Untermenü und „Add File…“, Sample-Modus im Slot-Editor.

**Tech Stack:** C++20, JUCE 8.0.15 (juce_audio_formats), Catch2 v3, MSVC x64, CMake.

**Spec:** `docs/superpowers/specs/2026-09-28-sample-player-design.md`

## Global Constraints

- `engine/` darf **keine** JUCE-Header einbinden.
- Im Audio-Thread: keine Allokation, keine Locks, kein Datei-I/O, keine Freigabe von Sample-Daten.
- Quelltexte UTF-8; Nicht-ASCII-Strings an JUCE über `juce::String::fromUTF8(...)` bzw. `ui::u8(...)`. Kommentare Deutsch, UI-Texte Englisch.
- `SourceType`-Reihenfolge `Empty = 0, Synth = 1, Sample = 2` nie ändern.
- Neues Slot-Feld `tune`: −24 … +24 st, Default 0, Parameter `sNN_tune`, `ParameterID`-Versionshinweis **3**. Alle bestehenden Felder behalten 1, `sNN_source` behält 2.
- Kit-Format **Version 3**; `source` ∈ {`synth`, `empty`, `sample`}; `sample` nur ab Version 3.
- Sample-Dateinamen sind relativ zu `<Kit>/`: nicht leer, ohne `/`, `\`, `:`, ohne `..`, ohne führende/abschließende Leerzeichen.
- Sample-Endungen: `.wav`, `.aif`, `.aiff`, `.mp3`, `.flac` (ohne Groß-/Kleinschreibung). Maximal **60 s** pro Sample. Stereo und mehr Kanäle werden zu Mono gemittelt.
- Startwerte beim Wechsel auf Sample: Tune 0, Attack 0, Release 0,05 s, Mode One Shot. Choke, Volume, Pan und Send bleiben.
- Fehlertexte des Loaders: `file not found`, `unsupported or damaged file`, `longer than 60 seconds`; Processor zusätzlich `no kit file`, `invalid file name`. Problemeintrag: `Slot N: <datei> – <grund>`.
- `juce::File` nie mit relativem Pfad konstruieren (JUCE-Assertion): relative Sample-Namen immer über `folder.getChildFile(name)`.
- Befehle aus dem Repo-Root:
  - Build: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
  - Engine-Tests: `build\tests\Release\DubgefahrenTests.exe "<tag>"`
  - Plugin-Tests: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "<tag>"`
  - Alles: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
- Commits enden mit `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

- **Host stellt ein Projekt wieder her, dessen Kit-Ordner verschoben oder gelöscht wurde.** Erwartet: Alle Sample-Slots werden leer, die Namen bleiben, eine Meldung listet sie auf, und es gibt keinen Absturz. → Test in Task 7 („restoring a state whose kit folder is gone …“).
- **Sample wird getauscht oder zurückgesetzt, während der Ladevorgang noch läuft oder die Stimme klingt.** Erwartet: Das veraltete Ergebnis wird verworfen, und nichts wird freigegeben, solange der Audio-Thread es lesen kann. → Tests in Task 7 („a stale load result is ignored“, „replaced sample data is freed only after an audio block“) und Task 3 („changing the sample data stops …“).
- **Extrem kurzes Sample (1 Sample) oder Sample kürzer als die Attack-Zeit.** Erwartet: Es gibt keinen Absturz, und die Stimme endet sauber. → Test in Task 2.
- **Manipulierte Sample-Namen im State oder in der Kit-Datei** (`..\x.wav`, `C:x.wav`, `a/b.wav`). Erwartet: Sie werden abgelehnt, und es wird nie außerhalb von `<Kit>/` gelesen. → Tests in Task 4 und Task 7.
- **Datei mit mehr als zwei Kanälen** (z. B. 3 oder 6). Erwartet: Mono-Mittelwert aller Kanäle, kein Absturz. → Test in Task 6.

---

## Dateistruktur

| Datei | Verantwortung |
|---|---|
| `engine/SampleData.h` (neu) | Mono-Puffer + Samplerate |
| `engine/SamplePlayer.h/.cpp` (neu) | Sample-Stimme (Interpolation, Tune, Envelope, Ende) |
| `engine/SoundSource.h` | `VoiceContext::sample` |
| `engine/SlotParams.h`, `engine/SlotFields.h/.cpp` | `tuneSemis`, `SlotField::Tune`, `FieldSpec::versionHint`, `hasSound(Sample)` |
| `engine/Kit.h` | `Kit::samples` |
| `engine/PadRouter.h/.cpp` | `TriggerSettings::untilEnd` |
| `engine/Engine.h/.cpp` | Sirene + Player pro Slot, Datenwechsel, Sample-Trigger |
| `plugin/KitFile.h/.cpp` | Format Version 3, `isValidSampleFileName` |
| `plugin/SampleFiles.h/.cpp` (neu) | Ordner, Dateiliste, Import, Kopieren beim Export |
| `plugin/SampleLoader.h/.cpp` (neu) | Dekodieren (sync + Hintergrund) |
| `plugin/ParameterLayout.cpp` | Versionshinweis pro Feld |
| `plugin/PluginProcessor.h/.cpp` | Sample-State, Laden, Übergabe, Garbage, Probleme |
| `plugin/ui/PadGrid.h/.cpp`, `plugin/ui/SlotEditor.h/.cpp`, `plugin/PluginEditor.h/.cpp` | UI |
| `engine/CMakeLists.txt`, `plugin/CMakeLists.txt`, `tests/CMakeLists.txt` | neue Dateien |
| `tests/plugin/SampleTestHelpers.h` (neu) | Temp-Ordner, Testdateien schreiben |

---

### Task 1: Datenmodell – Tune, Versionshinweis, Kit-Samples, SampleData

**Files:**
- Create: `engine/SampleData.h`
- Modify: `engine/SlotParams.h`, `engine/SlotFields.h`, `engine/SlotFields.cpp`, `engine/Kit.h`, `engine/SoundSource.h`, `engine/CMakeLists.txt`, `plugin/ParameterLayout.cpp`
- Test: `tests/test_SlotFields.cpp`, `tests/test_Kit.cpp`, `tests/plugin/test_PluginProcessor.cpp`

**Interfaces:**
- Produces:
  - `SlotParams::tuneSemis` (float, Default 0)
  - `SlotField::Tune` (Schlüssel `"tune"`)
  - `FieldSpec::versionHint` (int, Default 1)
  - `Kit::samples` (`std::array<std::string, kNumSlots>`)
  - `struct dg::SampleData { std::vector<float> samples; double sampleRate; }`
  - `VoiceContext::sample` (`const SampleData*`, Default nullptr, als viertes Feld)

- [ ] **Step 1: Failing tests**

`tests/test_SlotFields.cpp`: den ersten Test ersetzen und einen neuen anfügen:

```cpp
TEST_CASE("there are 19 slot fields with unique keys", "[fields]")
{
    STATIC_CHECK(kNumSlotFields == 19);
    std::set<std::string> keys;
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        const auto& s = fieldSpec(f);
        REQUIRE(std::string(s.key).size() > 0);
        keys.insert(s.key);
        REQUIRE(slotFieldFromKey(s.key) == f);
        REQUIRE(s.min <= s.def);
        REQUIRE(s.def <= s.max);
        if (s.kind == FieldKind::Choice)
            REQUIRE(static_cast<int>(s.choices.size()) == static_cast<int>(s.max) + 1);
    }
    CHECK(keys.size() == 19);
    CHECK_FALSE(slotFieldFromKey("doesNotExist").has_value());
}
```

Am Ende anfügen:

```cpp
TEST_CASE("tune spans two octaves and is newer than the siren fields", "[fields]")
{
    const auto& t = fieldSpec(SlotField::Tune);
    CHECK(std::string(t.key) == "tune");
    CHECK(t.min == -24.0f);
    CHECK(t.max == 24.0f);
    CHECK(t.def == 0.0f);
    CHECK(std::string(t.unit) == "st");
    CHECK(t.versionHint == 3);
    for (int i = 0; i < kNumSlotFields; ++i)
        if (static_cast<SlotField>(i) != SlotField::Tune)
            CHECK(fieldSpec(static_cast<SlotField>(i)).versionHint == 1);

    SlotParams p;
    setSlotField(p, SlotField::Tune, 7.0f);
    CHECK(p.tuneSemis == 7.0f);
}
```

`tests/test_Kit.cpp`, am Ende:

```cpp
TEST_CASE("factory and empty kits reference no samples", "[kit]")
{
    for (const auto& k : { makeFactoryKit(), makeEmptyKit() })
        for (const auto& s : k.samples)
            CHECK(s.empty());
}
```

`tests/plugin/test_PluginProcessor.cpp`, den Test „the plugin exposes 324 uniquely named parameters“ ersetzen:

```cpp
TEST_CASE("the plugin exposes 340 uniquely named parameters", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::set<juce::String> ids;
    for (auto* param : p.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            ids.insert(withId->paramID);
    CHECK(p.getParameters().size() == 16 * 20 + 20);
    CHECK(ids.size() == 340);
    CHECK(slotParamId(0, SlotField::Wave) == "s01_wave");
    CHECK(slotParamId(15, SlotField::FxSend) == "s16_send");
    CHECK(slotParamId(0, SlotField::Tune) == "s01_tune");
    CHECK(slotSourceParamId(15) == "s16_source");
    CHECK_FALSE(p.state().getParameter("s01_source")->isAutomatable());
    CHECK(p.state().getParameter("s01_tune")->isAutomatable());
}
```

- [ ] **Step 2: Build – schlägt fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler (`Tune`, `versionHint`, `samples` unbekannt).

- [ ] **Step 3: Implementierung**

`engine/SampleData.h` (neu):

```cpp
#pragma once
#include <vector>

namespace dg {

// Dekodiertes Sample (mono). Nach dem Erzeugen unveränderlich; geteilt per
// std::shared_ptr<const SampleData>, die Engine sieht nur einen nicht besitzenden Zeiger.
struct SampleData
{
    std::vector<float> samples;
    double sampleRate = 44100.0;
};

} // namespace dg
```

`engine/SlotParams.h`, in `SlotParams` vor `SourceType source`:

```cpp
    float tuneSemis = 0.0f;        // -24 .. 24, nur für Samples
```

`engine/SlotFields.h`:
- In `enum class SlotField` zwischen `Volume, Pan, FxSend,` und `Count` die Zeile `Tune,` einfügen. Ergebnis:

```cpp
    Volume, Pan, FxSend,
    Tune,
    Count
```

- In `struct FieldSpec` nach `choices` ergänzen:

```cpp
    int versionHint = 1;  // ParameterID-Versionshinweis (Version, in der das Feld dazukam)
```

`engine/SlotFields.cpp`:
- In `kSpecs` nach der `send`-Zeile ergänzen:

```cpp
    { "tune",       "Tune",           FieldKind::Float, -24.0f, 24.0f, 0.0f, 0.0f, "st", {}, 3 },
```

- In `getSlotField` vor `case SlotField::Count:`:

```cpp
        case SlotField::Tune:          return p.tuneSemis;
```

- In `setSlotField` vor `case SlotField::Count:`:

```cpp
        case SlotField::Tune:          p.tuneSemis = value; break;
```

`engine/Kit.h`, in `struct Kit` nach `names`:

```cpp
    std::array<std::string, kNumSlots> samples {}; // UTF-8, relativ zu <Kit>/, nur bei Sample-Slots
```

`engine/SoundSource.h`: `#include "engine/SampleData.h"` ergänzen. `VoiceContext` wird zu:

```cpp
struct VoiceContext
{
    const SlotParams* params = nullptr;
    double bpm = 120.0;
    PerfOffsets perf {};
    const SampleData* sample = nullptr; // nur für SamplePlayer, nicht besitzend
};
```

`engine/CMakeLists.txt`: in der Quellliste `SoundSource.h SirenVoice.h SirenVoice.cpp` zu `SoundSource.h SampleData.h SirenVoice.h SirenVoice.cpp` ändern.

`plugin/ParameterLayout.cpp`:
- `makeFloat` und `makeChoice` bekommen einen letzten Parameter `int version = kParameterVersion` und verwenden ihn in `juce::ParameterID { id, version }`.
- In `createParameterLayout`, Slot-Schleife, die drei Fälle auf den Versionshinweis des Feldes umstellen:

```cpp
                case FieldKind::Float:
                    layout.add(makeFloat(id, name, spec.min, spec.max, def, spec.skewCentre, u8(spec.unit), spec.versionHint));
                    break;
                case FieldKind::Choice:
                    layout.add(makeChoice(id, name, toStringArray(spec.choices), static_cast<int>(std::lround(def)), spec.versionHint));
                    break;
                case FieldKind::Bool:
                    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, spec.versionHint }, name, def > 0.5f));
                    break;
```

- [ ] **Step 4: Tests**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: 100 % bestanden.

- [ ] **Step 5: Commit**

```bash
git add engine/SampleData.h engine/SlotParams.h engine/SlotFields.h engine/SlotFields.cpp engine/Kit.h engine/SoundSource.h engine/CMakeLists.txt plugin/ParameterLayout.cpp tests/test_SlotFields.cpp tests/test_Kit.cpp tests/plugin/test_PluginProcessor.cpp
git commit -m "feat(engine): add tune field, sample data and kit sample references (#8)"
```

---

### Task 2: SamplePlayer

**Files:**
- Create: `engine/SamplePlayer.h`, `engine/SamplePlayer.cpp`, `tests/test_SamplePlayer.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SampleData`, `VoiceContext::sample`, `SlotParams::tuneSemis` (Task 1)
- Produces: `class dg::SamplePlayer final : public SoundSource` (prepare/start/release/kill/isActive/isReleasing/render)

- [ ] **Step 1: Failing tests** – `tests/test_SamplePlayer.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include "engine/SamplePlayer.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

SampleData sineData(float hz, double sampleRate, double seconds)
{
    SampleData d;
    d.sampleRate = sampleRate;
    d.samples = dgtest::sine(hz, sampleRate, static_cast<int>(seconds * sampleRate), 0.5f);
    return d;
}

SlotParams sampleParams()
{
    SlotParams p;
    p.source = SourceType::Sample;
    p.attackS = 0.0f;
    p.releaseS = 0.01f;
    p.tuneSemis = 0.0f;
    return p;
}

std::vector<float> renderN(SamplePlayer& v, const VoiceContext& ctx, int n)
{
    std::vector<float> out(static_cast<std::size_t>(n), 1.0f);
    v.render(out.data(), n, ctx);
    return out;
}
} // namespace

TEST_CASE("sample player without data does not start", "[sampler]")
{
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, nullptr };
    v.start(ctx, 1);
    CHECK_FALSE(v.isActive());
    SampleData empty;
    ctx.sample = &empty;
    v.start(ctx, 1);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(renderN(v, ctx, 64)) == 0.0f);
}

TEST_CASE("sample player keeps the pitch at equal and different sample rates", "[sampler]")
{
    for (double fileRate : { 48000.0, 44100.0 })
    {
        const auto d = sineData(441.0f, fileRate, 1.0);
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = sampleParams();
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 24000);
        CHECK_THAT(dgtest::estimateFrequency(out, kSr, 2400, out.size()), WithinAbs(441.0, 4.0));
        CHECK(dgtest::peakAbs(out, 2400) > 0.45f);
    }
}

TEST_CASE("tune and performance pitch transpose the sample", "[sampler]")
{
    const auto d = sineData(400.0f, kSr, 1.0);
    const auto freqWith = [&](float tune, float perf) {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = sampleParams();
        p.tuneSemis = tune;
        PerfOffsets po;
        po.pitchSemis = perf;
        VoiceContext ctx { &p, 120.0, po, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 9600);
        return dgtest::estimateFrequency(out, kSr, 960, out.size());
    };
    CHECK_THAT(freqWith(12.0f, 0.0f), WithinAbs(800.0, 8.0));
    CHECK_THAT(freqWith(-12.0f, 0.0f), WithinAbs(200.0, 4.0));
    CHECK_THAT(freqWith(0.0f, 12.0f), WithinAbs(800.0, 8.0));
}

TEST_CASE("sample player stops at the end of the sample", "[sampler]")
{
    const auto d = sineData(441.0f, kSr, 0.1); // 4800 Samples
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 9600);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 0, 4700) > 0.4f);
    CHECK(dgtest::peakAbs(out, 4802) == 0.0f);
}

TEST_CASE("a one-sample file and an attack longer than the sample end cleanly", "[sampler]")
{
    SampleData one;
    one.sampleRate = kSr;
    one.samples = { 0.5f };
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.attackS = 1.0f;
    VoiceContext ctx { &p, 120.0, {}, &one };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 64);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::allFinite(out));
    CHECK(dgtest::peakAbs(out, 3) == 0.0f);
}

TEST_CASE("release fades the sample out", "[sampler]")
{
    const auto d = sineData(441.0f, kSr, 1.0);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 4800);
    v.release(ctx);
    CHECK(v.isReleasing());
    const auto out = renderN(v, ctx, 4800);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 600) == 0.0f);
}

TEST_CASE("losing the data while playing stops the voice", "[sampler]")
{
    const auto d = sineData(441.0f, kSr, 1.0);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 480);
    ctx.sample = nullptr;
    const auto out = renderN(v, ctx, 480);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out) == 0.0f);
}
```

`tests/CMakeLists.txt`: in `add_executable(DubgefahrenTests …)` nach `test_SirenVoice.cpp` die Zeile `test_SamplePlayer.cpp` einfügen.

- [ ] **Step 2: Build – schlägt fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler `engine/SamplePlayer.h` nicht gefunden.

- [ ] **Step 3: Implementierung**

`engine/SamplePlayer.h`:

```cpp
#pragma once
#include "engine/Envelope.h"
#include "engine/SoundSource.h"

namespace dg {

// Spielt SampleData (mono) mit Tune und Performance-Pitch ab. Die Daten kommen bei jedem
// Aufruf über VoiceContext::sample; der Player besitzt sie nicht.
class SamplePlayer final : public SoundSource
{
public:
    void prepare(double sampleRate) override;
    void start(const VoiceContext& ctx, std::uint32_t seed) override;
    void release(const VoiceContext& ctx) override;
    void kill() override;
    bool isActive() const override { return env_.isActive(); }
    bool isReleasing() const override { return env_.isReleasing(); }
    void render(float* out, int numSamples, const VoiceContext& ctx) override;

private:
    void stop();

    double sampleRate_ = 44100.0;
    Envelope env_;
    double pos_ = 0.0;
    float perfCoeff_ = 1.0f;
    float smPitch_ = 0.0f;
};

} // namespace dg
```

`engine/SamplePlayer.cpp`:

```cpp
#include "engine/SamplePlayer.h"
#include <algorithm>
#include <cstdint>
#include "engine/DspMath.h"

namespace dg {

namespace {
constexpr float kPerfSmoothingS = 0.02f;

bool usable(const SampleData* d) { return d != nullptr && !d->samples.empty() && d->sampleRate > 0.0; }

// Catmull-Rom-Interpolation; Werte außerhalb des Puffers gelten als 0.
float cubicAt(const std::vector<float>& x, double pos)
{
    const auto n = static_cast<std::int64_t>(x.size());
    const auto i = static_cast<std::int64_t>(pos);
    const float t = static_cast<float>(pos - static_cast<double>(i));
    const auto at = [&](std::int64_t k) { return (k < 0 || k >= n) ? 0.0f : x[static_cast<std::size_t>(k)]; };
    const float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
    return y1 + 0.5f * t * (y2 - y0 + t * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + t * (3.0f * (y1 - y2) + y3 - y0)));
}
} // namespace

void SamplePlayer::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    env_.prepare(sampleRate);
    perfCoeff_ = onePoleCoeff(kPerfSmoothingS, sampleRate);
    pos_ = 0.0;
}

void SamplePlayer::start(const VoiceContext& ctx, std::uint32_t)
{
    if (!usable(ctx.sample))
        return;
    pos_ = 0.0;
    smPitch_ = ctx.perf.pitchSemis;
    env_.noteOn(ctx.params->attackS);
}

void SamplePlayer::release(const VoiceContext& ctx) { env_.noteOff(ctx.params->releaseS); }

void SamplePlayer::kill() { env_.kill(); }

void SamplePlayer::stop() { env_.prepare(sampleRate_); }

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

    const double length = static_cast<double>(d->samples.size());
    const double baseStep = d->sampleRate / sampleRate_;
    for (int i = 0; i < numSamples; ++i)
    {
        if (pos_ >= length || !env_.isActive())
        {
            stop();
            std::fill(out + i, out + numSamples, 0.0f);
            return;
        }
        smPitch_ += perfCoeff_ * (ctx.perf.pitchSemis - smPitch_);
        out[i] = cubicAt(d->samples, pos_) * env_.process();
        pos_ += baseStep * semitonesToRatio(ctx.params->tuneSemis + smPitch_);
    }
}

} // namespace dg
```

`engine/CMakeLists.txt`: `SoundSource.h SampleData.h SirenVoice.h SirenVoice.cpp` zu `SoundSource.h SampleData.h SirenVoice.h SirenVoice.cpp SamplePlayer.h SamplePlayer.cpp` ändern.

- [ ] **Step 4: Tests**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build; build\tests\Release\DubgefahrenTests.exe "[sampler]"`
Expected: alle `[sampler]`-Tests PASS.

- [ ] **Step 5: Commit**

```bash
git add engine/SamplePlayer.h engine/SamplePlayer.cpp engine/CMakeLists.txt tests/test_SamplePlayer.cpp tests/CMakeLists.txt
git commit -m "feat(engine): add sample player voice (#8)"
```

---

### Task 3: Engine – Sample-Slots spielen

**Files:**
- Modify: `engine/SlotParams.h`, `engine/PadRouter.h`, `engine/PadRouter.cpp`, `engine/Engine.h`, `engine/Engine.cpp`
- Test: `tests/test_PadRouter.cpp`, `tests/test_Engine.cpp`, `tests/test_Kit.cpp`

**Interfaces:**
- Consumes: `SamplePlayer`, `SampleData` (Tasks 1–2)
- Produces:
  - `EngineParams::samples` (`std::array<const SampleData*, kNumSlots>`, nicht besitzend)
  - `TriggerSettings::untilEnd` (bool, viertes Feld)
  - `hasSound(SourceType::Sample) == true`

- [ ] **Step 1: Failing tests**

`tests/test_Kit.cpp`: den Test „only synth slots have sound until the sample player exists“ ersetzen:

```cpp
TEST_CASE("synth and sample slots have sound, empty ones do not", "[kit]")
{
    CHECK(hasSound(SourceType::Synth));
    CHECK(hasSound(SourceType::Sample));
    CHECK_FALSE(hasSound(SourceType::Empty));
}
```

`tests/test_PadRouter.cpp`, am Ende:

```cpp
TEST_CASE("a one-shot until the end starts no length timer and ignores note off", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::OneShot);
    s[0].untilEnd = true;
    r.noteOn(36, s, v);
    CHECK(v.active[0]);
    CHECK(r.samplesUntilNextExpiry() == INT_MAX);
    r.noteOff(36, v);
    CHECK_FALSE(v.releasing[0]);
}
```

`tests/test_Engine.cpp`: `#include "engine/SampleData.h"` ergänzen. Im anonymen Namespace nach `panicEvent()` ergänzen:

```cpp
SampleData testSample(double seconds, float hz = 441.0f)
{
    SampleData d;
    d.sampleRate = kSr;
    d.samples = dgtest::sine(hz, kSr, static_cast<int>(seconds * kSr), 0.5f);
    return d;
}

void makeSampleSlot(EngineParams& p, int slot, const SampleData* d, TriggerMode mode)
{
    auto& s = p.slots[static_cast<std::size_t>(slot)];
    s.source = SourceType::Sample;
    s.trigMode = mode;
    s.attackS = 0.0f;
    s.releaseS = 0.01f;
    p.samples[static_cast<std::size_t>(slot)] = d;
}
```

Am Dateiende anfügen:

```cpp
TEST_CASE("a sample slot plays its sample through the slot mix", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    const auto d = testSample(1.0);
    makeSampleSlot(p, 2, &d, TriggerMode::OneShot);
    const auto o = run(e, p, 24000, { noteOn(38) });
    CHECK(e.activeMask() == (1u << 2));
    CHECK_THAT(dgtest::estimateFrequency(o.l, kSr, 2400, o.l.size()), WithinAbs(441.0, 4.0));
}

TEST_CASE("a sample slot without data stays silent", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    makeSampleSlot(p, 2, nullptr, TriggerMode::OneShot);
    const auto o = run(e, p, 4800, { noteOn(38), { EngineEvent::Type::PreviewOn, 0, 2 } });
    CHECK(e.activeMask() == 0u);
    CHECK(dgtest::peakAbs(o.l) == 0.0f);
}

TEST_CASE("a sample one-shot plays to the end regardless of the one-shot length", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    const auto d = testSample(0.5);
    makeSampleSlot(p, 0, &d, TriggerMode::OneShot);
    p.slots[0].oneShotS = 0.05f;
    run(e, p, 14400, { noteOn(36) }); // 0,3 s
    CHECK(e.activeMask() == 1u);
    run(e, p, 4800, { noteOff(36) }); // Loslassen ändert nichts
    CHECK(e.activeMask() == 1u);
    run(e, p, 14400); // 0,7 s gesamt
    CHECK(e.activeMask() == 0u);
}

TEST_CASE("a sample gate releases on note off and latch acts like gate", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    const auto d = testSample(2.0);
    makeSampleSlot(p, 0, &d, TriggerMode::Gate);
    makeSampleSlot(p, 1, &d, TriggerMode::Latch);
    run(e, p, 4800, { noteOn(36), noteOn(37) });
    CHECK(e.activeMask() == 3u);
    CHECK(e.latchedMask() == 0u);
    run(e, p, 4800, { noteOff(36), noteOff(37) });
    CHECK(e.activeMask() == 0u);
}

TEST_CASE("changing the sample data stops a playing sample slot", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    const auto d1 = testSample(2.0);
    const auto d2 = testSample(2.0, 880.0f);
    makeSampleSlot(p, 0, &d1, TriggerMode::OneShot);
    run(e, p, 4800, { noteOn(36) });
    REQUIRE(e.activeMask() == 1u);
    p.samples[0] = &d2;
    const auto o = run(e, p, 4800);
    CHECK(e.activeMask() == 0u);
    CHECK(dgtest::peakAbs(o.l, 300) < 1.0e-4f);
}

TEST_CASE("choke works between a siren and a sample slot", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    const auto d = testSample(2.0);
    p.slots[0].chokeGroup = 1;
    makeSampleSlot(p, 1, &d, TriggerMode::OneShot);
    p.slots[1].chokeGroup = 1;
    run(e, p, 4800, { noteOn(36) });
    run(e, p, 4800, { noteOn(37) });
    CHECK(e.activeMask() == 2u);
    run(e, p, 4800, { noteOn(36) });
    CHECK(e.activeMask() == 1u);
}

TEST_CASE("switching a playing slot from synth to sample hands the slot to the sample", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    run(e, p, 4800, { noteOn(36) }); // Sirene 440 Hz, Gate
    const auto d = testSample(2.0, 880.0f);
    makeSampleSlot(p, 0, &d, TriggerMode::OneShot);
    const auto o = run(e, p, 9600, { noteOn(36) });
    CHECK(e.activeMask() == 1u);
    CHECK_THAT(dgtest::estimateFrequency(o.l, kSr, 960, o.l.size()), WithinAbs(880.0, 6.0));
}
```

- [ ] **Step 2: Build und Tests – schlagen fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler (`untilEnd`, `EngineParams::samples` unbekannt).

- [ ] **Step 3: Implementierung**

`engine/SlotParams.h`: `hasSound` ersetzen:

```cpp
// Ob ein Slot mit dieser Quelle klingen kann. Ein Sample-Slot klingt zusätzlich nur mit geladenen Daten.
constexpr bool hasSound(SourceType t) { return t != SourceType::Empty; }
```

`engine/PadRouter.h`, `struct TriggerSettings` erweitern:

```cpp
struct TriggerSettings
{
    TriggerMode mode = TriggerMode::Gate;
    float oneShotS = 1.0f;
    int chokeGroup = 0;
    bool untilEnd = false; // One-Shot ohne Längen-Timer: die Stimme endet selbst (Sample)
};
```

`engine/PadRouter.cpp`, in `PadRouter::start` die Zuweisung an `oneShotRemaining_[slot]` ersetzen:

```cpp
    oneShotRemaining_[slot] = (mode == TriggerMode::OneShot && !settings[slot].untilEnd)
        ? std::max<std::int64_t>(1, std::llround(settings[slot].oneShotS * sampleRate_))
        : -1;
```

`engine/Engine.h`:
- `#include "engine/SamplePlayer.h"` ergänzen.
- In `struct EngineParams` nach `global`:

```cpp
    // Sample-Daten pro Slot (nicht besitzend). Der Processor hält sie am Leben, solange ein
    // Block sie gelesen haben kann.
    std::array<const SampleData*, kNumSlots> samples {};
```

- Im `private`-Teil nach `std::array<SirenVoice, kNumSlots> voices_ {};`:

```cpp
    std::array<SamplePlayer, kNumSlots> samplers_ {};
    std::array<bool, kNumSlots> useSample_ {};             // zuletzt gestartete Stimmenart
    std::array<const SampleData*, kNumSlots> lastSample_ {}; // Sample-Zeiger des vorigen Blocks
```

- `std::vector<float> mainL_, mainR_, sendL_, sendR_, voiceBuf_;` zu `std::vector<float> mainL_, mainR_, sendL_, sendR_, voiceBuf_, sampleBuf_;` ändern.

`engine/Engine.cpp`:

Nach den Includes einen anonymen Namespace einfügen:

```cpp
namespace {
bool slotPlayable(const SlotParams& p, const SampleData* d)
{
    if (p.source == SourceType::Synth)
        return true;
    return p.source == SourceType::Sample && d != nullptr && !d->samples.empty();
}
} // namespace
```

Die fünf `Engine::Bank`-Methoden ersetzen:

```cpp
void Engine::Bank::startVoice(int slot)
{
    const auto s = static_cast<std::size_t>(slot);
    e_.applied_[s] = e_.params_->global.perf; // der gestartete Slot wird Fokus
    const bool sample = e_.params_->slots[s].source == SourceType::Sample;
    // Wechselt die Stimmenart, blendet die bisherige per Kill-Fade aus.
    if (sample)
        e_.voices_[s].kill();
    else
        e_.samplers_[s].kill();
    e_.useSample_[s] = sample;
    if (sample)
        e_.samplers_[s].start(e_.contextFor(slot), e_.seed_++);
    else
        e_.voices_[s].start(e_.contextFor(slot), e_.seed_++);
}

void Engine::Bank::releaseVoice(int slot)
{
    const auto s = static_cast<std::size_t>(slot);
    const auto ctx = e_.contextFor(slot);
    e_.voices_[s].release(ctx);
    e_.samplers_[s].release(ctx);
}

void Engine::Bank::killVoice(int slot)
{
    const auto s = static_cast<std::size_t>(slot);
    e_.voices_[s].kill();
    e_.samplers_[s].kill();
}

bool Engine::Bank::isVoiceActive(int slot) const
{
    const auto s = static_cast<std::size_t>(slot);
    return e_.voices_[s].isActive() || e_.samplers_[s].isActive();
}

bool Engine::Bank::isVoiceReleasing(int slot) const
{
    const auto s = static_cast<std::size_t>(slot);
    return e_.useSample_[s] ? e_.samplers_[s].isReleasing() : e_.voices_[s].isReleasing();
}
```

In `Engine::prepare`:
- nach `for (auto& v : voices_) v.prepare(sampleRate);` ergänzen:

```cpp
    for (auto& v : samplers_)
        v.prepare(sampleRate);
    useSample_.fill(false);
    lastSample_.fill(nullptr);
```

- `{ &mainL_, &mainR_, &sendL_, &sendR_, &voiceBuf_ }` zu `{ &mainL_, &mainR_, &sendL_, &sendR_, &voiceBuf_, &sampleBuf_ }` ändern.

`Engine::contextFor` ersetzen:

```cpp
VoiceContext Engine::contextFor(int slot) const
{
    const auto s = static_cast<std::size_t>(slot);
    return VoiceContext { &params_->slots[s], bpm_, applied_[s], params_->samples[s] };
}
```

In `Engine::process` die `trig_`-Schleife und die `hasSound_`-Schleife ersetzen:

```cpp
    for (std::size_t s = 0; s < trig_.size(); ++s)
    {
        const SlotParams& sp = params.slots[s];
        if (sp.source == SourceType::Sample)
        {
            // Sample: One Shot spielt bis zum Ende, Latch wirkt wie Gate.
            const bool oneShot = sp.trigMode == TriggerMode::OneShot;
            trig_[s] = { oneShot ? TriggerMode::OneShot : TriggerMode::Gate, sp.oneShotS, sp.chokeGroup, oneShot };
        }
        else
            trig_[s] = { sp.trigMode, sp.oneShotS, sp.chokeGroup, false };
    }

    // Ein Slot, der stumm wird oder dessen Sample-Daten wechseln, verstummt sofort und
    // vergisst Latch/One-Shot. Der Player liest danach nie mehr die alten Daten.
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto i = static_cast<std::size_t>(s);
        const SampleData* data = params.samples[i];
        const bool has = slotPlayable(params.slots[i], data);
        const bool dataChanged = data != lastSample_[i] && samplers_[i].isActive();
        if ((!has && hasSound_[i]) || dataChanged)
            router_.killSlot(s, bank_);
        hasSound_[i] = has;
        lastSample_[i] = data;
    }
```

In `Engine::process` die `activeMask`-Schleife ersetzen:

```cpp
    std::uint32_t active = 0;
    for (int s = 0; s < kNumSlots; ++s)
        if (bank_.isVoiceActive(s))
            active |= 1u << s;
```

In `Engine::renderSubSegment` den Schleifenanfang bis einschließlich `voice.render(...)` ersetzen:

```cpp
    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        const auto s = static_cast<std::size_t>(slot);
        SirenVoice& voice = voices_[s];
        SamplePlayer& sampler = samplers_[s];
        if (!voice.isActive() && !sampler.isActive())
        {
            applied_[s] = PerfOffsets {};
            smInit_[s] = false;
            continue;
        }
        if (g.perfTarget == PerfTarget::All || slot == focus)
            applied_[s] = g.perf;

        const VoiceContext ctx = contextFor(slot);
        voice.render(voiceBuf_.data(), len, ctx);
        if (sampler.isActive())
        {
            sampler.render(sampleBuf_.data(), len, ctx);
            for (int i = 0; i < len; ++i)
                voiceBuf_[static_cast<std::size_t>(i)] += sampleBuf_[static_cast<std::size_t>(i)];
        }
```

(Der Rest der Schleife ab `const SlotParams& sp = params_->slots[s];` bleibt unverändert.)

- [ ] **Step 4: Tests**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: 100 % bestanden. Das gilt auch für die #7-Tests („empty slots ignore notes and previews“: Sample ohne Daten bleibt stumm).

- [ ] **Step 5: Commit**

```bash
git add engine/SlotParams.h engine/PadRouter.h engine/PadRouter.cpp engine/Engine.h engine/Engine.cpp tests/test_PadRouter.cpp tests/test_Engine.cpp tests/test_Kit.cpp
git commit -m "feat(engine): play sample slots with one-shot until end and gate (#8)"
```

---

### Task 4: Kit-Format Version 3

**Files:**
- Modify: `plugin/KitFile.h`, `plugin/KitFile.cpp`, `docs/superpowers/specs/2026-09-28-sample-player-design.md`
- Test: `tests/plugin/test_KitFile.cpp`

**Interfaces:**
- Consumes: `Kit::samples`, `SourceType::Sample` (Task 1)
- Produces: `bool dg::isValidSampleFileName(const juce::String& name)` (in `plugin/KitFile.h`)

- [ ] **Step 1: Failing tests** in `tests/plugin/test_KitFile.cpp`

- Im Test „invalid kit files are rejected with a message“: `newer.getDynamicObject()->setProperty("version", 3);` → `setProperty("version", 4);`.
- Test „kits with empty slots are written as version 2 and survive a round-trip“: Titel zu `"kits with empty slots survive a round-trip"` und `CHECK(static_cast<int>(v["version"]) == 2);` zu `== 3`.
- Test „unknown or missing sound sources are rejected in version 2“: direkt nach `auto sample = parsed(makeFactoryKit());` die Zeile `sample.getDynamicObject()->setProperty("version", 2);` einfügen.

Am Ende anfügen:

```cpp
TEST_CASE("sample slots survive a version 3 round-trip", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[3].source = SourceType::Sample;
    k.slots[3].tuneSemis = 5.0f;
    k.samples[3] = juce::String::fromUTF8("Hörner.wav").toStdString();
    k.names[3] = "Horn";

    const auto v = parsed(k);
    CHECK(static_cast<int>(v["version"]) == 3);
    CHECK(v["slots"][3]["source"].toString() == "sample");
    CHECK(v["slots"][3]["sample"].toString() == juce::String::fromUTF8("Hörner.wav"));
    CHECK(static_cast<double>(v["slots"][3]["params"]["tune"]) == 5.0);
    CHECK_FALSE(v["slots"][0].getDynamicObject()->hasProperty("sample"));

    const auto r = kitFromJsonString(kitToJsonString(k));
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->slots == k.slots);
    CHECK(r.kit->samples == k.samples);
    CHECK(r.kit->names == k.names);
}

TEST_CASE("sample slots need version 3 and a plain file name", "[kitfile]")
{
    Kit k = makeFactoryKit();
    k.slots[3].source = SourceType::Sample;
    k.samples[3] = "horn.wav";

    auto v2 = parsed(k);
    v2.getDynamicObject()->setProperty("version", 2);
    const auto r2 = reparse(v2);
    CHECK_FALSE(r2.kit.has_value());
    CHECK(r2.error == juce::String("Slot 4: unknown sound source."));

    for (const char* bad : { "", "../x.wav", "a/b.wav", "a\\b.wav", "C:x.wav", " x.wav" })
    {
        auto v = parsed(k);
        v["slots"][3].getDynamicObject()->setProperty("sample", bad);
        const auto r = reparse(v);
        CHECK_FALSE(r.kit.has_value());
        CHECK(r.error == juce::String("Slot 4: invalid sample file name."));
    }

    auto missing = parsed(k);
    missing["slots"][3].getDynamicObject()->removeProperty("sample");
    CHECK(reparse(missing).error == juce::String("Slot 4: invalid sample file name."));

    CHECK(isValidSampleFileName("horn.wav"));
    CHECK(isValidSampleFileName(juce::String::fromUTF8("Hörner (2).flac")));
}
```

- [ ] **Step 2: Build und Tests – schlagen fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler `isValidSampleFileName` unbekannt.

- [ ] **Step 3: Implementierung**

`plugin/KitFile.h`, nach `loadKitFile`:

```cpp
// Dateiname eines Samples relativ zum Kit-Ordner: nicht leer, ohne Pfadtrenner, Laufwerk,
// ".." und ohne führende/abschließende Leerzeichen.
bool isValidSampleFileName(const juce::String& name);
```

`plugin/KitFile.cpp`:
- Im anonymen Namespace `kVersion` auf `3` setzen und `constexpr const char* kSourceSample = "sample";` ergänzen.
- Nach dem anonymen Namespace:

```cpp
bool isValidSampleFileName(const juce::String& name)
{
    return name.isNotEmpty() && name.trim() == name && !name.containsAnyOf("/\\:") && !name.contains("..");
}
```

- In `kitToJsonString` den Schleifenrumpf ersetzen:

```cpp
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto i = static_cast<std::size_t>(s);
        const SourceType src = kit.slots[i].source;
        auto* slot = new juce::DynamicObject();
        slot->setProperty("source", src == SourceType::Synth    ? kSourceSynth
                                    : src == SourceType::Sample ? kSourceSample
                                                                : kSourceEmpty);
        slot->setProperty("name", juce::String::fromUTF8(kit.names[i].c_str()));
        if (src == SourceType::Sample)
            slot->setProperty("sample", juce::String::fromUTF8(kit.samples[i].c_str()));
        if (src != SourceType::Empty)
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

- In `kitFromJsonString` den Block `if (version >= 2) { … }` ersetzen:

```cpp
        if (version >= 2)
        {
            const auto src = slot["source"].toString();
            if (src == kSourceEmpty)
                source = SourceType::Empty;
            else if (src == kSourceSample && version >= 3)
                source = SourceType::Sample;
            else if (src != kSourceSynth)
                return fail(slotLabel(s) + ": unknown sound source.");
        }
```

- Direkt nach dem `if (source == SourceType::Empty) { … continue; }`-Block einfügen:

```cpp
        std::string sampleName;
        if (source == SourceType::Sample)
        {
            const auto file = slot["sample"].toString();
            if (!slot["sample"].isString() || !isValidSampleFileName(file))
                return fail(slotLabel(s) + ": invalid sample file name.");
            sampleName = file.toStdString();
        }
```

- Am Ende der Schleife die beiden Zuweisungen ersetzen:

```cpp
        p.source = source;
        kit.slots[static_cast<std::size_t>(s)] = p;
        kit.names[static_cast<std::size_t>(s)] = slot["name"].toString().substring(0, 32).toStdString();
        kit.samples[static_cast<std::size_t>(s)] = sampleName;
```

Spec §3.4: den Satz „erfordert einen nicht leeren String `sample` ohne Pfadtrenner (`/`, `\`) und ohne `..`“ ersetzen durch „erfordert einen nicht leeren String `sample` ohne `/`, `\`, `:`, ohne `..` und ohne führende/abschließende Leerzeichen (`isValidSampleFileName`)“.

- [ ] **Step 4: Tests**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build; build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[kitfile]"`
Expected: alle `[kitfile]`-Tests PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/KitFile.h plugin/KitFile.cpp tests/plugin/test_KitFile.cpp docs/superpowers/specs/2026-09-28-sample-player-design.md
git commit -m "feat(plugin): store sample slots in kit format version 3 (#8)"
```

---

### Task 5: Sample-Dateien – Ordner, Liste, Import, Export-Kopie

**Files:**
- Create: `plugin/SampleFiles.h`, `plugin/SampleFiles.cpp`, `tests/plugin/SampleTestHelpers.h`, `tests/plugin/test_SampleFiles.cpp`
- Modify: `plugin/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `isValidSampleFileName` (Task 4), `Kit::samples` (Task 1)
- Produces (namespace `dg`):
  - `const juce::StringArray& sampleFileExtensions()`
  - `juce::String sampleFileWildcard()`
  - `bool isSampleFile(const juce::File&)`
  - `juce::File sampleFolderFor(const juce::File& kitFile)`
  - `juce::StringArray listSampleFiles(const juce::File& folder)`
  - `juce::File importSampleFile(const juce::File& source, const juce::File& folder, juce::String& error)`
  - `juce::StringArray copyKitSamples(const Kit& kit, const juce::File& fromFolder, const juce::File& toFolder)`
  - Test-Header: `dgtest::TempDir`, `dgtest::writeSine(juce::AudioFormat&, const juce::File&, float hz, double sampleRate, int numSamples, int numChannels = 1, float amp = 0.5f)`

- [ ] **Step 1: Failing tests**

`tests/plugin/SampleTestHelpers.h`:

```cpp
#pragma once
#include <cmath>
#include <memory>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>

namespace dgtest {

struct TempDir
{
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("dgsamples", "", false);
    TempDir() { dir.createDirectory(); }
    ~TempDir() { dir.deleteRecursively(); }
};

// Schreibt einen Sinus in Kanal 0; weitere Kanäle bleiben still.
inline void writeSine(juce::AudioFormat& format, const juce::File& file, float hz, double sampleRate, int numSamples,
                      int numChannels = 1, float amp = 0.5f)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> out = std::make_unique<juce::FileOutputStream>(file);
    auto writer = format.createWriterFor(out, juce::AudioFormatWriterOptions {}
                                                  .withSampleRate(sampleRate)
                                                  .withNumChannels(numChannels)
                                                  .withBitsPerSample(24));
    REQUIRE(writer != nullptr);
    juce::AudioBuffer<float> buf(numChannels, numSamples);
    buf.clear();
    for (int i = 0; i < numSamples; ++i)
        buf.setSample(0, i, amp * static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * hz * i / sampleRate)));
    REQUIRE(writer->writeFromAudioSampleBuffer(buf, 0, numSamples));
}

} // namespace dgtest
```

`tests/plugin/test_SampleFiles.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "engine/Kit.h"
#include "plugin/SampleFiles.h"
#include "SampleTestHelpers.h"

using namespace dg;

namespace {
void writeText(const juce::File& f, const juce::String& text) { REQUIRE(f.replaceWithText(text)); }
} // namespace

TEST_CASE("the sample folder sits next to the kit file and has its name", "[samples]")
{
    dgtest::TempDir tmp;
    const auto kit = tmp.dir.getChildFile("Dub Kit.dgkit");
    CHECK(sampleFolderFor(kit) == tmp.dir.getChildFile("Dub Kit"));
    CHECK(sampleFolderFor(juce::File()) == juce::File());
}

TEST_CASE("only audio files are listed, sorted without regard to case", "[samples]")
{
    dgtest::TempDir tmp;
    for (const char* n : { "b.mp3", "a.WAV", "notes.txt", "C.flac", "d.aif", "e.aiff" })
        writeText(tmp.dir.getChildFile(n), "x");
    CHECK(listSampleFiles(tmp.dir) == juce::StringArray { "a.WAV", "b.mp3", "C.flac", "d.aif", "e.aiff" });
    CHECK(listSampleFiles(tmp.dir.getChildFile("missing")).isEmpty());
    CHECK(listSampleFiles(juce::File()).isEmpty());
    CHECK(sampleFileWildcard() == "*.wav;*.aif;*.aiff;*.mp3;*.flac");
}

TEST_CASE("importing a file copies it into the kit folder without overwriting", "[samples]")
{
    dgtest::TempDir tmp;
    const auto src = tmp.dir.getChildFile("elsewhere").getChildFile("horn.wav");
    src.getParentDirectory().createDirectory();
    writeText(src, "one");
    const auto folder = tmp.dir.getChildFile("Dub");

    juce::String error;
    const auto first = importSampleFile(src, folder, error);
    CHECK(first == folder.getChildFile("horn.wav"));
    CHECK(first.loadFileAsString() == "one");

    CHECK(importSampleFile(src, folder, error) == first); // gleicher Inhalt: wiederverwenden

    writeText(src, "two");
    const auto second = importSampleFile(src, folder, error);
    CHECK(second == folder.getChildFile("horn (2).wav"));
    CHECK(first.loadFileAsString() == "one");

    CHECK(importSampleFile(first, folder, error) == first); // liegt schon im Ordner

    CHECK(importSampleFile(tmp.dir.getChildFile("nope.wav"), folder, error) == juce::File());
    CHECK(error.isNotEmpty());
}

TEST_CASE("exporting copies the used samples and reports missing ones", "[samples]")
{
    dgtest::TempDir tmp;
    const auto from = tmp.dir.getChildFile("Old");
    const auto to = tmp.dir.getChildFile("New");
    from.createDirectory();
    to.createDirectory();
    writeText(from.getChildFile("horn.wav"), "horn");
    writeText(to.getChildFile("horn.wav"), "stale");

    Kit k = makeFactoryKit();
    k.slots[0].source = SourceType::Sample;
    k.samples[0] = "horn.wav";
    k.slots[1].source = SourceType::Sample;
    k.samples[1] = "horn.wav"; // doppelt genutzt: nur einmal kopieren
    k.slots[2].source = SourceType::Sample;
    k.samples[2] = "gone.wav";
    k.samples[3] = "ignored.wav"; // kein Sample-Slot

    const auto problems = copyKitSamples(k, from, to);
    CHECK(to.getChildFile("horn.wav").loadFileAsString() == "horn");
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].startsWith("gone.wav"));
    CHECK_FALSE(to.getChildFile("ignored.wav").exists());

    CHECK(copyKitSamples(k, from, from).isEmpty());
}
```

`tests/CMakeLists.txt`: die `target_sources(DubgefahrenPluginTests …)`-Zeile um `plugin/test_SampleFiles.cpp` erweitern.

- [ ] **Step 2: Build – schlägt fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler `plugin/SampleFiles.h` nicht gefunden.

- [ ] **Step 3: Implementierung**

`plugin/SampleFiles.h`:

```cpp
#pragma once
#include <juce_core/juce_core.h>
#include "engine/Kit.h"

namespace dg {

// Endungen, die als Sample gelten (klein, mit Punkt).
const juce::StringArray& sampleFileExtensions();
// Muster für den Dateidialog, z. B. "*.wav;*.aif;…".
juce::String sampleFileWildcard();
bool isSampleFile(const juce::File& file);
// Ordner <Kit>/ neben <Kit>.dgkit; leeres File, wenn kitFile leer ist.
juce::File sampleFolderFor(const juce::File& kitFile);
// Dateinamen der Samples im Ordner, alphabetisch ohne Groß-/Kleinschreibung.
juce::StringArray listSampleFiles(const juce::File& folder);
// Kopiert source in folder (legt ihn an). Eine gleichnamige Datei mit gleichem Inhalt wird
// wiederverwendet, sonst entsteht "name (2).ext" usw. Liefert die Zieldatei oder ein leeres
// File und setzt error.
juce::File importSampleFile(const juce::File& source, const juce::File& folder, juce::String& error);
// Kopiert die Dateien aller Sample-Slots von fromFolder nach toFolder (überschreibt).
// Liefert eine Problemliste "datei – grund".
juce::StringArray copyKitSamples(const Kit& kit, const juce::File& fromFolder, const juce::File& toFolder);

} // namespace dg
```

`plugin/SampleFiles.cpp`:

```cpp
#include "plugin/SampleFiles.h"
#include "plugin/KitFile.h"

namespace dg {

const juce::StringArray& sampleFileExtensions()
{
    static const juce::StringArray extensions { ".wav", ".aif", ".aiff", ".mp3", ".flac" };
    return extensions;
}

juce::String sampleFileWildcard()
{
    juce::StringArray patterns;
    for (const auto& e : sampleFileExtensions())
        patterns.add("*" + e);
    return patterns.joinIntoString(";");
}

bool isSampleFile(const juce::File& file)
{
    return sampleFileExtensions().contains(file.getFileExtension().toLowerCase());
}

juce::File sampleFolderFor(const juce::File& kitFile)
{
    if (kitFile == juce::File())
        return {};
    return kitFile.getParentDirectory().getChildFile(kitFile.getFileNameWithoutExtension());
}

juce::StringArray listSampleFiles(const juce::File& folder)
{
    juce::StringArray names;
    if (folder == juce::File() || !folder.isDirectory())
        return names;
    for (const auto& f : folder.findChildFiles(juce::File::findFiles, false))
        if (isSampleFile(f))
            names.add(f.getFileName());
    names.sort(true);
    return names;
}

juce::File importSampleFile(const juce::File& source, const juce::File& folder, juce::String& error)
{
    if (!source.existsAsFile())
    {
        error = "File not found: " + source.getFullPathName();
        return {};
    }
    if (source.getParentDirectory() == folder)
        return source;
    if (!folder.createDirectory())
    {
        error = "Could not create folder: " + folder.getFullPathName();
        return {};
    }
    const auto base = source.getFileNameWithoutExtension();
    const auto ext = source.getFileExtension();
    for (int n = 1; n < 1000; ++n)
    {
        const auto target = folder.getChildFile(n == 1 ? base + ext : base + " (" + juce::String(n) + ")" + ext);
        if (target.existsAsFile())
        {
            if (target.hasIdenticalContentTo(source))
                return target;
            continue;
        }
        if (source.copyFileTo(target))
            return target;
        error = "Could not copy file to: " + target.getFullPathName();
        return {};
    }
    error = "Too many files named " + source.getFileName();
    return {};
}

juce::StringArray copyKitSamples(const Kit& kit, const juce::File& fromFolder, const juce::File& toFolder)
{
    juce::StringArray problems;
    if (fromFolder == toFolder)
        return problems;
    juce::StringArray done;
    for (std::size_t s = 0; s < kNumSlots; ++s)
    {
        if (kit.slots[s].source != SourceType::Sample)
            continue;
        const auto name = juce::String::fromUTF8(kit.samples[s].c_str());
        if (done.contains(name))
            continue;
        done.add(name);
        const bool known = isValidSampleFileName(name) && fromFolder != juce::File();
        if (!known || !fromFolder.getChildFile(name).existsAsFile())
        {
            problems.add(name + juce::String::fromUTF8(" – file not found"));
            continue;
        }
        if (!toFolder.createDirectory() || !fromFolder.getChildFile(name).copyFileTo(toFolder.getChildFile(name)))
            problems.add(name + juce::String::fromUTF8(" – could not copy"));
    }
    return problems;
}

} // namespace dg
```

`plugin/CMakeLists.txt`: in `target_sources(dg_plugin_shared …)` nach den `KitFile`-Zeilen ergänzen:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/SampleFiles.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SampleFiles.cpp
```

- [ ] **Step 4: Tests**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build; build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[samples]"`
Expected: alle `[samples]`-Tests PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/SampleFiles.h plugin/SampleFiles.cpp plugin/CMakeLists.txt tests/plugin/SampleTestHelpers.h tests/plugin/test_SampleFiles.cpp tests/CMakeLists.txt
git commit -m "feat(plugin): add sample folder listing, import and export copy (#8)"
```

---

### Task 6: SampleLoader

**Files:**
- Create: `plugin/SampleLoader.h`, `plugin/SampleLoader.cpp`, `tests/plugin/test_SampleLoader.cpp`
- Modify: `plugin/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SampleData` (Task 1), `dgtest::writeSine`, `dgtest::TempDir` (Task 5)
- Produces:
  - `constexpr double dg::kMaxSampleSeconds = 60.0`
  - `struct dg::SampleLoadResult { int slot; std::uint64_t ticket; std::shared_ptr<const SampleData> data; juce::String error; }`
  - `class dg::SampleLoader` with
    - `static std::shared_ptr<const SampleData> decode(const juce::File&, juce::String& error)`
    - `void request(int slot, std::uint64_t ticket, const juce::File& file)`
    - `std::vector<SampleLoadResult> takeResults()`
    - `void waitForAll()`

- [ ] **Step 1: Failing tests** – `tests/plugin/test_SampleLoader.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include "plugin/SampleLoader.h"
#include "SampleTestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
float peak(const SampleData& d)
{
    float p = 0.0f;
    for (float v : d.samples)
        p = std::max(p, std::abs(v));
    return p;
}
} // namespace

TEST_CASE("wav, aiff and flac files decode to mono at their sample rate", "[loader]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    juce::WavAudioFormat wav;
    juce::AiffAudioFormat aiff;
    juce::FlacAudioFormat flac;
    const std::pair<juce::AudioFormat*, const char*> formats[] = { { &wav, "a.wav" }, { &aiff, "a.aiff" }, { &flac, "a.flac" } };
    for (const auto& [format, name] : formats)
    {
        const auto file = tmp.dir.getChildFile(name);
        dgtest::writeSine(*format, file, 441.0f, 44100.0, 22050);
        juce::String error;
        const auto d = SampleLoader::decode(file, error);
        INFO(name);
        REQUIRE(d != nullptr);
        CHECK(error.isEmpty());
        CHECK(d->sampleRate == 44100.0);
        CHECK(d->samples.size() == 22050);
        CHECK_THAT(peak(*d), WithinAbs(0.5, 0.01));
    }
}

TEST_CASE("multi-channel files are mixed down to mono", "[loader]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    juce::WavAudioFormat wav;
    for (int channels : { 2, 3 })
    {
        const auto file = tmp.dir.getChildFile("multi" + juce::String(channels) + ".wav");
        dgtest::writeSine(wav, file, 441.0f, 48000.0, 4800, channels);
        juce::String error;
        const auto d = SampleLoader::decode(file, error);
        REQUIRE(d != nullptr);
        CHECK_THAT(peak(*d), WithinAbs(0.5 / channels, 0.01));
    }
}

TEST_CASE("mp3 is registered as a basic format on Windows", "[loader]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    CHECK(formats.findFormatForFileExtension(".mp3") != nullptr);
}

TEST_CASE("missing, damaged and too long files are rejected with a reason", "[loader]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    juce::String error;

    CHECK(SampleLoader::decode(tmp.dir.getChildFile("none.wav"), error) == nullptr);
    CHECK(error == "file not found");

    const auto junk = tmp.dir.getChildFile("junk.wav");
    REQUIRE(junk.replaceWithText("not audio at all"));
    CHECK(SampleLoader::decode(junk, error) == nullptr);
    CHECK(error == "unsupported or damaged file");

    juce::WavAudioFormat wav;
    const auto longFile = tmp.dir.getChildFile("long.wav");
    dgtest::writeSine(wav, longFile, 100.0f, 8000.0, 8000 * 61);
    CHECK(SampleLoader::decode(longFile, error) == nullptr);
    CHECK(error == "longer than 60 seconds");
}

TEST_CASE("requests are decoded in the background and returned with their ticket", "[loader]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    juce::WavAudioFormat wav;
    const auto file = tmp.dir.getChildFile("a.wav");
    dgtest::writeSine(wav, file, 441.0f, 48000.0, 4800);

    SampleLoader loader;
    loader.request(3, 7, file);
    loader.request(5, 9, tmp.dir.getChildFile("none.wav"));
    loader.waitForAll();
    auto results = loader.takeResults();
    REQUIRE(results.size() == 2);
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) { return a.slot < b.slot; });
    CHECK(results[0].slot == 3);
    CHECK(results[0].ticket == 7);
    CHECK(results[0].data != nullptr);
    CHECK(results[1].slot == 5);
    CHECK(results[1].data == nullptr);
    CHECK(results[1].error == "file not found");
    CHECK(loader.takeResults().empty());
}
```

`tests/CMakeLists.txt`: die `target_sources(DubgefahrenPluginTests …)`-Zeile um `plugin/test_SampleLoader.cpp` erweitern.

- [ ] **Step 2: Build – schlägt fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler `plugin/SampleLoader.h` nicht gefunden.

- [ ] **Step 3: Implementierung**

`plugin/SampleLoader.h`:

```cpp
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <juce_audio_formats/juce_audio_formats.h>
#include "engine/SampleData.h"

namespace dg {

constexpr double kMaxSampleSeconds = 60.0;

struct SampleLoadResult
{
    int slot = 0;
    std::uint64_t ticket = 0;
    std::shared_ptr<const SampleData> data; // nullptr bei Fehler
    juce::String error;
};

// Dekodiert Audiodateien zu SampleData (Mono-Mittelwert aller Kanäle) auf einem eigenen Thread.
class SampleLoader
{
public:
    SampleLoader() = default;
    ~SampleLoader();

    // Dekodiert synchron. Bei Fehler nullptr und error ("file not found",
    // "unsupported or damaged file", "longer than 60 seconds").
    static std::shared_ptr<const SampleData> decode(const juce::File& file, juce::String& error);

    void request(int slot, std::uint64_t ticket, const juce::File& file);
    std::vector<SampleLoadResult> takeResults();
    // Blockiert, bis alle Aufträge fertig sind (Tests, Aufräumen).
    void waitForAll();

private:
    juce::ThreadPool pool_ { juce::ThreadPoolOptions {}.withNumberOfThreads(1).withThreadName("Dubgefahren samples") };
    juce::CriticalSection lock_;
    std::vector<SampleLoadResult> results_;

    JUCE_DECLARE_NON_COPYABLE(SampleLoader)
};

} // namespace dg
```

`plugin/SampleLoader.cpp`:

```cpp
#include "plugin/SampleLoader.h"

namespace dg {

SampleLoader::~SampleLoader()
{
    // Laufende Aufträge greifen auf lock_ und results_ zu: vor deren Zerstörung beenden.
    pool_.removeAllJobs(true, 10000);
}

std::shared_ptr<const SampleData> SampleLoader::decode(const juce::File& file, juce::String& error)
{
    if (!file.existsAsFile())
    {
        error = "file not found";
        return nullptr;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->numChannels == 0 || reader->lengthInSamples <= 0)
    {
        error = "unsupported or damaged file";
        return nullptr;
    }
    if (static_cast<double>(reader->lengthInSamples) > kMaxSampleSeconds * reader->sampleRate)
    {
        error = "longer than 60 seconds";
        return nullptr;
    }

    const int length = static_cast<int>(reader->lengthInSamples);
    const int channels = static_cast<int>(reader->numChannels);
    juce::AudioBuffer<float> buffer(channels, length);
    if (!reader->read(buffer.getArrayOfWritePointers(), channels, 0, length))
    {
        error = "unsupported or damaged file";
        return nullptr;
    }

    auto data = std::make_shared<SampleData>();
    data->sampleRate = reader->sampleRate;
    data->samples.assign(static_cast<std::size_t>(length), 0.0f);
    const float scale = 1.0f / static_cast<float>(channels);
    for (int c = 0; c < channels; ++c)
    {
        const float* src = buffer.getReadPointer(c);
        for (int i = 0; i < length; ++i)
            data->samples[static_cast<std::size_t>(i)] += src[i] * scale;
    }
    return data;
}

void SampleLoader::request(int slot, std::uint64_t ticket, const juce::File& file)
{
    pool_.addJob([this, slot, ticket, file] {
        SampleLoadResult r;
        r.slot = slot;
        r.ticket = ticket;
        r.data = decode(file, r.error);
        const juce::ScopedLock sl(lock_);
        results_.push_back(std::move(r));
    });
}

std::vector<SampleLoadResult> SampleLoader::takeResults()
{
    const juce::ScopedLock sl(lock_);
    std::vector<SampleLoadResult> out;
    out.swap(results_);
    return out;
}

void SampleLoader::waitForAll()
{
    while (pool_.getNumJobs() > 0)
        juce::Thread::sleep(1);
}

} // namespace dg
```

`plugin/CMakeLists.txt`:
- in `target_sources(dg_plugin_shared …)` nach den `SampleFiles`-Zeilen:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/SampleLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SampleLoader.cpp
```

- in `target_link_libraries(dg_plugin_shared INTERFACE …)` nach `juce::juce_audio_utils` die Zeile `juce::juce_audio_formats` ergänzen.

- [ ] **Step 4: Tests**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build; build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[loader]"`
Expected: alle `[loader]`-Tests PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/SampleLoader.h plugin/SampleLoader.cpp plugin/CMakeLists.txt tests/plugin/test_SampleLoader.cpp tests/CMakeLists.txt
git commit -m "feat(plugin): decode samples to mono in the background (#8)"
```

---

### Task 7: Processor – Sample-State, Laden, Übergabe, Probleme

**Files:**
- Modify: `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`
- Create: `tests/plugin/test_Samples.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes:
  - `SampleLoader`, `SampleLoadResult` (Task 6)
  - `sampleFolderFor` (Task 5)
  - `isValidSampleFileName` (Task 4)
  - `EngineParams::samples` (Task 3)
  - `Kit::samples`, `SlotParams::tuneSemis` (Task 1)
- Produces (`DubgefahrenProcessor`):
  - `void setSlot(int slot, const SlotParams&, const juce::String& name, const juce::String& sample = {})`
  - `void applyKit(const Kit&, const juce::File& kitFile = {})`
  - `juce::File kitFile() const`
  - `void setKitFile(const juce::File&)`
  - `juce::File sampleFolder() const`
  - `juce::String slotSample(int) const`
  - `void setSlotSample(int slot, const juce::String& fileName)`
  - `bool isSampleLoaded(int) const`
  - `juce::StringArray takeSampleProblems()`
  - `void waitForSampleLoads()`
  - `std::size_t pendingSampleGarbage() const`

- [ ] **Step 1: Failing tests** – `tests/plugin/test_Samples.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "engine/Kit.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "SampleTestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
struct SampleKit
{
    dgtest::TempDir tmp;
    juce::File kitFile = tmp.dir.getChildFile("Dub.dgkit");
    juce::File folder = tmp.dir.getChildFile("Dub");
    SampleKit()
    {
        folder.createDirectory();
        juce::WavAudioFormat wav;
        dgtest::writeSine(wav, folder.getChildFile("horn.wav"), 441.0f, 48000.0, 48000);
    }
};

void prepare(DubgefahrenProcessor& p)
{
    p.setPlayConfigDetails(0, 2, 48000.0, 512);
    p.prepareToPlay(48000.0, 512);
}

float processNote(DubgefahrenProcessor& p, int note)
{
    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)), 0);
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    p.processBlock(buf, on);
    return buf.getMagnitude(0, 0, 512);
}
} // namespace

TEST_CASE("choosing a sample sets sample defaults, loads it and plays it", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    CHECK(p.kitFile() == kit.kitFile);
    CHECK(p.sampleFolder() == kit.folder);

    p.setSlotSample(3, "horn.wav");
    CHECK(p.slotSource(3) == SourceType::Sample);
    CHECK(p.slotSample(3) == "horn.wav");
    CHECK(p.slotName(3) == "horn");
    const auto sp = readSlotFromParameters(p.state(), 3);
    CHECK(sp.tuneSemis == 0.0f);
    CHECK(sp.attackS == 0.0f);
    CHECK_THAT(sp.releaseS, WithinAbs(0.05, 1e-4));
    CHECK(sp.trigMode == TriggerMode::OneShot);
    CHECK_THAT(sp.volumeDb, WithinAbs(makeFactoryKit().slots[3].volumeDb, 1e-3)); // bleibt

    p.waitForSampleLoads();
    CHECK(p.isSampleLoaded(3));
    prepare(p);
    CHECK(processNote(p, 39) > 0.0f);
    CHECK((p.activeMask() & (1u << 3)) != 0u);
}

TEST_CASE("a missing sample empties the slot, keeps the name and reports it once", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(2, "missing.wav");
    p.waitForSampleLoads();
    CHECK(p.slotSource(2) == SourceType::Empty);
    CHECK(p.slotName(2) == "missing");
    CHECK(p.slotSample(2).isEmpty());
    const auto problems = p.takeSampleProblems();
    REQUIRE(problems.size() == 1);
    CHECK(problems[0] == juce::String::fromUTF8("Slot 3: missing.wav – file not found"));
    CHECK(p.takeSampleProblems().isEmpty());
}

TEST_CASE("without a kit file or with an unsafe name a sample slot fails", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.setSlotSample(0, "horn.wav"); // Factory-Zustand: keine Kit-Datei
    p.waitForSampleLoads();
    CHECK(p.slotSource(0) == SourceType::Empty);

    p.applyKit(makeFactoryKit(), kit.kitFile);
    SlotParams sp = readSlotFromParameters(p.state(), 1);
    sp.source = SourceType::Sample;
    p.setSlot(1, sp, "evil", "..\\Dub\\horn.wav");
    p.waitForSampleLoads();
    CHECK(p.slotSource(1) == SourceType::Empty);

    const auto problems = p.takeSampleProblems();
    REQUIRE(problems.size() == 2);
    CHECK(problems[0].endsWith("no kit file"));
    CHECK(problems[1].endsWith("invalid file name"));
}

TEST_CASE("state round-trip keeps the kit file and sample slots", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor a;
    a.applyKit(makeFactoryKit(), kit.kitFile);
    a.setSlotSample(4, "horn.wav");
    a.waitForSampleLoads();
    juce::MemoryBlock mb;
    a.getStateInformation(mb);

    DubgefahrenProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    b.waitForSampleLoads();
    CHECK(b.kitFile() == kit.kitFile);
    CHECK(b.slotSource(4) == SourceType::Sample);
    CHECK(b.slotSample(4) == "horn.wav");
    CHECK(b.isSampleLoaded(4));
    CHECK(b.takeSampleProblems().isEmpty());
}

TEST_CASE("restoring a state whose kit folder is gone empties the sample slots and reports them", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::MemoryBlock mb;
    {
        SampleKit kit;
        DubgefahrenProcessor a;
        a.applyKit(makeFactoryKit(), kit.kitFile);
        a.setSlotSample(4, "horn.wav");
        a.setSlotSample(6, "horn.wav");
        a.waitForSampleLoads();
        a.getStateInformation(mb);
    } // Kit-Ordner gelöscht

    DubgefahrenProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    b.waitForSampleLoads();
    CHECK(b.slotSource(4) == SourceType::Empty);
    CHECK(b.slotSource(6) == SourceType::Empty);
    CHECK(b.slotName(4) == "horn");
    CHECK(b.takeSampleProblems().size() == 2);
}

TEST_CASE("loading the factory kit clears the kit file and sample slots", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(4, "horn.wav");
    p.waitForSampleLoads();
    p.applyKit(makeFactoryKit(), {});
    p.waitForSampleLoads();
    CHECK(p.kitFile() == juce::File());
    CHECK(p.slotSource(4) == SourceType::Synth);
    CHECK(p.slotSample(4).isEmpty());
    CHECK_FALSE(p.isSampleLoaded(4));
}

TEST_CASE("currentKit and applyKit carry sample references", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor a;
    a.applyKit(makeFactoryKit(), kit.kitFile);
    a.setSlotSample(4, "horn.wav");
    const Kit k = a.currentKit();
    CHECK(k.samples[4] == "horn.wav");
    CHECK(k.samples[0].empty());

    DubgefahrenProcessor b;
    b.applyKit(k, kit.kitFile);
    b.waitForSampleLoads();
    CHECK(b.isSampleLoaded(4));
}

TEST_CASE("replaced sample data is freed only after an audio block", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    prepare(p);
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(4, "horn.wav");
    p.waitForSampleLoads();
    REQUIRE(p.isSampleLoaded(4));

    p.setSlotSample(4, "horn.wav"); // neu laden: alte Daten in die Garbage
    p.waitForSampleLoads();
    CHECK(p.pendingSampleGarbage() == 1);
    processNote(p, 0);
    p.waitForSampleLoads(); // räumt auf
    CHECK(p.pendingSampleGarbage() == 0);

    DubgefahrenProcessor idle; // ohne prepareToPlay: sofort frei
    idle.applyKit(makeFactoryKit(), kit.kitFile);
    idle.setSlotSample(4, "horn.wav");
    idle.waitForSampleLoads();
    idle.clearSlot(4);
    idle.waitForSampleLoads();
    CHECK(idle.pendingSampleGarbage() == 0);
}

TEST_CASE("a stale load result is ignored", "[plugin][samples]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SampleKit kit;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kit.kitFile);
    p.setSlotSample(4, "horn.wav");
    p.resetSlotToFactory(4); // bevor das Ergebnis übernommen wurde
    p.waitForSampleLoads();
    CHECK(p.slotSource(4) == SourceType::Synth);
    CHECK_FALSE(p.isSampleLoaded(4));
    CHECK(p.takeSampleProblems().isEmpty());
}
```

`tests/CMakeLists.txt`: die `target_sources(DubgefahrenPluginTests …)`-Zeile um `plugin/test_Samples.cpp` erweitern.

- [ ] **Step 2: Build – schlägt fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler (`kitFile`, `setSlotSample` … unbekannt).

- [ ] **Step 3a: `plugin/PluginProcessor.h`**

- Includes ergänzen: `#include <memory>`, `#include <utility>`, `#include "engine/SampleData.h"`, `#include "plugin/SampleLoader.h"`.
- Klassenkopf: `class DubgefahrenProcessor final : public juce::AudioProcessor, private juce::Timer`.
- `~DubgefahrenProcessor() override = default;` durch `~DubgefahrenProcessor() override;` ersetzen.
- `void releaseResources() override {}` durch `void releaseResources() override;` ersetzen.
- Die Deklarationen von `setSlot` und `applyKit` ersetzen durch:

```cpp
    void setSlot(int slot, const SlotParams& params, const juce::String& name, const juce::String& sample = {});
    // Übernimmt ein Kit samt zugehöriger Kit-Datei (leer = keine Kit-Datei, z. B. Factory-Kit).
    void applyKit(const Kit& kit, const juce::File& kitFile = {});
```

- Nach `resetSlotToFactory` ergänzen:

```cpp
    juce::File kitFile() const;
    void setKitFile(const juce::File& file); // nach einem Export
    juce::File sampleFolder() const;
    juce::String slotSample(int slot) const;
    // Wählt ein Sample aus dem Kit-Ordner (Quelle Sample, Name = Dateiname ohne Endung).
    void setSlotSample(int slot, const juce::String& fileName);
    bool isSampleLoaded(int slot) const;
    // Liefert die gesammelten Ladeprobleme ("Slot N: datei – grund") und leert die Liste.
    juce::StringArray takeSampleProblems();
    // Wartet auf alle Ladeaufträge und übernimmt die Ergebnisse (Tests).
    void waitForSampleLoads();
    std::size_t pendingSampleGarbage() const { return garbage_.size(); }
```

- Im `private`-Teil nach `juce::ValueTree namesTree() const;` ergänzen:

```cpp
    void timerCallback() override;
    void handleSampleResults();
    void requestSampleLoad(int slot);
    void releaseSample(int slot);
    void sampleLoadFailed(int slot, const juce::String& name, const juce::String& reason);
    void setSlotSampleRef(int slot, const juce::String& name);
    void collectGarbage();
```

- Nach `juce::AudioProcessLoadMeasurer loadMeasurer_;` ergänzen:

```cpp
    SampleLoader loader_;
    std::array<std::shared_ptr<const SampleData>, kNumSlots> sampleData_ {};
    std::array<std::atomic<const SampleData*>, kNumSlots> samplePtrs_ {};
    std::array<std::uint64_t, kNumSlots> sampleTickets_ {};
    // Ersetzte Daten mit dem Blockzähler beim Tausch; frei, sobald danach ein Block fertig ist.
    std::vector<std::pair<std::shared_ptr<const SampleData>, std::uint64_t>> garbage_;
    std::atomic<std::uint64_t> blocksProcessed_ { 0 };
    std::atomic<bool> audioActive_ { false };
    juce::StringArray sampleProblems_;
```

- [ ] **Step 3b: `plugin/PluginProcessor.cpp`**

Include ergänzen: `#include "plugin/KitFile.h"` und `#include "plugin/SampleFiles.h"`.

Im anonymen Namespace ergänzen:

```cpp
const juce::Identifier kSamplesId { "SLOTSAMPLES" };
const juce::Identifier kKitFileId { "kitFile" };

juce::Identifier sampleKey(int slot) { return juce::Identifier("s" + juce::String(slot + 1)); }
```

Konstruktor: am Ende `startTimerHz(20);` ergänzen. Danach:

```cpp
DubgefahrenProcessor::~DubgefahrenProcessor() { stopTimer(); }
```

`ensureStateChildren`: am Anfang `apvts_.state.getOrCreateChildWithName(kSamplesId, nullptr);` ergänzen.

`prepareToPlay`: am Ende `audioActive_.store(true);`. Danach neu:

```cpp
void DubgefahrenProcessor::releaseResources() { audioActive_.store(false); }
```

`processBlock`:
- direkt nach `cache_.read(engineParams_);`:

```cpp
    for (int s = 0; s < kNumSlots; ++s)
        engineParams_.samples[static_cast<std::size_t>(s)] =
            samplePtrs_[static_cast<std::size_t>(s)].load(std::memory_order_acquire);
```

- nach `engine_.process(...)`:

```cpp
    blocksProcessed_.fetch_add(1, std::memory_order_release);
```

`setStateInformation`: nach `ensureStateChildren();` einfügen:

```cpp
    for (int s = 0; s < kNumSlots; ++s)
    {
        if (slotSource(s) == SourceType::Sample)
            requestSampleLoad(s);
        else
            releaseSample(s);
    }
```

`setSlot` ersetzen:

```cpp
void DubgefahrenProcessor::setSlot(int slot, const SlotParams& params, const juce::String& name, const juce::String& sample)
{
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
```

`currentKit`: in der Schleife ergänzen:

```cpp
        if (k.slots[static_cast<std::size_t>(s)].source == SourceType::Sample)
            k.samples[static_cast<std::size_t>(s)] = slotSample(s).toStdString();
```

`applyKit` ersetzen:

```cpp
void DubgefahrenProcessor::applyKit(const Kit& kit, const juce::File& kitFile)
{
    apvts_.state.setProperty(kKitFileId, kitFile.getFullPathName(), nullptr);
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto i = static_cast<std::size_t>(s);
        setSlot(s, kit.slots[i], juce::String::fromUTF8(kit.names[i].c_str()), juce::String::fromUTF8(kit.samples[i].c_str()));
    }
    ++stateGeneration_;
}
```

Neue Methoden (nach `applyKit`):

```cpp
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
    SlotParams p = readSlotFromParameters(apvts_, slot);
    if (p.source != SourceType::Sample)
    {
        // Startwerte beim Wechsel auf Sample; Choke, Volume, Pan und Send bleiben.
        p.source = SourceType::Sample;
        p.tuneSemis = 0.0f;
        p.attackS = 0.0f;
        p.releaseS = 0.05f;
        p.trigMode = TriggerMode::OneShot;
    }
    const auto name = fileName.containsChar('.') ? fileName.upToLastOccurrenceOf(".", false, false) : fileName;
    setSlot(slot, p, name, fileName);
}

bool DubgefahrenProcessor::isSampleLoaded(int slot) const
{
    return samplePtrs_[static_cast<std::size_t>(slot)].load() != nullptr;
}

juce::StringArray DubgefahrenProcessor::takeSampleProblems()
{
    auto problems = sampleProblems_;
    sampleProblems_.clear();
    return problems;
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
    loader_.request(slot, ticket, folder.getChildFile(name));
}

void DubgefahrenProcessor::releaseSample(int slot)
{
    const auto s = static_cast<std::size_t>(slot);
    ++sampleTickets_[s]; // offene Ladeaufträge für diesen Slot werden ungültig
    if (sampleData_[s] == nullptr)
        return;
    samplePtrs_[s].store(nullptr, std::memory_order_release);
    garbage_.emplace_back(std::move(sampleData_[s]), blocksProcessed_.load(std::memory_order_acquire));
    sampleData_[s].reset();
}

void DubgefahrenProcessor::sampleLoadFailed(int slot, const juce::String& name, const juce::String& reason)
{
    SlotParams p = readSlotFromParameters(apvts_, slot);
    p.source = SourceType::Empty;
    writeSlotToParameters(apvts_, slot, p);
    setSlotSampleRef(slot, {});
    releaseSample(slot);
    sampleProblems_.add("Slot " + juce::String(slot + 1) + ": " + (name.isEmpty() ? juce::String("(no file)") : name)
                        + juce::String::fromUTF8(" – ") + reason);
    ++stateGeneration_;
}

void DubgefahrenProcessor::handleSampleResults()
{
    for (auto& r : loader_.takeResults())
    {
        const auto s = static_cast<std::size_t>(r.slot);
        if (r.ticket != sampleTickets_[s])
            continue; // veraltet: Slot wurde inzwischen geändert
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
    // Ein Block, der beim Tausch lief, kann den alten Zeiger noch lesen. Ist der Zähler seitdem
    // weitergezählt, ist dieser Block fertig; spätere Blöcke sehen nur noch den neuen Zeiger.
    const bool active = audioActive_.load();
    const auto done = blocksProcessed_.load(std::memory_order_acquire);
    std::erase_if(garbage_, [&](const auto& g) { return !active || done > g.second; });
}

void DubgefahrenProcessor::timerCallback() { handleSampleResults(); }

void DubgefahrenProcessor::waitForSampleLoads()
{
    loader_.waitForAll();
    handleSampleResults();
}
```

- [ ] **Step 4: Tests**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: 100 % bestanden, einschließlich aller bisherigen Processor- und Editor-Tests.

- [ ] **Step 5: Commit**

```bash
git add plugin/PluginProcessor.h plugin/PluginProcessor.cpp tests/plugin/test_Samples.cpp tests/CMakeLists.txt
git commit -m "feat(plugin): load, hand over and persist sample slots (#8)"
```

---

### Task 8: Benutzeroberfläche

**Files:**
- Modify: `plugin/ui/PadGrid.h`, `plugin/ui/PadGrid.cpp`, `plugin/ui/SlotEditor.h`, `plugin/ui/SlotEditor.cpp`, `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`
- Test: `tests/plugin/test_Editor.cpp`

**Interfaces:**
- Consumes:
  - Processor-API (Task 7)
  - `listSampleFiles`, `sampleFileWildcard`, `importSampleFile`, `copyKitSamples`, `sampleFolderFor` (Task 5)
- Produces:
  - `PadGrid::isSample(int) const`
  - `SlotEditor::onChooseSample`, `showsSampleControls()`, `sampleButtonText()`, `isLatchSelectable()`, `sampleButton()`
  - `DubgefahrenEditor::buildSourceMenu(const juce::StringArray&) const`
  - `DubgefahrenEditor::buildSampleMenu(const juce::StringArray&)` (static)
  - `DubgefahrenEditor::exportKitTo(const juce::File&)`
  - Test accessors on `DubgefahrenEditor`: `padShowsSample`, `slotEditorShowsSampleControls`, `slotEditorSampleText`, `slotEditorLatchSelectable`, `lastMessage`

- [ ] **Step 1: Failing tests** in `tests/plugin/test_Editor.cpp`

`#include "plugin/SampleFiles.h"`, `#include "engine/Kit.h"` und `#include "SampleTestHelpers.h"` ergänzen.

Den Test „source menu offers synth and a disabled sample entry“ ersetzen und neue Tests anfügen:

```cpp
TEST_CASE("without a kit file the source menu offers synth and a disabled sample hint", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    CHECK(menuItems(e->buildSourceMenu({})) == Items { { "Synth", true }, { "Sample (export the kit first)", false } });
}

TEST_CASE("with a kit file the source menu has a sample submenu with the folder's files", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), tmp.dir.getChildFile("Dub.dgkit"));
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    CHECK(menuItems(e->buildSourceMenu({ "a.wav" })) == Items { { "Synth", true }, { "Sample", true } });
    const auto addFile = juce::String::fromUTF8("Add File…");
    CHECK(menuItems(DubgefahrenEditor::buildSampleMenu({ "a.wav", "b.mp3" }))
          == Items { { "a.wav", true }, { "b.mp3", true }, { addFile, true } });
    CHECK(menuItems(DubgefahrenEditor::buildSampleMenu({}))
          == Items { { "(no samples in kit folder)", false }, { addFile, true } });
}

TEST_CASE("a sample slot shows the sample controls, its file and no latch", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    const auto kitFile = tmp.dir.getChildFile("Dub.dgkit");
    sampleFolderFor(kitFile).createDirectory();
    juce::WavAudioFormat wav;
    dgtest::writeSine(wav, sampleFolderFor(kitFile).getChildFile("horn.wav"), 441.0f, 48000.0, 4800);

    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), kitFile);
    p.setSlotSample(2, "horn.wav");
    p.waitForSampleLoads();
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    e->selectSlot(2);
    CHECK(e->slotEditorShowsSampleControls());
    CHECK(e->slotEditorSampleText() == "horn.wav");
    CHECK_FALSE(e->slotEditorLatchSelectable());
    CHECK_FALSE(e->slotEditorShowsEmptyHint());
    CHECK(e->padShowsSample(2));
    CHECK_FALSE(e->padShowsSample(0));

    e->selectSlot(0);
    CHECK_FALSE(e->slotEditorShowsSampleControls());
    CHECK(e->slotEditorLatchSelectable());
}

TEST_CASE("sample load problems are shown once as a message", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), tmp.dir.getChildFile("Dub.dgkit"));
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    p.setSlotSample(1, "missing.wav");
    p.waitForSampleLoads();
    e->pollProcessorState();
    CHECK(e->lastMessage().contains("Some samples could not be loaded"));
    CHECK(e->lastMessage().contains("missing.wav"));
    CHECK(p.takeSampleProblems().isEmpty());
}

TEST_CASE("exporting to a new kit copies the samples and switches the kit file", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    dgtest::TempDir tmp;
    const auto oldKit = tmp.dir.getChildFile("Old.dgkit");
    sampleFolderFor(oldKit).createDirectory();
    juce::WavAudioFormat wav;
    dgtest::writeSine(wav, sampleFolderFor(oldKit).getChildFile("horn.wav"), 441.0f, 48000.0, 4800);

    DubgefahrenProcessor p;
    p.applyKit(makeFactoryKit(), oldKit);
    p.setSlotSample(2, "horn.wav");
    p.waitForSampleLoads();
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = static_cast<DubgefahrenEditor*>(editor.get());

    const auto newKit = tmp.dir.getChildFile("New.dgkit");
    CHECK(e->exportKitTo(newKit));
    CHECK(newKit.existsAsFile());
    CHECK(sampleFolderFor(newKit).getChildFile("horn.wav").existsAsFile());
    CHECK(p.kitFile() == newKit);
    CHECK(p.isSampleLoaded(2));
}
```

- [ ] **Step 2: Build – schlägt fehl**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: Compile-Fehler (`buildSampleMenu`, `exportKitTo` … unbekannt).

- [ ] **Step 3a: `plugin/ui/PadGrid.h/.cpp`**

`PadGrid.h`: nach `bool isEmpty(int slot) const;` ergänzen:

```cpp
    bool isSample(int slot) const;
```

`PadGrid.cpp`:
- `Pad::setContent` ersetzen:

```cpp
    void setContent(const juce::String& n, bool empty, bool sample)
    {
        name_ = n;
        empty_ = empty;
        sample_ = sample;
        repaint();
    }

    bool isEmpty() const { return empty_; }
    bool isSample() const { return sample_; }
```

- In `paint` nach dem `drawFittedText`-Aufruf:

```cpp
        if (sample_ && !empty_)
        {
            g.setColour(colours::textDim);
            g.setFont(juce::FontOptions(13.0f));
            g.drawText(u8("∿"), juce::Rectangle<float>(r.getRight() - 34.0f, r.getY() + 3.0f, 16.0f, 14.0f),
                       juce::Justification::centred);
        }
```

- Member ergänzen: `bool sample_ = false;` (zu den anderen bools).
- `refreshNames` ersetzen und `isSample` ergänzen:

```cpp
void PadGrid::refreshNames()
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto source = proc_.slotSource(s);
        pads_[static_cast<std::size_t>(s)]->setContent(proc_.slotName(s), !hasSound(source), source == SourceType::Sample);
    }
}

bool PadGrid::isSample(int slot) const { return pads_[static_cast<std::size_t>(slot)]->isSample(); }
```

- [ ] **Step 3b: `plugin/ui/SlotEditor.h`**

Im `public`-Teil nach `std::function<void()> onRename;`:

```cpp
    std::function<void()> onChooseSample;
    bool showsSampleControls() const { return sampleButton_.isVisible(); }
    juce::String sampleButtonText() const { return sampleButton_.getButtonText(); }
    bool isLatchSelectable() const { return trigMode_.box.isItemEnabled(kLatchItemId); }
    juce::Component& sampleButton() { return sampleButton_; }
```

Im `private`-Teil:

```cpp
    enum class Mode { Empty, Synth, Sample };
    static constexpr int kLatchItemId = 2; // ComboBox-IDs 1..3 = Gate, Latch, One-Shot
    Mode mode_ = Mode::Synth;
    Knob tune_ { u8("Tune") };
    juce::TextButton sampleButton_;
    std::vector<juce::Component*> sampleControls_;
```

- [ ] **Step 3c: `plugin/ui/SlotEditor.cpp`**

Im anonymen Namespace ergänzen:

```cpp
constexpr int kNumSampleRows = 3;
const char* const kSampleRowNames[kNumSampleRows] = { "SAMPLE\nAMP", "TRIG", "MIX" };
```

Konstruktor, nach `addChildComponent(emptyHint_);`:

```cpp
    sampleControls_ = { &tune_, &attack_, &release_, &trigMode_, &choke_, &vol_, &pan_, &send_ };
    addChildComponent(tune_);
    sampleButton_.onClick = [this] {
        if (onChooseSample)
            onChooseSample();
    };
    addChildComponent(sampleButton_);
```

`setSlot`: nach `send_.attach(...)` ergänzen:

```cpp
    tune_.attach(s, slotParamId(slot, SlotField::Tune));
```

`refresh` ersetzen:

```cpp
void SlotEditor::refresh()
{
    const auto source = proc_.slotSource(slot_);
    mode_ = source == SourceType::Empty ? Mode::Empty : source == SourceType::Sample ? Mode::Sample : Mode::Synth;
    const bool empty = mode_ == Mode::Empty;
    const auto title = "Slot " + juce::String(slot_ + 1);
    header_.setText(empty ? title : title + u8(" · ") + proc_.slotName(slot_), juce::dontSendNotification);
    emptyHint_.setVisible(empty);
    renameButton_.setVisible(!empty);

    for (auto* c : controls_)
        c->setVisible(mode_ == Mode::Synth);
    tune_.setVisible(false);
    if (mode_ == Mode::Sample)
        for (auto* c : sampleControls_)
            c->setVisible(true);
    sampleButton_.setVisible(mode_ == Mode::Sample);
    sampleButton_.setButtonText(proc_.slotSample(slot_));
    trigMode_.box.setItemEnabled(kLatchItemId, mode_ != Mode::Sample); // Latch wirkt bei Samples wie Gate
    resized();
    repaint(); // Zeilenbeschriftungen
}
```

`paint`: die `for`-Schleife über die Zeilen ersetzen:

```cpp
    const bool sample = mode_ == Mode::Sample;
    const int rows = sample ? kNumSampleRows : kNumRows;
    for (int row = 0; row < rows; ++row)
        g.drawFittedText(sample ? kSampleRowNames[row] : kRowNames[row], 12, kTopOffset + row * kRowHeight,
                         kRowLabelWidth - 12, kRowHeight, juce::Justification::centredLeft, 2);
```

`resized` ersetzen:

```cpp
void SlotEditor::resized()
{
    auto top = getLocalBounds().reduced(12, 8).removeFromTop(32);
    renameButton_.setBounds(top.removeFromRight(110));
    top.removeFromRight(8);
    sampleButton_.setBounds(top.removeFromRight(180));
    header_.setBounds(top);

    const auto place = [this](int row, int col, juce::Component& c) {
        c.setBounds(kRowLabelWidth + col * kCellWidth, kTopOffset + row * kRowHeight, kCellWidth - 8, kRowHeight - 6);
    };
    if (mode_ == Mode::Sample)
    {
        place(0, 0, tune_);     place(0, 1, attack_); place(0, 2, release_);
        place(1, 0, trigMode_); place(1, 1, choke_);
        place(2, 0, vol_);      place(2, 1, pan_);    place(2, 2, send_);
    }
    else
    {
        place(0, 0, wave_);     place(0, 1, pitch_);     place(0, 2, pw_);       place(0, 3, attack_);     place(0, 4, release_);
        place(1, 0, lfoShape_); place(1, 1, lfoRate_);   place(1, 2, lfoSync_);  place(1, 3, lfoSyncDiv_); place(1, 4, lfoDepth_);
        place(2, 0, sweepAmt_); place(2, 1, sweepTime_); place(2, 2, trigMode_); place(2, 3, oneShot_);    place(2, 4, choke_);
        place(3, 0, vol_);      place(3, 1, pan_);       place(3, 2, send_);
    }

    emptyHint_.setBounds(getLocalBounds().withTrimmedTop(kTopOffset).reduced(24));
}
```

- [ ] **Step 3d: `plugin/PluginEditor.h`**

- `std::optional<std::pair<SlotParams, juce::String>> clipboard_;` ersetzen durch:

```cpp
    struct ClipboardSlot
    {
        SlotParams params;
        juce::String name;
        juce::String sample;
    };
    std::optional<ClipboardSlot> clipboard_;
    juce::String lastMessage_;
```

- Im `public`-Teil:
  - `static juce::PopupMenu buildSourceMenu();` ersetzen durch:

```cpp
    juce::PopupMenu buildSourceMenu(const juce::StringArray& sampleFiles) const;
    static juce::PopupMenu buildSampleMenu(const juce::StringArray& sampleFiles);
    // Schreibt das aktuelle Kit nach file, kopiert bei neuem Kit die Samples mit.
    bool exportKitTo(const juce::File& file);
```

  - Nach `bool padShowsEmpty(int slot) const …` ergänzen:

```cpp
    bool padShowsSample(int slot) const { return pads_.isSample(slot); }
    bool slotEditorShowsSampleControls() const { return slotEditor_.showsSampleControls(); }
    juce::String slotEditorSampleText() const { return slotEditor_.sampleButtonText(); }
    bool slotEditorLatchSelectable() const { return slotEditor_.isLatchSelectable(); }
    juce::String lastMessage() const { return lastMessage_; }
```

- Im `private`-Teil nach `void showSourceMenu(int slot);`:

```cpp
    void showSampleMenu(int slot);
    void chooseSource(int slot, int result, const juce::StringArray& files);
    void addSampleFile(int slot);
```

- [ ] **Step 3e: `plugin/PluginEditor.cpp`**

- Include ergänzen: `#include "plugin/SampleFiles.h"`.
- `enum SourceMenuId …` ersetzen:

```cpp
enum SourceMenuId { kSourceSynth = 1, kSourceSampleDisabled, kSourceNoSamples, kSourceAddFile, kSourceSampleBase = 100 };
```

- Konstruktor, nach `slotEditor_.onRename = …`:

```cpp
    slotEditor_.onChooseSample = [this] { showSampleMenu(selectedSlot_); };
```

- `pollProcessorState`: am Ende der Funktion ergänzen:

```cpp
    const auto problems = proc_.takeSampleProblems();
    if (!problems.isEmpty())
        showMessage("Some samples could not be loaded", problems.joinIntoString("\n"));
```

- `showMessage` ersetzen:

```cpp
void DubgefahrenEditor::showMessage(const juce::String& title, const juce::String& text)
{
    lastMessage_ = title + "\n" + text;
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title, text);
}
```

- In `showKitMenu`: `applyKit(makeFactoryKit())` → `applyKit(makeFactoryKit(), {})` und `applyKit(makeEmptyKit())` → `applyKit(makeEmptyKit(), {})`.
- In `loadKit`: `proc_.applyKit(*result.kit);` → `proc_.applyKit(*result.kit, file);`.
- In `exportKit` den Rumpf des `launchAsync`-Callbacks nach `const auto file = …` ersetzen, sodass er so aussieht:

```cpp
                              const auto file = fc.getResult().withFileExtension(kKitExtension);
                              safe->exportKitTo(file);
```

- Neue Methode nach `exportKit`:

```cpp
bool DubgefahrenEditor::exportKitTo(const juce::File& file)
{
    const Kit kit = proc_.currentKit();
    juce::StringArray problems;
    if (file != proc_.kitFile())
        problems = copyKitSamples(kit, proc_.sampleFolder(), sampleFolderFor(file));
    juce::String error;
    if (!saveKitFile(kit, file, error))
    {
        showMessage("Could not save kit", error);
        return false;
    }
    proc_.setKitFile(file);
    if (!problems.isEmpty())
        showMessage("Some samples could not be copied", problems.joinIntoString("\n"));
    return true;
}
```

- In `showPadMenu`, Copy- und Paste-Fall ersetzen:

```cpp
                                             case kPadCopy:
                                                 self.clipboard_ = ClipboardSlot { self.proc_.currentKit().slots[static_cast<std::size_t>(slot)],
                                                                                   self.proc_.slotName(slot), self.proc_.slotSample(slot) };
                                                 break;
                                             case kPadPaste:
                                                 if (self.clipboard_)
                                                     self.proc_.setSlot(slot, self.clipboard_->params, self.clipboard_->name, self.clipboard_->sample);
                                                 break;
```

- `buildSourceMenu` und `showSourceMenu` ersetzen, neue Methoden ergänzen:

```cpp
juce::PopupMenu DubgefahrenEditor::buildSourceMenu(const juce::StringArray& sampleFiles) const
{
    juce::PopupMenu menu;
    menu.addItem(kSourceSynth, "Synth");
    if (proc_.kitFile() == juce::File())
        menu.addItem(kSourceSampleDisabled, "Sample (export the kit first)", false);
    else
        menu.addSubMenu("Sample", buildSampleMenu(sampleFiles));
    return menu;
}

juce::PopupMenu DubgefahrenEditor::buildSampleMenu(const juce::StringArray& sampleFiles)
{
    juce::PopupMenu menu;
    if (sampleFiles.isEmpty())
        menu.addItem(kSourceNoSamples, "(no samples in kit folder)", false);
    for (int i = 0; i < sampleFiles.size(); ++i)
        menu.addItem(kSourceSampleBase + i, sampleFiles[i]);
    menu.addSeparator();
    menu.addItem(kSourceAddFile, juce::String::fromUTF8("Add File…"));
    return menu;
}

void DubgefahrenEditor::showSourceMenu(int slot)
{
    // Gleiche Optionen wie das Kit-Menü, verankert am angeklickten Pad.
    const auto files = listSampleFiles(proc_.sampleFolder());
    buildSourceMenu(files).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(pads_.pad(slot)),
                                         [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot, files](int result) {
                                             if (safe != nullptr)
                                                 safe->chooseSource(slot, result, files);
                                         });
}

void DubgefahrenEditor::showSampleMenu(int slot)
{
    const auto files = listSampleFiles(proc_.sampleFolder());
    buildSampleMenu(files).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(slotEditor_.sampleButton()),
                                         [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot, files](int result) {
                                             if (safe != nullptr)
                                                 safe->chooseSource(slot, result, files);
                                         });
}

void DubgefahrenEditor::chooseSource(int slot, int result, const juce::StringArray& files)
{
    if (result == kSourceSynth)
        proc_.resetSlotToFactory(slot);
    else if (result == kSourceAddFile)
    {
        addSampleFile(slot);
        return;
    }
    else if (result >= kSourceSampleBase && result - kSourceSampleBase < files.size())
        proc_.setSlotSample(slot, files[result - kSourceSampleBase]);
    else
        return;
    refreshAll();
}

void DubgefahrenEditor::addSampleFile(int slot)
{
    chooser_ = std::make_unique<juce::FileChooser>("Add Sample", juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                   sampleFileWildcard());
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot](const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              juce::String error;
                              const auto target = importSampleFile(fc.getResult(), safe->proc_.sampleFolder(), error);
                              if (target == juce::File())
                              {
                                  safe->showMessage("Could not add sample", error);
                                  return;
                              }
                              safe->proc_.setSlotSample(slot, target.getFileName());
                              safe->refreshAll();
                          });
}
```

- [ ] **Step 4: Tests**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: 100 % bestanden.

- [ ] **Step 5: Commit**

```bash
git add plugin/ui/PadGrid.h plugin/ui/PadGrid.cpp plugin/ui/SlotEditor.h plugin/ui/SlotEditor.cpp plugin/PluginEditor.h plugin/PluginEditor.cpp tests/plugin/test_Editor.cpp
git commit -m "feat(ui): choose, show and export sample slots (#8)"
```

---

### Task 9: README, Gesamttest, Validator, Folge-Issue

**Files:**
- Modify: `README.md`

- [ ] **Step 1: README** – im Abschnitt „Kits“ die Zeile „An empty pad is silent; …“ ersetzen durch:

```markdown
An empty pad is silent; click it to choose a sound source: Synth, or Sample.
Samples (WAV, AIFF, MP3, FLAC, up to 60 s, played in mono) live in a folder named like the kit,
next to the `.dgkit` file (`Kits\Dub.dgkit` → `Kits\Dub\`). "Add File…" copies a file into that
folder; exporting to a new kit copies the used samples along. Samples need a kit file, so export
a new kit once before adding samples. A missing sample empties its pad and shows a notice.
```

- [ ] **Step 2: Alle Tests**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: 100 % bestanden.

- [ ] **Step 3: VST3-Validator**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
Expected: Validator ohne Fehler.

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: describe sample slots and kit sample folders (#8)"
```

- [ ] **Step 5: Folge-Issue anlegen** (vom Nutzer im Brainstorming zugesagt)

```bash
gh issue create --title "Sample start/end, loop and reverse" --label enhancement --body "Follow-up to #8: per-slot sample start and end points, loop and reverse playback. Deferred from the #8 design (docs/superpowers/specs/2026-09-28-sample-player-design.md, section 2)."
```

- [ ] **Step 6: Manuelle Abnahme (Nutzer, Ableton)**
  - Ein Kit exportieren, dann auf ein leeres Pad „Sample ▸ Add File…“ mit einer WAV-, einer MP3- und einer FLAC-Datei wählen. Alle drei spielen.
  - Tune, Attack, Release, Gate/One Shot, Choke gegen eine Sirene, Volume/Pan/FX-Send prüfen.
  - Set speichern und neu laden: Die Samples sind wieder da.
  - Kit-Ordner umbenennen und das Set neu laden: Die Pads werden leer, und eine Meldung listet die Samples.
  - Export in ein neues Kit: Der Ordner `New\` enthält die Samples. Import des neuen Kits auf einem anderen Pfad funktioniert.
