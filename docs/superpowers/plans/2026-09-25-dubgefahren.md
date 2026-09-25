# Dubgefahren Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ein VST3-Instrument für Windows, das 16 einzeln einstellbare Dubsirenen über MIDI-Noten 36–51 spielt und durch eine gemeinsame Dub-Effektkette schickt.

**Architecture:** Eine JUCE-freie C++20-DSP-Bibliothek (`engine/`, Catch2-getestet) enthält Stimmen, Pad-Logik und Effekte. Eine dünne JUCE-8-Hülle (`plugin/`) stellt Parameter (APVTS), State, Kit-Dateien, Config und die Oberfläche bereit und ruft pro Audioblock `Engine::process` auf.

**Tech Stack:** C++20, CMake ≥ 3.25, Visual Studio 2026 (MSVC, x64), JUCE 8.0.15, Catch2 v3.16.0, VST3 SDK v3.8.1_build_84 (nur für den Validator), pluginval.

**Spec:** `docs/superpowers/specs/2026-09-25-dubgefahren-design.md`

## Global Constraints

- Plattform: nur Windows x64, nur VST3, kein Standalone, kein AU/CLAP.
- JUCE-Version: `8.0.15` (FetchContent, gepinnt). Catch2: `v3.16.0`. VST3 SDK (Validator): `v3.8.1_build_84`.
- `engine/` darf **keine** JUCE-Header einbinden.
- Pads: MIDI-Noten **36–51** → Slots 1–16 (intern Index 0–15). Andere Noten und MIDI-Nachrichten werden ignoriert. Velocity wird ignoriert; Note-On mit Velocity 0 ist ein Note-Off.
- Genau eine Stimme pro Slot, maximal 16 gleichzeitig.
- Choke-Fade und Kill-Fade: **5 ms**. Performance-Glättung: **20 ms**. Limiter-Ceiling: **−0,3 dBFS** (Faktor 0.966051).
- Lautstärke −60 dB bedeutet stumm (−inf).
- Im Audio-Thread: keine Allokation, keine Locks, kein Datei-I/O.
- Quelltexte sind UTF-8 (`/utf-8`). Nicht-ASCII-Strings an JUCE immer über `juce::String::fromUTF8(...)`.
- Kit-Dateiendung `.dgkit`, JSON, Format-Kennung `"dubgefahren-kit"`, Version `1`.
- Config: `Dubgefahren.vst3\Contents\Resources\Dubgefahren.config.json`, Schlüssel `kitFolder`, nur lesen. Standard-Kit-Ordner: `Dokumente\Dubgefahren\Kits`.
- Install-Ziel: `%CommonProgramFiles%\VST3`; eine vorhandene Config im installierten Bundle wird nie überschrieben.
- Alle Befehle werden aus dem Repo-Root ausgeführt. Build/Test: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 <configure|build|test|install|validate>`.

## Review Focus

- **Trigger-Modus wird geändert, während ein Pad gehalten wird** (z. B. per Automation Gate → Latch): Das Note-Off muss die Stimme trotzdem beenden, keine hängenden Töne. → Test in Task 7 (`startedMode`).
- **Host liefert größere Blöcke als in `prepareToPlay` angekündigt** oder wechselt die Samplerate: kein Absturz, keine Überläufe. → Test in Task 13 (Blöcke > maxBlock).
- **Note-On mit Velocity 0** (von Controllern/Ableton als Note-Off gesendet): muss als Note-Off wirken. → Test in Task 14.
- **Doppeltes Antippen eines Latch-Pads, während die Stimme noch im Release ist:** muss die Sirene neu starten, nicht stumm bleiben. → Test in Task 7.
- **Slot-Namen mit Umlauten (ä ö ü ß) in State und Kit-Dateien:** müssen verlustfrei gespeichert und geladen werden. → Tests in Task 14 und Task 15.

---

## Dateistruktur

```
CMakeLists.txt                  Root: Abhängigkeiten, Unterprojekte
build.ps1                       configure | build | test | install | validate
.gitignore
README.md                       Setup, Build, Config, Ableton-Mapping, Abnahme-Checkliste
cmake/CopyExampleConfig.cmake   Post-Build: Beispiel-Config ins Bundle (nur wenn keine da)
cmake/InstallBundle.cmake       Install: Bundle kopieren, Config nie überschreiben
resources/Dubgefahren.config.json  Beispiel-Config
engine/
  CMakeLists.txt
  DspMath.h/.cpp                Konstanten, dB, Halbtöne, One-Pole
  SlotParams.h/.cpp             Enums, SlotParams, Sync-Teiler, kNumSlots
  SlotFields.h/.cpp             Feld-Tabelle (Key, Name, Bereich, Default) + get/set
  Oscillator.h/.cpp             PolyBLEP-Oszillator
  Lfo.h/.cpp                    Pitch-LFO inkl. S&H und Sync-Rate
  Envelope.h/.cpp               Attack/Release/Kill
  SoundSource.h                 Schnittstelle Klangquelle, PerfOffsets, VoiceContext
  SirenVoice.h/.cpp             Sirenen-Stimme
  PadRouter.h/.cpp              Trigger-Modi, Choke, Fokus, Latch-Stopp, Panic
  FxParams.h/.cpp               FxParams, DelayDivision
  Drive.h                       Sättigung
  SvFilter.h/.cpp               TPT-SVF mit LP/BP/HP-Morph
  Limiter.h                     Peak-Limiter
  TapeDelay.h/.cpp              Tape-Delay
  SpringReverb.h/.cpp           Federhall
  FxChain.h/.cpp                Drive → Filter → Delay → Hall → Master → Limiter
  Kit.h/.cpp                    Kit-Struktur + Werks-Kit
  Engine.h/.cpp                 Gesamte Engine
plugin/
  CMakeLists.txt
  ParameterLayout.h/.cpp        Parameter-IDs, Layout, ParamCache, Slot lesen/schreiben
  PluginProcessor.h/.cpp        AudioProcessor
  KitFile.h/.cpp                Kit ↔ JSON
  Config.h/.cpp                 Config lesen, Kit-Ordner auflösen
  PluginEditor.h/.cpp           Hauptfenster
  ui/DgLookAndFeel.h/.cpp       Farben, Knob-Zeichnung
  ui/Controls.h/.cpp            Knob, Choice, Toggle mit Parameter-Attachments
  ui/PadGrid.h/.cpp             16 Pads
  ui/SlotEditor.h/.cpp          Editor für einen Slot
  ui/FxPanel.h/.cpp             Effekt-Leiste
  ui/PerformancePanel.h/.cpp    Performance-Leiste
tests/
  CMakeLists.txt
  TestHelpers.h
  test_*.cpp                    Engine-Tests (ohne JUCE)
  plugin/test_*.cpp             Plugin-Tests (mit JUCE)
  InstallBundleTest.cmake       Test der Install-Logik
tools/vst3validator/CMakeLists.txt  Baut Steinbergs validator separat
```

---

### Task 1: Projektgerüst, Build-Skript, erste Engine-Funktionen

**Files:**
- Create: `CMakeLists.txt`, `build.ps1`, `.gitignore`, `engine/CMakeLists.txt`, `engine/DspMath.h`, `engine/DspMath.cpp`, `tests/CMakeLists.txt`, `tests/TestHelpers.h`, `tests/test_DspMath.cpp`

**Interfaces:**
- Consumes: nichts.
- Produces:
  - Namespace `dg`. `constexpr float kPi, kTwoPi, kMinVolumeDb = -60.0f`.
  - `float semitonesToRatio(float semis)`, `float dbToGain(float db)`, `float volumeDbToGain(float db)` (≤ −60 → 0), `float onePoleCoeff(float timeSeconds, double sampleRate)`.
  - CMake-Targets `dg_engine` (STATIC, Include-Root = Repo-Root, also `#include "engine/X.h"`), `DubgefahrenTests` (Catch2).
  - `tests/TestHelpers.h` mit Namespace `dgtest` (siehe Code).

- [ ] **Step 1: `.gitignore` anlegen**

```gitignore
build/
.vs/
*.user
tools/bin/
```

- [ ] **Step 2: Root-`CMakeLists.txt` anlegen**

```cmake
cmake_minimum_required(VERSION 3.25)
project(Dubgefahren VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW ON)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.0
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

enable_testing()
add_subdirectory(engine)
add_subdirectory(tests)
```

- [ ] **Step 3: `engine/CMakeLists.txt` anlegen**

```cmake
add_library(dg_engine STATIC
    DspMath.h DspMath.cpp)
target_include_directories(dg_engine PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_features(dg_engine PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(dg_engine PRIVATE /W4 /permissive-)
endif()
```

- [ ] **Step 4: `tests/CMakeLists.txt` und `tests/TestHelpers.h` anlegen**

`tests/CMakeLists.txt`:

```cmake
add_executable(DubgefahrenTests
    TestHelpers.h
    test_DspMath.cpp)
target_link_libraries(DubgefahrenTests PRIVATE dg_engine Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(DubgefahrenTests)
```

`tests/TestHelpers.h`:

```cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace dgtest {

inline std::size_t clampEnd(const std::vector<float>& x, std::size_t to)
{
    return std::min(to, x.size());
}

inline int countRisingZeroCrossings(const std::vector<float>& x, std::size_t from = 0,
                                    std::size_t to = std::numeric_limits<std::size_t>::max())
{
    int count = 0;
    to = clampEnd(x, to);
    for (std::size_t i = std::max<std::size_t>(from, 1); i < to; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            ++count;
    return count;
}

inline float estimateFrequency(const std::vector<float>& x, double sampleRate, std::size_t from, std::size_t to)
{
    to = clampEnd(x, to);
    const double seconds = static_cast<double>(to - from) / sampleRate;
    return static_cast<float>(countRisingZeroCrossings(x, from, to) / seconds);
}

inline float peakAbs(const std::vector<float>& x, std::size_t from = 0,
                     std::size_t to = std::numeric_limits<std::size_t>::max())
{
    float p = 0.0f;
    to = clampEnd(x, to);
    for (std::size_t i = from; i < to; ++i)
        p = std::max(p, std::abs(x[i]));
    return p;
}

inline float rms(const std::vector<float>& x, std::size_t from = 0,
                 std::size_t to = std::numeric_limits<std::size_t>::max())
{
    to = clampEnd(x, to);
    if (to <= from)
        return 0.0f;
    double sum = 0.0;
    for (std::size_t i = from; i < to; ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return static_cast<float>(std::sqrt(sum / static_cast<double>(to - from)));
}

inline float maxStep(const std::vector<float>& x, std::size_t from = 1,
                     std::size_t to = std::numeric_limits<std::size_t>::max())
{
    float m = 0.0f;
    to = clampEnd(x, to);
    for (std::size_t i = std::max<std::size_t>(from, 1); i < to; ++i)
        m = std::max(m, std::abs(x[i] - x[i - 1]));
    return m;
}

inline bool allFinite(const std::vector<float>& x)
{
    return std::all_of(x.begin(), x.end(), [](float v) { return std::isfinite(v); });
}

inline std::vector<float> sine(float freq, double sampleRate, int numSamples, float amp = 1.0f)
{
    std::vector<float> out(static_cast<std::size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        out[static_cast<std::size_t>(i)] =
            amp * static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * freq * i / sampleRate));
    return out;
}

inline std::vector<float> noise(int numSamples, float amp, std::uint32_t seed = 1)
{
    std::vector<float> out(static_cast<std::size_t>(numSamples));
    std::uint32_t s = seed == 0 ? 1u : seed;
    for (auto& v : out)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        v = amp * (static_cast<float>(s) / 4294967295.0f * 2.0f - 1.0f);
    }
    return out;
}

} // namespace dgtest
```

- [ ] **Step 5: Failing test `tests/test_DspMath.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "engine/DspMath.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("semitonesToRatio", "[math]")
{
    CHECK_THAT(dg::semitonesToRatio(12.0f), WithinAbs(2.0, 1e-5));
    CHECK_THAT(dg::semitonesToRatio(-12.0f), WithinAbs(0.5, 1e-5));
    CHECK_THAT(dg::semitonesToRatio(0.0f), WithinAbs(1.0, 1e-6));
}

TEST_CASE("dbToGain and volumeDbToGain", "[math]")
{
    CHECK_THAT(dg::dbToGain(0.0f), WithinAbs(1.0, 1e-6));
    CHECK_THAT(dg::dbToGain(-6.0f), WithinAbs(0.501187, 1e-4));
    CHECK_THAT(dg::dbToGain(6.0f), WithinAbs(1.995262, 1e-4));
    CHECK(dg::volumeDbToGain(dg::kMinVolumeDb) == 0.0f);
    CHECK(dg::volumeDbToGain(-100.0f) == 0.0f);
    CHECK_THAT(dg::volumeDbToGain(-59.0f), WithinAbs(dg::dbToGain(-59.0f), 1e-9));
}

TEST_CASE("onePoleCoeff", "[math]")
{
    CHECK(dg::onePoleCoeff(0.0f, 48000.0) == 1.0f);
    const float a = dg::onePoleCoeff(0.02f, 48000.0);
    CHECK(a > 0.0f);
    CHECK(a < 0.01f);
}
```

- [ ] **Step 6: `build.ps1` anlegen**

```powershell
param(
    [Parameter(Position = 0)]
    [ValidateSet('configure', 'build', 'test', 'install', 'validate', 'all')]
    [string]$Command = 'all',
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$buildDir = Join-Path $root 'build'

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $c = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path $c) { return $c }
    }
    throw 'cmake nicht gefunden (weder im PATH noch in Visual Studio).'
}

$cmake = Find-CMake
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'

function Invoke-Checked([string]$what, [scriptblock]$block) {
    & $block
    if ($LASTEXITCODE -ne 0) { throw "$what fehlgeschlagen (Exit $LASTEXITCODE)" }
}

function Do-Configure { Invoke-Checked 'configure' { & $cmake -S $root -B $buildDir -A x64 } }
function Do-Build {
    if (-not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) { Do-Configure }
    Invoke-Checked 'build' { & $cmake --build $buildDir --config $Config --parallel }
}
function Do-Test { Invoke-Checked 'test' { & $ctest --test-dir $buildDir -C $Config --output-on-failure } }
function Do-Install { Invoke-Checked 'install' { & $cmake --install $buildDir --config $Config } }

switch ($Command) {
    'configure' { Do-Configure }
    'build'     { Do-Build }
    'test'      { Do-Build; Do-Test }
    'install'   { Do-Build; Do-Install }
    'all'       { Do-Configure; Do-Build; Do-Test }
}
```

(`validate` wird in Task 17 ergänzt; bis dahin tut der Wert nichts.)

- [ ] **Step 7: Build ausführen und Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `DspMath.cpp` bzw. `engine/DspMath.h` fehlt (CMake-Fehler „Cannot find source file“ oder Compilerfehler C1083). Der erste Configure lädt JUCE und Catch2 herunter und dauert einige Minuten.

- [ ] **Step 8: `engine/DspMath.h` und `engine/DspMath.cpp` implementieren**

`engine/DspMath.h`:

```cpp
#pragma once
#include <cmath>

namespace dg {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kMinVolumeDb = -60.0f; // -60 dB bedeutet stumm

inline float semitonesToRatio(float semis) { return std::exp2(semis / 12.0f); }

float dbToGain(float db);

// Wie dbToGain, aber kMinVolumeDb und darunter sind exakt 0.
float volumeDbToGain(float db);

// Koeffizient für y += a * (x - y) mit Zeitkonstante timeSeconds.
inline float onePoleCoeff(float timeSeconds, double sampleRate)
{
    if (timeSeconds <= 0.0f)
        return 1.0f;
    return 1.0f - std::exp(-1.0f / (timeSeconds * static_cast<float>(sampleRate)));
}

} // namespace dg
```

`engine/DspMath.cpp`:

```cpp
#include "engine/DspMath.h"

namespace dg {

float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

float volumeDbToGain(float db) { return db <= kMinVolumeDb ? 0.0f : dbToGain(db); }

} // namespace dg
```

- [ ] **Step 9: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS – ctest meldet `100% tests passed` (3 Tests).

- [ ] **Step 10: Commit**

```bash
git add .gitignore CMakeLists.txt build.ps1 engine tests
git commit -m "build: project scaffold with engine library and Catch2 tests"
```

---

### Task 2: SlotParams und Feld-Tabelle

**Files:**
- Create: `engine/SlotParams.h`, `engine/SlotParams.cpp`, `engine/SlotFields.h`, `engine/SlotFields.cpp`, `tests/test_SlotFields.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nichts außer DspMath.
- Produces:
  - `constexpr int kNumSlots = 16, kFirstNote = 36;`
  - `enum class Waveform { Sine, Triangle, Saw, Square }`, `enum class LfoShape { Square, Triangle, SawUp, SawDown, SampleHold }`, `enum class TriggerMode { Gate, Latch, OneShot }`, `enum class SyncDivision { D1_32, D1_16T, D1_16, D1_8T, D1_8, D1_4T, D1_4, D1_2, Bar1, Bars2, Bars4 }`
  - `float syncDivisionBeats(SyncDivision)`
  - `struct SlotParams` (Felder siehe Code, `operator==` defaulted)
  - `enum class SlotField { Wave, PulseWidth, Pitch, LfoShape, LfoRate, LfoSync, LfoSyncDiv, LfoDepth, SweepAmount, SweepTime, Attack, Release, TrigMode, OneShotLength, Choke, Volume, Pan, FxSend, Count }`, `constexpr int kNumSlotFields = 18`
  - `enum class FieldKind { Float, Choice, Bool }`, `struct FieldSpec { key, name, kind, min, max, def, skewCentre, unit, choices }`
  - `const FieldSpec& fieldSpec(SlotField)`, `float getSlotField(const SlotParams&, SlotField)`, `void setSlotField(SlotParams&, SlotField, float)` (klemmt, rundet Choice/Bool, NaN → Default), `SlotParams makeDefaultSlotParams()`, `std::optional<SlotField> slotFieldFromKey(std::string_view)`

- [ ] **Step 1: Failing test `tests/test_SlotFields.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <set>
#include <string>
#include "engine/SlotFields.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

TEST_CASE("there are 18 slot fields with unique keys", "[fields]")
{
    STATIC_CHECK(kNumSlotFields == 18);
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
    CHECK(keys.size() == 18);
    CHECK_FALSE(slotFieldFromKey("doesNotExist").has_value());
}

TEST_CASE("default SlotParams match the field table", "[fields]")
{
    CHECK(makeDefaultSlotParams() == SlotParams{});
}

TEST_CASE("get and set round-trip every field at min, max and default", "[fields]")
{
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        const auto& s = fieldSpec(f);
        for (float v : { s.min, s.max, s.def })
        {
            SlotParams p;
            setSlotField(p, f, v);
            CHECK_THAT(getSlotField(p, f), WithinAbs(v, 1e-5));
        }
    }
}

TEST_CASE("setSlotField clamps, rounds and rejects NaN", "[fields]")
{
    SlotParams p;
    setSlotField(p, SlotField::Pitch, 1.0e6f);
    CHECK(p.pitchHz == fieldSpec(SlotField::Pitch).max);
    setSlotField(p, SlotField::Pitch, -5.0f);
    CHECK(p.pitchHz == fieldSpec(SlotField::Pitch).min);
    setSlotField(p, SlotField::Wave, 2.6f);
    CHECK(p.wave == Waveform::Square);
    setSlotField(p, SlotField::Choke, 99.0f);
    CHECK(p.chokeGroup == 4);
    setSlotField(p, SlotField::LfoSync, 0.7f);
    CHECK(p.lfoSync);
    setSlotField(p, SlotField::Volume, std::numeric_limits<float>::quiet_NaN());
    CHECK(p.volumeDb == fieldSpec(SlotField::Volume).def);
}

TEST_CASE("sync division beats", "[fields]")
{
    CHECK_THAT(syncDivisionBeats(SyncDivision::D1_32), WithinAbs(0.125, 1e-6));
    CHECK_THAT(syncDivisionBeats(SyncDivision::D1_16T), WithinAbs(1.0 / 6.0, 1e-6));
    CHECK_THAT(syncDivisionBeats(SyncDivision::D1_4), WithinAbs(1.0, 1e-6));
    CHECK_THAT(syncDivisionBeats(SyncDivision::Bar1), WithinAbs(4.0, 1e-6));
    CHECK_THAT(syncDivisionBeats(SyncDivision::Bars4), WithinAbs(16.0, 1e-6));
}
```

In `tests/CMakeLists.txt` anhängen:

```cmake
target_sources(DubgefahrenTests PRIVATE test_SlotFields.cpp)
```

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/SlotFields.h'`.

- [ ] **Step 3: `engine/SlotParams.h` / `.cpp` implementieren**

`engine/SlotParams.h`:

```cpp
#pragma once

namespace dg {

constexpr int kNumSlots = 16;
constexpr int kFirstNote = 36; // Slot 1 = Note 36 (C1 in Ableton)

enum class Waveform { Sine, Triangle, Saw, Square };
enum class LfoShape { Square, Triangle, SawUp, SawDown, SampleHold };
enum class TriggerMode { Gate, Latch, OneShot };
enum class SyncDivision { D1_32, D1_16T, D1_16, D1_8T, D1_8, D1_4T, D1_4, D1_2, Bar1, Bars2, Bars4 };

// Länge einer LFO-Periode in Viertelnoten (4/4-Takt).
float syncDivisionBeats(SyncDivision d);

struct SlotParams
{
    Waveform wave = Waveform::Square;
    float pulseWidth = 0.5f;       // 0.05 .. 0.95
    float pitchHz = 440.0f;        // 20 .. 5000
    LfoShape lfoShape = LfoShape::Square;
    float lfoRateHz = 2.0f;        // 0.05 .. 40
    bool lfoSync = false;
    SyncDivision lfoSyncDiv = SyncDivision::D1_8;
    float lfoDepthSemis = 12.0f;   // 0 .. 48
    float sweepSemis = 0.0f;       // -48 .. 48
    float sweepTimeS = 0.5f;       // 0.01 .. 10
    float attackS = 0.005f;        // 0 .. 5
    float releaseS = 0.3f;         // 0 .. 10
    TriggerMode trigMode = TriggerMode::Gate;
    float oneShotS = 1.0f;         // 0.05 .. 10
    int chokeGroup = 0;            // 0 = keine, 1 .. 4
    float volumeDb = -6.0f;        // -60 (stumm) .. 6
    float pan = 0.0f;              // -1 .. 1
    float fxSend = 0.3f;           // 0 .. 1

    bool operator==(const SlotParams&) const = default;
};

} // namespace dg
```

`engine/SlotParams.cpp`:

```cpp
#include "engine/SlotParams.h"

namespace dg {

float syncDivisionBeats(SyncDivision d)
{
    switch (d)
    {
        case SyncDivision::D1_32:  return 0.125f;
        case SyncDivision::D1_16T: return 1.0f / 6.0f;
        case SyncDivision::D1_16:  return 0.25f;
        case SyncDivision::D1_8T:  return 1.0f / 3.0f;
        case SyncDivision::D1_8:   return 0.5f;
        case SyncDivision::D1_4T:  return 2.0f / 3.0f;
        case SyncDivision::D1_4:   return 1.0f;
        case SyncDivision::D1_2:   return 2.0f;
        case SyncDivision::Bar1:   return 4.0f;
        case SyncDivision::Bars2:  return 8.0f;
        case SyncDivision::Bars4:  return 16.0f;
    }
    return 1.0f;
}

} // namespace dg
```

- [ ] **Step 4: `engine/SlotFields.h` / `.cpp` implementieren**

`engine/SlotFields.h`:

```cpp
#pragma once
#include <optional>
#include <span>
#include <string_view>
#include "engine/SlotParams.h"

namespace dg {

enum class SlotField
{
    Wave, PulseWidth, Pitch,
    LfoShape, LfoRate, LfoSync, LfoSyncDiv, LfoDepth,
    SweepAmount, SweepTime,
    Attack, Release,
    TrigMode, OneShotLength, Choke,
    Volume, Pan, FxSend,
    Count
};
constexpr int kNumSlotFields = static_cast<int>(SlotField::Count);

enum class FieldKind { Float, Choice, Bool };

struct FieldSpec
{
    const char* key;      // Parameter-ID-Suffix und Kit-JSON-Schlüssel
    const char* name;     // Anzeigename (UTF-8)
    FieldKind kind;
    float min;
    float max;
    float def;
    float skewCentre;     // 0 = linear, sonst Wert in Mittelstellung
    const char* unit;
    std::span<const char* const> choices; // nur bei Choice
};

const FieldSpec& fieldSpec(SlotField f);
float getSlotField(const SlotParams& p, SlotField f);
void setSlotField(SlotParams& p, SlotField f, float value);
SlotParams makeDefaultSlotParams();
std::optional<SlotField> slotFieldFromKey(std::string_view key);

} // namespace dg
```

`engine/SlotFields.cpp`:

```cpp
#include "engine/SlotFields.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace dg {

namespace {

constexpr const char* kWaveChoices[] = { "Sinus", "Dreieck", "Sägezahn", "Rechteck" };
constexpr const char* kLfoShapeChoices[] = { "Rechteck", "Dreieck", "Sägezahn auf", "Sägezahn ab", "Sample & Hold" };
constexpr const char* kSyncDivChoices[] = { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2",
                                            "1 Takt", "2 Takte", "4 Takte" };
constexpr const char* kTrigModeChoices[] = { "Gate", "Latch", "One-Shot" };
constexpr const char* kChokeChoices[] = { "Keine", "1", "2", "3", "4" };

using C = std::span<const char* const>;

const std::array<FieldSpec, kNumSlotFields> kSpecs { {
    { "wave",       "Welle",          FieldKind::Choice, 0.0f, 3.0f, 3.0f, 0.0f, "", C(kWaveChoices) },
    { "pw",         "Pulsbreite",     FieldKind::Float, 0.05f, 0.95f, 0.5f, 0.0f, "", {} },
    { "pitch",      "Tonhöhe",        FieldKind::Float, 20.0f, 5000.0f, 440.0f, 400.0f, "Hz", {} },
    { "lfoShape",   "LFO-Form",       FieldKind::Choice, 0.0f, 4.0f, 0.0f, 0.0f, "", C(kLfoShapeChoices) },
    { "lfoRate",    "LFO-Rate",       FieldKind::Float, 0.05f, 40.0f, 2.0f, 2.0f, "Hz", {} },
    { "lfoSync",    "LFO-Sync",       FieldKind::Bool, 0.0f, 1.0f, 0.0f, 0.0f, "", {} },
    { "lfoSyncDiv", "LFO-Sync-Rate",  FieldKind::Choice, 0.0f, 10.0f, 4.0f, 0.0f, "", C(kSyncDivChoices) },
    { "lfoDepth",   "LFO-Tiefe",      FieldKind::Float, 0.0f, 48.0f, 12.0f, 0.0f, "st", {} },
    { "sweepAmt",   "Sweep-Betrag",   FieldKind::Float, -48.0f, 48.0f, 0.0f, 0.0f, "st", {} },
    { "sweepTime",  "Sweep-Zeit",     FieldKind::Float, 0.01f, 10.0f, 0.5f, 0.5f, "s", {} },
    { "attack",     "Attack",         FieldKind::Float, 0.0f, 5.0f, 0.005f, 0.2f, "s", {} },
    { "release",    "Release",        FieldKind::Float, 0.0f, 10.0f, 0.3f, 0.5f, "s", {} },
    { "trigMode",   "Trigger-Modus",  FieldKind::Choice, 0.0f, 2.0f, 0.0f, 0.0f, "", C(kTrigModeChoices) },
    { "oneShot",    "One-Shot-Länge", FieldKind::Float, 0.05f, 10.0f, 1.0f, 1.0f, "s", {} },
    { "choke",      "Choke-Gruppe",   FieldKind::Choice, 0.0f, 4.0f, 0.0f, 0.0f, "", C(kChokeChoices) },
    { "vol",        "Lautstärke",     FieldKind::Float, -60.0f, 6.0f, -6.0f, 0.0f, "dB", {} },
    { "pan",        "Panorama",       FieldKind::Float, -1.0f, 1.0f, 0.0f, 0.0f, "", {} },
    { "send",       "FX-Send",        FieldKind::Float, 0.0f, 1.0f, 0.3f, 0.0f, "", {} },
} };

} // namespace

const FieldSpec& fieldSpec(SlotField f) { return kSpecs[static_cast<std::size_t>(f)]; }

float getSlotField(const SlotParams& p, SlotField f)
{
    switch (f)
    {
        case SlotField::Wave:          return static_cast<float>(p.wave);
        case SlotField::PulseWidth:    return p.pulseWidth;
        case SlotField::Pitch:         return p.pitchHz;
        case SlotField::LfoShape:      return static_cast<float>(p.lfoShape);
        case SlotField::LfoRate:       return p.lfoRateHz;
        case SlotField::LfoSync:       return p.lfoSync ? 1.0f : 0.0f;
        case SlotField::LfoSyncDiv:    return static_cast<float>(p.lfoSyncDiv);
        case SlotField::LfoDepth:      return p.lfoDepthSemis;
        case SlotField::SweepAmount:   return p.sweepSemis;
        case SlotField::SweepTime:     return p.sweepTimeS;
        case SlotField::Attack:        return p.attackS;
        case SlotField::Release:       return p.releaseS;
        case SlotField::TrigMode:      return static_cast<float>(p.trigMode);
        case SlotField::OneShotLength: return p.oneShotS;
        case SlotField::Choke:         return static_cast<float>(p.chokeGroup);
        case SlotField::Volume:        return p.volumeDb;
        case SlotField::Pan:           return p.pan;
        case SlotField::FxSend:        return p.fxSend;
        case SlotField::Count:         break;
    }
    return 0.0f;
}

void setSlotField(SlotParams& p, SlotField f, float value)
{
    const auto& s = fieldSpec(f);
    if (!std::isfinite(value))
        value = s.def;
    value = std::clamp(value, s.min, s.max);
    const int i = static_cast<int>(std::lround(value));

    switch (f)
    {
        case SlotField::Wave:          p.wave = static_cast<Waveform>(i); break;
        case SlotField::PulseWidth:    p.pulseWidth = value; break;
        case SlotField::Pitch:         p.pitchHz = value; break;
        case SlotField::LfoShape:      p.lfoShape = static_cast<LfoShape>(i); break;
        case SlotField::LfoRate:       p.lfoRateHz = value; break;
        case SlotField::LfoSync:       p.lfoSync = i != 0; break;
        case SlotField::LfoSyncDiv:    p.lfoSyncDiv = static_cast<SyncDivision>(i); break;
        case SlotField::LfoDepth:      p.lfoDepthSemis = value; break;
        case SlotField::SweepAmount:   p.sweepSemis = value; break;
        case SlotField::SweepTime:     p.sweepTimeS = value; break;
        case SlotField::Attack:        p.attackS = value; break;
        case SlotField::Release:       p.releaseS = value; break;
        case SlotField::TrigMode:      p.trigMode = static_cast<TriggerMode>(i); break;
        case SlotField::OneShotLength: p.oneShotS = value; break;
        case SlotField::Choke:         p.chokeGroup = i; break;
        case SlotField::Volume:        p.volumeDb = value; break;
        case SlotField::Pan:           p.pan = value; break;
        case SlotField::FxSend:        p.fxSend = value; break;
        case SlotField::Count:         break;
    }
}

SlotParams makeDefaultSlotParams()
{
    SlotParams p;
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        setSlotField(p, f, fieldSpec(f).def);
    }
    return p;
}

std::optional<SlotField> slotFieldFromKey(std::string_view key)
{
    for (int i = 0; i < kNumSlotFields; ++i)
        if (key == kSpecs[static_cast<std::size_t>(i)].key)
            return static_cast<SlotField>(i);
    return std::nullopt;
}

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen:

```cmake
target_sources(dg_engine PRIVATE SlotParams.h SlotParams.cpp SlotFields.h SlotFields.cpp)
```

- [ ] **Step 5: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS (alle Tests, inkl. `[fields]`).

- [ ] **Step 6: Commit**

```bash
git add engine tests
git commit -m "feat(engine): slot parameters and field table"
```

---

### Task 3: PolyBLEP-Oszillator

**Files:**
- Create: `engine/Oscillator.h`, `engine/Oscillator.cpp`, `tests/test_Oscillator.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Waveform` (Task 2), `kTwoPi` (Task 1).
- Produces: `class Oscillator { void prepare(double sampleRate); void resetPhase(); float process(Waveform w, float freqHz, float pulseWidth); }` – gibt den aktuellen Sample-Wert zurück und rückt dann die Phase vor. Ausgabe etwa in [−1, 1].

- [ ] **Step 1: Failing test `tests/test_Oscillator.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <numeric>
#include <vector>
#include "engine/Oscillator.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
std::vector<float> render(Waveform w, float freq, float pw, int n, double sr = 48000.0)
{
    Oscillator osc;
    osc.prepare(sr);
    std::vector<float> out(static_cast<std::size_t>(n));
    for (auto& v : out)
        v = osc.process(w, freq, pw);
    return out;
}
} // namespace

TEST_CASE("oscillator frequency matches for all waveforms", "[osc]")
{
    for (auto w : { Waveform::Sine, Waveform::Triangle, Waveform::Saw, Waveform::Square })
    {
        const auto x = render(w, 440.0f, 0.5f, 48000);
        CHECK_THAT(dgtest::estimateFrequency(x, 48000.0, 0, x.size()), WithinAbs(440.0, 3.0));
    }
}

TEST_CASE("oscillator output is bounded", "[osc]")
{
    for (auto w : { Waveform::Sine, Waveform::Triangle, Waveform::Saw, Waveform::Square })
    {
        for (float f : { 20.0f, 1000.0f, 18000.0f, 40000.0f })
        {
            const auto x = render(w, f, 0.3f, 4800);
            CHECK(dgtest::allFinite(x));
            CHECK(dgtest::peakAbs(x) <= 1.3f);
        }
    }
}

TEST_CASE("square pulse width sets the duty cycle", "[osc]")
{
    const auto x = render(Waveform::Square, 100.0f, 0.25f, 48000);
    const double mean = std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(x.size());
    CHECK_THAT(mean, WithinAbs(-0.5, 0.02));
}

TEST_CASE("resetPhase restarts the waveform", "[osc]")
{
    Oscillator osc;
    osc.prepare(48000.0);
    for (int i = 0; i < 123; ++i)
        osc.process(Waveform::Sine, 440.0f, 0.5f);
    osc.resetPhase();
    CHECK_THAT(osc.process(Waveform::Sine, 440.0f, 0.5f), WithinAbs(0.0, 1e-6));
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_Oscillator.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/Oscillator.h'`.

- [ ] **Step 3: Implementieren**

`engine/Oscillator.h`:

```cpp
#pragma once
#include "engine/SlotParams.h"

namespace dg {

class Oscillator
{
public:
    void prepare(double sampleRate);
    void resetPhase() { phase_ = 0.0f; }
    // Liefert den aktuellen Wert und rückt die Phase vor.
    float process(Waveform w, float freqHz, float pulseWidth);

private:
    double sampleRate_ = 44100.0;
    float phase_ = 0.0f;
};

} // namespace dg
```

`engine/Oscillator.cpp`:

```cpp
#include "engine/Oscillator.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

namespace {
// Polynomielle Korrektur an Sprungstellen (PolyBLEP).
float polyBlep(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}
} // namespace

void Oscillator::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    phase_ = 0.0f;
}

float Oscillator::process(Waveform w, float freqHz, float pulseWidth)
{
    const float dt = std::clamp(freqHz / static_cast<float>(sampleRate_), 0.0f, 0.5f);
    const float p = phase_;
    float out = 0.0f;

    switch (w)
    {
        case Waveform::Sine:
            out = std::sin(kTwoPi * p);
            break;
        case Waveform::Triangle:
            out = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
            break;
        case Waveform::Saw:
            out = 2.0f * p - 1.0f - polyBlep(p, dt);
            break;
        case Waveform::Square:
        {
            const float pw = std::clamp(pulseWidth, 0.05f, 0.95f);
            out = p < pw ? 1.0f : -1.0f;
            out += polyBlep(p, dt);
            float t2 = p - pw + 1.0f;
            if (t2 >= 1.0f)
                t2 -= 1.0f;
            out -= polyBlep(t2, dt);
            break;
        }
    }

    phase_ += dt;
    if (phase_ >= 1.0f)
        phase_ -= 1.0f;
    return out;
}

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE Oscillator.h Oscillator.cpp)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): PolyBLEP oscillator"
```

---

### Task 4: LFO

**Files:**
- Create: `engine/Lfo.h`, `engine/Lfo.cpp`, `tests/test_Lfo.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `LfoShape`, `SyncDivision`, `syncDivisionBeats` (Task 2).
- Produces:
  - `class Lfo { void prepare(double sampleRate); void reset(std::uint32_t seed); float process(LfoShape shape, float rateHz); }` – Ausgabe unipolar **[0, 1]**; `reset` setzt Phase 0 und würfelt den S&H-Wert neu.
  - Formen bei Phase p: Square `p < 0.5 ? 0 : 1`; Triangle `p < 0.5 ? 2p : 2 − 2p`; SawUp `p`; SawDown `1 − p`; SampleHold: Zufallswert, neu bei jedem Phasenumlauf.
  - `float lfoRateFromSync(SyncDivision d, double bpm)` = `bpm / 60 / beats`.

- [ ] **Step 1: Failing test `tests/test_Lfo.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include "engine/Lfo.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;
std::vector<float> render(LfoShape s, float rate, int n, std::uint32_t seed = 1)
{
    Lfo lfo;
    lfo.prepare(kSr);
    lfo.reset(seed);
    std::vector<float> out(static_cast<std::size_t>(n));
    for (auto& v : out)
        v = lfo.process(s, rate);
    return out;
}
} // namespace

TEST_CASE("square LFO starts low and goes high after half a period", "[lfo]")
{
    const auto x = render(LfoShape::Square, 1.0f, 48000);
    CHECK(x[0] == 0.0f);
    CHECK(x[20000] == 0.0f);
    CHECK(x[30000] == 1.0f);
}

TEST_CASE("triangle, saw up and saw down shapes", "[lfo]")
{
    const auto tri = render(LfoShape::Triangle, 1.0f, 48000);
    CHECK_THAT(tri[0], WithinAbs(0.0, 1e-4));
    CHECK_THAT(tri[24000], WithinAbs(1.0, 1e-3));
    CHECK_THAT(tri[12000], WithinAbs(0.5, 1e-3));

    const auto up = render(LfoShape::SawUp, 1.0f, 48000);
    CHECK_THAT(up[0], WithinAbs(0.0, 1e-4));
    CHECK_THAT(up[36000], WithinAbs(0.75, 1e-3));

    const auto down = render(LfoShape::SawDown, 1.0f, 48000);
    CHECK_THAT(down[0], WithinAbs(1.0, 1e-4));
    CHECK_THAT(down[36000], WithinAbs(0.25, 1e-3));
}

TEST_CASE("LFO rate: saw up wraps at the expected rate", "[lfo]")
{
    // 4.5 Hz: Umläufe bei ~10667, 21333, 32000, 42667 Samples → 4 innerhalb 1 s
    const auto x = render(LfoShape::SawUp, 4.5f, 48000);
    int wraps = 0;
    for (std::size_t i = 1; i < x.size(); ++i)
        if (x[i] < x[i - 1])
            ++wraps;
    CHECK(wraps == 4);
}

TEST_CASE("sample and hold is constant within a period and in range", "[lfo]")
{
    const auto x = render(LfoShape::SampleHold, 10.0f, 48000, 42);
    for (int period = 0; period < 9; ++period)
    {
        const auto start = static_cast<std::size_t>(period * 4800 + 10);
        for (std::size_t i = start; i < start + 4700; ++i)
            REQUIRE(x[i] == x[start]);
        REQUIRE(x[start] >= 0.0f);
        REQUIRE(x[start] <= 1.0f);
    }
    bool changed = false;
    for (int period = 1; period < 9; ++period)
        changed |= x[static_cast<std::size_t>(period * 4800 + 10)] != x[10];
    CHECK(changed);
}

TEST_CASE("sample and hold is deterministic per seed", "[lfo]")
{
    CHECK(render(LfoShape::SampleHold, 10.0f, 9600, 7) == render(LfoShape::SampleHold, 10.0f, 9600, 7));
    CHECK(render(LfoShape::SampleHold, 10.0f, 9600, 7) != render(LfoShape::SampleHold, 10.0f, 9600, 8));
}

TEST_CASE("reset restarts at phase 0", "[lfo]")
{
    Lfo lfo;
    lfo.prepare(kSr);
    lfo.reset(1);
    for (int i = 0; i < 30000; ++i)
        lfo.process(LfoShape::SawUp, 1.0f);
    lfo.reset(1);
    CHECK_THAT(lfo.process(LfoShape::SawUp, 1.0f), WithinAbs(0.0, 1e-6));
}

TEST_CASE("sync rate from tempo", "[lfo]")
{
    CHECK_THAT(lfoRateFromSync(SyncDivision::D1_4, 120.0), WithinAbs(2.0, 1e-5));
    CHECK_THAT(lfoRateFromSync(SyncDivision::Bar1, 120.0), WithinAbs(0.5, 1e-5));
    CHECK_THAT(lfoRateFromSync(SyncDivision::D1_16T, 120.0), WithinAbs(12.0, 1e-4));
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_Lfo.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/Lfo.h'`.

- [ ] **Step 3: Implementieren**

`engine/Lfo.h`:

```cpp
#pragma once
#include <cstdint>
#include "engine/SlotParams.h"

namespace dg {

class Lfo
{
public:
    void prepare(double sampleRate);
    void reset(std::uint32_t seed);
    // Liefert den aktuellen Wert in [0, 1] und rückt die Phase vor.
    float process(LfoShape shape, float rateHz);

private:
    float nextRandom();

    double sampleRate_ = 44100.0;
    float phase_ = 0.0f;
    float holdValue_ = 0.0f;
    std::uint32_t rng_ = 1;
};

float lfoRateFromSync(SyncDivision d, double bpm);

} // namespace dg
```

`engine/Lfo.cpp`:

```cpp
#include "engine/Lfo.h"
#include <algorithm>

namespace dg {

void Lfo::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset(1);
}

void Lfo::reset(std::uint32_t seed)
{
    rng_ = seed == 0 ? 1u : seed;
    phase_ = 0.0f;
    holdValue_ = nextRandom();
}

float Lfo::nextRandom()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_) / 4294967295.0f;
}

float Lfo::process(LfoShape shape, float rateHz)
{
    const float p = phase_;
    float out = 0.0f;
    switch (shape)
    {
        case LfoShape::Square:     out = p < 0.5f ? 0.0f : 1.0f; break;
        case LfoShape::Triangle:   out = p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p; break;
        case LfoShape::SawUp:      out = p; break;
        case LfoShape::SawDown:    out = 1.0f - p; break;
        case LfoShape::SampleHold: out = holdValue_; break;
    }

    phase_ += std::max(0.0f, rateHz) / static_cast<float>(sampleRate_);
    if (phase_ >= 1.0f)
    {
        phase_ -= static_cast<float>(static_cast<int>(phase_));
        holdValue_ = nextRandom();
    }
    return out;
}

float lfoRateFromSync(SyncDivision d, double bpm)
{
    return static_cast<float>(bpm / 60.0) / syncDivisionBeats(d);
}

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE Lfo.h Lfo.cpp)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): pitch LFO with sample and hold and tempo sync"
```

---

### Task 5: Hüllkurve

**Files:**
- Create: `engine/Envelope.h`, `engine/Envelope.cpp`, `tests/test_Envelope.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `constexpr float kMinEnvTimeS = 0.001f, kKillTimeS = 0.005f;`
  - `class Envelope { enum class Stage { Idle, Attack, Sustain, Release, Kill }; void prepare(double); void noteOn(float attackS); void noteOff(float releaseS); void kill(); float process(); Stage stage() const; bool isActive() const; bool isReleasing() const; float level() const; }`
  - Attack läuft linear mit Rate `1/attack` vom aktuellen Pegel auf 1 (kein Sprung bei Neustart). Release/Kill linear vom aktuellen Pegel auf 0 in der jeweiligen Zeit. `isReleasing()` ist wahr in Release und Kill. `noteOff` in Kill oder Idle wird ignoriert.

- [ ] **Step 1: Failing test `tests/test_Envelope.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include "engine/Envelope.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;
float run(Envelope& e, int n)
{
    float v = 0.0f;
    for (int i = 0; i < n; ++i)
        v = e.process();
    return v;
}
} // namespace

TEST_CASE("attack reaches full level after attack time and sustains", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    CHECK_FALSE(e.isActive());
    e.noteOn(0.01f);
    CHECK(e.isActive());
    CHECK(run(e, 240) < 0.6f);
    CHECK_THAT(run(e, 250), WithinAbs(1.0, 1e-6));
    CHECK(e.stage() == Envelope::Stage::Sustain);
    CHECK_THAT(run(e, 10000), WithinAbs(1.0, 1e-6));
}

TEST_CASE("release reaches zero after release time and becomes idle", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    e.noteOn(0.0f);
    run(e, 100);
    e.noteOff(0.1f);
    CHECK(e.isReleasing());
    CHECK(run(e, 4700) > 0.0f);
    run(e, 110);
    CHECK(e.level() == 0.0f);
    CHECK_FALSE(e.isActive());
}

TEST_CASE("kill fades out within 5 ms without big steps", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    e.noteOn(0.0f);
    run(e, 100);
    e.kill();
    float prev = e.level();
    float maxStep = 0.0f;
    for (int i = 0; i < 241; ++i)
    {
        const float v = e.process();
        maxStep = std::max(maxStep, prev - v);
        prev = v;
    }
    CHECK_FALSE(e.isActive());
    CHECK(maxStep <= 1.0f / 240.0f + 1e-5f);
}

TEST_CASE("retrigger during release continues from current level", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    e.noteOn(0.0f);
    run(e, 100);
    e.noteOff(1.0f);
    const float before = run(e, 24000); // ~0.5
    e.noteOn(1.0f);
    const float after = e.process();
    CHECK(after >= before);
    CHECK(after - before < 0.001f);
    CHECK(e.stage() == Envelope::Stage::Attack);
}

TEST_CASE("zero attack uses the 1 ms minimum", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    e.noteOn(0.0f);
    CHECK(e.process() < 0.1f);
    CHECK_THAT(run(e, 48), WithinAbs(1.0, 1e-6));
}

TEST_CASE("noteOff and kill while idle do nothing", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    e.noteOff(0.1f);
    e.kill();
    CHECK_FALSE(e.isActive());
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_Envelope.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/Envelope.h'`.

- [ ] **Step 3: Implementieren**

`engine/Envelope.h`:

```cpp
#pragma once

namespace dg {

constexpr float kMinEnvTimeS = 0.001f;
constexpr float kKillTimeS = 0.005f;

class Envelope
{
public:
    enum class Stage { Idle, Attack, Sustain, Release, Kill };

    void prepare(double sampleRate);
    void noteOn(float attackS);
    void noteOff(float releaseS);
    void kill();
    float process();

    Stage stage() const { return stage_; }
    bool isActive() const { return stage_ != Stage::Idle; }
    bool isReleasing() const { return stage_ == Stage::Release || stage_ == Stage::Kill; }
    float level() const { return level_; }

private:
    double sampleRate_ = 44100.0;
    Stage stage_ = Stage::Idle;
    float level_ = 0.0f;
    float step_ = 0.0f;
};

} // namespace dg
```

`engine/Envelope.cpp`:

```cpp
#include "engine/Envelope.h"
#include <algorithm>

namespace dg {

void Envelope::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    stage_ = Stage::Idle;
    level_ = 0.0f;
    step_ = 0.0f;
}

void Envelope::noteOn(float attackS)
{
    const float t = std::max(attackS, kMinEnvTimeS);
    step_ = 1.0f / (t * static_cast<float>(sampleRate_));
    stage_ = Stage::Attack;
}

void Envelope::noteOff(float releaseS)
{
    if (stage_ == Stage::Idle || stage_ == Stage::Kill)
        return;
    const float t = std::max(releaseS, kMinEnvTimeS);
    step_ = std::max(level_, 1.0e-6f) / (t * static_cast<float>(sampleRate_));
    stage_ = Stage::Release;
}

void Envelope::kill()
{
    if (stage_ == Stage::Idle)
        return;
    step_ = std::max(level_, 1.0e-6f) / (kKillTimeS * static_cast<float>(sampleRate_));
    stage_ = Stage::Kill;
}

float Envelope::process()
{
    switch (stage_)
    {
        case Stage::Idle:
        case Stage::Sustain:
            break;
        case Stage::Attack:
            level_ += step_;
            if (level_ >= 1.0f)
            {
                level_ = 1.0f;
                stage_ = Stage::Sustain;
            }
            break;
        case Stage::Release:
        case Stage::Kill:
            level_ -= step_;
            if (level_ <= 0.0f)
            {
                level_ = 0.0f;
                stage_ = Stage::Idle;
            }
            break;
    }
    return level_;
}

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE Envelope.h Envelope.cpp)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): linear attack/release/kill envelope"
```

---

### Task 6: SoundSource-Schnittstelle und SirenVoice

**Files:**
- Create: `engine/SoundSource.h`, `engine/SirenVoice.h`, `engine/SirenVoice.cpp`, `tests/test_SirenVoice.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Oscillator` (Task 3), `Lfo`, `lfoRateFromSync` (Task 4), `Envelope` (Task 5), `SlotParams` (Task 2), `semitonesToRatio`, `onePoleCoeff` (Task 1).
- Produces:
  - `struct PerfOffsets { float pitchSemis = 0, rateFactor = 1, depthSemis = 0, sweepSemis = 0; operator== defaulted }`
  - `struct VoiceContext { const SlotParams* params; double bpm; PerfOffsets perf; }`
  - `class SoundSource` (abstrakt): `prepare(double)`, `start(const VoiceContext&, std::uint32_t seed)`, `release(const VoiceContext&)`, `kill()`, `isActive() const`, `isReleasing() const`, `render(float* out, int numSamples, const VoiceContext&)` – **überschreibt** `out` (Mono, ohne Lautstärke/Pan).
  - `class SirenVoice final : public SoundSource` plus `float currentFrequency() const` (zuletzt berechnete Frequenz, für Tests).
  - Tonhöhe: `pitchHz · 2^((lfo·depth + sweep + perf.pitch)/12)`, geklemmt auf 10 Hz–18 kHz. `depth = max(0, lfoDepth + perf.depth)`, `rate = (sync ? lfoRateFromSync : lfoRateHz) · perf.rateFactor`, Sweep linear von `sweepSemis + perf.sweep` auf 0 über `sweepTimeS`. Performance-Offsets werden mit 20 ms geglättet (Rate-Faktor im log2-Bereich); bei `start` werden sie ohne Glättung übernommen.

- [ ] **Step 1: Failing test `tests/test_SirenVoice.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "engine/SirenVoice.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {
constexpr double kSr = 48000.0;

SlotParams plainSine(float hz)
{
    SlotParams p;
    p.wave = Waveform::Sine;
    p.pitchHz = hz;
    p.lfoDepthSemis = 0.0f;
    p.sweepSemis = 0.0f;
    p.attackS = 0.0f;
    p.releaseS = 0.05f;
    return p;
}

std::vector<float> renderN(SirenVoice& v, const VoiceContext& ctx, int n)
{
    std::vector<float> out(static_cast<std::size_t>(n));
    v.render(out.data(), n, ctx);
    return out;
}
} // namespace

TEST_CASE("inactive voice renders silence", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(440.0f);
    VoiceContext ctx { &p, 120.0, {} };
    std::vector<float> out(512, 1.0f);
    v.render(out.data(), 512, ctx);
    CHECK(dgtest::peakAbs(out) == 0.0f);
    CHECK_FALSE(v.isActive());
}

TEST_CASE("plain voice plays its base pitch", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(440.0f);
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    const auto x = renderN(v, ctx, 48000);
    CHECK_THAT(dgtest::estimateFrequency(x, kSr, 480, x.size()), WithinAbs(440.0, 3.0));
    CHECK(dgtest::peakAbs(x, 4800) > 0.95f);
}

TEST_CASE("sweep starts at base plus amount and ends at base", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(400.0f);
    p.sweepSemis = 12.0f;
    p.sweepTimeS = 0.5f;
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    renderN(v, ctx, 1);
    CHECK_THAT(v.currentFrequency(), WithinRel(800.0, 0.01));
    renderN(v, ctx, 12000); // letztes Sample bei 0,25 s → Hälfte des Sweeps
    CHECK_THAT(v.currentFrequency(), WithinRel(400.0 * std::exp2(6.0 / 12.0), 0.01));
    renderN(v, ctx, 13000);
    CHECK_THAT(v.currentFrequency(), WithinRel(400.0, 0.001));
}

TEST_CASE("square LFO jumps between base and base plus depth", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(300.0f);
    p.lfoShape = LfoShape::Square;
    p.lfoRateHz = 1.0f;
    p.lfoDepthSemis = 12.0f;
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    renderN(v, ctx, 19200); // 0.4 s
    CHECK_THAT(v.currentFrequency(), WithinRel(300.0, 0.001));
    renderN(v, ctx, 14400); // 0.7 s
    CHECK_THAT(v.currentFrequency(), WithinRel(600.0, 0.001));
}

TEST_CASE("synced LFO follows tempo", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(300.0f);
    p.lfoShape = LfoShape::Square;
    p.lfoSync = true;
    p.lfoSyncDiv = SyncDivision::D1_4; // 2 Hz bei 120 bpm
    p.lfoDepthSemis = 12.0f;
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    renderN(v, ctx, 4800); // 0.1 s
    CHECK_THAT(v.currentFrequency(), WithinRel(300.0, 0.001));
    renderN(v, ctx, 9600); // 0.3 s
    CHECK_THAT(v.currentFrequency(), WithinRel(600.0, 0.001));
}

TEST_CASE("performance pitch offset is applied immediately on start and smoothed afterwards", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(200.0f);
    VoiceContext ctx { &p, 120.0, {} };
    ctx.perf.pitchSemis = 12.0f;
    v.start(ctx, 1);
    renderN(v, ctx, 1);
    CHECK_THAT(v.currentFrequency(), WithinRel(400.0, 0.001));

    ctx.perf.pitchSemis = 0.0f;
    renderN(v, ctx, 48);  // 1 ms: noch nicht angekommen
    CHECK(v.currentFrequency() > 380.0f);
    renderN(v, ctx, 9600); // 200 ms: angekommen
    CHECK_THAT(v.currentFrequency(), WithinRel(200.0, 0.001));
}

TEST_CASE("negative depth offset clamps LFO depth at zero", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(300.0f);
    p.lfoDepthSemis = 5.0f;
    p.lfoRateHz = 1.0f;
    VoiceContext ctx { &p, 120.0, {} };
    ctx.perf.depthSemis = -24.0f;
    v.start(ctx, 1);
    renderN(v, ctx, 33600); // 0.7 s, LFO wäre oben
    CHECK_THAT(v.currentFrequency(), WithinRel(300.0, 0.001));
}

TEST_CASE("frequency is clamped to 18 kHz", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(5000.0f);
    p.sweepSemis = 48.0f;
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    renderN(v, ctx, 1);
    CHECK(v.currentFrequency() == 18000.0f);
}

TEST_CASE("voice ends after release", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(440.0f);
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    renderN(v, ctx, 480);
    v.release(ctx);
    CHECK(v.isReleasing());
    renderN(v, ctx, 2400 + 10);
    CHECK_FALSE(v.isActive());
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_SirenVoice.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/SirenVoice.h'`.

- [ ] **Step 3: Implementieren**

`engine/SoundSource.h`:

```cpp
#pragma once
#include <cstdint>
#include "engine/SlotParams.h"

namespace dg {

// Performance-Offsets, die zur Laufzeit auf eine Stimme wirken (Mitte = neutral).
struct PerfOffsets
{
    float pitchSemis = 0.0f;
    float rateFactor = 1.0f;
    float depthSemis = 0.0f;
    float sweepSemis = 0.0f;

    bool operator==(const PerfOffsets&) const = default;
};

struct VoiceContext
{
    const SlotParams* params = nullptr;
    double bpm = 120.0;
    PerfOffsets perf {};
};

// Klangquelle eines Slots. Heute SirenVoice, später z. B. ein SamplePlayer.
class SoundSource
{
public:
    virtual ~SoundSource() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void start(const VoiceContext& ctx, std::uint32_t seed) = 0;
    virtual void release(const VoiceContext& ctx) = 0;
    virtual void kill() = 0;
    virtual bool isActive() const = 0;
    virtual bool isReleasing() const = 0;
    // Überschreibt out[0..numSamples) mit Mono-Ausgabe (ohne Lautstärke und Pan).
    virtual void render(float* out, int numSamples, const VoiceContext& ctx) = 0;
};

} // namespace dg
```

`engine/SirenVoice.h`:

```cpp
#pragma once
#include "engine/Envelope.h"
#include "engine/Lfo.h"
#include "engine/Oscillator.h"
#include "engine/SoundSource.h"

namespace dg {

class SirenVoice final : public SoundSource
{
public:
    void prepare(double sampleRate) override;
    void start(const VoiceContext& ctx, std::uint32_t seed) override;
    void release(const VoiceContext& ctx) override;
    void kill() override;
    bool isActive() const override { return env_.isActive(); }
    bool isReleasing() const override { return env_.isReleasing(); }
    void render(float* out, int numSamples, const VoiceContext& ctx) override;

    float currentFrequency() const { return lastFreq_; }

private:
    double sampleRate_ = 44100.0;
    Oscillator osc_;
    Lfo lfo_;
    Envelope env_;
    float perfCoeff_ = 1.0f;
    float smPitch_ = 0.0f;
    float smLogRate_ = 0.0f;
    float smDepth_ = 0.0f;
    float smSweep_ = 0.0f;
    double sweepElapsedS_ = 0.0;
    float lastFreq_ = 0.0f;
};

} // namespace dg
```

`engine/SirenVoice.cpp`:

```cpp
#include "engine/SirenVoice.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

namespace {
constexpr float kPerfSmoothingS = 0.02f;
constexpr float kMinFreq = 10.0f;
constexpr float kMaxFreq = 18000.0f;
} // namespace

void SirenVoice::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    osc_.prepare(sampleRate);
    lfo_.prepare(sampleRate);
    env_.prepare(sampleRate);
    perfCoeff_ = onePoleCoeff(kPerfSmoothingS, sampleRate);
    lastFreq_ = 0.0f;
}

void SirenVoice::start(const VoiceContext& ctx, std::uint32_t seed)
{
    if (!env_.isActive())
        osc_.resetPhase(); // bei Neustart einer klingenden Stimme Phase behalten → kein Knacks
    lfo_.reset(seed);
    sweepElapsedS_ = 0.0;
    smPitch_ = ctx.perf.pitchSemis;
    smLogRate_ = std::log2(std::max(ctx.perf.rateFactor, 1.0e-3f));
    smDepth_ = ctx.perf.depthSemis;
    smSweep_ = ctx.perf.sweepSemis;
    env_.noteOn(ctx.params->attackS);
}

void SirenVoice::release(const VoiceContext& ctx) { env_.noteOff(ctx.params->releaseS); }

void SirenVoice::kill() { env_.kill(); }

void SirenVoice::render(float* out, int numSamples, const VoiceContext& ctx)
{
    if (!env_.isActive())
    {
        std::fill(out, out + numSamples, 0.0f);
        return;
    }

    const SlotParams& p = *ctx.params;
    const float targetLogRate = std::log2(std::max(ctx.perf.rateFactor, 1.0e-3f));
    const float baseRate = p.lfoSync ? lfoRateFromSync(p.lfoSyncDiv, ctx.bpm) : p.lfoRateHz;
    const double dtS = 1.0 / sampleRate_;

    for (int i = 0; i < numSamples; ++i)
    {
        smPitch_ += perfCoeff_ * (ctx.perf.pitchSemis - smPitch_);
        smLogRate_ += perfCoeff_ * (targetLogRate - smLogRate_);
        smDepth_ += perfCoeff_ * (ctx.perf.depthSemis - smDepth_);
        smSweep_ += perfCoeff_ * (ctx.perf.sweepSemis - smSweep_);

        const float rate = baseRate * std::exp2(smLogRate_);
        const float depth = std::max(0.0f, p.lfoDepthSemis + smDepth_);
        const float lfo = lfo_.process(p.lfoShape, rate);

        float sweep = 0.0f;
        if (sweepElapsedS_ < p.sweepTimeS)
            sweep = (p.sweepSemis + smSweep_) * static_cast<float>(1.0 - sweepElapsedS_ / p.sweepTimeS);
        sweepElapsedS_ += dtS;

        const float semis = lfo * depth + sweep + smPitch_;
        lastFreq_ = std::clamp(p.pitchHz * semitonesToRatio(semis), kMinFreq, kMaxFreq);

        out[i] = osc_.process(p.wave, lastFreq_, p.pulseWidth) * env_.process();
    }
}

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE SoundSource.h SirenVoice.h SirenVoice.cpp)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): siren voice behind SoundSource interface"
```

---

### Task 7: PadRouter (Trigger-Modi, Choke, Fokus, Latch-Stopp, Panic)

**Files:**
- Create: `engine/PadRouter.h`, `engine/PadRouter.cpp`, `tests/test_PadRouter.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `TriggerMode`, `kNumSlots`, `kFirstNote` (Task 2).
- Produces:
  - `class VoiceControl` (abstrakt): `startVoice(int)`, `releaseVoice(int)`, `killVoice(int)`, `isVoiceActive(int) const`, `isVoiceReleasing(int) const`.
  - `struct TriggerSettings { TriggerMode mode; float oneShotS; int chokeGroup; }`, `using TriggerSettingsArray = std::array<TriggerSettings, kNumSlots>`.
  - `enum class LatchStopAction { Continue, Release, Stop }`.
  - `int slotForNote(int note)` (−1 außerhalb 36–51).
  - `class PadRouter`: `prepare(double)`, `noteOn(int note, const TriggerSettingsArray&, VoiceControl&)`, `noteOff(int note, VoiceControl&)`, `previewOn(int slot, const TriggerSettingsArray&, VoiceControl&)`, `previewOff(int slot, VoiceControl&)`, `advance(int numSamples, VoiceControl&)`, `transportStopped(LatchStopAction, VoiceControl&)`, `panic(VoiceControl&)`, `int focusSlot() const`, `void setFocusSlot(int)`, `bool isLatched(int) const`, `std::uint32_t latchedMask() const`.
  - Note-Off wertet den Modus aus, mit dem die Stimme **gestartet** wurde (`startedMode_`), nicht den aktuellen.

- [ ] **Step 1: Failing test `tests/test_PadRouter.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <string>
#include <vector>
#include "engine/PadRouter.h"

using namespace dg;

namespace {
constexpr double kSr = 48000.0;

struct FakeVoices final : VoiceControl
{
    std::array<bool, kNumSlots> active {};
    std::array<bool, kNumSlots> releasing {};
    std::vector<std::string> log;

    void startVoice(int s) override { active[s] = true; releasing[s] = false; log.push_back("start " + std::to_string(s)); }
    void releaseVoice(int s) override { releasing[s] = true; log.push_back("release " + std::to_string(s)); }
    void killVoice(int s) override { active[s] = false; releasing[s] = false; log.push_back("kill " + std::to_string(s)); }
    bool isVoiceActive(int s) const override { return active[s]; }
    bool isVoiceReleasing(int s) const override { return releasing[s]; }
};

TriggerSettingsArray allMode(TriggerMode m)
{
    TriggerSettingsArray a {};
    for (auto& t : a)
        t = { m, 1.0f, 0 };
    return a;
}

PadRouter makeRouter()
{
    PadRouter r;
    r.prepare(kSr);
    return r;
}

using Log = std::vector<std::string>;
} // namespace

TEST_CASE("notes outside 36..51 are ignored", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Gate);
    r.noteOn(35, s, v);
    r.noteOn(52, s, v);
    r.noteOff(35, v);
    CHECK(v.log.empty());
    CHECK(slotForNote(36) == 0);
    CHECK(slotForNote(51) == 15);
    CHECK(slotForNote(52) == -1);
}

TEST_CASE("gate: note on starts, note off releases, retrigger restarts", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Gate);
    r.noteOn(36, s, v);
    r.noteOn(36, s, v);
    r.noteOff(36, v);
    CHECK(v.log == Log { "start 0", "start 0", "release 0" });
}

TEST_CASE("latch: toggles on note on, ignores note off", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Latch);
    r.noteOn(37, s, v);
    CHECK(r.isLatched(1));
    r.noteOff(37, v);
    r.noteOn(37, s, v);
    CHECK_FALSE(r.isLatched(1));
    CHECK(v.log == Log { "start 1", "release 1" });
}

TEST_CASE("latch: tapping again while releasing restarts the siren", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Latch);
    r.noteOn(37, s, v);
    r.noteOn(37, s, v); // Release
    r.noteOn(37, s, v); // noch im Release → Neustart
    CHECK(v.log == Log { "start 1", "release 1", "start 1" });
    CHECK(r.isLatched(1));
}

TEST_CASE("one-shot: releases automatically after its length and ignores note off", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::OneShot);
    s[2].oneShotS = 0.5f;
    r.noteOn(38, s, v);
    r.noteOff(38, v);
    r.advance(23999, v);
    CHECK(v.log == Log { "start 2" });
    r.advance(1, v);
    CHECK(v.log == Log { "start 2", "release 2" });
    r.advance(48000, v);
    CHECK(v.log.size() == 2);
}

TEST_CASE("one-shot: retrigger restarts the timer", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::OneShot);
    s[2].oneShotS = 0.5f;
    r.noteOn(38, s, v);
    r.advance(20000, v);
    r.noteOn(38, s, v);
    r.advance(20000, v);
    CHECK(v.log == Log { "start 2", "start 2" });
    r.advance(4000, v);
    CHECK(v.log.back() == "release 2");
}

TEST_CASE("choke kills active voices in the same group only", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Latch);
    s[0].chokeGroup = 1;
    s[1].chokeGroup = 1;
    s[2].chokeGroup = 2;
    s[3].chokeGroup = 0;
    r.noteOn(36, s, v); // slot 0
    r.noteOn(38, s, v); // slot 2
    r.noteOn(39, s, v); // slot 3
    r.noteOn(37, s, v); // slot 1 → würgt slot 0 ab
    CHECK(v.log == Log { "start 0", "start 2", "start 3", "kill 0", "start 1" });
    CHECK_FALSE(r.isLatched(0));
    CHECK(r.isLatched(2));
}

TEST_CASE("choke skips inactive voices and group 0 never chokes", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Gate);
    s[0].chokeGroup = 1;
    s[1].chokeGroup = 1;
    r.noteOn(37, s, v);
    CHECK(v.log == Log { "start 1" });

    auto r2 = makeRouter();
    FakeVoices v2;
    const auto s0 = allMode(TriggerMode::Gate);
    r2.noteOn(36, s0, v2);
    r2.noteOn(37, s0, v2);
    CHECK(v2.log == Log { "start 0", "start 1" });
}

TEST_CASE("focus follows the last note on, including latch release", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Latch);
    CHECK(r.focusSlot() == 0);
    r.noteOn(40, s, v);
    CHECK(r.focusSlot() == 4);
    r.noteOn(45, s, v);
    CHECK(r.focusSlot() == 9);
    r.noteOn(40, s, v); // beendet slot 4
    CHECK(r.focusSlot() == 4);
    r.setFocusSlot(7);
    CHECK(r.focusSlot() == 7);
}

TEST_CASE("transport stop acts on latched voices only", "[router]")
{
    auto s = allMode(TriggerMode::Latch);
    s[1].mode = TriggerMode::Gate;

    SECTION("continue")
    {
        auto r = makeRouter();
        FakeVoices v;
        r.noteOn(36, s, v);
        r.noteOn(37, s, v);
        r.transportStopped(LatchStopAction::Continue, v);
        CHECK(v.log == Log { "start 0", "start 1" });
        CHECK(r.isLatched(0));
    }
    SECTION("release")
    {
        auto r = makeRouter();
        FakeVoices v;
        r.noteOn(36, s, v);
        r.noteOn(37, s, v);
        r.transportStopped(LatchStopAction::Release, v);
        CHECK(v.log == Log { "start 0", "start 1", "release 0" });
        CHECK_FALSE(r.isLatched(0));
    }
    SECTION("stop")
    {
        auto r = makeRouter();
        FakeVoices v;
        r.noteOn(36, s, v);
        r.noteOn(37, s, v);
        r.transportStopped(LatchStopAction::Stop, v);
        CHECK(v.log == Log { "start 0", "start 1", "kill 0" });
        CHECK(r.latchedMask() == 0u);
    }
}

TEST_CASE("panic kills everything and clears latches and one-shot timers", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Latch);
    s[1].mode = TriggerMode::OneShot;
    r.noteOn(36, s, v);
    r.noteOn(37, s, v);
    r.panic(v);
    CHECK(v.log == Log { "start 0", "start 1", "kill 0", "kill 1" });
    CHECK(r.latchedMask() == 0u);
    r.advance(480000, v);
    CHECK(v.log.size() == 4);
}

TEST_CASE("preview behaves like gate regardless of mode and sets focus", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Latch);
    s[5].chokeGroup = 1;
    s[6].chokeGroup = 1;
    r.noteOn(42, s, v); // slot 6, gelatcht
    r.previewOn(5, s, v);
    CHECK(r.focusSlot() == 5);
    CHECK_FALSE(r.isLatched(5));
    r.previewOff(5, v);
    CHECK(v.log == Log { "start 6", "kill 6", "start 5", "release 5" });
    r.previewOff(5, v);    // zweites Loslassen tut nichts
    r.previewOn(99, s, v); // ungültiger Slot
    CHECK(v.log.size() == 4);
}

TEST_CASE("note off uses the mode the voice was started with", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Gate);
    r.noteOn(36, s, v);
    s[0].mode = TriggerMode::Latch; // Automation, während das Pad gehalten wird
    r.noteOff(36, v);
    CHECK(v.log == Log { "start 0", "release 0" });
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_PadRouter.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/PadRouter.h'`.

- [ ] **Step 3: Implementieren**

`engine/PadRouter.h`:

```cpp
#pragma once
#include <array>
#include <cstdint>
#include "engine/SlotParams.h"

namespace dg {

class VoiceControl
{
public:
    virtual ~VoiceControl() = default;
    virtual void startVoice(int slot) = 0;
    virtual void releaseVoice(int slot) = 0;
    virtual void killVoice(int slot) = 0;
    virtual bool isVoiceActive(int slot) const = 0;
    virtual bool isVoiceReleasing(int slot) const = 0;
};

struct TriggerSettings
{
    TriggerMode mode = TriggerMode::Gate;
    float oneShotS = 1.0f;
    int chokeGroup = 0;
};
using TriggerSettingsArray = std::array<TriggerSettings, kNumSlots>;

enum class LatchStopAction { Continue, Release, Stop };

int slotForNote(int note);

class PadRouter
{
public:
    void prepare(double sampleRate);

    void noteOn(int note, const TriggerSettingsArray& settings, VoiceControl& voices);
    void noteOff(int note, VoiceControl& voices);
    void previewOn(int slot, const TriggerSettingsArray& settings, VoiceControl& voices);
    void previewOff(int slot, VoiceControl& voices);
    void advance(int numSamples, VoiceControl& voices);
    void transportStopped(LatchStopAction action, VoiceControl& voices);
    void panic(VoiceControl& voices);

    int focusSlot() const { return focus_; }
    void setFocusSlot(int slot);
    bool isLatched(int slot) const;
    std::uint32_t latchedMask() const;

private:
    void start(int slot, TriggerMode mode, const TriggerSettingsArray& settings, VoiceControl& voices);
    void clearSlot(int slot);

    double sampleRate_ = 44100.0;
    int focus_ = 0;
    std::array<TriggerMode, kNumSlots> startedMode_ {};
    std::array<bool, kNumSlots> latched_ {};
    std::array<std::int64_t, kNumSlots> oneShotRemaining_ {};
    std::array<bool, kNumSlots> previewHeld_ {};
};

} // namespace dg
```

`engine/PadRouter.cpp`:

```cpp
#include "engine/PadRouter.h"
#include <algorithm>
#include <cmath>

namespace dg {

int slotForNote(int note)
{
    const int s = note - kFirstNote;
    return (s >= 0 && s < kNumSlots) ? s : -1;
}

void PadRouter::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    focus_ = 0;
    for (int s = 0; s < kNumSlots; ++s)
        clearSlot(s);
}

void PadRouter::clearSlot(int s)
{
    latched_[s] = false;
    oneShotRemaining_[s] = -1;
    previewHeld_[s] = false;
    startedMode_[s] = TriggerMode::Gate;
}

void PadRouter::start(int slot, TriggerMode mode, const TriggerSettingsArray& settings, VoiceControl& voices)
{
    const int group = settings[slot].chokeGroup;
    if (group > 0)
    {
        for (int s = 0; s < kNumSlots; ++s)
        {
            if (s != slot && settings[s].chokeGroup == group && voices.isVoiceActive(s))
            {
                voices.killVoice(s);
                clearSlot(s);
            }
        }
    }

    voices.startVoice(slot);
    focus_ = slot;
    startedMode_[slot] = mode;
    latched_[slot] = mode == TriggerMode::Latch;
    previewHeld_[slot] = false;
    oneShotRemaining_[slot] = mode == TriggerMode::OneShot
        ? std::max<std::int64_t>(1, std::llround(settings[slot].oneShotS * sampleRate_))
        : -1;
}

void PadRouter::noteOn(int note, const TriggerSettingsArray& settings, VoiceControl& voices)
{
    const int slot = slotForNote(note);
    if (slot < 0)
        return;

    const TriggerMode mode = settings[slot].mode;
    if (mode == TriggerMode::Latch && latched_[slot] && voices.isVoiceActive(slot) && !voices.isVoiceReleasing(slot))
    {
        voices.releaseVoice(slot);
        latched_[slot] = false;
        focus_ = slot;
        return;
    }
    start(slot, mode, settings, voices);
}

void PadRouter::noteOff(int note, VoiceControl& voices)
{
    const int slot = slotForNote(note);
    if (slot < 0)
        return;
    if (startedMode_[slot] == TriggerMode::Gate && !previewHeld_[slot]
        && voices.isVoiceActive(slot) && !voices.isVoiceReleasing(slot))
        voices.releaseVoice(slot);
}

void PadRouter::previewOn(int slot, const TriggerSettingsArray& settings, VoiceControl& voices)
{
    if (slot < 0 || slot >= kNumSlots)
        return;
    start(slot, TriggerMode::Gate, settings, voices);
    previewHeld_[slot] = true;
}

void PadRouter::previewOff(int slot, VoiceControl& voices)
{
    if (slot < 0 || slot >= kNumSlots || !previewHeld_[slot])
        return;
    previewHeld_[slot] = false;
    if (voices.isVoiceActive(slot) && !voices.isVoiceReleasing(slot))
        voices.releaseVoice(slot);
}

void PadRouter::advance(int numSamples, VoiceControl& voices)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
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

void PadRouter::transportStopped(LatchStopAction action, VoiceControl& voices)
{
    if (action == LatchStopAction::Continue)
        return;
    for (int s = 0; s < kNumSlots; ++s)
    {
        if (!latched_[s])
            continue;
        latched_[s] = false;
        if (!voices.isVoiceActive(s))
            continue;
        if (action == LatchStopAction::Stop)
            voices.killVoice(s);
        else if (!voices.isVoiceReleasing(s))
            voices.releaseVoice(s);
    }
}

void PadRouter::panic(VoiceControl& voices)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        if (voices.isVoiceActive(s))
            voices.killVoice(s);
        clearSlot(s);
    }
}

void PadRouter::setFocusSlot(int slot)
{
    if (slot >= 0 && slot < kNumSlots)
        focus_ = slot;
}

bool PadRouter::isLatched(int slot) const
{
    return slot >= 0 && slot < kNumSlots && latched_[slot];
}

std::uint32_t PadRouter::latchedMask() const
{
    std::uint32_t m = 0;
    for (int s = 0; s < kNumSlots; ++s)
        if (latched_[s])
            m |= 1u << s;
    return m;
}

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE PadRouter.h PadRouter.cpp)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): pad router with trigger modes, choke, focus and panic"
```

---

### Task 8: Drive, Filter, Limiter

**Files:**
- Create: `engine/Drive.h`, `engine/SvFilter.h`, `engine/SvFilter.cpp`, `engine/Limiter.h`, `tests/test_FxUnits.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `kPi`, `onePoleCoeff` (Task 1).
- Produces:
  - `float driveSample(float x, float drive)` – `drive` 0..1; 0 = unverändert.
  - `class SvFilter { void prepare(double); void reset(); void setParams(float cutoffHz, float resonance, float type); float process(float x, int channel); }` – Kanal 0 oder 1; `type` 0 = LP, 0.5 = BP (Spitzenverstärkung 1), 1 = HP, dazwischen überblendet; Resonanz 0..1 → Q 0.5..20.
  - `constexpr float kLimiterCeiling = 0.966051f;` `class Limiter { void prepare(double); void reset(); void process(float& l, float& r); }` – Ausgabe garantiert |x| ≤ Ceiling, Release 100 ms.

- [ ] **Step 1: Failing test `tests/test_FxUnits.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include "engine/Drive.h"
#include "engine/Limiter.h"
#include "engine/SvFilter.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

float filterGain(float freq, float cutoff, float res, float type)
{
    SvFilter f;
    f.prepare(kSr);
    f.setParams(cutoff, res, type);
    const auto x = dgtest::sine(freq, kSr, 48000);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        y[i] = f.process(x[i], 0);
    return dgtest::peakAbs(y, 24000);
}
} // namespace

TEST_CASE("drive 0 is transparent, drive 1 saturates and stays bounded", "[fx]")
{
    for (float x : { -1.5f, -0.3f, 0.0f, 0.2f, 0.9f })
        CHECK(driveSample(x, 0.0f) == x);
    CHECK(std::abs(driveSample(10.0f, 1.0f)) < 0.3f);
    CHECK(std::abs(driveSample(1.0e6f, 1.0f)) < 0.3f);
    CHECK(driveSample(0.5f, 0.7f) > driveSample(0.2f, 0.7f));
}

TEST_CASE("filter low pass, band pass and high pass responses", "[fx]")
{
    CHECK_THAT(filterGain(100.0f, 1000.0f, 0.0f, 0.0f), WithinAbs(1.0, 0.05));
    CHECK(filterGain(10000.0f, 1000.0f, 0.0f, 0.0f) < 0.05f);

    CHECK_THAT(filterGain(10000.0f, 1000.0f, 0.0f, 1.0f), WithinAbs(1.0, 0.05));
    CHECK(filterGain(100.0f, 1000.0f, 0.0f, 1.0f) < 0.05f);

    CHECK_THAT(filterGain(1000.0f, 1000.0f, 0.0f, 0.5f), WithinAbs(1.0, 0.05));
    CHECK(filterGain(100.0f, 1000.0f, 0.0f, 0.5f) < 0.3f);
    CHECK(filterGain(10000.0f, 1000.0f, 0.0f, 0.5f) < 0.3f);
}

TEST_CASE("filter stays stable at maximum resonance and extreme cutoff", "[fx]")
{
    for (float cutoff : { 20.0f, 1000.0f, 30000.0f })
    {
        SvFilter f;
        f.prepare(kSr);
        f.setParams(cutoff, 1.0f, 0.0f);
        const auto x = dgtest::noise(48000 * 30, 0.5f);
        std::vector<float> y(x.size());
        for (std::size_t i = 0; i < x.size(); ++i)
            y[i] = f.process(x[i], 1);
        CHECK(dgtest::allFinite(y));
        CHECK(dgtest::peakAbs(y) < 50.0f);
    }
}

TEST_CASE("limiter never exceeds the ceiling and recovers", "[fx]")
{
    Limiter lim;
    lim.prepare(kSr);
    const auto loud = dgtest::sine(200.0f, kSr, 4800, 4.0f);
    float peak = 0.0f;
    for (float v : loud)
    {
        float l = v, r = -v;
        lim.process(l, r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(peak <= kLimiterCeiling + 1e-6f);

    const auto quiet = dgtest::sine(200.0f, kSr, 48000, 0.5f);
    std::vector<float> out(quiet.size());
    for (std::size_t i = 0; i < quiet.size(); ++i)
    {
        float l = quiet[i], r = quiet[i];
        lim.process(l, r);
        out[i] = l;
    }
    CHECK_THAT(dgtest::peakAbs(out, 43200), WithinAbs(0.5, 0.01));
}

TEST_CASE("limiter leaves quiet signals untouched", "[fx]")
{
    Limiter lim;
    lim.prepare(kSr);
    for (float v : dgtest::sine(300.0f, kSr, 4800, 0.1f))
    {
        float l = v, r = v;
        lim.process(l, r);
        REQUIRE(l == v);
    }
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_FxUnits.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/Drive.h'`.

- [ ] **Step 3: Implementieren**

`engine/Drive.h`:

```cpp
#pragma once
#include <algorithm>
#include <cmath>

namespace dg {

// drive 0..1: 0 = unverändert, 1 = kräftige tanh-Sättigung mit Pegelausgleich.
inline float driveSample(float x, float drive)
{
    if (drive <= 0.0f)
        return x;
    const float g = 1.0f + 19.0f * std::min(drive, 1.0f);
    const float wet = std::tanh(g * x) / std::sqrt(g);
    const float mix = std::min(1.0f, drive * 10.0f);
    return x + mix * (wet - x);
}

} // namespace dg
```

`engine/SvFilter.h`:

```cpp
#pragma once

namespace dg {

// Zustandsvariabler Filter (TPT, Zavalishin) mit stufenloser Überblendung LP → BP → HP.
class SvFilter
{
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(float cutoffHz, float resonance, float type);
    float process(float x, int channel);

private:
    double sampleRate_ = 44100.0;
    float g_ = 0.0f, k_ = 2.0f, a1_ = 1.0f, a2_ = 0.0f, a3_ = 0.0f;
    float wLp_ = 1.0f, wBp_ = 0.0f, wHp_ = 0.0f;
    float ic1_[2] {};
    float ic2_[2] {};
};

} // namespace dg
```

`engine/SvFilter.cpp`:

```cpp
#include "engine/SvFilter.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

void SvFilter::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
    setParams(20000.0f, 0.0f, 0.0f);
}

void SvFilter::reset()
{
    ic1_[0] = ic1_[1] = 0.0f;
    ic2_[0] = ic2_[1] = 0.0f;
}

void SvFilter::setParams(float cutoffHz, float resonance, float type)
{
    const float sr = static_cast<float>(sampleRate_);
    const float fc = std::clamp(cutoffHz, 20.0f, 0.49f * sr);
    g_ = std::tan(kPi * fc / sr);
    const float q = 0.5f * std::pow(40.0f, std::clamp(resonance, 0.0f, 1.0f));
    k_ = 1.0f / q;
    a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
    a2_ = g_ * a1_;
    a3_ = g_ * a2_;

    const float t = std::clamp(type, 0.0f, 1.0f);
    if (t < 0.5f)
    {
        wLp_ = 1.0f - 2.0f * t;
        wBp_ = 2.0f * t;
        wHp_ = 0.0f;
    }
    else
    {
        wLp_ = 0.0f;
        wBp_ = 2.0f - 2.0f * t;
        wHp_ = 2.0f * t - 1.0f;
    }
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

`engine/Limiter.h`:

```cpp
#pragma once
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

constexpr float kLimiterCeiling = 0.966051f; // -0,3 dBFS

// Peak-Limiter ohne Lookahead: sofortiger Attack, 100 ms Release, harte Sicherung am Ceiling.
class Limiter
{
public:
    void prepare(double sampleRate)
    {
        releaseCoeff_ = onePoleCoeff(0.1f, sampleRate);
        reset();
    }

    void reset() { gain_ = 1.0f; }

    void process(float& l, float& r)
    {
        const float peak = std::max(std::abs(l), std::abs(r));
        const float desired = peak > kLimiterCeiling ? kLimiterCeiling / peak : 1.0f;
        if (desired < gain_)
            gain_ = desired;
        else
            gain_ += (desired - gain_) * releaseCoeff_;
        if (gain_ < 1.0f)
        {
            l *= gain_;
            r *= gain_;
        }
        l = std::clamp(l, -kLimiterCeiling, kLimiterCeiling);
        r = std::clamp(r, -kLimiterCeiling, kLimiterCeiling);
    }

private:
    float gain_ = 1.0f;
    float releaseCoeff_ = 0.001f;
};

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE Drive.h SvFilter.h SvFilter.cpp Limiter.h)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): drive, morphing state variable filter and limiter"
```

---

### Task 9: Tape-Delay und FxParams

**Files:**
- Create: `engine/FxParams.h`, `engine/FxParams.cpp`, `engine/TapeDelay.h`, `engine/TapeDelay.cpp`, `tests/test_TapeDelay.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `onePoleCoeff`, `kTwoPi`, `kPi` (Task 1).
- Produces:
  - `enum class DelayDivision { D1_16T, D1_16, D1_16D, D1_8T, D1_8, D1_8D, D1_4T, D1_4, D1_4D, D1_2T, D1_2, D1_2D, D1_1 }`, `constexpr int kNumDelayDivisions = 13;`
  - `double delayDivisionSeconds(DelayDivision, double bpm)` (bpm geklemmt auf 30..400).
  - `struct FxParams { drive, cutoffHz, resonance, filterType, delayDiv, delayFeedback, delayTone, delayWow, delayMix, reverbDecay, reverbTone, reverbMix, masterDb }` mit Defaults aus dem Code unten – diese Defaults sind die Parameter-Defaults in Task 14.
  - `class TapeDelay { void prepare(double); void reset(); void setParams(float timeS, float feedback, float tone, float wow); void process(float inL, float inR, float& wetL, float& wetR); }` – liefert nur das Wet-Signal. Erster `setParams` nach `prepare`/`reset` setzt die Zeit sofort, spätere Änderungen gleiten (~250 ms).

- [ ] **Step 1: Failing test `tests/test_TapeDelay.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <vector>
#include "engine/FxParams.h"
#include "engine/TapeDelay.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

std::vector<float> impulseResponse(TapeDelay& d, int n)
{
    std::vector<float> out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, l, r);
        out[static_cast<std::size_t>(i)] = l;
    }
    return out;
}
} // namespace

TEST_CASE("delay division to seconds", "[delay]")
{
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_4, 120.0), WithinAbs(0.5, 1e-9));
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_8D, 120.0), WithinAbs(0.375, 1e-9));
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_4T, 120.0), WithinAbs(1.0 / 3.0, 1e-9));
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_1, 20.0), WithinAbs(8.0, 1e-9)); // bpm auf 30 geklemmt
}

TEST_CASE("single echo at the delay time without feedback", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.0f, 1.0f, 0.0f);
    const auto y = impulseResponse(d, 48000);
    const auto peakIt = std::max_element(y.begin(), y.end());
    const auto peakIndex = std::distance(y.begin(), peakIt);
    CHECK(peakIndex >= 12000);
    CHECK(peakIndex <= 12002);
    CHECK(*peakIt > 0.5f);
    CHECK(dgtest::peakAbs(y, 13000) < 0.01f);
    CHECK(dgtest::peakAbs(y, 0, 11990) == 0.0f);
}

TEST_CASE("feedback produces a quieter second echo", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.5f, 1.0f, 0.0f);
    const auto y = impulseResponse(d, 48000);
    const float first = dgtest::peakAbs(y, 11990, 12500);
    const float second = dgtest::peakAbs(y, 23990, 24500);
    CHECK(second / first > 0.3f);
    CHECK(second / first < 0.6f);
}

TEST_CASE("110 percent feedback stays finite and bounded", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.1f, 1.1f, 1.0f, 1.0f);
    const auto burst = dgtest::noise(24000, 0.5f);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 48000 * 30; ++i)
    {
        const float in = i < 24000 ? burst[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(finite);
    CHECK(peak < 4.0f);
}

TEST_CASE("channels are independent", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.01f, 0.3f, 0.5f, 0.0f);
    float maxRight = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(i == 0 ? 1.0f : 0.0f, 0.0f, l, r);
        maxRight = std::max(maxRight, std::abs(r));
    }
    CHECK(maxRight == 0.0f);
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_TapeDelay.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/FxParams.h'`.

- [ ] **Step 3: Implementieren**

`engine/FxParams.h`:

```cpp
#pragma once

namespace dg {

enum class DelayDivision { D1_16T, D1_16, D1_16D, D1_8T, D1_8, D1_8D, D1_4T, D1_4, D1_4D, D1_2T, D1_2, D1_2D, D1_1 };
constexpr int kNumDelayDivisions = 13;

double delayDivisionSeconds(DelayDivision d, double bpm);

struct FxParams
{
    float drive = 0.0f;              // 0 .. 1
    float cutoffHz = 20000.0f;       // 20 .. 20000
    float resonance = 0.1f;          // 0 .. 1
    float filterType = 0.0f;         // 0 = LP, 0.5 = BP, 1 = HP
    DelayDivision delayDiv = DelayDivision::D1_8D;
    float delayFeedback = 0.45f;     // 0 .. 1.1
    float delayTone = 0.5f;          // 0 .. 1
    float delayWow = 0.2f;           // 0 .. 1
    float delayMix = 0.35f;          // 0 .. 1
    float reverbDecay = 0.5f;        // 0 .. 1
    float reverbTone = 0.5f;         // 0 .. 1
    float reverbMix = 0.25f;         // 0 .. 1
    float masterDb = 0.0f;           // -60 (stumm) .. 6
};

} // namespace dg
```

`engine/FxParams.cpp`:

```cpp
#include "engine/FxParams.h"
#include <algorithm>

namespace dg {

double delayDivisionSeconds(DelayDivision d, double bpm)
{
    static constexpr double kBeats[kNumDelayDivisions] = {
        1.0 / 6.0, 0.25, 0.375,   // 1/16T, 1/16, 1/16D
        1.0 / 3.0, 0.5, 0.75,     // 1/8T,  1/8,  1/8D
        2.0 / 3.0, 1.0, 1.5,      // 1/4T,  1/4,  1/4D
        4.0 / 3.0, 2.0, 3.0,      // 1/2T,  1/2,  1/2D
        4.0                       // 1/1
    };
    const double clampedBpm = std::clamp(bpm, 30.0, 400.0);
    return kBeats[static_cast<int>(d)] * 60.0 / clampedBpm;
}

} // namespace dg
```

`engine/TapeDelay.h`:

```cpp
#pragma once
#include <array>
#include <vector>

namespace dg {

class TapeDelay
{
public:
    static constexpr float kMaxDelaySeconds = 10.0f;

    void prepare(double sampleRate);
    void reset();
    void setParams(float timeSeconds, float feedback, float tone, float wow);
    // Liefert nur das Wet-Signal.
    void process(float inL, float inR, float& wetL, float& wetR);

private:
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
    std::array<float, 2> lp_ {};
    float wowPhase1_ = 0.0f;
    float wowPhase2_ = 0.0f;
};

} // namespace dg
```

`engine/TapeDelay.cpp`:

```cpp
#include "engine/TapeDelay.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

void TapeDelay::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    const auto size = static_cast<std::size_t>(std::ceil(sampleRate * kMaxDelaySeconds)) + 4;
    for (auto& b : buf_)
        b.assign(size, 0.0f);
    glideCoeff_ = onePoleCoeff(0.25f, sampleRate);
    reset();
}

void TapeDelay::reset()
{
    for (auto& b : buf_)
        std::fill(b.begin(), b.end(), 0.0f);
    write_ = 0;
    lp_ = { 0.0f, 0.0f };
    wowPhase1_ = wowPhase2_ = 0.0f;
    snapped_ = false;
}

void TapeDelay::setParams(float timeSeconds, float feedback, float tone, float wow)
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
    const float cutoff = 500.0f * std::pow(24.0f, std::clamp(tone, 0.0f, 1.0f)); // 500 Hz .. 12 kHz
    lpCoeff_ = 1.0f - std::exp(-kTwoPi * cutoff / sr);
}

float TapeDelay::read(const std::vector<float>& buf, float delaySamples) const
{
    const float size = static_cast<float>(buf.size());
    float pos = static_cast<float>(write_) - delaySamples;
    while (pos < 0.0f)
        pos += size;
    const auto i0 = static_cast<std::size_t>(pos);
    const float frac = pos - static_cast<float>(i0);
    const std::size_t i1 = (i0 + 1) % buf.size();
    return buf[i0] + frac * (buf[i1] - buf[i0]);
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

    const float delay = std::max(1.0f, current_ + mod);
    const float in[2] = { inL, inR };
    float wet[2] = {};
    for (int ch = 0; ch < 2; ++ch)
    {
        const float r = read(buf_[static_cast<std::size_t>(ch)], delay);
        lp_[static_cast<std::size_t>(ch)] += lpCoeff_ * (r - lp_[static_cast<std::size_t>(ch)]);
        wet[ch] = lp_[static_cast<std::size_t>(ch)];
        // Soft-Clipper im Feedback-Weg: auch bei 110 % bleibt alles begrenzt.
        buf_[static_cast<std::size_t>(ch)][write_] = in[ch] + std::tanh(feedback_ * wet[ch]);
    }
    write_ = (write_ + 1) % buf_[0].size();
    wetL = wet[0];
    wetR = wet[1];
}

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE FxParams.h FxParams.cpp TapeDelay.h TapeDelay.cpp)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): tape delay with glide, wow and soft-clipped feedback"
```

---

### Task 10: Federhall

**Files:**
- Create: `engine/SpringReverb.h`, `engine/SpringReverb.cpp`, `tests/test_SpringReverb.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `class SpringReverb { void prepare(double); void reset(); void setParams(float decay, float tone); void process(float inL, float inR, float& wetL, float& wetR); }` – nur Wet-Signal; Struktur je Kanal: 8 kurze Allpässe (Dispersion) → 4 parallele gedämpfte Kammfilter → 2 Allpass-Diffusoren. Kammfilter-Feedback `0.70 + 0.27·decay`, Dämpfung `0.05 + 0.6·(1 − tone)`.

- [ ] **Step 1: Failing test `tests/test_SpringReverb.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "engine/SpringReverb.h"
#include "TestHelpers.h"

using namespace dg;

namespace {
constexpr double kSr = 48000.0;

struct Stereo { std::vector<float> l, r; };

Stereo impulse(float decay, float tone, int n)
{
    SpringReverb rv;
    rv.prepare(kSr);
    rv.setParams(decay, tone);
    Stereo out { std::vector<float>(static_cast<std::size_t>(n)), std::vector<float>(static_cast<std::size_t>(n)) };
    for (int i = 0; i < n; ++i)
    {
        const float in = i == 0 ? 1.0f : 0.0f;
        rv.process(in, in, out.l[static_cast<std::size_t>(i)], out.r[static_cast<std::size_t>(i)]);
    }
    return out;
}
} // namespace

TEST_CASE("reverb is silent for silent input", "[reverb]")
{
    SpringReverb rv;
    rv.prepare(kSr);
    rv.setParams(1.0f, 0.5f);
    for (int i = 0; i < 4800; ++i)
    {
        float l = 1.0f, r = 1.0f;
        rv.process(0.0f, 0.0f, l, r);
        REQUIRE(l == 0.0f);
        REQUIRE(r == 0.0f);
    }
}

TEST_CASE("impulse produces a tail", "[reverb]")
{
    const auto y = impulse(0.5f, 0.5f, 48000);
    CHECK(dgtest::rms(y.l, 4800, 24000) > 1.0e-4f);
}

TEST_CASE("longer decay gives a longer tail", "[reverb]")
{
    const auto shortTail = impulse(0.0f, 0.5f, 96000);
    const auto longTail = impulse(1.0f, 0.5f, 96000);
    CHECK(dgtest::rms(longTail.l, 48000, 96000) > 2.0f * dgtest::rms(shortTail.l, 48000, 96000));
}

TEST_CASE("left and right differ", "[reverb]")
{
    const auto y = impulse(0.5f, 0.5f, 9600);
    CHECK(y.l != y.r);
}

TEST_CASE("reverb stays stable at maximum decay", "[reverb]")
{
    SpringReverb rv;
    rv.prepare(kSr);
    rv.setParams(1.0f, 1.0f);
    const auto x = dgtest::noise(48000 * 30, 0.5f);
    float peak = 0.0f;
    bool finite = true;
    for (float v : x)
    {
        float l = 0.0f, r = 0.0f;
        rv.process(v, -v, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(finite);
    CHECK(peak < 10.0f);
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_SpringReverb.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/SpringReverb.h'`.

- [ ] **Step 3: Implementieren**

`engine/SpringReverb.h`:

```cpp
#pragma once
#include <array>
#include <cstddef>
#include <vector>

namespace dg {

class SpringReverb
{
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(float decay, float tone);
    // Liefert nur das Wet-Signal.
    void process(float inL, float inR, float& wetL, float& wetR);

private:
    struct Line
    {
        std::vector<float> buf;
        std::size_t idx = 0;
        void init(int length);
        void clear();
    };

    struct Allpass
    {
        Line line;
        float g = 0.5f;
        float process(float x);
    };

    struct Comb
    {
        Line line;
        float store = 0.0f;
        float process(float x, float feedback, float damp);
    };

    struct Channel
    {
        std::array<Allpass, 8> dispersion;
        std::array<Comb, 4> combs;
        std::array<Allpass, 2> diffusers;
    };

    float processChannel(Channel& c, float x);

    std::array<Channel, 2> channels_;
    float feedback_ = 0.8f;
    float damp_ = 0.3f;
};

} // namespace dg
```

`engine/SpringReverb.cpp`:

```cpp
#include "engine/SpringReverb.h"
#include <algorithm>
#include <cmath>

namespace dg {

void SpringReverb::Line::init(int length)
{
    buf.assign(static_cast<std::size_t>(std::max(1, length)), 0.0f);
    idx = 0;
}

void SpringReverb::Line::clear()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
    idx = 0;
}

float SpringReverb::Allpass::process(float x)
{
    const float b = line.buf[line.idx];
    const float y = -x + b;
    line.buf[line.idx] = x + b * g;
    if (++line.idx >= line.buf.size())
        line.idx = 0;
    return y;
}

float SpringReverb::Comb::process(float x, float feedback, float damp)
{
    const float y = line.buf[line.idx];
    store = y * (1.0f - damp) + store * damp;
    line.buf[line.idx] = x + store * feedback;
    if (++line.idx >= line.buf.size())
        line.idx = 0;
    return y;
}

void SpringReverb::prepare(double sampleRate)
{
    const double scale = sampleRate / 44100.0;
    static constexpr int kDispersion[8] = { 3, 5, 7, 11, 13, 17, 19, 23 };
    static constexpr double kCombMs[4] = { 29.7, 37.1, 41.1, 43.7 };
    static constexpr double kDiffuserMs[2] = { 5.0, 1.7 };

    for (std::size_t ch = 0; ch < channels_.size(); ++ch)
    {
        auto& c = channels_[ch];
        const double spreadMs = ch == 0 ? 0.0 : 0.53; // rechter Kanal leicht versetzt
        for (std::size_t i = 0; i < c.dispersion.size(); ++i)
        {
            c.dispersion[i].line.init(static_cast<int>(std::lround(kDispersion[i] * scale)) + static_cast<int>(ch));
            c.dispersion[i].g = 0.6f;
        }
        for (std::size_t i = 0; i < c.combs.size(); ++i)
            c.combs[i].line.init(static_cast<int>(std::lround((kCombMs[i] + spreadMs) * 0.001 * sampleRate)));
        for (std::size_t i = 0; i < c.diffusers.size(); ++i)
        {
            c.diffusers[i].line.init(static_cast<int>(std::lround((kDiffuserMs[i] + spreadMs * 0.5) * 0.001 * sampleRate)));
            c.diffusers[i].g = 0.5f;
        }
    }
    reset();
}

void SpringReverb::reset()
{
    for (auto& c : channels_)
    {
        for (auto& a : c.dispersion)
            a.line.clear();
        for (auto& cb : c.combs)
        {
            cb.line.clear();
            cb.store = 0.0f;
        }
        for (auto& a : c.diffusers)
            a.line.clear();
    }
}

void SpringReverb::setParams(float decay, float tone)
{
    feedback_ = 0.70f + 0.27f * std::clamp(decay, 0.0f, 1.0f);
    damp_ = 0.05f + 0.6f * (1.0f - std::clamp(tone, 0.0f, 1.0f));
}

float SpringReverb::processChannel(Channel& c, float x)
{
    float d = x;
    for (auto& a : c.dispersion)
        d = a.process(d);
    float s = 0.0f;
    for (auto& cb : c.combs)
        s += cb.process(d * 0.08f, feedback_, damp_); // Eingangspegel so gewählt, dass Wet ≈ Dry-Pegel
    for (auto& a : c.diffusers)
        s = a.process(s);
    return s;
}

void SpringReverb::process(float inL, float inR, float& wetL, float& wetR)
{
    wetL = processChannel(channels_[0], inL);
    wetR = processChannel(channels_[1], inR);
}

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE SpringReverb.h SpringReverb.cpp)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): spring reverb"
```

---

### Task 11: Effektkette

**Files:**
- Create: `engine/FxChain.h`, `engine/FxChain.cpp`, `tests/test_FxChain.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `driveSample` (Task 8), `SvFilter` (Task 8), `Limiter`, `kLimiterCeiling` (Task 8), `TapeDelay`, `FxParams`, `delayDivisionSeconds` (Task 9), `SpringReverb` (Task 10), `volumeDbToGain` (Task 1).
- Produces: `class FxChain { void prepare(double sampleRate); void reset(); void process(float* mainL, float* mainR, float* sendL, float* sendR, int numSamples, const FxParams& p, double bpm); }`
  - Eingänge: `main` = Stimmenanteil `(1 − send)`, `send` = Stimmenanteil `send`. Beide Busse gehen durch Drive und Filter (getrennte Filterzustände). Dann `s1 = send + delayMix·delay(send)`, `s2 = s1 + reverbMix·reverb(s1)`, `out = (main + s2)·master` → Limiter. Das Ergebnis steht danach in `mainL/mainR`; `send` wird überschrieben.
  - NaN/Inf im Ausgang → alle Effektzustände zurücksetzen und für dieses Sample 0 ausgeben.
  - Cutoff wird pro Block im log-Bereich mit ~20 ms geglättet.

- [ ] **Step 1: Failing test `tests/test_FxChain.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <limits>
#include <vector>
#include "engine/FxChain.h"
#include "engine/Limiter.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

FxParams neutral()
{
    FxParams p;
    p.drive = 0.0f;
    p.cutoffHz = 20000.0f;
    p.resonance = 0.1f;
    p.filterType = 0.0f;
    p.delayMix = 0.0f;
    p.reverbMix = 0.0f;
    p.masterDb = 0.0f;
    return p;
}

struct Buses
{
    std::vector<float> ml, mr, sl, sr;
    explicit Buses(int n)
        : ml(static_cast<std::size_t>(n)), mr(static_cast<std::size_t>(n)),
          sl(static_cast<std::size_t>(n)), sr(static_cast<std::size_t>(n)) {}
};

void run(FxChain& fx, Buses& b, const FxParams& p, int block = 512)
{
    const int n = static_cast<int>(b.ml.size());
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min(block, n - pos);
        fx.process(b.ml.data() + pos, b.mr.data() + pos, b.sl.data() + pos, b.sr.data() + pos, len, p, 120.0);
    }
}
} // namespace

TEST_CASE("neutral settings pass the main bus through", "[fxchain]")
{
    FxChain fx;
    fx.prepare(kSr);
    Buses b(48000);
    b.ml = dgtest::sine(440.0f, kSr, 48000, 0.5f);
    b.mr = b.ml;
    run(fx, b, neutral());
    CHECK_THAT(dgtest::peakAbs(b.ml, 24000), WithinAbs(0.5, 0.01));
}

TEST_CASE("only the send bus reaches the delay", "[fxchain]")
{
    auto p = neutral();
    p.delayMix = 1.0f;
    p.delayFeedback = 0.0f;
    p.delayTone = 1.0f;
    p.delayWow = 0.0f;
    p.delayDiv = DelayDivision::D1_4; // 0,5 s bei 120 bpm

    FxChain fx;
    fx.prepare(kSr);
    Buses viaSend(48000);
    viaSend.sl[0] = viaSend.sr[0] = 1.0f;
    run(fx, viaSend, p);
    CHECK(dgtest::peakAbs(viaSend.ml, 23990, 24200) > 0.3f);

    FxChain fx2;
    fx2.prepare(kSr);
    Buses viaMain(48000);
    viaMain.ml[0] = viaMain.mr[0] = 1.0f;
    run(fx2, viaMain, p);
    CHECK(dgtest::peakAbs(viaMain.ml, 23990, 24200) < 1.0e-3f);
}

TEST_CASE("extreme settings stay finite and below the ceiling", "[fxchain]")
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
    p.masterDb = 6.0f;

    FxChain fx;
    fx.prepare(kSr);
    Buses b(48000 * 30);
    b.ml = dgtest::noise(48000 * 30, 0.5f, 3);
    b.mr = dgtest::noise(48000 * 30, 0.5f, 4);
    b.sl = dgtest::noise(48000 * 30, 0.5f, 5);
    b.sr = dgtest::noise(48000 * 30, 0.5f, 6);
    run(fx, b, p);
    CHECK(dgtest::allFinite(b.ml));
    CHECK(dgtest::allFinite(b.mr));
    CHECK(dgtest::peakAbs(b.ml) <= kLimiterCeiling + 1.0e-6f);
    CHECK(dgtest::peakAbs(b.mr) <= kLimiterCeiling + 1.0e-6f);
}

TEST_CASE("NaN input is contained and processing recovers", "[fxchain]")
{
    FxChain fx;
    fx.prepare(kSr);
    Buses b(9600);
    b.ml = dgtest::sine(440.0f, kSr, 9600, 0.5f);
    b.mr = b.ml;
    b.ml[100] = std::numeric_limits<float>::quiet_NaN();
    run(fx, b, neutral());
    CHECK(dgtest::allFinite(b.ml));
    CHECK(dgtest::peakAbs(b.ml, 4800) > 0.4f);
}

TEST_CASE("master at -60 dB is silent", "[fxchain]")
{
    auto p = neutral();
    p.masterDb = -60.0f;
    FxChain fx;
    fx.prepare(kSr);
    Buses b(4800);
    b.ml = dgtest::sine(440.0f, kSr, 4800, 0.5f);
    run(fx, b, p);
    CHECK(dgtest::peakAbs(b.ml) == 0.0f);
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_FxChain.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/FxChain.h'`.

- [ ] **Step 3: Implementieren**

`engine/FxChain.h`:

```cpp
#pragma once
#include "engine/FxParams.h"
#include "engine/Limiter.h"
#include "engine/SpringReverb.h"
#include "engine/SvFilter.h"
#include "engine/TapeDelay.h"

namespace dg {

class FxChain
{
public:
    void prepare(double sampleRate);
    void reset();
    // Ergebnis steht danach in mainL/mainR; sendL/sendR werden überschrieben.
    void process(float* mainL, float* mainR, float* sendL, float* sendR, int numSamples,
                 const FxParams& p, double bpm);

private:
    double sampleRate_ = 44100.0;
    SvFilter filterMain_;
    SvFilter filterSend_;
    TapeDelay delay_;
    SpringReverb reverb_;
    Limiter limiter_;
    float cutoff_ = 20000.0f;
    bool cutoffInit_ = false;
};

} // namespace dg
```

`engine/FxChain.cpp`:

```cpp
#include "engine/FxChain.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"
#include "engine/Drive.h"

namespace dg {

void FxChain::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    filterMain_.prepare(sampleRate);
    filterSend_.prepare(sampleRate);
    delay_.prepare(sampleRate);
    reverb_.prepare(sampleRate);
    limiter_.prepare(sampleRate);
    reset();
}

void FxChain::reset()
{
    filterMain_.reset();
    filterSend_.reset();
    delay_.reset();
    reverb_.reset();
    limiter_.reset();
    cutoffInit_ = false;
}

void FxChain::process(float* mainL, float* mainR, float* sendL, float* sendR, int numSamples,
                      const FxParams& p, double bpm)
{
    const float target = std::clamp(p.cutoffHz, 20.0f, 20000.0f);
    if (!cutoffInit_)
    {
        cutoff_ = target;
        cutoffInit_ = true;
    }
    const float a = 1.0f - std::exp(-static_cast<float>(numSamples) / (0.02f * static_cast<float>(sampleRate_)));
    cutoff_ = std::exp(std::log(cutoff_) + a * (std::log(target) - std::log(cutoff_)));

    filterMain_.setParams(cutoff_, p.resonance, p.filterType);
    filterSend_.setParams(cutoff_, p.resonance, p.filterType);
    delay_.setParams(static_cast<float>(delayDivisionSeconds(p.delayDiv, bpm)), p.delayFeedback, p.delayTone, p.delayWow);
    reverb_.setParams(p.reverbDecay, p.reverbTone);
    const float master = volumeDbToGain(p.masterDb);

    for (int i = 0; i < numSamples; ++i)
    {
        const float ml = filterMain_.process(driveSample(mainL[i], p.drive), 0);
        const float mr = filterMain_.process(driveSample(mainR[i], p.drive), 1);
        float sl = filterSend_.process(driveSample(sendL[i], p.drive), 0);
        float sr = filterSend_.process(driveSample(sendR[i], p.drive), 1);

        float dl = 0.0f, dr = 0.0f;
        delay_.process(sl, sr, dl, dr);
        sl += p.delayMix * dl;
        sr += p.delayMix * dr;

        float rl = 0.0f, rr = 0.0f;
        reverb_.process(sl, sr, rl, rr);
        sl += p.reverbMix * rl;
        sr += p.reverbMix * rr;

        float outL = (ml + sl) * master;
        float outR = (mr + sr) * master;
        if (!std::isfinite(outL) || !std::isfinite(outR))
        {
            reset();
            outL = outR = 0.0f;
        }
        limiter_.process(outL, outR);
        mainL[i] = outL;
        mainR[i] = outR;
        sendL[i] = sl;
        sendR[i] = sr;
    }
}

} // namespace dg
```

Hinweis: Nach `reset()` innerhalb der Schleife sind `cutoffInit_` und der Delay-Snap zurückgesetzt; der nächste Block setzt sie wieder. Das ist gewollt.

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE FxChain.h FxChain.cpp)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): dub fx chain with send routing and limiter"
```

---

### Task 12: Kit-Struktur und Werks-Kit

**Files:**
- Create: `engine/Kit.h`, `engine/Kit.cpp`, `tests/test_Kit.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SlotParams`, `kNumSlots` (Task 2), `SlotFields` (Task 2, nur im Test).
- Produces: `struct Kit { std::array<SlotParams, kNumSlots> slots; std::array<std::string, kNumSlots> names; }` (Namen UTF-8), `Kit makeFactoryKit()`.

- [ ] **Step 1: Failing test `tests/test_Kit.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>
#include "engine/Kit.h"
#include "engine/SlotFields.h"

using namespace dg;

TEST_CASE("factory kit has 16 unique non-empty names", "[kit]")
{
    const Kit k = makeFactoryKit();
    std::set<std::string> names;
    for (const auto& n : k.names)
    {
        CHECK_FALSE(n.empty());
        names.insert(n);
    }
    CHECK(names.size() == 16);
    CHECK(k.names[0] == "Classic");
    CHECK(k.names[15] == "Drop");
}

TEST_CASE("factory kit values are inside the parameter ranges", "[kit]")
{
    const Kit k = makeFactoryKit();
    for (const auto& p : k.slots)
    {
        for (int i = 0; i < kNumSlotFields; ++i)
        {
            const auto f = static_cast<SlotField>(i);
            SlotParams copy = p;
            setSlotField(copy, f, getSlotField(p, f));
            REQUIRE(copy == p);
        }
    }
}

TEST_CASE("factory kit trigger modes and choke groups follow the spec", "[kit]")
{
    const Kit k = makeFactoryKit();
    const std::set<int> oneShots { 3, 4, 5, 8, 9, 15 };
    const std::set<int> latches { 1, 6, 7, 12 };
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto& p = k.slots[static_cast<std::size_t>(s)];
        if (oneShots.count(s))
        {
            CHECK(p.trigMode == TriggerMode::OneShot);
            CHECK(p.chokeGroup == 1);
        }
        else
        {
            CHECK(p.chokeGroup == 0);
            CHECK(p.trigMode == (latches.count(s) ? TriggerMode::Latch : TriggerMode::Gate));
        }
    }
    CHECK(k.slots[3].sweepSemis > 0.0f);  // Laser fällt von oben
    CHECK(k.slots[4].sweepSemis < 0.0f);  // Riser steigt
    CHECK(k.slots[5].sweepSemis > 0.0f);  // Faller fällt
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_Kit.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/Kit.h'`.

- [ ] **Step 3: Implementieren**

`engine/Kit.h`:

```cpp
#pragma once
#include <array>
#include <string>
#include "engine/SlotParams.h"

namespace dg {

struct Kit
{
    std::array<SlotParams, kNumSlots> slots {};
    std::array<std::string, kNumSlots> names {}; // UTF-8
};

Kit makeFactoryKit();

} // namespace dg
```

`engine/Kit.cpp`:

```cpp
#include "engine/Kit.h"

namespace dg {

namespace {

struct Preset
{
    const char* name;
    Waveform wave;
    float pitchHz;
    LfoShape shape;
    float rateHz;
    float depthSemis;
    float sweepSemis;
    float sweepTimeS;
    float attackS;
    float releaseS;
    TriggerMode mode;
    float oneShotS;
    int choke;
    float volumeDb;
    float send;
};

using W = Waveform;
using L = LfoShape;
using T = TriggerMode;

// Sweep > 0: startet höher und fällt auf die Grundtonhöhe; Sweep < 0: startet tiefer und steigt.
constexpr Preset kPresets[kNumSlots] = {
    { "Classic",     W::Square,   600.0f,  L::Square,     4.0f,  7.0f,  0.0f,  0.5f,  0.005f, 0.4f,  T::Gate,    1.0f,  0, -9.0f,  0.4f },
    { "Wail",        W::Square,   500.0f,  L::Triangle,   0.5f, 12.0f,  0.0f,  0.5f,  0.05f,  1.0f,  T::Latch,   1.0f,  0, -9.0f,  0.5f },
    { "Trill",       W::Square,   900.0f,  L::Square,    12.0f,  3.0f,  0.0f,  0.5f,  0.005f, 0.2f,  T::Gate,    1.0f,  0, -10.0f, 0.3f },
    { "Laser",       W::Saw,      250.0f,  L::Square,     1.0f,  0.0f, 36.0f,  0.25f, 0.001f, 0.05f, T::OneShot, 0.3f,  1, -10.0f, 0.5f },
    { "Riser",       W::Saw,     1200.0f,  L::Square,     1.0f,  0.0f, -24.0f, 4.0f,  0.5f,   0.5f,  T::OneShot, 4.0f,  1, -10.0f, 0.4f },
    { "Faller",      W::Saw,      300.0f,  L::Square,     1.0f,  0.0f, 24.0f,  4.0f,  0.01f,  0.5f,  T::OneShot, 4.0f,  1, -10.0f, 0.4f },
    { "Alarm",       W::Square,   700.0f,  L::Square,     2.0f,  7.0f,  0.0f,  0.5f,  0.005f, 0.3f,  T::Latch,   1.0f,  0, -10.0f, 0.3f },
    { "UFO",         W::Sine,     800.0f,  L::SampleHold, 8.0f, 24.0f,  0.0f,  0.5f,  0.02f,  0.6f,  T::Latch,   1.0f,  0, -8.0f,  0.5f },
    { "Bleep",       W::Sine,    1500.0f,  L::Square,     1.0f,  0.0f,  0.0f,  0.5f,  0.001f, 0.05f, T::OneShot, 0.12f, 1, -8.0f,  0.6f },
    { "Zap",         W::Saw,      190.0f,  L::Square,     1.0f,  0.0f, 48.0f,  0.08f, 0.001f, 0.03f, T::OneShot, 0.1f,  1, -10.0f, 0.5f },
    { "Siren Up",    W::Square,   400.0f,  L::SawUp,      1.5f, 12.0f,  0.0f,  0.5f,  0.005f, 0.4f,  T::Gate,    1.0f,  0, -10.0f, 0.4f },
    { "Siren Down",  W::Square,   400.0f,  L::SawDown,    1.5f, 12.0f,  0.0f,  0.5f,  0.005f, 0.4f,  T::Gate,    1.0f,  0, -10.0f, 0.4f },
    { "Deep Wobble", W::Triangle,  80.0f,  L::Triangle,   0.8f, 12.0f,  0.0f,  0.5f,  0.05f,  0.8f,  T::Latch,   1.0f,  0, -3.0f,  0.2f },
    { "Chirp",       W::Square,  2500.0f,  L::SawUp,     10.0f, 12.0f,  0.0f,  0.5f,  0.005f, 0.2f,  T::Gate,    1.0f,  0, -12.0f, 0.4f },
    { "Horn",        W::Saw,      220.0f,  L::Square,     1.0f,  0.0f,  5.0f,  0.15f, 0.01f,  0.2f,  T::Gate,    1.0f,  0, -9.0f,  0.3f },
    { "Drop",        W::Sine,     100.0f,  L::Square,     1.0f,  0.0f, 48.0f,  3.0f,  0.005f, 0.5f,  T::OneShot, 3.0f,  1, -6.0f,  0.4f },
};

} // namespace

Kit makeFactoryKit()
{
    Kit k;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const Preset& pr = kPresets[s];
        SlotParams p;
        p.wave = pr.wave;
        p.pitchHz = pr.pitchHz;
        p.lfoShape = pr.shape;
        p.lfoRateHz = pr.rateHz;
        p.lfoDepthSemis = pr.depthSemis;
        p.sweepSemis = pr.sweepSemis;
        p.sweepTimeS = pr.sweepTimeS;
        p.attackS = pr.attackS;
        p.releaseS = pr.releaseS;
        p.trigMode = pr.mode;
        p.oneShotS = pr.oneShotS;
        p.chokeGroup = pr.choke;
        p.volumeDb = pr.volumeDb;
        p.fxSend = pr.send;
        k.slots[static_cast<std::size_t>(s)] = p;
        k.names[static_cast<std::size_t>(s)] = pr.name;
    }
    return k;
}

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE Kit.h Kit.cpp)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): kit structure and factory kit"
```

---

### Task 13: Engine

**Files:**
- Create: `engine/Engine.h`, `engine/Engine.cpp`, `tests/test_Engine.cpp`
- Modify: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SirenVoice`, `PerfOffsets`, `VoiceContext` (Task 6), `PadRouter`, `VoiceControl`, `TriggerSettingsArray`, `LatchStopAction` (Task 7), `FxChain` (Task 11), `FxParams` (Task 9), `kLimiterCeiling` (Task 8), `makeFactoryKit` (Task 12, Test), `volumeDbToGain`, `kPi` (Task 1).
- Produces:
  - `enum class PerfTarget { Focus, All }`
  - `struct GlobalParams { FxParams fx; PerfOffsets perf; PerfTarget perfTarget = Focus; LatchStopAction latchStop = Release; }`
  - `struct EngineParams { std::array<SlotParams, kNumSlots> slots; GlobalParams global; }`
  - `struct TransportInfo { bool isPlaying = false; double bpm = 120.0; }`
  - `struct EngineEvent { enum class Type { NoteOn, NoteOff, PreviewOn, PreviewOff, Panic }; Type type; int sampleOffset; int value; }` – `value` ist die Note (NoteOn/Off) bzw. der Slot (Preview).
  - `class Engine { void prepare(double sampleRate, int maxBlockSize); void process(float* outL, float* outR, int numSamples, const EngineParams&, const EngineEvent* events, int numEvents, const TransportInfo&); std::uint32_t activeMask() const; std::uint32_t latchedMask() const; int focusSlot() const; const PerfOffsets& appliedPerf(int slot) const; }`
  - `process` **überschreibt** `outL/outR`. Events müssen nach `sampleOffset` sortiert sein; Offsets ≥ `numSamples` werden am Blockende ausgeführt. Blöcke größer als `maxBlockSize` werden intern zerlegt. `activeMask/latchedMask/focusSlot` sind thread-sicher lesbar (Atomics).
  - Pan: Equal-Power, Mitte = 0,707 pro Kanal. Send-Aufteilung: `main += v·(1 − send)`, `send += v·send`.

- [ ] **Step 1: Failing test `tests/test_Engine.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include "engine/Engine.h"
#include "engine/Kit.h"
#include "engine/Limiter.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

EngineParams testParams()
{
    EngineParams p;
    for (auto& s : p.slots)
    {
        s.wave = Waveform::Sine;
        s.pitchHz = 440.0f;
        s.lfoDepthSemis = 0.0f;
        s.sweepSemis = 0.0f;
        s.attackS = 0.0f;
        s.releaseS = 0.01f;
        s.volumeDb = 0.0f;
        s.pan = 0.0f;
        s.fxSend = 0.0f;
        s.trigMode = TriggerMode::Gate;
        s.chokeGroup = 0;
    }
    p.global.fx.drive = 0.0f;
    p.global.fx.cutoffHz = 20000.0f;
    p.global.fx.delayMix = 0.0f;
    p.global.fx.reverbMix = 0.0f;
    p.global.fx.masterDb = 0.0f;
    return p;
}

struct Out { std::vector<float> l, r; };

Out run(Engine& e, const EngineParams& p, int n, std::vector<EngineEvent> ev = {}, TransportInfo t = {})
{
    Out o { std::vector<float>(static_cast<std::size_t>(n)), std::vector<float>(static_cast<std::size_t>(n)) };
    e.process(o.l.data(), o.r.data(), n, p, ev.data(), static_cast<int>(ev.size()), t);
    return o;
}

EngineEvent noteOn(int note, int offset = 0) { return { EngineEvent::Type::NoteOn, offset, note }; }
EngineEvent noteOff(int note, int offset = 0) { return { EngineEvent::Type::NoteOff, offset, note }; }
EngineEvent panicEvent() { return { EngineEvent::Type::Panic, 0, 0 }; }
} // namespace

TEST_CASE("engine is silent without events", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    const auto o = run(e, testParams(), 4800);
    CHECK(dgtest::peakAbs(o.l) == 0.0f);
    CHECK(e.activeMask() == 0u);
}

TEST_CASE("note 36 plays slot 1 at its pitch with centre pan", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    const auto p = testParams();
    const auto o = run(e, p, 48000, { noteOn(36) });
    CHECK_THAT(dgtest::estimateFrequency(o.l, kSr, 4800, o.l.size()), WithinAbs(440.0, 3.0));
    CHECK(dgtest::peakAbs(o.l, 4800) > 0.6f);
    CHECK(dgtest::peakAbs(o.l, 4800) < 0.8f);
    CHECK_THAT(dgtest::peakAbs(o.r, 4800), WithinAbs(dgtest::peakAbs(o.l, 4800), 1e-3));
    CHECK(e.activeMask() == 1u);
    CHECK(e.focusSlot() == 0);
}

TEST_CASE("notes outside the pad range are ignored", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    const auto o = run(e, testParams(), 4800, { noteOn(35), noteOn(52) });
    CHECK(dgtest::peakAbs(o.l) == 0.0f);
}

TEST_CASE("events are sample accurate", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    const auto o = run(e, testParams(), 512, { noteOn(36, 100) });
    CHECK(dgtest::peakAbs(o.l, 0, 100) == 0.0f);
    CHECK(dgtest::peakAbs(o.l, 101) > 0.0f);
}

TEST_CASE("gate note off releases the voice", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    const auto p = testParams();
    run(e, p, 4800, { noteOn(36) });
    run(e, p, 4800, { noteOff(36) });
    CHECK(e.activeMask() == 0u);
    const auto o = run(e, p, 4800);
    CHECK(dgtest::peakAbs(o.l) < 1.0e-4f);
}

TEST_CASE("panic silences latched voices within 5 ms", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[0].trigMode = TriggerMode::Latch;
    run(e, p, 4800, { noteOn(36) });
    CHECK(e.latchedMask() == 1u);
    const auto o = run(e, p, 4800, { panicEvent() });
    CHECK(e.activeMask() == 0u);
    CHECK(e.latchedMask() == 0u);
    CHECK(dgtest::peakAbs(o.l, 300) < 1.0e-4f);
}

TEST_CASE("choke fades the previous voice without a click", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[0].pitchHz = 200.0f;
    p.slots[0].chokeGroup = 1;
    p.slots[1].chokeGroup = 1;
    p.slots[1].volumeDb = -60.0f; // stumm, damit nur der Fade von Slot 1 zu hören ist
    run(e, p, 4800, { noteOn(36) });
    const auto o = run(e, p, 4800, { noteOn(37) });
    CHECK(dgtest::maxStep(o.l, 1, 400) <= 0.03f);
    CHECK(dgtest::peakAbs(o.l, 300) < 1.0e-4f);
    CHECK(e.activeMask() == 2u);
}

TEST_CASE("transport stop handles latched voices according to the setting", "[engine]")
{
    auto p = testParams();
    p.slots[0].trigMode = TriggerMode::Latch;
    const TransportInfo playing { true, 120.0 };
    const TransportInfo stopped { false, 120.0 };

    SECTION("continue")
    {
        Engine e;
        e.prepare(kSr, 512);
        p.global.latchStop = LatchStopAction::Continue;
        run(e, p, 512, { noteOn(36) }, playing);
        run(e, p, 4800, {}, stopped);
        CHECK(e.activeMask() == 1u);
    }
    SECTION("release")
    {
        Engine e;
        e.prepare(kSr, 512);
        p.global.latchStop = LatchStopAction::Release;
        run(e, p, 512, { noteOn(36) }, playing);
        run(e, p, 300, {}, stopped);
        CHECK(e.activeMask() == 1u); // Release (10 ms) läuft noch
        run(e, p, 300, {}, stopped);
        CHECK(e.activeMask() == 0u);
    }
    SECTION("stop")
    {
        Engine e;
        e.prepare(kSr, 512);
        p.global.latchStop = LatchStopAction::Stop;
        run(e, p, 512, { noteOn(36) }, playing);
        run(e, p, 300, {}, stopped);
        CHECK(e.activeMask() == 0u);
    }
    SECTION("no action without a playing to stopped transition")
    {
        Engine e;
        e.prepare(kSr, 512);
        p.global.latchStop = LatchStopAction::Stop;
        run(e, p, 512, { noteOn(36) }, stopped);
        run(e, p, 512, {}, stopped);
        CHECK(e.activeMask() == 1u);
    }
}

TEST_CASE("performance offsets follow the focus target", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[0].trigMode = TriggerMode::Latch;
    p.slots[1].trigMode = TriggerMode::Latch;
    run(e, p, 64, { noteOn(36), noteOn(37) }); // Fokus = Slot 2 (Index 1)

    p.global.perf.pitchSemis = 12.0f;
    p.global.perfTarget = PerfTarget::Focus;
    run(e, p, 64);
    CHECK(e.appliedPerf(1).pitchSemis == 12.0f);
    CHECK(e.appliedPerf(0).pitchSemis == 0.0f);

    p.global.perfTarget = PerfTarget::All;
    run(e, p, 64);
    CHECK(e.appliedPerf(0).pitchSemis == 12.0f);

    p.global.perfTarget = PerfTarget::Focus;
    p.global.perf.pitchSemis = 5.0f;
    run(e, p, 64);
    CHECK(e.appliedPerf(0).pitchSemis == 12.0f); // behält seinen letzten Offset
    CHECK(e.appliedPerf(1).pitchSemis == 5.0f);
}

TEST_CASE("blocks larger than maxBlockSize are processed correctly", "[engine]")
{
    Engine e;
    e.prepare(kSr, 64);
    const auto o = run(e, testParams(), 1000, { noteOn(36, 900) });
    CHECK(dgtest::peakAbs(o.l, 0, 900) == 0.0f);
    CHECK(dgtest::peakAbs(o.l, 901) > 0.0f);
}

TEST_CASE("events beyond the block are applied at the end", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    run(e, testParams(), 512, { noteOn(36, 600) });
    CHECK(e.activeMask() == 1u);
}

TEST_CASE("preview events start and release a slot", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[4].trigMode = TriggerMode::Latch;
    run(e, p, 512, { { EngineEvent::Type::PreviewOn, 0, 4 } });
    CHECK(e.activeMask() == (1u << 4));
    CHECK(e.focusSlot() == 4);
    run(e, p, 2400, { { EngineEvent::Type::PreviewOff, 0, 4 } });
    CHECK(e.activeMask() == 0u);
}

TEST_CASE("the whole factory kit renders finite and below the ceiling", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    EngineParams p;
    const Kit k = makeFactoryKit();
    p.slots = k.slots;
    std::vector<EngineEvent> ev;
    for (int n = 36; n <= 51; ++n)
        ev.push_back(noteOn(n));

    std::vector<float> all;
    for (int block = 0; block < 188; ++block) // ~2 s
    {
        const auto o = run(e, p, 512, block == 0 ? ev : std::vector<EngineEvent> {});
        all.insert(all.end(), o.l.begin(), o.l.end());
    }
    CHECK(dgtest::allFinite(all));
    CHECK(dgtest::peakAbs(all) <= kLimiterCeiling + 1.0e-6f);
    CHECK(dgtest::rms(all) > 0.01f);
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenTests PRIVATE test_Engine.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'engine/Engine.h'`.

- [ ] **Step 3: Implementieren**

`engine/Engine.h`:

```cpp
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>
#include "engine/FxChain.h"
#include "engine/FxParams.h"
#include "engine/PadRouter.h"
#include "engine/SirenVoice.h"

namespace dg {

enum class PerfTarget { Focus, All };

struct GlobalParams
{
    FxParams fx {};
    PerfOffsets perf {};
    PerfTarget perfTarget = PerfTarget::Focus;
    LatchStopAction latchStop = LatchStopAction::Release;
};

struct EngineParams
{
    std::array<SlotParams, kNumSlots> slots {};
    GlobalParams global {};
};

struct TransportInfo
{
    bool isPlaying = false;
    double bpm = 120.0;
};

struct EngineEvent
{
    enum class Type { NoteOn, NoteOff, PreviewOn, PreviewOff, Panic };
    Type type = Type::NoteOn;
    int sampleOffset = 0;
    int value = 0; // Note (NoteOn/NoteOff) oder Slot (Preview)
};

class Engine
{
public:
    void prepare(double sampleRate, int maxBlockSize);

    // Überschreibt outL/outR. events müssen nach sampleOffset sortiert sein.
    void process(float* outL, float* outR, int numSamples, const EngineParams& params,
                 const EngineEvent* events, int numEvents, const TransportInfo& transport);

    std::uint32_t activeMask() const { return activeMask_.load(std::memory_order_relaxed); }
    std::uint32_t latchedMask() const { return latchedMask_.load(std::memory_order_relaxed); }
    int focusSlot() const { return focus_.load(std::memory_order_relaxed); }
    // Nur im Audio-Thread bzw. in Tests lesen.
    const PerfOffsets& appliedPerf(int slot) const { return applied_[static_cast<std::size_t>(slot)]; }

private:
    class Bank final : public VoiceControl
    {
    public:
        explicit Bank(Engine& e) : e_(e) {}
        void startVoice(int slot) override;
        void releaseVoice(int slot) override;
        void killVoice(int slot) override;
        bool isVoiceActive(int slot) const override;
        bool isVoiceReleasing(int slot) const override;

    private:
        Engine& e_;
    };

    void processChunk(float* outL, float* outR, int n, const EngineEvent* events, int numEvents, int base);
    void handleEvent(const EngineEvent& ev);
    void renderSegment(int start, int len);
    VoiceContext contextFor(int slot) const;

    double sampleRate_ = 44100.0;
    int maxBlock_ = 512;
    std::array<SirenVoice, kNumSlots> voices_ {};
    std::array<PerfOffsets, kNumSlots> applied_ {};
    PadRouter router_;
    Bank bank_ { *this };
    FxChain fx_;
    std::vector<float> mainL_, mainR_, sendL_, sendR_, voiceBuf_;

    const EngineParams* params_ = nullptr;
    TriggerSettingsArray trig_ {};
    double bpm_ = 120.0;
    bool wasPlaying_ = false;
    std::uint32_t seed_ = 1;

    std::atomic<std::uint32_t> activeMask_ { 0 };
    std::atomic<std::uint32_t> latchedMask_ { 0 };
    std::atomic<int> focus_ { 0 };
};

} // namespace dg
```

`engine/Engine.cpp`:

```cpp
#include "engine/Engine.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

void Engine::Bank::startVoice(int slot)
{
    const auto s = static_cast<std::size_t>(slot);
    e_.applied_[s] = e_.params_->global.perf; // der gestartete Slot wird Fokus
    e_.voices_[s].start(e_.contextFor(slot), e_.seed_++);
}

void Engine::Bank::releaseVoice(int slot)
{
    e_.voices_[static_cast<std::size_t>(slot)].release(e_.contextFor(slot));
}

void Engine::Bank::killVoice(int slot) { e_.voices_[static_cast<std::size_t>(slot)].kill(); }

bool Engine::Bank::isVoiceActive(int slot) const { return e_.voices_[static_cast<std::size_t>(slot)].isActive(); }

bool Engine::Bank::isVoiceReleasing(int slot) const
{
    return e_.voices_[static_cast<std::size_t>(slot)].isReleasing();
}

void Engine::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    maxBlock_ = std::max(1, maxBlockSize);
    for (auto& v : voices_)
        v.prepare(sampleRate);
    applied_.fill(PerfOffsets {});
    router_.prepare(sampleRate);
    fx_.prepare(sampleRate);
    for (auto* b : { &mainL_, &mainR_, &sendL_, &sendR_, &voiceBuf_ })
        b->assign(static_cast<std::size_t>(maxBlock_), 0.0f);
    wasPlaying_ = false;
    activeMask_.store(0);
    latchedMask_.store(0);
    focus_.store(0);
}

VoiceContext Engine::contextFor(int slot) const
{
    const auto s = static_cast<std::size_t>(slot);
    return VoiceContext { &params_->slots[s], bpm_, applied_[s] };
}

void Engine::process(float* outL, float* outR, int numSamples, const EngineParams& params,
                     const EngineEvent* events, int numEvents, const TransportInfo& transport)
{
    params_ = &params;
    bpm_ = transport.bpm > 0.0 ? transport.bpm : 120.0;
    for (std::size_t s = 0; s < trig_.size(); ++s)
        trig_[s] = { params.slots[s].trigMode, params.slots[s].oneShotS, params.slots[s].chokeGroup };

    if (wasPlaying_ && !transport.isPlaying)
        router_.transportStopped(params.global.latchStop, bank_);
    wasPlaying_ = transport.isPlaying;

    int ev = 0;
    for (int done = 0; done < numSamples;)
    {
        const int n = std::min(maxBlock_, numSamples - done);
        const int first = ev;
        while (ev < numEvents && events[ev].sampleOffset < done + n)
            ++ev;
        processChunk(outL + done, outR + done, n, events + first, ev - first, done);
        done += n;
    }
    for (; ev < numEvents; ++ev)
        handleEvent(events[ev]);

    std::uint32_t active = 0;
    for (int s = 0; s < kNumSlots; ++s)
        if (voices_[static_cast<std::size_t>(s)].isActive())
            active |= 1u << s;
    activeMask_.store(active, std::memory_order_relaxed);
    latchedMask_.store(router_.latchedMask(), std::memory_order_relaxed);
    focus_.store(router_.focusSlot(), std::memory_order_relaxed);
}

void Engine::processChunk(float* outL, float* outR, int n, const EngineEvent* events, int numEvents, int base)
{
    const auto len = static_cast<std::size_t>(n);
    std::fill_n(mainL_.begin(), len, 0.0f);
    std::fill_n(mainR_.begin(), len, 0.0f);
    std::fill_n(sendL_.begin(), len, 0.0f);
    std::fill_n(sendR_.begin(), len, 0.0f);

    int pos = 0;
    for (int i = 0; i < numEvents; ++i)
    {
        const int offset = std::clamp(events[i].sampleOffset - base, 0, n);
        if (offset > pos)
        {
            renderSegment(pos, offset - pos);
            pos = offset;
        }
        handleEvent(events[i]);
    }
    if (pos < n)
        renderSegment(pos, n - pos);

    fx_.process(mainL_.data(), mainR_.data(), sendL_.data(), sendR_.data(), n, params_->global.fx, bpm_);
    std::copy_n(mainL_.begin(), len, outL);
    std::copy_n(mainR_.begin(), len, outR);
}

void Engine::handleEvent(const EngineEvent& ev)
{
    switch (ev.type)
    {
        case EngineEvent::Type::NoteOn:     router_.noteOn(ev.value, trig_, bank_); break;
        case EngineEvent::Type::NoteOff:    router_.noteOff(ev.value, bank_); break;
        case EngineEvent::Type::PreviewOn:  router_.previewOn(ev.value, trig_, bank_); break;
        case EngineEvent::Type::PreviewOff: router_.previewOff(ev.value, bank_); break;
        case EngineEvent::Type::Panic:      router_.panic(bank_); break;
    }
}

void Engine::renderSegment(int start, int len)
{
    if (len <= 0)
        return;
    const GlobalParams& g = params_->global;
    const int focus = router_.focusSlot();

    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        const auto s = static_cast<std::size_t>(slot);
        SirenVoice& voice = voices_[s];
        if (!voice.isActive())
        {
            applied_[s] = PerfOffsets {};
            continue;
        }
        if (g.perfTarget == PerfTarget::All || slot == focus)
            applied_[s] = g.perf;

        voice.render(voiceBuf_.data(), len, contextFor(slot));

        const SlotParams& sp = params_->slots[s];
        const float gain = volumeDbToGain(sp.volumeDb);
        const float angle = (std::clamp(sp.pan, -1.0f, 1.0f) + 1.0f) * kPi * 0.25f;
        const float gl = std::cos(angle) * gain;
        const float gr = std::sin(angle) * gain;
        const float send = std::clamp(sp.fxSend, 0.0f, 1.0f);
        const float dry = 1.0f - send;

        for (int i = 0; i < len; ++i)
        {
            const float v = voiceBuf_[static_cast<std::size_t>(i)];
            const auto o = static_cast<std::size_t>(start + i);
            mainL_[o] += v * gl * dry;
            mainR_[o] += v * gr * dry;
            sendL_[o] += v * gl * send;
            sendR_[o] += v * gr * send;
        }
    }
    router_.advance(len, bank_);
}

} // namespace dg
```

In `engine/CMakeLists.txt` anhängen: `target_sources(dg_engine PRIVATE Engine.h Engine.cpp)`

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add engine tests
git commit -m "feat(engine): engine with event splitting, focus offsets and fx"
```

---

### Task 14: Plugin-Hülle – Parameter, Processor, State (VST3 baut)

**Files:**
- Create: `plugin/CMakeLists.txt`, `plugin/ParameterLayout.h`, `plugin/ParameterLayout.cpp`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `tests/plugin/test_PluginProcessor.cpp`
- Modify: `CMakeLists.txt` (Root), `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Engine`, `EngineParams`, `EngineEvent`, `TransportInfo`, `PerfTarget` (Task 13), `SlotFields` (Task 2), `Kit`, `makeFactoryKit` (Task 12), `FxParams`, `kNumDelayDivisions` (Task 9), `LatchStopAction` (Task 7).
- Produces:
  - Namespace `dg::pid` mit den 20 globalen Parameter-IDs: `drive, fltCutoff, fltRes, fltType, dlyTime, dlyFeedback, dlyTone, dlyWow, dlyMix, revDecay, revTone, revMix, masterVol, perfPitch, perfRate, perfDepth, perfSweep, perfTarget, latchStop, panic`.
  - `juce::String slotParamId(int slot, SlotField f)` → z. B. `"s01_wave"`.
  - `createParameterLayout(const Kit& defaults)` – globale Parameter zuerst (für die Ableton-Liste), dann 16 × 18 Slot-Parameter mit Namen `"S01 Welle"` usw.; Slot-Defaults = Werte aus `defaults`.
  - `SlotParams readSlotFromParameters(juce::AudioProcessorValueTreeState&, int slot)`, `void writeSlotToParameters(juce::AudioProcessorValueTreeState&, int slot, const SlotParams&)` (nur Message-Thread).
  - `class ParamCache { explicit ParamCache(APVTS&); void read(EngineParams&) const; bool panicPressed() const; }` (Audio-Thread).
  - `class DubgefahrenProcessor` mit Message-Thread-API: `state()`, `slotName(int)`, `setSlotName(int, const juce::String&)`, `setSlot(int, const SlotParams&, const juce::String&)`, `currentKit()`, `applyKit(const Kit&)`, `previewPress(int)`, `previewRelease(int)`, `activeMask()`, `latchedMask()`, `focusSlot()`, `uiScale()`, `setUiScale(float)`, `editorFollowsFocus()`, `setEditorFollowsFocus(bool)`.
  - CMake: `dg_plugin_shared` (INTERFACE, alle Plugin-Quellen + JUCE-Module), Plugin-Target `Dubgefahren` / `Dubgefahren_VST3`, Test-Target `DubgefahrenPluginTests`.

- [ ] **Step 1: CMake für Plugin und Plugin-Tests anlegen**

Im Root-`CMakeLists.txt` die Unterprojekte ersetzen durch:

```cmake
enable_testing()
add_subdirectory(engine)
add_subdirectory(plugin)
add_subdirectory(tests)
```

`plugin/CMakeLists.txt`:

```cmake
add_library(dg_plugin_shared INTERFACE)
target_sources(dg_plugin_shared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ParameterLayout.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ParameterLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp)
target_include_directories(dg_plugin_shared INTERFACE ${PROJECT_SOURCE_DIR})
target_compile_definitions(dg_plugin_shared INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(dg_plugin_shared INTERFACE
    dg_engine
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

juce_add_plugin(Dubgefahren
    COMPANY_NAME "atze187"
    BUNDLE_ID "de.atze187.dubgefahren"
    PLUGIN_MANUFACTURER_CODE Atze
    PLUGIN_CODE Dgfn
    FORMATS VST3
    PRODUCT_NAME "Dubgefahren"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)
target_link_libraries(Dubgefahren PRIVATE dg_plugin_shared)
```

In `tests/CMakeLists.txt` anhängen:

```cmake
juce_add_console_app(DubgefahrenPluginTests PRODUCT_NAME "DubgefahrenPluginTests")
target_sources(DubgefahrenPluginTests PRIVATE plugin/test_PluginProcessor.cpp)
target_link_libraries(DubgefahrenPluginTests PRIVATE dg_plugin_shared Catch2::Catch2WithMain)
catch_discover_tests(DubgefahrenPluginTests)
```

- [ ] **Step 2: Failing test `tests/plugin/test_PluginProcessor.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <set>
#include "engine/Kit.h"
#include "engine/SlotFields.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
bool approxEqual(const SlotParams& a, const SlotParams& b)
{
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        const float va = getSlotField(a, f);
        const float vb = getSlotField(b, f);
        if (std::abs(va - vb) > 1.0e-3f * std::max(1.0f, std::abs(va)))
            return false;
    }
    return true;
}

void setParam(DubgefahrenProcessor& p, const juce::String& id, float realValue)
{
    auto* param = p.state().getParameter(id);
    REQUIRE(param != nullptr);
    param->setValueNotifyingHost(param->convertTo0to1(realValue));
}

void processBlocks(DubgefahrenProcessor& p, int blocks, juce::MidiBuffer firstMidi = {})
{
    juce::AudioBuffer<float> buf(2, 512);
    for (int i = 0; i < blocks; ++i)
    {
        buf.clear();
        juce::MidiBuffer midi;
        if (i == 0)
            midi = firstMidi;
        p.processBlock(buf, midi);
    }
}

void prepare(DubgefahrenProcessor& p)
{
    p.setPlayConfigDetails(0, 2, 48000.0, 512);
    p.prepareToPlay(48000.0, 512);
}
} // namespace

TEST_CASE("the plugin exposes 308 uniquely named parameters", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    std::set<juce::String> ids;
    for (auto* param : p.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            ids.insert(withId->paramID);
    CHECK(p.getParameters().size() == 16 * 18 + 20);
    CHECK(ids.size() == 308);
    CHECK(slotParamId(0, SlotField::Wave) == "s01_wave");
    CHECK(slotParamId(15, SlotField::FxSend) == "s16_send");
}

TEST_CASE("slot parameters and names default to the factory kit", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    const Kit k = makeFactoryKit();
    for (int s = 0; s < kNumSlots; ++s)
    {
        CHECK(approxEqual(readSlotFromParameters(p.state(), s), k.slots[static_cast<std::size_t>(s)]));
        CHECK(p.slotName(s) == juce::String::fromUTF8(k.names[static_cast<std::size_t>(s)].c_str()));
    }
}

TEST_CASE("state round-trip keeps parameters, names with umlauts and UI settings", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a;
    a.state().getParameter("s03_pitch")->setValueNotifyingHost(0.25f);
    a.state().getParameter(pid::delayMix)->setValueNotifyingHost(0.8f);
    a.setSlotName(2, juce::String::fromUTF8("Größe äöü"));
    a.setUiScale(1.5f);
    a.setEditorFollowsFocus(false);

    juce::MemoryBlock mb;
    a.getStateInformation(mb);

    DubgefahrenProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    CHECK_THAT(b.state().getParameter("s03_pitch")->getValue(), WithinAbs(0.25, 1e-4));
    CHECK_THAT(b.state().getParameter(pid::delayMix)->getValue(), WithinAbs(0.8, 1e-4));
    CHECK(b.slotName(2) == juce::String::fromUTF8("Größe äöü"));
    CHECK(b.slotName(0) == "Classic");
    CHECK_THAT(b.uiScale(), WithinAbs(1.5, 1e-6));
    CHECK_FALSE(b.editorFollowsFocus());
}

TEST_CASE("invalid state data is ignored", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.setStateInformation("garbage", 7);
    CHECK(p.slotName(0) == "Classic");
    CHECK(approxEqual(readSlotFromParameters(p.state(), 0), makeFactoryKit().slots[0]));
}

TEST_CASE("applyKit and currentKit round-trip", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    Kit k = makeFactoryKit();
    k.slots[0].pitchHz = 1234.0f;
    k.slots[7].trigMode = TriggerMode::OneShot;
    k.names[0] = "Neu";
    p.applyKit(k);
    const Kit c = p.currentKit();
    CHECK(approxEqual(c.slots[0], k.slots[0]));
    CHECK(c.slots[7].trigMode == TriggerMode::OneShot);
    CHECK(c.names[0] == "Neu");
}

TEST_CASE("processBlock plays note 36 and treats velocity 0 as note off", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    prepare(p);

    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    p.processBlock(buf, on);
    CHECK(buf.getMagnitude(0, 0, 512) > 0.0f);
    CHECK(p.activeMask() == 1u);

    juce::MidiBuffer off;
    off.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(0)), 0);
    processBlocks(p, 60, off); // Classic: Release 0,4 s
    CHECK(p.activeMask() == 0u);
}

TEST_CASE("panic parameter stops latched voices on its rising edge", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    prepare(p);
    setParam(p, slotParamId(0, SlotField::TrigMode), 1.0f); // Latch

    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    processBlocks(p, 1, on);
    CHECK(p.latchedMask() == 1u);

    setParam(p, pid::panic, 1.0f);
    processBlocks(p, 2);
    CHECK(p.activeMask() == 0u);
    CHECK(p.latchedMask() == 0u);
    setParam(p, pid::panic, 0.0f);
}

TEST_CASE("UI preview starts a slot and moves the focus", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    prepare(p);
    p.previewPress(4);
    processBlocks(p, 1);
    CHECK((p.activeMask() & (1u << 4)) != 0u);
    CHECK(p.focusSlot() == 4);
    p.previewRelease(4);
    processBlocks(p, 1);
}
```

- [ ] **Step 3: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure; powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – CMake meldet fehlende Quelldateien `plugin/ParameterLayout.cpp` bzw. der Compiler `cannot open include file 'plugin/ParameterLayout.h'`.

- [ ] **Step 4: `plugin/ParameterLayout.h` / `.cpp` implementieren**

`plugin/ParameterLayout.h`:

```cpp
#pragma once
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Kit.h"
#include "engine/SlotFields.h"

namespace dg::pid {
inline constexpr const char* drive = "drive";
inline constexpr const char* cutoff = "fltCutoff";
inline constexpr const char* resonance = "fltRes";
inline constexpr const char* filterType = "fltType";
inline constexpr const char* delayTime = "dlyTime";
inline constexpr const char* delayFeedback = "dlyFeedback";
inline constexpr const char* delayTone = "dlyTone";
inline constexpr const char* delayWow = "dlyWow";
inline constexpr const char* delayMix = "dlyMix";
inline constexpr const char* reverbDecay = "revDecay";
inline constexpr const char* reverbTone = "revTone";
inline constexpr const char* reverbMix = "revMix";
inline constexpr const char* masterVol = "masterVol";
inline constexpr const char* perfPitch = "perfPitch";
inline constexpr const char* perfRate = "perfRate";
inline constexpr const char* perfDepth = "perfDepth";
inline constexpr const char* perfSweep = "perfSweep";
inline constexpr const char* perfTarget = "perfTarget";
inline constexpr const char* latchStop = "latchStop";
inline constexpr const char* panic = "panic";
} // namespace dg::pid

namespace dg {

constexpr int kParameterVersion = 1;

juce::String slotParamId(int slot, SlotField f);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout(const Kit& defaults);

// Nur im Message-Thread aufrufen.
SlotParams readSlotFromParameters(juce::AudioProcessorValueTreeState& apvts, int slot);
void writeSlotToParameters(juce::AudioProcessorValueTreeState& apvts, int slot, const SlotParams& p);

// Hält die rohen Parameterzeiger für den Audio-Thread.
class ParamCache
{
public:
    explicit ParamCache(juce::AudioProcessorValueTreeState& apvts);
    void read(EngineParams& out) const;
    bool panicPressed() const;

private:
    using Ptr = std::atomic<float>*;
    std::array<std::array<Ptr, kNumSlotFields>, kNumSlots> slots_ {};
    Ptr drive_, cutoff_, resonance_, filterType_;
    Ptr delayTime_, delayFeedback_, delayTone_, delayWow_, delayMix_;
    Ptr reverbDecay_, reverbTone_, reverbMix_, masterVol_;
    Ptr perfPitch_, perfRate_, perfDepth_, perfSweep_, perfTarget_, latchStop_, panic_;
};

} // namespace dg
```

`plugin/ParameterLayout.cpp`:

```cpp
#include "plugin/ParameterLayout.h"
#include <algorithm>
#include <cmath>

namespace dg {

namespace {

juce::NormalisableRange<float> rangeFor(float min, float max, float skewCentre)
{
    juce::NormalisableRange<float> r(min, max);
    if (skewCentre > min && skewCentre < max)
        r.setSkewForCentre(skewCentre);
    return r;
}

juce::StringArray toStringArray(std::span<const char* const> items)
{
    juce::StringArray a;
    for (const char* c : items)
        a.add(juce::String::fromUTF8(c));
    return a;
}

std::unique_ptr<juce::RangedAudioParameter> makeFloat(const juce::String& id, const juce::String& name,
                                                      float min, float max, float def, float skewCentre,
                                                      const juce::String& unit)
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, kParameterVersion }, name, rangeFor(min, max, skewCentre), def,
        juce::AudioParameterFloatAttributes().withLabel(unit));
}

std::unique_ptr<juce::RangedAudioParameter> makeChoice(const juce::String& id, const juce::String& name,
                                                       const juce::StringArray& choices, int def)
{
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id, kParameterVersion }, name, choices, def);
}

juce::String u8(const char* s) { return juce::String::fromUTF8(s); }

std::atomic<float>* raw(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    auto* p = apvts.getRawParameterValue(id);
    jassert(p != nullptr);
    return p;
}

float load(const std::atomic<float>* p) { return p->load(std::memory_order_relaxed); }

int loadIndex(const std::atomic<float>* p, int maxIndex)
{
    return std::clamp(static_cast<int>(std::lround(load(p))), 0, maxIndex);
}

} // namespace

juce::String slotParamId(int slot, SlotField f)
{
    return juce::String::formatted("s%02d_", slot + 1) + fieldSpec(f).key;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout(const Kit& defaults)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const FxParams fx {};

    layout.add(makeFloat(pid::drive, "Drive", 0.0f, 1.0f, fx.drive, 0.0f, ""));
    layout.add(makeFloat(pid::cutoff, "Filter Cutoff", 20.0f, 20000.0f, fx.cutoffHz, 1000.0f, "Hz"));
    layout.add(makeFloat(pid::resonance, "Filter Resonanz", 0.0f, 1.0f, fx.resonance, 0.0f, ""));
    layout.add(makeFloat(pid::filterType, "Filter Typ", 0.0f, 1.0f, fx.filterType, 0.0f, ""));
    layout.add(makeChoice(pid::delayTime, "Delay Zeit",
                          { "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1" },
                          static_cast<int>(fx.delayDiv)));
    layout.add(makeFloat(pid::delayFeedback, "Delay Feedback", 0.0f, 1.1f, fx.delayFeedback, 0.0f, ""));
    layout.add(makeFloat(pid::delayTone, "Delay Tone", 0.0f, 1.0f, fx.delayTone, 0.0f, ""));
    layout.add(makeFloat(pid::delayWow, "Delay Wow", 0.0f, 1.0f, fx.delayWow, 0.0f, ""));
    layout.add(makeFloat(pid::delayMix, "Delay Mix", 0.0f, 1.0f, fx.delayMix, 0.0f, ""));
    layout.add(makeFloat(pid::reverbDecay, "Hall Decay", 0.0f, 1.0f, fx.reverbDecay, 0.0f, ""));
    layout.add(makeFloat(pid::reverbTone, "Hall Tone", 0.0f, 1.0f, fx.reverbTone, 0.0f, ""));
    layout.add(makeFloat(pid::reverbMix, "Hall Mix", 0.0f, 1.0f, fx.reverbMix, 0.0f, ""));
    layout.add(makeFloat(pid::masterVol, "Master", -60.0f, 6.0f, fx.masterDb, 0.0f, "dB"));

    layout.add(makeFloat(pid::perfPitch, "Perf Pitch", -24.0f, 24.0f, 0.0f, 0.0f, "st"));
    layout.add(makeFloat(pid::perfRate, "Perf Rate", 0.25f, 4.0f, 1.0f, 1.0f, "x"));
    layout.add(makeFloat(pid::perfDepth, "Perf Tiefe", -24.0f, 24.0f, 0.0f, 0.0f, "st"));
    layout.add(makeFloat(pid::perfSweep, "Perf Sweep", -24.0f, 24.0f, 0.0f, 0.0f, "st"));
    layout.add(makeChoice(pid::perfTarget, "Perf Ziel", { "Fokus", "Alle" }, 0));
    layout.add(makeChoice(pid::latchStop, "Latch bei Stopp", { "Weiterlaufen", "Ausklingen", "Sofort stoppen" }, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { pid::panic, kParameterVersion }, "Panic", false));

    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto prefix = juce::String::formatted("S%02d ", s + 1);
        for (int i = 0; i < kNumSlotFields; ++i)
        {
            const auto f = static_cast<SlotField>(i);
            const auto& spec = fieldSpec(f);
            const auto id = slotParamId(s, f);
            const auto name = prefix + u8(spec.name);
            const float def = getSlotField(defaults.slots[static_cast<std::size_t>(s)], f);
            switch (spec.kind)
            {
                case FieldKind::Float:
                    layout.add(makeFloat(id, name, spec.min, spec.max, def, spec.skewCentre, u8(spec.unit)));
                    break;
                case FieldKind::Choice:
                    layout.add(makeChoice(id, name, toStringArray(spec.choices), static_cast<int>(std::lround(def))));
                    break;
                case FieldKind::Bool:
                    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, kParameterVersion }, name, def > 0.5f));
                    break;
            }
        }
    }
    return layout;
}

SlotParams readSlotFromParameters(juce::AudioProcessorValueTreeState& apvts, int slot)
{
    SlotParams p;
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        setSlotField(p, f, raw(apvts, slotParamId(slot, f))->load());
    }
    return p;
}

void writeSlotToParameters(juce::AudioProcessorValueTreeState& apvts, int slot, const SlotParams& p)
{
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        auto* param = apvts.getParameter(slotParamId(slot, f));
        jassert(param != nullptr);
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(getSlotField(p, f)));
        param->endChangeGesture();
    }
}

ParamCache::ParamCache(juce::AudioProcessorValueTreeState& apvts)
    : drive_(raw(apvts, pid::drive)), cutoff_(raw(apvts, pid::cutoff)), resonance_(raw(apvts, pid::resonance)),
      filterType_(raw(apvts, pid::filterType)), delayTime_(raw(apvts, pid::delayTime)),
      delayFeedback_(raw(apvts, pid::delayFeedback)), delayTone_(raw(apvts, pid::delayTone)),
      delayWow_(raw(apvts, pid::delayWow)), delayMix_(raw(apvts, pid::delayMix)),
      reverbDecay_(raw(apvts, pid::reverbDecay)), reverbTone_(raw(apvts, pid::reverbTone)),
      reverbMix_(raw(apvts, pid::reverbMix)), masterVol_(raw(apvts, pid::masterVol)),
      perfPitch_(raw(apvts, pid::perfPitch)), perfRate_(raw(apvts, pid::perfRate)),
      perfDepth_(raw(apvts, pid::perfDepth)), perfSweep_(raw(apvts, pid::perfSweep)),
      perfTarget_(raw(apvts, pid::perfTarget)), latchStop_(raw(apvts, pid::latchStop)),
      panic_(raw(apvts, pid::panic))
{
    for (int s = 0; s < kNumSlots; ++s)
        for (int i = 0; i < kNumSlotFields; ++i)
            slots_[static_cast<std::size_t>(s)][static_cast<std::size_t>(i)] =
                raw(apvts, slotParamId(s, static_cast<SlotField>(i)));
}

void ParamCache::read(EngineParams& out) const
{
    for (std::size_t s = 0; s < slots_.size(); ++s)
        for (std::size_t i = 0; i < slots_[s].size(); ++i)
            setSlotField(out.slots[s], static_cast<SlotField>(i), load(slots_[s][i]));

    FxParams& fx = out.global.fx;
    fx.drive = load(drive_);
    fx.cutoffHz = load(cutoff_);
    fx.resonance = load(resonance_);
    fx.filterType = load(filterType_);
    fx.delayDiv = static_cast<DelayDivision>(loadIndex(delayTime_, kNumDelayDivisions - 1));
    fx.delayFeedback = load(delayFeedback_);
    fx.delayTone = load(delayTone_);
    fx.delayWow = load(delayWow_);
    fx.delayMix = load(delayMix_);
    fx.reverbDecay = load(reverbDecay_);
    fx.reverbTone = load(reverbTone_);
    fx.reverbMix = load(reverbMix_);
    fx.masterDb = load(masterVol_);

    out.global.perf = PerfOffsets { load(perfPitch_), load(perfRate_), load(perfDepth_), load(perfSweep_) };
    out.global.perfTarget = loadIndex(perfTarget_, 1) == 1 ? PerfTarget::All : PerfTarget::Focus;
    out.global.latchStop = static_cast<LatchStopAction>(loadIndex(latchStop_, 2));
}

bool ParamCache::panicPressed() const { return load(panic_) > 0.5f; }

} // namespace dg
```

- [ ] **Step 5: `plugin/PluginProcessor.h` / `.cpp` implementieren**

`plugin/PluginProcessor.h`:

```cpp
#pragma once
#include <array>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Kit.h"
#include "plugin/ParameterLayout.h"

namespace dg {

class DubgefahrenProcessor final : public juce::AudioProcessor
{
public:
    DubgefahrenProcessor();
    ~DubgefahrenProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Dubgefahren"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- Message-Thread-API (Editor, Tests) ---
    juce::AudioProcessorValueTreeState& state() { return apvts_; }
    juce::String slotName(int slot) const;
    void setSlotName(int slot, const juce::String& name);
    void setSlot(int slot, const SlotParams& params, const juce::String& name);
    Kit currentKit();
    void applyKit(const Kit& kit);
    void previewPress(int slot);
    void previewRelease(int slot);
    std::uint32_t activeMask() const { return engine_.activeMask(); }
    std::uint32_t latchedMask() const { return engine_.latchedMask(); }
    int focusSlot() const { return engine_.focusSlot(); }
    float uiScale() const;
    void setUiScale(float scale);
    bool editorFollowsFocus() const;
    void setEditorFollowsFocus(bool follow);

private:
    static constexpr int kStateVersion = 1;
    static constexpr int kUiFifoSize = 128;

    void pushUiEvent(EngineEvent::Type type, int slot);
    void ensureStateChildren();
    juce::ValueTree namesTree() const;

    juce::AudioProcessorValueTreeState apvts_;
    ParamCache cache_;
    Engine engine_;
    EngineParams engineParams_;
    std::vector<EngineEvent> events_;
    juce::AbstractFifo uiFifo_ { kUiFifoSize };
    std::array<EngineEvent, kUiFifoSize> uiEvents_ {};
    bool lastPanic_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DubgefahrenProcessor)
};

} // namespace dg
```

`plugin/PluginProcessor.cpp`:

```cpp
#include "plugin/PluginProcessor.h"
#include <algorithm>

namespace dg {

namespace {
const juce::Identifier kNamesId { "SLOTNAMES" };
const juce::Identifier kUiScaleId { "uiScale" };
const juce::Identifier kFollowFocusId { "followFocus" };
const juce::Identifier kVersionId { "version" };

juce::Identifier nameKey(int slot) { return juce::Identifier("n" + juce::String(slot + 1)); }
} // namespace

DubgefahrenProcessor::DubgefahrenProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "DUBGEFAHREN", createParameterLayout(makeFactoryKit())),
      cache_(apvts_)
{
    events_.reserve(2048);
    ensureStateChildren();
    engine_.prepare(44100.0, 512); // gültiger Zustand schon vor prepareToPlay
}

void DubgefahrenProcessor::ensureStateChildren()
{
    auto names = apvts_.state.getOrCreateChildWithName(kNamesId, nullptr);
    const Kit factory = makeFactoryKit();
    for (int s = 0; s < kNumSlots; ++s)
        if (!names.hasProperty(nameKey(s)))
            names.setProperty(nameKey(s), juce::String::fromUTF8(factory.names[static_cast<std::size_t>(s)].c_str()), nullptr);
    if (!apvts_.state.hasProperty(kUiScaleId))
        apvts_.state.setProperty(kUiScaleId, 1.0f, nullptr);
    if (!apvts_.state.hasProperty(kFollowFocusId))
        apvts_.state.setProperty(kFollowFocusId, true, nullptr);
}

juce::ValueTree DubgefahrenProcessor::namesTree() const { return apvts_.state.getChildWithName(kNamesId); }

void DubgefahrenProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepare(sampleRate, samplesPerBlock);
    lastPanic_ = false;
}

bool DubgefahrenProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void DubgefahrenProcessor::pushUiEvent(EngineEvent::Type type, int slot)
{
    const auto scope = uiFifo_.write(1);
    if (scope.blockSize1 > 0)
        uiEvents_[static_cast<std::size_t>(scope.startIndex1)] = EngineEvent { type, 0, slot };
}

void DubgefahrenProcessor::previewPress(int slot) { pushUiEvent(EngineEvent::Type::PreviewOn, slot); }
void DubgefahrenProcessor::previewRelease(int slot) { pushUiEvent(EngineEvent::Type::PreviewOff, slot); }

void DubgefahrenProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)
    {
        buffer.clear();
        return;
    }

    cache_.read(engineParams_);
    events_.clear();
    const auto push = [this](const EngineEvent& e) {
        if (events_.size() < events_.capacity())
            events_.push_back(e);
    };

    {
        const auto scope = uiFifo_.read(uiFifo_.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i)
            push(uiEvents_[static_cast<std::size_t>(scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i)
            push(uiEvents_[static_cast<std::size_t>(scope.startIndex2 + i)]);
    }

    const bool panic = cache_.panicPressed();
    if (panic && !lastPanic_)
        push(EngineEvent { EngineEvent::Type::Panic, 0, 0 });
    lastPanic_ = panic;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int offset = std::clamp(meta.samplePosition, 0, std::max(0, numSamples - 1));
        if (m.isNoteOn())
            push(EngineEvent { EngineEvent::Type::NoteOn, offset, m.getNoteNumber() });
        else if (m.isNoteOff()) // enthält Note-On mit Velocity 0
            push(EngineEvent { EngineEvent::Type::NoteOff, offset, m.getNoteNumber() });
    }

    TransportInfo transport;
    if (auto* playHead = getPlayHead())
    {
        if (const auto pos = playHead->getPosition())
        {
            transport.isPlaying = pos->getIsPlaying();
            if (const auto bpm = pos->getBpm())
                transport.bpm = *bpm;
        }
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
    engine_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples, engineParams_,
                    events_.data(), static_cast<int>(events_.size()), transport);
    midi.clear();
}

juce::AudioProcessorEditor* DubgefahrenProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this); // wird in Task 16 ersetzt
}

void DubgefahrenProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    state.setProperty(kVersionId, kStateVersion, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void DubgefahrenProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(apvts_.state.getType()))
        return;
    // Versionsfeld: ältere Stände werden hier bei Bedarf migriert (Version 1: nichts zu tun).
    apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    ensureStateChildren();
}

juce::String DubgefahrenProcessor::slotName(int slot) const
{
    return namesTree().getProperty(nameKey(slot)).toString();
}

void DubgefahrenProcessor::setSlotName(int slot, const juce::String& name)
{
    auto names = apvts_.state.getOrCreateChildWithName(kNamesId, nullptr);
    names.setProperty(nameKey(slot), name.substring(0, 32), nullptr);
}

void DubgefahrenProcessor::setSlot(int slot, const SlotParams& params, const juce::String& name)
{
    writeSlotToParameters(apvts_, slot, params);
    setSlotName(slot, name);
}

Kit DubgefahrenProcessor::currentKit()
{
    Kit k;
    for (int s = 0; s < kNumSlots; ++s)
    {
        k.slots[static_cast<std::size_t>(s)] = readSlotFromParameters(apvts_, s);
        k.names[static_cast<std::size_t>(s)] = slotName(s).toStdString();
    }
    return k;
}

void DubgefahrenProcessor::applyKit(const Kit& kit)
{
    for (int s = 0; s < kNumSlots; ++s)
        setSlot(s, kit.slots[static_cast<std::size_t>(s)],
                juce::String::fromUTF8(kit.names[static_cast<std::size_t>(s)].c_str()));
}

float DubgefahrenProcessor::uiScale() const
{
    return static_cast<float>(apvts_.state.getProperty(kUiScaleId, 1.0f));
}

void DubgefahrenProcessor::setUiScale(float scale)
{
    apvts_.state.setProperty(kUiScaleId, std::clamp(scale, 0.75f, 2.0f), nullptr);
}

bool DubgefahrenProcessor::editorFollowsFocus() const
{
    return static_cast<bool>(apvts_.state.getProperty(kFollowFocusId, true));
}

void DubgefahrenProcessor::setEditorFollowsFocus(bool follow)
{
    apvts_.state.setProperty(kFollowFocusId, follow, nullptr);
}

} // namespace dg

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dg::DubgefahrenProcessor(); }
```

- [ ] **Step 6: Build und Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS – Engine- und Plugin-Tests grün; es existiert `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3`.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt plugin tests
git commit -m "feat(plugin): JUCE processor with parameters, state and MIDI routing"
```

---

### Task 15: Kit-Dateien und Config

**Files:**
- Create: `plugin/KitFile.h`, `plugin/KitFile.cpp`, `plugin/Config.h`, `plugin/Config.cpp`, `tests/plugin/test_KitFile.cpp`, `tests/plugin/test_Config.cpp`
- Modify: `plugin/CMakeLists.txt`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Kit`, `makeFactoryKit` (Task 12), `SlotFields` (Task 2), `DubgefahrenProcessor` (Task 14).
- Produces:
  - `constexpr const char* kKitExtension = ".dgkit";`
  - `juce::String kitToJsonString(const Kit&)`, `struct KitParseResult { std::optional<Kit> kit; juce::String error; }`, `KitParseResult kitFromJsonString(const juce::String&)`, `bool saveKitFile(const Kit&, const juce::File&, juce::String& error)`, `KitParseResult loadKitFile(const juce::File&)`.
  - `struct KitFolderInfo { juce::File folder; juce::String warning; }`, `juce::String expandEnvironmentVariables(const juce::String&)`, `KitFolderInfo resolveKitFolder(const juce::File& configFile, const juce::File& defaultFolder)`, `juce::File defaultKitFolder()`, `juce::File pluginConfigFile()`.
  - `DubgefahrenProcessor::kitFolder() const` → `const KitFolderInfo&`, einmal im Konstruktor aufgelöst.
  - JSON-Format: `{"format":"dubgefahren-kit","version":1,"slots":[{"name":"…","params":{"wave":3,…}} ×16]}`. Regeln: falsches JSON/Format, Version > 1, ≠ 16 Slots, fehlender Name, fehlende `params`, nicht-numerischer Wert → Fehler mit deutscher Meldung. Fehlende Schlüssel → Default, unbekannte Schlüssel → ignoriert, Werte außerhalb des Bereichs → geklemmt. Dateien > 1 MB → Fehler.

- [ ] **Step 1: Failing tests schreiben**

`tests/plugin/test_KitFile.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include "engine/Kit.h"
#include "engine/SlotFields.h"
#include "plugin/KitFile.h"

using namespace dg;

namespace {
juce::var parsed(const Kit& k) { return juce::JSON::parse(kitToJsonString(k)); }

KitParseResult reparse(const juce::var& v) { return kitFromJsonString(juce::JSON::toString(v)); }

struct TempDir
{
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("dgkit", "", false);
    TempDir() { dir.createDirectory(); }
    ~TempDir() { dir.deleteRecursively(); }
};
} // namespace

TEST_CASE("factory kit survives a JSON round-trip", "[kitfile]")
{
    const Kit k = makeFactoryKit();
    const auto r = kitFromJsonString(kitToJsonString(k));
    REQUIRE(r.kit.has_value());
    CHECK(r.error.isEmpty());
    CHECK(r.kit->slots == k.slots);
    CHECK(r.kit->names == k.names);
}

TEST_CASE("names with umlauts survive save and load", "[kitfile]")
{
    TempDir tmp;
    Kit k = makeFactoryKit();
    k.names[3] = juce::String::fromUTF8("Größenwahn äöü").toStdString();
    const auto file = tmp.dir.getChildFile(juce::String("test") + kKitExtension);
    juce::String error;
    REQUIRE(saveKitFile(k, file, error));
    const auto r = loadKitFile(file);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->names[3] == k.names[3]);
}

TEST_CASE("invalid kit files are rejected with a message", "[kitfile]")
{
    const Kit k = makeFactoryKit();

    CHECK_FALSE(kitFromJsonString("{ not json").kit.has_value());

    auto wrongFormat = parsed(k);
    wrongFormat.getDynamicObject()->setProperty("format", "something-else");
    CHECK_FALSE(reparse(wrongFormat).kit.has_value());

    auto newer = parsed(k);
    newer.getDynamicObject()->setProperty("version", 2);
    const auto rNewer = reparse(newer);
    CHECK_FALSE(rNewer.kit.has_value());
    CHECK(rNewer.error.isNotEmpty());

    auto fewer = parsed(k);
    fewer["slots"].getArray()->removeLast();
    CHECK_FALSE(reparse(fewer).kit.has_value());

    auto notNumber = parsed(k);
    notNumber["slots"][0]["params"].getDynamicObject()->setProperty("pitch", "laut");
    CHECK_FALSE(reparse(notNumber).kit.has_value());

    auto noName = parsed(k);
    noName["slots"][2].getDynamicObject()->removeProperty("name");
    CHECK_FALSE(reparse(noName).kit.has_value());

    CHECK_FALSE(loadKitFile(juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("does_not_exist_dg.dgkit")).kit.has_value());
}

TEST_CASE("kit values are clamped, missing keys default and unknown keys are ignored", "[kitfile]")
{
    auto v = parsed(makeFactoryKit());
    auto* params = v["slots"][0]["params"].getDynamicObject();
    params->setProperty("pitch", 99999.0);
    params->removeProperty("pw");
    params->setProperty("futureThing", 1.0);
    const auto r = reparse(v);
    REQUIRE(r.kit.has_value());
    CHECK(r.kit->slots[0].pitchHz == fieldSpec(SlotField::Pitch).max);
    CHECK(r.kit->slots[0].pulseWidth == fieldSpec(SlotField::PulseWidth).def);
}
```

`tests/plugin/test_Config.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include "plugin/Config.h"

using namespace dg;

namespace {
struct TempDir
{
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("dgcfg", "", false);
    TempDir() { dir.createDirectory(); }
    ~TempDir() { dir.deleteRecursively(); }
};

void writeConfig(const juce::File& f, const juce::String& kitFolder)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("kitFolder", kitFolder);
    f.replaceWithText(juce::JSON::toString(juce::var(obj)));
}
} // namespace

TEST_CASE("missing config uses and creates the default folder", "[config]")
{
    TempDir tmp;
    const auto def = tmp.dir.getChildFile("Default");
    const auto info = resolveKitFolder(tmp.dir.getChildFile("none.json"), def);
    CHECK(info.folder == def);
    CHECK(info.warning.isEmpty());
    CHECK(def.isDirectory());
}

TEST_CASE("valid config folder is used", "[config]")
{
    TempDir tmp;
    const auto kits = tmp.dir.getChildFile("MyKits");
    kits.createDirectory();
    const auto cfg = tmp.dir.getChildFile("cfg.json");
    writeConfig(cfg, kits.getFullPathName());
    const auto info = resolveKitFolder(cfg, tmp.dir.getChildFile("Default"));
    CHECK(info.folder == kits);
    CHECK(info.warning.isEmpty());
}

TEST_CASE("empty kitFolder falls back without warning", "[config]")
{
    TempDir tmp;
    const auto cfg = tmp.dir.getChildFile("cfg.json");
    writeConfig(cfg, "");
    const auto def = tmp.dir.getChildFile("Default");
    const auto info = resolveKitFolder(cfg, def);
    CHECK(info.folder == def);
    CHECK(info.warning.isEmpty());
}

TEST_CASE("invalid config, missing folder and relative path fall back with a warning", "[config]")
{
    TempDir tmp;
    const auto def = tmp.dir.getChildFile("Default");
    const auto cfg = tmp.dir.getChildFile("cfg.json");

    cfg.replaceWithText("{ kaputt");
    auto info = resolveKitFolder(cfg, def);
    CHECK(info.folder == def);
    CHECK(info.warning.isNotEmpty());

    writeConfig(cfg, tmp.dir.getChildFile("gibtsnicht").getFullPathName());
    info = resolveKitFolder(cfg, def);
    CHECK(info.folder == def);
    CHECK(info.warning.isNotEmpty());

    writeConfig(cfg, "relativ\\ordner");
    info = resolveKitFolder(cfg, def);
    CHECK(info.folder == def);
    CHECK(info.warning.isNotEmpty());
}

TEST_CASE("environment variables are expanded", "[config]")
{
    const auto temp = juce::SystemStats::getEnvironmentVariable("TEMP", {});
    REQUIRE(temp.isNotEmpty());
    CHECK(expandEnvironmentVariables("%TEMP%\\x") == temp + "\\x");
    CHECK(expandEnvironmentVariables("%DG_DOES_NOT_EXIST_42%\\x") == "%DG_DOES_NOT_EXIST_42%\\x");
    CHECK(expandEnvironmentVariables("a%%b") == "a%%b");
    CHECK(expandEnvironmentVariables("kein prozent") == "kein prozent");

    TempDir tmp;
    const auto cfg = tmp.dir.getChildFile("cfg.json");
    const auto rel = tmp.dir.getFullPathName().fromFirstOccurrenceOf(temp, false, true);
    if (tmp.dir.getFullPathName().startsWithIgnoreCase(temp))
    {
        writeConfig(cfg, "%TEMP%" + rel);
        const auto info = resolveKitFolder(cfg, tmp.dir.getChildFile("Default"));
        CHECK(info.folder == tmp.dir);
        CHECK(info.warning.isEmpty());
    }
}
```

In `tests/CMakeLists.txt` anhängen:

```cmake
target_sources(DubgefahrenPluginTests PRIVATE plugin/test_KitFile.cpp plugin/test_Config.cpp)
```

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'plugin/KitFile.h'`.

- [ ] **Step 3: `plugin/KitFile.h` / `.cpp` implementieren**

`plugin/KitFile.h`:

```cpp
#pragma once
#include <optional>
#include <juce_core/juce_core.h>
#include "engine/Kit.h"

namespace dg {

constexpr const char* kKitExtension = ".dgkit";

struct KitParseResult
{
    std::optional<Kit> kit;
    juce::String error;
};

juce::String kitToJsonString(const Kit& kit);
KitParseResult kitFromJsonString(const juce::String& text);
bool saveKitFile(const Kit& kit, const juce::File& file, juce::String& error);
KitParseResult loadKitFile(const juce::File& file);

} // namespace dg
```

`plugin/KitFile.cpp`:

```cpp
#include "plugin/KitFile.h"
#include "engine/SlotFields.h"

namespace dg {

namespace {
constexpr const char* kFormat = "dubgefahren-kit";
constexpr int kVersion = 1;
constexpr juce::int64 kMaxFileBytes = 1024 * 1024;

KitParseResult fail(const juce::String& message) { return { std::nullopt, message }; }

bool isNumber(const juce::var& v) { return v.isDouble() || v.isInt() || v.isInt64() || v.isBool(); }

juce::String slotLabel(int s) { return "Slot " + juce::String(s + 1); }
} // namespace

juce::String kitToJsonString(const Kit& kit)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("format", kFormat);
    root->setProperty("version", kVersion);

    juce::Array<juce::var> slots;
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto* slot = new juce::DynamicObject();
        slot->setProperty("name", juce::String::fromUTF8(kit.names[static_cast<std::size_t>(s)].c_str()));
        auto* params = new juce::DynamicObject();
        for (int i = 0; i < kNumSlotFields; ++i)
        {
            const auto f = static_cast<SlotField>(i);
            params->setProperty(juce::Identifier(fieldSpec(f).key),
                                static_cast<double>(getSlotField(kit.slots[static_cast<std::size_t>(s)], f)));
        }
        slot->setProperty("params", juce::var(params));
        slots.add(juce::var(slot));
    }
    root->setProperty("slots", slots);
    return juce::JSON::toString(juce::var(root));
}

KitParseResult kitFromJsonString(const juce::String& text)
{
    juce::var root;
    if (juce::JSON::parse(text, root).failed() || !root.isObject())
        return fail("Die Datei ist kein gültiges JSON.");
    if (root["format"].toString() != kFormat)
        return fail("Die Datei ist kein Dubgefahren-Kit.");
    if (!isNumber(root["version"]))
        return fail("Die Kit-Version fehlt.");
    if (static_cast<int>(root["version"]) > kVersion)
        return fail("Das Kit stammt aus einer neueren Dubgefahren-Version.");

    const auto* slots = root["slots"].getArray();
    if (slots == nullptr || slots->size() != kNumSlots)
        return fail("Das Kit muss genau 16 Slots enthalten.");

    Kit kit;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const juce::var& slot = (*slots)[s];
        if (!slot.isObject())
            return fail(slotLabel(s) + " ist ungültig.");
        if (!slot["name"].isString())
            return fail(slotLabel(s) + " hat keinen Namen.");
        const auto* params = slot["params"].getDynamicObject();
        if (params == nullptr)
            return fail(slotLabel(s) + " hat keine Parameter.");

        SlotParams p = makeDefaultSlotParams();
        for (const auto& prop : params->getProperties())
        {
            const auto field = slotFieldFromKey(prop.name.toString().toStdString());
            if (!field)
                continue; // unbekannte Schlüssel (neuere Versionen) ignorieren
            if (!isNumber(prop.value))
                return fail(slotLabel(s) + ": Wert für \"" + prop.name.toString() + "\" ist keine Zahl.");
            setSlotField(p, *field, static_cast<float>(static_cast<double>(prop.value)));
        }
        kit.slots[static_cast<std::size_t>(s)] = p;
        kit.names[static_cast<std::size_t>(s)] = slot["name"].toString().substring(0, 32).toStdString();
    }
    return { kit, {} };
}

bool saveKitFile(const Kit& kit, const juce::File& file, juce::String& error)
{
    if (!file.getParentDirectory().createDirectory())
    {
        error = "Ordner konnte nicht angelegt werden: " + file.getParentDirectory().getFullPathName();
        return false;
    }
    if (!file.replaceWithText(kitToJsonString(kit), false, false, "\n"))
    {
        error = "Datei konnte nicht geschrieben werden: " + file.getFullPathName();
        return false;
    }
    return true;
}

KitParseResult loadKitFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return fail("Datei nicht gefunden: " + file.getFullPathName());
    if (file.getSize() > kMaxFileBytes)
        return fail("Die Datei ist zu groß für ein Kit.");
    return kitFromJsonString(file.loadFileAsString());
}

} // namespace dg
```

- [ ] **Step 4: `plugin/Config.h` / `.cpp` implementieren**

`plugin/Config.h`:

```cpp
#pragma once
#include <juce_core/juce_core.h>

namespace dg {

struct KitFolderInfo
{
    juce::File folder;
    juce::String warning; // leer, wenn alles in Ordnung ist
};

juce::String expandEnvironmentVariables(const juce::String& text);
KitFolderInfo resolveKitFolder(const juce::File& configFile, const juce::File& defaultFolder);
juce::File defaultKitFolder();
juce::File pluginConfigFile();

} // namespace dg
```

`plugin/Config.cpp`:

```cpp
#include "plugin/Config.h"

namespace dg {

juce::String expandEnvironmentVariables(const juce::String& text)
{
    juce::String out;
    int i = 0;
    while (i < text.length())
    {
        const int start = text.indexOfChar(i, '%');
        const int end = start < 0 ? -1 : text.indexOfChar(start + 1, '%');
        if (start < 0 || end < 0)
        {
            out << text.substring(i);
            break;
        }
        out << text.substring(i, start);
        const auto name = text.substring(start + 1, end);
        const auto value = name.isEmpty() ? juce::String() : juce::SystemStats::getEnvironmentVariable(name, {});
        out << (value.isNotEmpty() ? value : text.substring(start, end + 1));
        i = end + 1;
    }
    return out;
}

KitFolderInfo resolveKitFolder(const juce::File& configFile, const juce::File& defaultFolder)
{
    KitFolderInfo info { defaultFolder, {} };

    if (configFile.existsAsFile())
    {
        juce::var root;
        if (juce::JSON::parse(configFile.loadFileAsString(), root).failed() || !root.isObject())
        {
            info.warning = "Die Config-Datei ist ungültig und wird ignoriert: " + configFile.getFullPathName();
        }
        else
        {
            const auto raw = root["kitFolder"].toString().trim();
            if (raw.isNotEmpty())
            {
                const auto expanded = expandEnvironmentVariables(raw);
                if (!juce::File::isAbsolutePath(expanded))
                    info.warning = "kitFolder in der Config ist kein absoluter Pfad: " + expanded;
                else if (!juce::File(expanded).isDirectory())
                    info.warning = "Der Kit-Ordner aus der Config existiert nicht: " + expanded;
                else
                    info.folder = juce::File(expanded);
            }
        }
    }

    if (info.folder == defaultFolder && !defaultFolder.isDirectory())
        defaultFolder.createDirectory();
    return info;
}

juce::File defaultKitFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Dubgefahren")
        .getChildFile("Kits");
}

juce::File pluginConfigFile()
{
    // <Bundle>.vst3/Contents/x86_64-win/<Name>.vst3 → <Bundle>.vst3/Contents/Resources/...
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory()
        .getParentDirectory()
        .getChildFile("Resources")
        .getChildFile("Dubgefahren.config.json");
}

} // namespace dg
```

In `plugin/CMakeLists.txt` im `target_sources(dg_plugin_shared INTERFACE …)`-Block ergänzen:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/KitFile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/KitFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cpp
```

- [ ] **Step 5: Processor um den Kit-Ordner erweitern**

In `plugin/PluginProcessor.h`: `#include "plugin/Config.h"` ergänzen, in den öffentlichen Teil

```cpp
    const KitFolderInfo& kitFolder() const { return kitFolder_; }
```

und in den privaten Teil (nach `lastPanic_`)

```cpp
    KitFolderInfo kitFolder_;
```

In `plugin/PluginProcessor.cpp` im Konstruktor-Rumpf nach `ensureStateChildren();` einfügen:

```cpp
    kitFolder_ = resolveKitFolder(pluginConfigFile(), defaultKitFolder());
```

- [ ] **Step 6: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add plugin tests
git commit -m "feat(plugin): kit file import/export and configurable kit folder"
```

---

### Task 16: Oberfläche

**Files:**
- Create: `plugin/ui/DgLookAndFeel.h`, `plugin/ui/DgLookAndFeel.cpp`, `plugin/ui/Controls.h`, `plugin/ui/Controls.cpp`, `plugin/ui/PadGrid.h`, `plugin/ui/PadGrid.cpp`, `plugin/ui/SlotEditor.h`, `plugin/ui/SlotEditor.cpp`, `plugin/ui/FxPanel.h`, `plugin/ui/FxPanel.cpp`, `plugin/ui/PerformancePanel.h`, `plugin/ui/PerformancePanel.cpp`, `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`, `tests/plugin/test_Editor.cpp`
- Modify: `plugin/CMakeLists.txt`, `plugin/PluginProcessor.cpp` (`createEditor`), `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: gesamte Message-Thread-API von `DubgefahrenProcessor` (Task 14/15), `slotParamId`, `pid::*` (Task 14), `KitFile` (Task 15), `makeFactoryKit` (Task 12).
- Produces: `class DubgefahrenEditor : public juce::AudioProcessorEditor` mit Basisgröße 1000 × 640, skalierbar 75–200 % (Seitenverhältnis fest, Skalierung über `AffineTransform`, gespeichert via `setUiScale`).
- Abweichung vom Spec-Sketch: Trigger-Modus ist ein Auswahlfeld (ComboBox) statt drei Segment-Knöpfen – gleiche Funktion, weniger Code.

- [ ] **Step 1: Failing test `tests/plugin/test_Editor.cpp` schreiben**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

using namespace dg;

TEST_CASE("editor opens with the stored scale and follows focus", "[editor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.setUiScale(1.5f);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    REQUIRE(dynamic_cast<DubgefahrenEditor*>(editor.get()) != nullptr);
    CHECK(editor->getWidth() == 1500);
    CHECK(editor->getHeight() == 960);

    auto* e = static_cast<DubgefahrenEditor*>(editor.get());
    CHECK(e->selectedSlot() == 0);
    e->selectSlot(6);
    CHECK(e->selectedSlot() == 6);
}
```

In `tests/CMakeLists.txt` anhängen: `target_sources(DubgefahrenPluginTests PRIVATE plugin/test_Editor.cpp)`

- [ ] **Step 2: Build ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL – `cannot open include file 'plugin/PluginEditor.h'`.

- [ ] **Step 3: LookAndFeel und Controls implementieren**

`plugin/ui/DgLookAndFeel.h`:

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace dg::ui {

namespace colours {
inline const juce::Colour background { 0xff121417 };
inline const juce::Colour panel { 0xff1b1f24 };
inline const juce::Colour outline { 0xff2c323a };
inline const juce::Colour text { 0xffd8dde3 };
inline const juce::Colour textDim { 0xff8a939e };
inline const juce::Colour accent { 0xfff2b134 };
inline const juce::Colour playing { 0xff4cd07d };
inline const juce::Colour latched { 0xff3aa0ff };
} // namespace colours

class DgLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DgLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;
};

} // namespace dg::ui
```

`plugin/ui/DgLookAndFeel.cpp`:

```cpp
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

DgLookAndFeel::DgLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, colours::background);
    setColour(juce::Label::textColourId, colours::text);
    setColour(juce::Slider::textBoxTextColourId, colours::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, colours::panel);
    setColour(juce::ComboBox::outlineColourId, colours::outline);
    setColour(juce::ComboBox::textColourId, colours::text);
    setColour(juce::ComboBox::arrowColourId, colours::textDim);
    setColour(juce::TextButton::buttonColourId, colours::panel);
    setColour(juce::TextButton::buttonOnColourId, colours::accent);
    setColour(juce::TextButton::textColourOffId, colours::text);
    setColour(juce::TextButton::textColourOnId, colours::background);
    setColour(juce::ToggleButton::textColourId, colours::text);
    setColour(juce::ToggleButton::tickColourId, colours::accent);
    setColour(juce::ToggleButton::tickDisabledColourId, colours::textDim);
    setColour(juce::PopupMenu::backgroundColourId, colours::panel);
    setColour(juce::PopupMenu::textColourId, colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour(juce::PopupMenu::highlightedTextColourId, colours::background);
}

void DgLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                     float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float lineW = 3.0f;
    const float arcR = radius - lineW;
    const juce::PathStrokeType stroke(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(colours::outline);
    g.strokePath(track, stroke);

    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar
        ? rotaryStartAngle + static_cast<float>(slider.valueToProportionOfLength(0.0)) * (rotaryEndAngle - rotaryStartAngle)
        : rotaryStartAngle;

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, std::min(from, angle), std::max(from, angle), true);
    g.setColour(slider.isEnabled() ? colours::accent : colours::textDim);
    g.strokePath(value, stroke);

    const juce::Point<float> tip(centre.x + (arcR - 6.0f) * std::sin(angle), centre.y - (arcR - 6.0f) * std::cos(angle));
    g.setColour(colours::text);
    g.drawLine({ centre, tip }, 2.0f);
}

} // namespace dg::ui
```

`plugin/ui/Controls.h`:

```cpp
#pragma once
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace dg::ui {

inline juce::String u8(const char* s) { return juce::String::fromUTF8(s); }

class Knob final : public juce::Component
{
public:
    explicit Knob(const juce::String& labelText);
    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void resized() override;

    juce::Slider slider;
    juce::Label label;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

class Choice final : public juce::Component
{
public:
    explicit Choice(const juce::String& labelText);
    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void resized() override;

    juce::ComboBox box;
    juce::Label label;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment_;
};

class Toggle final : public juce::Component
{
public:
    explicit Toggle(const juce::String& text);
    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void resized() override;

    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
};

} // namespace dg::ui
```

`plugin/ui/Controls.cpp`:

```cpp
#include "plugin/ui/Controls.h"
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

namespace {
void setupLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, colours::textDim);
    l.setFont(juce::FontOptions(12.0f));
}
} // namespace

Knob::Knob(const juce::String& labelText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    setupLabel(label, labelText);
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void Knob::attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    attachment_.reset();
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds(r.removeFromTop(14));
    slider.setBounds(r);
}

Choice::Choice(const juce::String& labelText)
{
    setupLabel(label, labelText);
    addAndMakeVisible(box);
    addAndMakeVisible(label);
}

void Choice::attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    attachment_.reset();
    box.clear(juce::dontSendNotification);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId)))
        box.addItemList(choice->choices, 1);
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramId, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds(r.removeFromTop(14));
    box.setBounds(r.withSizeKeepingCentre(r.getWidth(), std::min(24, r.getHeight())));
}

Toggle::Toggle(const juce::String& text)
{
    button.setButtonText(text);
    addAndMakeVisible(button);
}

void Toggle::attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    attachment_.reset();
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramId, button);
}

void Toggle::resized() { button.setBounds(getLocalBounds().withSizeKeepingCentre(getWidth(), 24)); }

} // namespace dg::ui
```

- [ ] **Step 4: PadGrid, SlotEditor, FxPanel, PerformancePanel implementieren**

`plugin/ui/PadGrid.h`:

```cpp
#pragma once
#include <array>
#include <functional>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/SlotParams.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

class PadGrid final : public juce::Component
{
public:
    explicit PadGrid(DubgefahrenProcessor& proc);
    ~PadGrid() override;

    void setPadStates(std::uint32_t active, std::uint32_t latched, int focus);
    void setSelected(int slot);
    void refreshNames();

    std::function<void(int)> onSelect;
    std::function<void(int)> onContextMenu;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class Pad;
    DubgefahrenProcessor& proc_;
    std::array<std::unique_ptr<Pad>, kNumSlots> pads_;
};

} // namespace dg::ui
```

`plugin/ui/PadGrid.cpp`:

```cpp
#include "plugin/ui/PadGrid.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/Controls.h"
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

class PadGrid::Pad final : public juce::Component
{
public:
    Pad(PadGrid& owner, int slot) : owner_(owner), slot_(slot) {}

    void setState(bool active, bool latched, bool focus, bool selected)
    {
        if (active == active_ && latched == latched_ && focus == focus_ && selected == selected_)
            return;
        active_ = active;
        latched_ = latched;
        focus_ = focus;
        selected_ = selected;
        repaint();
    }

    void setName(const juce::String& n)
    {
        name_ = n;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced(3.0f);
        g.setColour(active_ ? colours::playing.withAlpha(0.35f) : colours::panel);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(focus_ ? colours::accent : colours::outline);
        g.drawRoundedRectangle(r, 6.0f, selected_ ? 3.0f : 1.5f);

        g.setColour(colours::textDim);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(juce::String(slot_ + 1), r.reduced(6.0f), juce::Justification::topLeft);
        g.setColour(colours::text);
        g.setFont(juce::FontOptions(13.0f));
        g.drawFittedText(name_, r.reduced(6.0f).toNearestInt(), juce::Justification::centred, 2);

        if (latched_)
        {
            g.setColour(colours::latched);
            g.fillEllipse(r.getRight() - 14.0f, r.getY() + 6.0f, 8.0f, 8.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (owner_.onContextMenu)
                owner_.onContextMenu(slot_);
            return;
        }
        held_ = true;
        owner_.proc_.previewPress(slot_);
        if (owner_.onSelect)
            owner_.onSelect(slot_);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (held_)
            owner_.proc_.previewRelease(slot_);
        held_ = false;
    }

private:
    PadGrid& owner_;
    const int slot_;
    juce::String name_;
    bool active_ = false, latched_ = false, focus_ = false, selected_ = false, held_ = false;
};

PadGrid::PadGrid(DubgefahrenProcessor& proc) : proc_(proc)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        pads_[static_cast<std::size_t>(s)] = std::make_unique<Pad>(*this, s);
        addAndMakeVisible(*pads_[static_cast<std::size_t>(s)]);
    }
    refreshNames();
}

PadGrid::~PadGrid() = default;

void PadGrid::setPadStates(std::uint32_t active, std::uint32_t latched, int focus)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& pad = *pads_[static_cast<std::size_t>(s)];
        const bool selected = pad.getProperties()["selected"];
        pad.setState((active >> s) & 1u, (latched >> s) & 1u, s == focus, selected);
    }
}

void PadGrid::setSelected(int slot)
{
    for (int s = 0; s < kNumSlots; ++s)
        pads_[static_cast<std::size_t>(s)]->getProperties().set("selected", s == slot);
    setPadStates(proc_.activeMask(), proc_.latchedMask(), proc_.focusSlot());
    for (auto& p : pads_)
        p->repaint();
}

void PadGrid::refreshNames()
{
    for (int s = 0; s < kNumSlots; ++s)
        pads_[static_cast<std::size_t>(s)]->setName(proc_.slotName(s));
}

void PadGrid::paint(juce::Graphics& g)
{
    auto legend = getLocalBounds().removeFromBottom(20).toFloat();
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(colours::playing);
    g.drawText(u8("● spielt"), legend.removeFromLeft(80.0f), juce::Justification::centredLeft);
    g.setColour(colours::accent);
    g.drawText(u8("◆ Fokus"), legend.removeFromLeft(80.0f), juce::Justification::centredLeft);
    g.setColour(colours::latched);
    g.drawText(u8("● gelatcht"), legend.removeFromLeft(90.0f), juce::Justification::centredLeft);
}

void PadGrid::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(24);
    const int w = area.getWidth() / 4;
    const int h = area.getHeight() / 4;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int col = s % 4;
        const int rowFromBottom = s / 4; // Pad 1 unten links wie beim BU16
        pads_[static_cast<std::size_t>(s)]->setBounds(area.getX() + col * w, area.getY() + (3 - rowFromBottom) * h, w, h);
    }
}

} // namespace dg::ui
```

`plugin/ui/SlotEditor.h`:

```cpp
#pragma once
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/Controls.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

class SlotEditor final : public juce::Component
{
public:
    explicit SlotEditor(DubgefahrenProcessor& proc);
    void setSlot(int slot);
    int slot() const { return slot_; }
    void refreshName();

    std::function<void()> onRename;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    DubgefahrenProcessor& proc_;
    int slot_ = -1;
    juce::Label header_;
    juce::TextButton renameButton_ { u8("Umbenennen") };

    Choice wave_ { u8("Welle") };
    Knob pitch_ { u8("Tonhöhe") };
    Knob pw_ { u8("Pulsbreite") };
    Choice lfoShape_ { u8("Form") };
    Knob lfoRate_ { u8("Rate") };
    Toggle lfoSync_ { u8("Sync") };
    Choice lfoSyncDiv_ { u8("Sync-Rate") };
    Knob lfoDepth_ { u8("Tiefe") };
    Knob sweepAmt_ { u8("Betrag") };
    Knob sweepTime_ { u8("Zeit") };
    Knob attack_ { u8("Attack") };
    Knob release_ { u8("Release") };
    Choice trigMode_ { u8("Modus") };
    Knob oneShot_ { u8("Länge") };
    Choice choke_ { u8("Choke") };
    Knob vol_ { u8("Lautstärke") };
    Knob pan_ { u8("Pan") };
    Knob send_ { u8("FX-Send") };
};

} // namespace dg::ui
```

`plugin/ui/SlotEditor.cpp`:

```cpp
#include "plugin/ui/SlotEditor.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

namespace {
// 4 Zeilen passen in die Editorhöhe von 408 px (Basisgröße 1000 × 640).
constexpr int kNumRows = 4;
constexpr int kRowHeight = 90;
constexpr int kRowLabelWidth = 64;
constexpr int kCellWidth = 104;
const char* const kRowNames[kNumRows] = { "OSZ\nAMP", "LFO", "SWEEP\nTRIG", "MIX" };
} // namespace

SlotEditor::SlotEditor(DubgefahrenProcessor& proc) : proc_(proc)
{
    header_.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    header_.setColour(juce::Label::textColourId, colours::text);
    addAndMakeVisible(header_);
    renameButton_.onClick = [this] {
        if (onRename)
            onRename();
    };
    addAndMakeVisible(renameButton_);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &wave_, &pitch_, &pw_, &lfoShape_, &lfoRate_, &lfoSync_, &lfoSyncDiv_, &lfoDepth_, &sweepAmt_,
             &sweepTime_, &attack_, &release_, &trigMode_, &oneShot_, &choke_, &vol_, &pan_, &send_ })
        addAndMakeVisible(*c);
}

void SlotEditor::setSlot(int slot)
{
    if (slot == slot_)
        return;
    slot_ = slot;
    auto& s = proc_.state();
    wave_.attach(s, slotParamId(slot, SlotField::Wave));
    pitch_.attach(s, slotParamId(slot, SlotField::Pitch));
    pw_.attach(s, slotParamId(slot, SlotField::PulseWidth));
    lfoShape_.attach(s, slotParamId(slot, SlotField::LfoShape));
    lfoRate_.attach(s, slotParamId(slot, SlotField::LfoRate));
    lfoSync_.attach(s, slotParamId(slot, SlotField::LfoSync));
    lfoSyncDiv_.attach(s, slotParamId(slot, SlotField::LfoSyncDiv));
    lfoDepth_.attach(s, slotParamId(slot, SlotField::LfoDepth));
    sweepAmt_.attach(s, slotParamId(slot, SlotField::SweepAmount));
    sweepTime_.attach(s, slotParamId(slot, SlotField::SweepTime));
    attack_.attach(s, slotParamId(slot, SlotField::Attack));
    release_.attach(s, slotParamId(slot, SlotField::Release));
    trigMode_.attach(s, slotParamId(slot, SlotField::TrigMode));
    oneShot_.attach(s, slotParamId(slot, SlotField::OneShotLength));
    choke_.attach(s, slotParamId(slot, SlotField::Choke));
    vol_.attach(s, slotParamId(slot, SlotField::Volume));
    pan_.attach(s, slotParamId(slot, SlotField::Pan));
    send_.attach(s, slotParamId(slot, SlotField::FxSend));
    refreshName();
}

void SlotEditor::refreshName()
{
    header_.setText("Slot " + juce::String(slot_ + 1) + u8(" · ") + proc_.slotName(slot_), juce::dontSendNotification);
}

void SlotEditor::paint(juce::Graphics& g)
{
    g.setColour(colours::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(colours::textDim);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    for (int row = 0; row < kNumRows; ++row)
        g.drawFittedText(kRowNames[row], 12, 44 + row * kRowHeight, kRowLabelWidth - 12, kRowHeight,
                         juce::Justification::centredLeft, 2);
}

void SlotEditor::resized()
{
    auto top = getLocalBounds().reduced(12, 8).removeFromTop(32);
    renameButton_.setBounds(top.removeFromRight(110));
    header_.setBounds(top);

    const auto place = [this](int row, int col, juce::Component& c) {
        c.setBounds(kRowLabelWidth + col * kCellWidth, 44 + row * kRowHeight, kCellWidth - 8, kRowHeight - 6);
    };
    place(0, 0, wave_);     place(0, 1, pitch_);     place(0, 2, pw_);       place(0, 3, attack_);     place(0, 4, release_);
    place(1, 0, lfoShape_); place(1, 1, lfoRate_);   place(1, 2, lfoSync_);  place(1, 3, lfoSyncDiv_); place(1, 4, lfoDepth_);
    place(2, 0, sweepAmt_); place(2, 1, sweepTime_); place(2, 2, trigMode_); place(2, 3, oneShot_);    place(2, 4, choke_);
    place(3, 0, vol_);      place(3, 1, pan_);       place(3, 2, send_);
}

} // namespace dg::ui
```

`plugin/ui/FxPanel.h`:

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

private:
    Knob drive_ { u8("Drive") };
    Knob cutoff_ { u8("Cutoff") };
    Knob resonance_ { u8("Reso") };
    Knob filterType_ { u8("LP·BP·HP") };
    Choice delayTime_ { u8("Zeit") };
    Knob delayFeedback_ { u8("Feedback") };
    Knob delayTone_ { u8("Tone") };
    Knob delayWow_ { u8("Wow") };
    Knob delayMix_ { u8("Mix") };
    Knob reverbDecay_ { u8("Decay") };
    Knob reverbTone_ { u8("Tone") };
    Knob reverbMix_ { u8("Mix") };
    Knob master_ { u8("Master") };
};

} // namespace dg::ui
```

`plugin/ui/FxPanel.cpp`:

```cpp
#include "plugin/ui/FxPanel.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

namespace {
constexpr int kCell = 72;
struct Group { const char* title; int firstCell; int cells; };
constexpr Group kGroups[] = { { "DRIVE", 0, 1 }, { "FILTER", 1, 3 }, { "DELAY", 4, 5 }, { "HALL", 9, 3 }, { "MASTER", 12, 1 } };
} // namespace

FxPanel::FxPanel(DubgefahrenProcessor& proc)
{
    auto& s = proc.state();
    drive_.attach(s, pid::drive);
    cutoff_.attach(s, pid::cutoff);
    resonance_.attach(s, pid::resonance);
    filterType_.attach(s, pid::filterType);
    delayTime_.attach(s, pid::delayTime);
    delayFeedback_.attach(s, pid::delayFeedback);
    delayTone_.attach(s, pid::delayTone);
    delayWow_.attach(s, pid::delayWow);
    delayMix_.attach(s, pid::delayMix);
    reverbDecay_.attach(s, pid::reverbDecay);
    reverbTone_.attach(s, pid::reverbTone);
    reverbMix_.attach(s, pid::reverbMix);
    master_.attach(s, pid::masterVol);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &drive_, &cutoff_, &resonance_, &filterType_, &delayTime_, &delayFeedback_, &delayTone_, &delayWow_,
             &delayMix_, &reverbDecay_, &reverbTone_, &reverbMix_, &master_ })
        addAndMakeVisible(*c);
}

void FxPanel::paint(juce::Graphics& g)
{
    g.setColour(colours::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    for (const auto& grp : kGroups)
    {
        const int x = 12 + grp.firstCell * kCell;
        g.setColour(colours::textDim);
        g.drawText(grp.title, x, 4, grp.cells * kCell, 16, juce::Justification::centredLeft);
        g.setColour(colours::outline);
        g.drawVerticalLine(x - 4, 6.0f, static_cast<float>(getHeight() - 6));
    }
}

void FxPanel::resized()
{
    juce::Component* order[] = { &drive_, &cutoff_, &resonance_, &filterType_, &delayTime_, &delayFeedback_,
                                 &delayTone_, &delayWow_, &delayMix_, &reverbDecay_, &reverbTone_, &reverbMix_, &master_ };
    for (int i = 0; i < 13; ++i)
        order[i]->setBounds(12 + i * kCell, 20, kCell - 6, getHeight() - 24);
}

} // namespace dg::ui
```

`plugin/ui/PerformancePanel.h`:

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/ui/Controls.h"

namespace dg {
class DubgefahrenProcessor;
}

namespace dg::ui {

class PerformancePanel final : public juce::Component
{
public:
    explicit PerformancePanel(DubgefahrenProcessor& proc);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    Knob pitch_ { u8("Pitch") };
    Knob rate_ { u8("Rate") };
    Knob depth_ { u8("Tiefe") };
    Knob sweep_ { u8("Sweep") };
    Choice target_ { u8("Ziel") };
    Choice latchStop_ { u8("Latch bei Stopp") };
};

} // namespace dg::ui
```

`plugin/ui/PerformancePanel.cpp`:

```cpp
#include "plugin/ui/PerformancePanel.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

PerformancePanel::PerformancePanel(DubgefahrenProcessor& proc)
{
    auto& s = proc.state();
    pitch_.attach(s, pid::perfPitch);
    rate_.attach(s, pid::perfRate);
    depth_.attach(s, pid::perfDepth);
    sweep_.attach(s, pid::perfSweep);
    target_.attach(s, pid::perfTarget);
    latchStop_.attach(s, pid::latchStop);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &pitch_, &rate_, &depth_, &sweep_, &target_, &latchStop_ })
        addAndMakeVisible(*c);
}

void PerformancePanel::paint(juce::Graphics& g)
{
    g.setColour(colours::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(colours::textDim);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("PERFORMANCE", 12, 0, 120, getHeight(), juce::Justification::centredLeft);
}

void PerformancePanel::resized()
{
    int x = 130;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &pitch_, &rate_, &depth_, &sweep_ })
    {
        c->setBounds(x, 4, 72, getHeight() - 8);
        x += 76;
    }
    target_.setBounds(x + 20, 8, 120, 44);
    latchStop_.setBounds(x + 160, 8, 170, 44);
}

} // namespace dg::ui
```

- [ ] **Step 5: Editor implementieren und im Processor einhängen**

`plugin/PluginEditor.h`:

```cpp
#pragma once
#include <memory>
#include <optional>
#include <utility>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/SlotParams.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/FxPanel.h"
#include "plugin/ui/PadGrid.h"
#include "plugin/ui/PerformancePanel.h"
#include "plugin/ui/SlotEditor.h"

namespace dg {

class DubgefahrenProcessor;

class DubgefahrenEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kBaseWidth = 1000;
    static constexpr int kBaseHeight = 640;

    explicit DubgefahrenEditor(DubgefahrenProcessor& proc);
    ~DubgefahrenEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void selectSlot(int slot);
    int selectedSlot() const { return selectedSlot_; }

private:
    void timerCallback() override;
    void layoutContent();
    void refreshAll();
    void setPanic(bool down);
    void showKitMenu();
    void showPadMenu(int slot);
    void renameSlot(int slot);
    void importKit();
    void exportKit();
    void loadKit(const juce::File& file);
    void showMessage(const juce::String& title, const juce::String& text);
    void maybeShowConfigWarning();

    DubgefahrenProcessor& proc_;
    ui::DgLookAndFeel lnf_;
    juce::Component content_;
    juce::Label title_;
    juce::TextButton kitButton_ { juce::String::fromUTF8("Kit ▾") };
    juce::TextButton importButton_ { "Import" };
    juce::TextButton exportButton_ { "Export" };
    juce::TextButton panicButton_ { "PANIC" };
    juce::ToggleButton followFocus_ { "Editor folgt Fokus" };
    ui::PadGrid pads_;
    ui::SlotEditor slotEditor_;
    ui::FxPanel fx_;
    ui::PerformancePanel perf_;
    std::unique_ptr<juce::FileChooser> chooser_;
    std::optional<std::pair<SlotParams, juce::String>> clipboard_;
    int selectedSlot_ = 0;
    int lastFocus_ = -1;
    bool configWarningShown_ = false;
};

} // namespace dg
```

`plugin/PluginEditor.cpp`:

```cpp
#include "plugin/PluginEditor.h"
#include "engine/Kit.h"
#include "plugin/KitFile.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"

namespace dg {

DubgefahrenEditor::DubgefahrenEditor(DubgefahrenProcessor& proc)
    : AudioProcessorEditor(proc), proc_(proc), pads_(proc), slotEditor_(proc), fx_(proc), perf_(proc)
{
    setLookAndFeel(&lnf_);
    addAndMakeVisible(content_);

    title_.setText("DUBGEFAHREN", juce::dontSendNotification);
    title_.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, ui::colours::accent);

    kitButton_.onClick = [this] { showKitMenu(); };
    importButton_.onClick = [this] { importKit(); };
    exportButton_.onClick = [this] { exportKit(); };
    panicButton_.onStateChange = [this] { setPanic(panicButton_.isDown()); };
    followFocus_.setToggleState(proc_.editorFollowsFocus(), juce::dontSendNotification);
    followFocus_.onClick = [this] { proc_.setEditorFollowsFocus(followFocus_.getToggleState()); };

    pads_.onSelect = [this](int s) { selectSlot(s); };
    pads_.onContextMenu = [this](int s) { showPadMenu(s); };
    slotEditor_.onRename = [this] { renameSlot(selectedSlot_); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &title_, &kitButton_, &importButton_, &exportButton_, &panicButton_, &followFocus_, &pads_, &slotEditor_, &fx_, &perf_ })
        content_.addAndMakeVisible(*c);

    content_.setSize(kBaseWidth, kBaseHeight);
    layoutContent();
    selectSlot(proc_.focusSlot());

    setResizable(true, true);
    setResizeLimits(kBaseWidth * 3 / 4, kBaseHeight * 3 / 4, kBaseWidth * 2, kBaseHeight * 2);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(kBaseWidth) / kBaseHeight);
    const float scale = proc_.uiScale();
    setSize(juce::roundToInt(kBaseWidth * scale), juce::roundToInt(kBaseHeight * scale));
    startTimerHz(30);
}

DubgefahrenEditor::~DubgefahrenEditor()
{
    stopTimer();
    setPanic(false);
    setLookAndFeel(nullptr);
}

void DubgefahrenEditor::paint(juce::Graphics& g) { g.fillAll(ui::colours::background); }

void DubgefahrenEditor::resized()
{
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(kBaseWidth);
    content_.setBounds(0, 0, kBaseWidth, kBaseHeight);
    content_.setTransform(juce::AffineTransform::scale(scale));
    proc_.setUiScale(scale);
}

void DubgefahrenEditor::layoutContent()
{
    auto r = juce::Rectangle<int>(0, 0, kBaseWidth, kBaseHeight).reduced(12);
    auto header = r.removeFromTop(36);
    title_.setBounds(header.removeFromLeft(220));
    panicButton_.setBounds(header.removeFromRight(90).reduced(2));
    header.removeFromRight(12);
    exportButton_.setBounds(header.removeFromRight(80).reduced(2));
    importButton_.setBounds(header.removeFromRight(80).reduced(2));
    kitButton_.setBounds(header.removeFromRight(110).reduced(2));
    followFocus_.setBounds(header.removeFromRight(170));

    perf_.setBounds(r.removeFromBottom(60));
    r.removeFromBottom(8);
    fx_.setBounds(r.removeFromBottom(96));
    r.removeFromBottom(8);
    pads_.setBounds(r.removeFromLeft(360));
    r.removeFromLeft(12);
    slotEditor_.setBounds(r);
}

void DubgefahrenEditor::selectSlot(int slot)
{
    if (slot < 0 || slot >= kNumSlots)
        return;
    selectedSlot_ = slot;
    slotEditor_.setSlot(slot);
    pads_.setSelected(slot);
}

void DubgefahrenEditor::refreshAll()
{
    pads_.refreshNames();
    slotEditor_.refreshName();
}

void DubgefahrenEditor::timerCallback()
{
    const int focus = proc_.focusSlot();
    pads_.setPadStates(proc_.activeMask(), proc_.latchedMask(), focus);
    if (focus != lastFocus_)
    {
        lastFocus_ = focus;
        if (proc_.editorFollowsFocus() && focus != selectedSlot_)
            selectSlot(focus);
    }
}

void DubgefahrenEditor::setPanic(bool down)
{
    if (auto* p = proc_.state().getParameter(pid::panic))
    {
        const float target = down ? 1.0f : 0.0f;
        if (p->getValue() == target)
            return;
        p->beginChangeGesture();
        p->setValueNotifyingHost(target);
        p->endChangeGesture();
    }
}

void DubgefahrenEditor::showMessage(const juce::String& title, const juce::String& text)
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title, text);
}

void DubgefahrenEditor::maybeShowConfigWarning()
{
    const auto& info = proc_.kitFolder();
    if (configWarningShown_ || info.warning.isEmpty())
        return;
    configWarningShown_ = true;
    showMessage("Hinweis zur Config", info.warning + "\n\nVerwendet wird: " + info.folder.getFullPathName());
}

void DubgefahrenEditor::showKitMenu()
{
    maybeShowConfigWarning();
    juce::PopupMenu menu;
    menu.addItem(1, "Werks-Kit laden");
    menu.addSeparator();
    auto files = proc_.kitFolder().folder.findChildFiles(juce::File::findFiles, false, juce::String("*") + kKitExtension);
    files.sort();
    if (files.isEmpty())
        menu.addItem(2, "(keine Kits im Ordner)", false);
    for (int i = 0; i < files.size(); ++i)
        menu.addItem(100 + i, files[i].getFileNameWithoutExtension());

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(kitButton_),
                       [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), files](int result) {
                           if (safe == nullptr || result == 0)
                               return;
                           if (result == 1)
                               safe->proc_.applyKit(makeFactoryKit());
                           else if (result >= 100)
                               safe->loadKit(files[result - 100]);
                           safe->refreshAll();
                       });
}

void DubgefahrenEditor::loadKit(const juce::File& file)
{
    const auto result = loadKitFile(file);
    if (!result.kit)
    {
        showMessage("Kit konnte nicht geladen werden", result.error);
        return;
    }
    proc_.applyKit(*result.kit);
    refreshAll();
}

void DubgefahrenEditor::importKit()
{
    maybeShowConfigWarning();
    chooser_ = std::make_unique<juce::FileChooser>("Kit importieren", proc_.kitFolder().folder,
                                                   juce::String("*") + kKitExtension);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<DubgefahrenEditor>(this)](const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              safe->loadKit(fc.getResult());
                          });
}

void DubgefahrenEditor::exportKit()
{
    maybeShowConfigWarning();
    chooser_ = std::make_unique<juce::FileChooser>("Kit exportieren",
                                                   proc_.kitFolder().folder.getChildFile(juce::String("Mein Kit") + kKitExtension),
                                                   juce::String("*") + kKitExtension);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<DubgefahrenEditor>(this)](const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              const auto file = fc.getResult().withFileExtension(kKitExtension);
                              juce::String error;
                              if (!saveKitFile(safe->proc_.currentKit(), file, error))
                                  safe->showMessage("Kit konnte nicht gespeichert werden", error);
                          });
}

void DubgefahrenEditor::showPadMenu(int slot)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Kopieren");
    menu.addItem(2, juce::String::fromUTF8("Einfügen"), clipboard_.has_value());
    menu.addItem(3, juce::String::fromUTF8("Auf Werkseinstellung zurücksetzen"));
    menu.addItem(4, "Umbenennen");
    menu.showMenuAsync(juce::PopupMenu::Options(),
                       [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot](int result) {
                           if (safe == nullptr)
                               return;
                           auto& self = *safe;
                           switch (result)
                           {
                               case 1:
                                   self.clipboard_ = std::make_pair(self.proc_.currentKit().slots[static_cast<std::size_t>(slot)],
                                                                    self.proc_.slotName(slot));
                                   break;
                               case 2:
                                   if (self.clipboard_)
                                       self.proc_.setSlot(slot, self.clipboard_->first, self.clipboard_->second);
                                   break;
                               case 3:
                               {
                                   const Kit factory = makeFactoryKit();
                                   self.proc_.setSlot(slot, factory.slots[static_cast<std::size_t>(slot)],
                                                      juce::String::fromUTF8(factory.names[static_cast<std::size_t>(slot)].c_str()));
                                   break;
                               }
                               case 4:
                                   self.renameSlot(slot);
                                   break;
                               default:
                                   break;
                           }
                           self.refreshAll();
                       });
}

void DubgefahrenEditor::renameSlot(int slot)
{
    auto* window = new juce::AlertWindow("Slot umbenennen", "Neuer Name:", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor("name", proc_.slotName(slot));
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Abbrechen", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), window, slot](int result) {
                                    if (safe == nullptr || result != 1)
                                        return;
                                    const auto name = window->getTextEditorContents("name").trim();
                                    if (name.isNotEmpty())
                                        safe->proc_.setSlotName(slot, name);
                                    safe->refreshAll();
                                }),
                            true);
}

} // namespace dg
```

In `plugin/PluginProcessor.cpp` `#include "plugin/PluginEditor.h"` ergänzen und `createEditor` ersetzen:

```cpp
juce::AudioProcessorEditor* DubgefahrenProcessor::createEditor()
{
    return new DubgefahrenEditor(*this);
}
```

In `plugin/CMakeLists.txt` im `target_sources(dg_plugin_shared INTERFACE …)`-Block ergänzen:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/DgLookAndFeel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/DgLookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/Controls.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/Controls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/PadGrid.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/PadGrid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/SlotEditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/SlotEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/FxPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/FxPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/PerformancePanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/PerformancePanel.cpp
```

- [ ] **Step 6: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS (inkl. `[editor]`).

- [ ] **Step 7: Sichtprüfung im Plugin-Host von JUCE (optional, aber empfohlen)**

Der einfachste Weg ohne Ableton: das Plugin in Ableton laden (Task 17 installiert es) oder JUCEs `AudioPluginHost` bauen. Prüfen: Pads zeigen Namen, Klick auf ein Pad hört vor und wählt den Slot, Knobs bewegen sich, Rechtsklick-Menü erscheint, Fenster lässt sich skalieren. Auffälligkeiten als Notiz im Commit-Text festhalten.

- [ ] **Step 8: Commit**

```bash
git add plugin tests
git commit -m "feat(plugin): editor with pad grid, slot editor, fx and performance panels"
```

---

### Task 17: Installation, Beispiel-Config, Validierung, README

**Files:**
- Create: `resources/Dubgefahren.config.json`, `cmake/CopyExampleConfig.cmake`, `cmake/InstallBundle.cmake`, `tests/InstallBundleTest.cmake`, `tools/vst3validator/CMakeLists.txt`, `README.md`
- Modify: `plugin/CMakeLists.txt`, `tests/CMakeLists.txt`, `build.ps1`

**Interfaces:**
- Consumes: Target `Dubgefahren_VST3` (Task 14).
- Produces: Post-Build-Schritt legt die Beispiel-Config ins Build-Bundle; `cmake --install` kopiert das Bundle nach `DG_VST3_INSTALL_DIR` (Standard `%CommonProgramFiles%\VST3`) ohne eine vorhandene Config zu überschreiben; `build.ps1 validate` baut Steinbergs Validator und startet ihn sowie – falls vorhanden – pluginval.

- [ ] **Step 1: Failing test für die Install-Logik schreiben**

`tests/InstallBundleTest.cmake`:

```cmake
# Aufruf: cmake -DSCRIPT=<InstallBundle.cmake> -DWORK=<leerer Ordner> -P InstallBundleTest.cmake
set(bundle "${WORK}/src/Dubgefahren.vst3")
set(dest "${WORK}/dest")
file(REMOVE_RECURSE "${WORK}")
file(WRITE "${bundle}/Contents/x86_64-win/Dubgefahren.vst3" "binary-v1")
file(WRITE "${bundle}/Contents/Resources/Dubgefahren.config.json" "{ \"kitFolder\": \"\" }")

# Erste Installation: alles wird kopiert.
execute_process(COMMAND ${CMAKE_COMMAND} -DBUNDLE=${bundle} -DDEST=${dest} -P ${SCRIPT} RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "Install-Skript fehlgeschlagen")
endif()
file(READ "${dest}/Dubgefahren.vst3/Contents/Resources/Dubgefahren.config.json" cfg)
if(NOT cfg MATCHES "kitFolder")
    message(FATAL_ERROR "Config wurde bei Erstinstallation nicht kopiert")
endif()

# Nutzer ändert die Config, neue Plugin-Version wird installiert.
file(WRITE "${dest}/Dubgefahren.vst3/Contents/Resources/Dubgefahren.config.json" "USER-CONFIG")
file(WRITE "${bundle}/Contents/x86_64-win/Dubgefahren.vst3" "binary-v2")
execute_process(COMMAND ${CMAKE_COMMAND} -DBUNDLE=${bundle} -DDEST=${dest} -P ${SCRIPT} RESULT_VARIABLE rc)
file(READ "${dest}/Dubgefahren.vst3/Contents/Resources/Dubgefahren.config.json" cfg)
file(READ "${dest}/Dubgefahren.vst3/Contents/x86_64-win/Dubgefahren.vst3" bin)
if(NOT cfg STREQUAL "USER-CONFIG")
    message(FATAL_ERROR "Vorhandene Config wurde überschrieben")
endif()
if(NOT bin STREQUAL "binary-v2")
    message(FATAL_ERROR "Plugin-Binary wurde nicht aktualisiert")
endif()
file(REMOVE_RECURSE "${WORK}")
```

In `tests/CMakeLists.txt` anhängen:

```cmake
add_test(NAME install_keeps_existing_config
    COMMAND ${CMAKE_COMMAND}
        -DSCRIPT=${PROJECT_SOURCE_DIR}/cmake/InstallBundle.cmake
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/install_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/InstallBundleTest.cmake)
```

- [ ] **Step 2: Test ausführen, Fehlschlag prüfen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure; powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: FAIL – `install_keeps_existing_config` scheitert („Install-Skript fehlgeschlagen“, weil `cmake/InstallBundle.cmake` fehlt).

- [ ] **Step 3: Install-Skripte und Beispiel-Config anlegen**

`cmake/InstallBundle.cmake`:

```cmake
# Kopiert ein VST3-Bundle nach DEST. Eine vorhandene Dubgefahren.config.json im Ziel bleibt unangetastet.
# Aufruf: cmake -DBUNDLE=<pfad/Dubgefahren.vst3> -DDEST=<zielordner> -P InstallBundle.cmake
cmake_minimum_required(VERSION 3.25)
if(NOT IS_DIRECTORY "${BUNDLE}")
    message(FATAL_ERROR "Bundle nicht gefunden: ${BUNDLE}")
endif()
get_filename_component(name "${BUNDLE}" NAME)
set(target "${DEST}/${name}")
file(GLOB_RECURSE files RELATIVE "${BUNDLE}" "${BUNDLE}/*")
foreach(f IN LISTS files)
    if(f MATCHES "Dubgefahren\\.config\\.json$" AND EXISTS "${target}/${f}")
        message(STATUS "Config vorhanden, nicht überschrieben: ${target}/${f}")
        continue()
    endif()
    get_filename_component(dir "${target}/${f}" DIRECTORY)
    file(MAKE_DIRECTORY "${dir}")
    file(COPY_FILE "${BUNDLE}/${f}" "${target}/${f}")
    message(STATUS "Installiert: ${target}/${f}")
endforeach()
```

`cmake/CopyExampleConfig.cmake`:

```cmake
# Legt die Beispiel-Config ins Build-Bundle, falls dort noch keine liegt.
# Aufruf: cmake -DBUNDLE=<pfad/Dubgefahren.vst3> -DSOURCE=<Beispiel-Config> -P CopyExampleConfig.cmake
set(dest "${BUNDLE}/Contents/Resources/Dubgefahren.config.json")
if(NOT EXISTS "${dest}")
    file(MAKE_DIRECTORY "${BUNDLE}/Contents/Resources")
    file(COPY_FILE "${SOURCE}" "${dest}")
endif()
```

`resources/Dubgefahren.config.json`:

```json
{
  "_hinweis": "kitFolder auf den gewünschten Ordner setzen, z. B. \"D:\\\\Musik\\\\Dubgefahren\\\\Kits\". Umgebungsvariablen wie %USERPROFILE% sind erlaubt. Leer = Dokumente\\Dubgefahren\\Kits. Bearbeiten erfordert Adminrechte, wenn das Plugin unter Program Files liegt.",
  "kitFolder": ""
}
```

Am Ende von `plugin/CMakeLists.txt` anhängen:

```cmake
set(DG_VST3_INSTALL_DIR "$ENV{CommonProgramFiles}/VST3" CACHE PATH "Zielordner für die VST3-Installation")
get_target_property(DG_VST3_BUNDLE Dubgefahren_VST3 JUCE_PLUGIN_ARTEFACT_FILE)

add_custom_command(TARGET Dubgefahren_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DBUNDLE=${DG_VST3_BUNDLE}
        -DSOURCE=${PROJECT_SOURCE_DIR}/resources/Dubgefahren.config.json
        -P ${PROJECT_SOURCE_DIR}/cmake/CopyExampleConfig.cmake
    VERBATIM)

install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\"
    \"-DBUNDLE=${DG_VST3_BUNDLE}\"
    \"-DDEST=${DG_VST3_INSTALL_DIR}\"
    -P \"${PROJECT_SOURCE_DIR}/cmake/InstallBundle.cmake\"
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR \"Installation fehlgeschlagen (Adminrechte?)\")
endif()")
```

- [ ] **Step 4: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS, inkl. `install_keeps_existing_config`. Zusätzlich existiert `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3\Contents\Resources\Dubgefahren.config.json`.

- [ ] **Step 5: Validator-Projekt und `validate`-Befehl anlegen**

`tools/vst3validator/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(DgVst3Validator LANGUAGES C CXX)
include(FetchContent)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_HOSTING_SAMPLES ON CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG v3.8.1_build_84
    GIT_SHALLOW ON
    GIT_SUBMODULES base cmake pluginterfaces public.sdk)
FetchContent_MakeAvailable(vst3sdk)
```

In `build.ps1` vor dem `switch` einfügen:

```powershell
function Do-Validate {
    $bundle = Join-Path $buildDir "plugin\Dubgefahren_artefacts\$Config\VST3\Dubgefahren.vst3"
    if (-not (Test-Path $bundle)) { Do-Build }
    $vDir = Join-Path $buildDir 'vst3validator'
    Invoke-Checked 'validator configure' { & $cmake -S (Join-Path $root 'tools\vst3validator') -B $vDir -A x64 }
    Invoke-Checked 'validator build' { & $cmake --build $vDir --config Release --target validator --parallel }
    $validator = Get-ChildItem -Path $vDir -Recurse -Filter 'validator.exe' | Select-Object -First 1
    if (-not $validator) { throw 'validator.exe nicht gefunden' }
    Invoke-Checked 'VST3 validator' { & $validator.FullName $bundle }

    $pluginval = Join-Path $root 'tools\bin\pluginval.exe'
    if (Test-Path $pluginval) {
        Invoke-Checked 'pluginval' { & $pluginval --strictness-level 5 --validate-in-process --validate $bundle }
    } else {
        Write-Host "pluginval nicht gefunden ($pluginval) – übersprungen. Siehe README."
    }
}
```

und im `switch` ergänzen:

```powershell
    'validate'  { Do-Validate }
```

- [ ] **Step 6: Validierung ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
Expected: Steinbergs Validator meldet am Ende `0 tests failed` (Zeile „Result: … tests passed, 0 tests failed“). pluginval wird übersprungen, solange `tools\bin\pluginval.exe` fehlt; ist es vorhanden, endet es mit `SUCCESS`. Bei Fehlern: Ausgabe lesen, Ursache mit superpowers:systematic-debugging beheben, erneut ausführen.

Hinweis: pluginval ist ein Download (GitHub-Release von Tracktion/pluginval, `pluginval_Windows.zip`). Den Download **nicht** selbst ausführen, sondern die Person fragen, ob sie ihn nach `tools\bin\` legen möchte.

- [ ] **Step 7: `README.md` schreiben**

~~~~markdown
# Dubgefahren

VST3-Instrument für Windows: 16 einzeln einstellbare Dubsirenen, spielbar über MIDI-Noten 36–51
(z. B. Intech Studio Grid BU16), mit gemeinsamer Dub-Effektkette (Drive, Filter, Tape-Delay, Federhall).

## Voraussetzungen

- Windows 10/11 x64
- Visual Studio 2026 mit „Desktopentwicklung mit C++“ (bringt MSVC und CMake mit)
- Git

JUCE 8.0.15 und Catch2 werden beim ersten Configure automatisch geladen.

## Bauen und testen

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 all
```

Weitere Befehle: `configure`, `build`, `test`, `install`, `validate` (Option `-Config Debug|Release`, Standard Release).

Das Plugin liegt danach unter `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3`.

## Installieren

In einer **Administrator**-PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 install
```

Kopiert das Bundle nach `%CommonProgramFiles%\VST3`. Eine dort bereits vorhandene
`Dubgefahren.config.json` wird nicht überschrieben. Anderes Ziel: `cmake -DDG_VST3_INSTALL_DIR=<Ordner> build`.

## Config: Kit-Ordner festlegen

Datei: `%CommonProgramFiles%\VST3\Dubgefahren.vst3\Contents\Resources\Dubgefahren.config.json`
(Bearbeiten als Administrator).

```json
{ "kitFolder": "D:\\Musik\\Dubgefahren\\Kits" }
```

Umgebungsvariablen wie `%USERPROFILE%` sind erlaubt. Leer, fehlend oder ungültig → `Dokumente\Dubgefahren\Kits`.
Bei einer fehlerhaften Config zeigt das Plugin beim Öffnen von Kit-Menü, Import oder Export einen Hinweis.

## Ableton Live einrichten

1. Dubgefahren als Instrument auf einen MIDI-Track legen (ersetzt das Effekt-Drumrack).
2. **BU16:** Pads senden Noten 36–51 (C1–D#2) → Slots 1–16. Pad 1 liegt unten links, wie im Plugin.
3. **PO16 (Effekte):** Im Plugin-Gerät „Konfigurieren“ bzw. MIDI-Map-Modus (Strg+M) die Parameter
   `Drive`, `Filter Cutoff`, `Filter Resonanz`, `Filter Typ`, `Delay Zeit`, `Delay Feedback`, `Delay Tone`,
   `Delay Wow`, `Delay Mix`, `Hall Decay`, `Hall Tone`, `Hall Mix`, `Master` auf die Potis legen.
   Die globalen Parameter stehen in der Liste ganz oben.
4. **TEK2 (Performance):** `Perf Pitch`, `Perf Rate`, `Perf Tiefe`, `Perf Sweep`. Mittelstellung = keine Änderung.
5. **PFB4 (Schalter):** `Perf Ziel` (Fokus/Alle), `Panic` (Taster), `Latch bei Stopp`.

## Kits

Kit-Menü → Werks-Kit oder Kits aus dem Kit-Ordner. Import/Export als `.dgkit` (JSON, 16 Slots, ohne Effekte).
Rechtsklick auf ein Pad: Kopieren, Einfügen, Auf Werkseinstellung zurücksetzen, Umbenennen.

## Validierung

`build.ps1 validate` baut Steinbergs VST3-Validator und prüft das Plugin. Für pluginval
`pluginval_Windows.zip` von https://github.com/Tracktion/pluginval/releases laden und
`pluginval.exe` nach `tools\bin\` legen.

## Abnahme-Checkliste (Ableton)

- [ ] Plugin erscheint als Instrument und lädt ohne Fehlermeldung.
- [ ] BU16-Pads 1–16 spielen die Slots 1–16; Pad-Anzeige im Plugin leuchtet mit.
- [ ] Gate: klingt nur solange gedrückt. Latch: an/aus per Tipp. One-Shot: feste Länge.
- [ ] Choke: Laser, Riser, Faller, Bleep, Zap, Drop würgen sich gegenseitig ohne Knacksen ab.
- [ ] Mehrere Sirenen klingen gleichzeitig.
- [ ] PO16 regelt die Effekte, TEK2 die Performance-Regler; Fokus/Alle verhält sich wie erwartet.
- [ ] Transport-Stopp mit allen drei „Latch bei Stopp“-Einstellungen geprüft.
- [ ] Panic stoppt alles, Delay-/Hall-Fahnen klingen aus.
- [ ] Set speichern, Live neu starten, Set laden: Einstellungen und Slot-Namen sind wieder da.
- [ ] Kit exportieren, Slots ändern, Kit importieren: Slots wiederhergestellt, Effekte unverändert.
- [ ] Fenster skalieren (75–200 %), Größe bleibt nach Neuladen erhalten.
- [ ] Delay-Feedback auf Maximum: Eigenoszillation ohne Übersteuern.
~~~~

- [ ] **Step 8: Tests ausführen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: PASS.

- [ ] **Step 9: Commit**

```bash
git add cmake resources tools/vst3validator plugin/CMakeLists.txt tests build.ps1 README.md
git commit -m "build: install with config preservation, validation and README"
```

---

## Abschluss

Nach Task 17: superpowers:finishing-a-development-branch verwenden. Die manuelle Abnahme in Ableton (Checkliste in der README) kann nur die Person selbst durchführen – sie bekommt die Checkliste und den Hinweis auf `build.ps1 install` (Administrator-PowerShell).
