# UI-Umbau: Material und Zustandslicht – Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Die Oberfläche bekommt Tiefe (Verläufe, Schatten, Regler mit Körper) und Licht für Zustände (spielende Pads, Fokus, Latch) sowie eine eingebettete Schrift – ohne Änderung an Layout oder Verhalten (Issue #15).

**Architecture:**
- `plugin/ui/Fonts` liefert die eingebettete Schrift (Inter) über `ui::font()`; `DgLookAndFeel` gibt sie für alle Standard-Widgets aus.
- `plugin/ui/Surfaces` enthält die gemeinsamen Zeichenhelfer für Panels, Trenner und Fensterhintergrund.
- `plugin/ui/PadGlow.h` ist die reine Helligkeitskurve der Pads. `PadGrid` schreibt sie im 30-Hz-Timer fort.
- Schatten und Glow ragen über ihre Komponenten hinaus. Deshalb zeichnet `DubgefahrenEditor::paint` Panel-Schatten und Pad-Glow unter den Komponenten.
- Schatten und Glow rendert melatonin_blur (gecacht pro Objekt).

**Tech Stack:** C++20, JUCE 8.0.15, melatonin_blur v1.4, Inter 4.1 (TTF), Catch2 v3, MSVC x64, CMake.

**Spec:** `docs/superpowers/specs/2026-09-30-ui-restyle-design.md`

## Global Constraints

- Layout, Größen, Bedienung, Parameter, Kit-Format und Engine bleiben **unverändert**. Kein `setBounds` und keine Layout-Konstante wird geändert.
- Neue Abhängigkeiten: nur melatonin_blur **v1.4** (MIT) und Inter **4.1**, Schnitte Regular und SemiBold (SIL OFL 1.1). Kein UI-Framework, keine Bild-Regler, kein WebView.
- `juce::LookAndFeel::setDefaultLookAndFeel` und `getTypefaceForFont` werden **nicht** verwendet (mehrere Plugin-Instanzen teilen sich einen Prozess). Schrift ausschließlich über `ui::font(...)`.
- In `plugin/` steht nach Task 1 kein `juce::FontOptions(`-Aufruf mit Zahl mehr außerhalb von `plugin/ui/Fonts.cpp`.
- melatonin-Schatten sind **Member** eines langlebigen Objekts (Komponente, Look-and-Feel), nie lokale Variablen in `paint` (sonst kein Cache).
- Animiert wird nur Farbe/Deckkraft eines Glows (`setColor`), nie Radius, Spread oder Pfad.
- Pad-Helligkeit: Trigger = 1; Haltewert 0,7; Zeitkonstante 80 ms; Einrasten bei Abstand < 0,005; Ausklingen linear mit 1/200 ms.
- Pad-Grundfarbe `colours::padBase` = `0xff4cd07d`. `colours::playing` entfällt.
- Ring am Pad: Fokus = `colours::accent`; ausgewählt = 2,5 px statt 1,5 px; weder noch = kein Ring.
- Unverändert bleiben: CPU-Anzeige und ihre Warnfarben, Popup-Menüs (nur Schrift), Dialoge (nur Schrift).
- Quelltexte UTF-8; Nicht-ASCII-Strings an JUCE über `ui::u8(...)`. Kommentare Deutsch, UI-Texte Englisch.
- `engine/` wird nicht angefasst.
- Befehle aus dem Repo-Root:
  - Build: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
  - Neu konfigurieren (nach CMake-Änderungen): `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
  - Plugin-Tests: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "<tag>"`
  - Alles: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
  - Validator: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
- Bilder zur Sichtprüfung: Ist die Umgebungsvariable `DG_SNAPSHOT_DIR` gesetzt, schreiben die Render-Tests PNG-Dateien dorthin. Der Ordner liegt außerhalb des Repos.
- Commits enden mit `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

- **Zwei Plugin-Instanzen im selben Host, ein Editor wird geschlossen, der andere bleibt offen.** Erwartet: Der offene Editor behält Inter; kein Absturz, kein Rückfall auf die Systemschrift. → Test in Task 1 („the embedded font survives closing another editor“).
- **Der UI-Thread hängt (Host-Fenster verdeckt, Debugger, Laden eines Projekts) und der Timer liefert ein sehr großes, ein Null- oder ein negatives Zeitintervall.** Erwartet: Die Helligkeit bleibt in 0…1, wird nie NaN, und die Animation ist danach einfach fertig. → Tests in Task 3 („huge, zero and negative time steps …“).
- **Ein Slot wird geleert oder sein Sample fehlt, während das Pad leuchtet.** Erwartet: Das Pad zeichnet keinen Glow mehr und leuchtet nicht, auch wenn die Engine den Slot noch als aktiv meldet. → Test in Task 4 („an empty pad never lights up“).
- **Winzige oder leere Zeichenflächen** (Regler mit 0×0 oder 3×3 Pixeln beim ersten Layout, Button mit Höhe 0). Erwartet: kein Absturz, keine negative Radien, einfach nichts gezeichnet. → Test in Task 5 („controls draw nothing harmful into tiny or empty areas“).
- **Fenster bei 0,75× und 2×.** Erwartet: Schatten und Glow sitzen an der richtigen Stelle und sind nicht abgeschnitten oder unscharf. → Test in Task 2 („the editor paints at every window scale“) plus Sichtprüfung der PNGs in Task 2 und Task 4.

---

## Dateistruktur

| Datei | Verantwortung |
|---|---|
| `plugin/assets/fonts/Inter-Regular.ttf`, `Inter-SemiBold.ttf`, `OFL.txt` (neu) | eingebettete Schrift samt Lizenz |
| `plugin/ui/Fonts.h/.cpp` (neu) | `ui::font()`, Laden der Typefaces |
| `plugin/ui/Surfaces.h/.cpp` (neu) | Panel-Körper, Panel-Schatten, Trenner, Fensterhintergrund |
| `plugin/ui/PadGlow.h` (neu) | Helligkeitskurve, ohne JUCE |
| `plugin/ui/DgLookAndFeel.h/.cpp` | Palette, Fonts, Regler, Buttons, Auswahlboxen, Schalter |
| `plugin/ui/PadGrid.h/.cpp` | Pad-Zeichnung, Glow, Animation, Legende |
| `plugin/ui/SlotEditor.cpp`, `FxPanel.cpp`, `PerformancePanel.cpp`, `Controls.cpp`, `CpuMeter.cpp` | nutzen `ui::font()` und `drawPanelBody()` |
| `plugin/PluginEditor.h/.cpp` | Hintergrund, Panel-Schatten, Pad-Glow, Titel |
| `CMakeLists.txt`, `plugin/CMakeLists.txt`, `tests/CMakeLists.txt` | Abhängigkeiten, neue Dateien |
| `tests/plugin/RenderTestHelpers.h` (neu) | Snapshot und optionales PNG |
| `tests/plugin/test_Look.cpp`, `tests/plugin/test_PadGlow.cpp` (neu) | Tests |

---

### Task 1: Eingebettete Schrift

**Files:**
- Create: `plugin/assets/fonts/Inter-Regular.ttf`, `plugin/assets/fonts/Inter-SemiBold.ttf`, `plugin/assets/fonts/OFL.txt`
- Create: `plugin/ui/Fonts.h`, `plugin/ui/Fonts.cpp`
- Create: `tests/plugin/test_Look.cpp`
- Modify: `plugin/CMakeLists.txt`, `tests/CMakeLists.txt`
- Modify: `plugin/ui/DgLookAndFeel.h`, `plugin/ui/DgLookAndFeel.cpp`
- Modify: `plugin/PluginEditor.cpp:23`, `plugin/ui/Controls.cpp:12`, `plugin/ui/CpuMeter.cpp:15`, `plugin/ui/FxPanel.cpp:40`, `plugin/ui/PadGrid.cpp:52,56,63,170`, `plugin/ui/PerformancePanel.cpp:26`, `plugin/ui/SlotEditor.cpp:24,41,116`

**Interfaces:**
- Produces:
  - `juce::FontOptions dg::ui::font(float height, bool bold = false)`
  - `juce::Font dg::ui::withEmbeddedTypeface(const juce::Font& f)`
  - `struct dg::ui::EmbeddedFonts { juce::Typeface::Ptr regular, semiBold; }`

- [ ] **Step 1: Schriftdateien holen**

Lädt `Inter-4.1.zip` (ca. 34 MB) von der Release-Seite des Projekts rsms/inter. Die Datei bleibt im Temp-Ordner, ins Repo kommen nur zwei TTF und die Lizenz.

```powershell
$zip = Join-Path $env:TEMP 'Inter-4.1.zip'
$dir = Join-Path $env:TEMP 'Inter-4.1'
Invoke-WebRequest -Uri 'https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip' -OutFile $zip
Expand-Archive $zip -DestinationPath $dir -Force
Get-ChildItem $dir -Recurse -Include 'Inter-Regular.ttf','Inter-SemiBold.ttf','LICENSE.txt' | Select-Object FullName, Length
```

Expected: je eine Zeile für `Inter-Regular.ttf`, `Inter-SemiBold.ttf` (statische TTF, je einige hundert KB) und `LICENSE.txt`. Dann mit den ausgegebenen Pfaden kopieren:

```powershell
New-Item -ItemType Directory -Force plugin\assets\fonts | Out-Null
Copy-Item '<Pfad>\Inter-Regular.ttf'  plugin\assets\fonts\Inter-Regular.ttf
Copy-Item '<Pfad>\Inter-SemiBold.ttf' plugin\assets\fonts\Inter-SemiBold.ttf
Copy-Item '<Pfad>\LICENSE.txt'        plugin\assets\fonts\OFL.txt
```

Prüfen, dass `OFL.txt` mit „SIL Open Font License“ beginnt bzw. diesen Text enthält.

- [ ] **Step 2: Den fehlschlagenden Test schreiben**

`tests/plugin/test_Look.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ui/DgLookAndFeel.h"
#include "plugin/ui/Fonts.h"

using namespace dg;

namespace {
bool isInter(const juce::Font& f) { return f.getTypefacePtr() != nullptr && f.getTypefacePtr()->getName().startsWith("Inter"); }
} // namespace

TEST_CASE("the embedded font is used for regular and bold text", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    const juce::Font regular(ui::font(12.0f));
    const juce::Font bold(ui::font(12.0f, true));
    CHECK(isInter(regular));
    CHECK(isInter(bold));
    CHECK(regular.getTypefacePtr() != bold.getTypefacePtr());
    CHECK(regular.getHeight() == 12.0f);
}

TEST_CASE("the look and feel hands out the embedded font for standard widgets", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;

    juce::Label label;
    label.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    const auto labelFont = lnf.getLabelFont(label);
    CHECK(isInter(labelFont));
    CHECK(labelFont.getHeight() == 14.0f);
    CHECK(labelFont.getTypefacePtr() == juce::Font(ui::font(14.0f, true)).getTypefacePtr());

    juce::TextButton button("x");
    CHECK(isInter(lnf.getTextButtonFont(button, 24)));
    juce::ComboBox box;
    box.setSize(100, 24);
    CHECK(isInter(lnf.getComboBoxFont(box)));
    CHECK(isInter(lnf.getPopupMenuFont()));
    CHECK(isInter(lnf.getAlertWindowTitleFont()));
    CHECK(isInter(lnf.getAlertWindowMessageFont()));
    CHECK(isInter(lnf.getAlertWindowFont()));
}

TEST_CASE("the embedded font keeps a label's letter spacing", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Label label;
    label.setFont(juce::Font(ui::font(22.0f, true).withKerningFactor(0.12f)));
    CHECK(lnf.getLabelFont(label).getExtraKerningFactor() == 0.12f);
}

TEST_CASE("the embedded font survives closing another editor", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor a, b;
    std::unique_ptr<juce::AudioProcessorEditor> editorA(a.createEditor());
    std::unique_ptr<juce::AudioProcessorEditor> editorB(b.createEditor());
    const auto before = juce::Font(ui::font(12.0f)).getTypefacePtr();
    editorA.reset();
    const auto after = juce::Font(ui::font(12.0f)).getTypefacePtr();
    CHECK(after == before);
    CHECK(isInter(juce::Font(ui::font(12.0f))));
    editorB.reset();
    CHECK(isInter(juce::Font(ui::font(12.0f)))); // ohne Editor wird neu geladen
}
```

In `tests/CMakeLists.txt` die Quellenliste von `DubgefahrenPluginTests` um `plugin/test_Look.cpp` erweitern:

```cmake
target_sources(DubgefahrenPluginTests PRIVATE plugin/test_PluginProcessor.cpp plugin/test_KitFile.cpp plugin/test_Config.cpp plugin/test_Editor.cpp plugin/test_CpuMeter.cpp plugin/test_SampleFiles.cpp plugin/test_SampleLoader.cpp plugin/test_Samples.cpp plugin/test_Look.cpp)
```

- [ ] **Step 3: Build ausführen und das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL, `plugin/ui/Fonts.h` nicht gefunden.

- [ ] **Step 4: Schrift einbetten und `Fonts` schreiben**

`plugin/CMakeLists.txt`, vor `add_library(dg_plugin_shared INTERFACE)` einfügen:

```cmake
juce_add_binary_data(dg_fonts
    HEADER_NAME DgFonts.h
    NAMESPACE dg_fonts
    SOURCES
        assets/fonts/Inter-Regular.ttf
        assets/fonts/Inter-SemiBold.ttf)
```

In `target_sources(dg_plugin_shared INTERFACE ...)` nach den `DgLookAndFeel`-Zeilen ergänzen:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/Fonts.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/Fonts.cpp
```

In `target_link_libraries(dg_plugin_shared INTERFACE ...)` nach `dg_engine` ergänzen:

```cmake
    dg_fonts
```

`plugin/ui/Fonts.h`:

```cpp
#pragma once
#include <juce_graphics/juce_graphics.h>

namespace dg::ui {

// Die eingebetteten Typefaces (Inter). Wer eine juce::SharedResourcePointer<EmbeddedFonts>
// hält, hält sie geladen; DgLookAndFeel tut das für die Lebensdauer des Editors.
struct EmbeddedFonts
{
    EmbeddedFonts();
    juce::Typeface::Ptr regular;
    juce::Typeface::Ptr semiBold;
};

// Eingebettete Schrift in der gegebenen Höhe. bold liefert den Schnitt SemiBold.
juce::FontOptions font(float height, bool bold = false);

// Gleiche Höhe, Fettung und Laufweite wie f, aber in der eingebetteten Schrift.
juce::Font withEmbeddedTypeface(const juce::Font& f);

} // namespace dg::ui
```

`plugin/ui/Fonts.cpp`:

```cpp
#include "plugin/ui/Fonts.h"
#include <algorithm>
#include <juce_core/juce_core.h>
#include "DgFonts.h"

namespace dg::ui {

EmbeddedFonts::EmbeddedFonts()
    : regular(juce::Typeface::createSystemTypefaceFor(dg_fonts::InterRegular_ttf, static_cast<std::size_t>(dg_fonts::InterRegular_ttfSize))),
      semiBold(juce::Typeface::createSystemTypefaceFor(dg_fonts::InterSemiBold_ttf, static_cast<std::size_t>(dg_fonts::InterSemiBold_ttfSize)))
{
}

juce::FontOptions font(float height, bool bold)
{
    // Kein statisches Objekt: Die Typefaces sollen vor dem Entladen der DLL freigegeben sein.
    const juce::SharedResourcePointer<EmbeddedFonts> fonts;
    return juce::FontOptions(bold ? fonts->semiBold : fonts->regular).withHeight(std::max(1.0f, height));
}

juce::Font withEmbeddedTypeface(const juce::Font& f)
{
    return juce::Font(font(f.getHeight(), f.isBold()).withKerningFactor(f.getExtraKerningFactor()));
}

} // namespace dg::ui
```

- [ ] **Step 5: Look-and-Feel gibt die Schrift aus**

`plugin/ui/DgLookAndFeel.h`: Include `#include "plugin/ui/Fonts.h"` ergänzen und die Klasse erweitern:

```cpp
class DgLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DgLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;

    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;

private:
    juce::SharedResourcePointer<EmbeddedFonts> fonts_; // hält die Typefaces geladen
};
```

`plugin/ui/DgLookAndFeel.cpp`, vor dem schließenden Namespace ergänzen:

```cpp
juce::Font DgLookAndFeel::getLabelFont(juce::Label& label) { return withEmbeddedTypeface(label.getFont()); }

juce::Font DgLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(font(std::min(15.0f, static_cast<float>(buttonHeight) * 0.6f)));
}

juce::Font DgLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return juce::Font(font(std::min(15.0f, static_cast<float>(box.getHeight()) * 0.85f)));
}

juce::Font DgLookAndFeel::getPopupMenuFont() { return juce::Font(font(15.0f)); }
juce::Font DgLookAndFeel::getAlertWindowTitleFont() { return juce::Font(font(17.0f, true)); }
juce::Font DgLookAndFeel::getAlertWindowMessageFont() { return juce::Font(font(15.0f)); }
juce::Font DgLookAndFeel::getAlertWindowFont() { return juce::Font(font(14.0f)); }
```

- [ ] **Step 6: Alle Font-Aufrufe der UI umstellen**

Jede Datei bekommt `#include "plugin/ui/Fonts.h"`. Ersetzungen (links alt, rechts neu):

| Stelle | alt | neu |
|---|---|---|
| `plugin/PluginEditor.cpp:23` | `juce::FontOptions(22.0f, juce::Font::bold)` | `ui::font(22.0f, true).withKerningFactor(0.12f)` |
| `plugin/ui/Controls.cpp:12` | `juce::FontOptions(12.0f)` | `font(12.0f)` |
| `plugin/ui/CpuMeter.cpp:15` | `juce::FontOptions(13.0f)` | `font(13.0f)` |
| `plugin/ui/FxPanel.cpp:40` | `juce::FontOptions(11.0f, juce::Font::bold)` | `font(11.0f, true)` |
| `plugin/ui/PadGrid.cpp:52` | `juce::FontOptions(11.0f)` | `font(11.0f)` |
| `plugin/ui/PadGrid.cpp:56` | `juce::FontOptions(13.0f)` | `font(13.0f)` |
| `plugin/ui/PadGrid.cpp:63` | `juce::FontOptions(13.0f)` | `font(13.0f)` |
| `plugin/ui/PadGrid.cpp:170` | `juce::FontOptions(11.0f)` | `font(11.0f)` |
| `plugin/ui/PerformancePanel.cpp:26` | `juce::FontOptions(11.0f, juce::Font::bold)` | `font(11.0f, true)` |
| `plugin/ui/SlotEditor.cpp:24` | `juce::FontOptions(18.0f, juce::Font::bold)` | `font(18.0f, true)` |
| `plugin/ui/SlotEditor.cpp:41` | `juce::FontOptions(15.0f)` | `font(15.0f)` |
| `plugin/ui/SlotEditor.cpp:116` | `juce::FontOptions(12.0f, juce::Font::bold)` | `font(12.0f, true)` |

Danach prüfen, dass nichts übrig ist:

Run: `git grep -n "FontOptions(" -- plugin`
Expected: nur Treffer in `plugin/ui/Fonts.cpp`.

- [ ] **Step 7: Konfigurieren, bauen, Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[look]"`
Expected: PASS, 4 Testfälle.

Falls die Symbole `dg_fonts::InterRegular_ttf` nicht gefunden werden: in `build\plugin\juce_binarydata_dg_fonts\JuceLibraryCode\DgFonts.h` die tatsächlichen Namen nachlesen und in `Fonts.cpp` übernehmen.

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

- [ ] **Step 8: Commit**

```bash
git add plugin/assets/fonts plugin/ui/Fonts.h plugin/ui/Fonts.cpp plugin/ui/DgLookAndFeel.h plugin/ui/DgLookAndFeel.cpp plugin/CMakeLists.txt tests/CMakeLists.txt tests/plugin/test_Look.cpp plugin/PluginEditor.cpp plugin/ui/Controls.cpp plugin/ui/CpuMeter.cpp plugin/ui/FxPanel.cpp plugin/ui/PadGrid.cpp plugin/ui/PerformancePanel.cpp plugin/ui/SlotEditor.cpp
git commit -m "feat(ui): embed Inter and use it for the whole editor (#15)"
```

---

### Task 2: melatonin_blur, Panels und Fensterhintergrund

Dieser Task klärt auch das erste Risiko der Spec: ob Schatten bei skaliertem Fenster richtig sitzen.

**Files:**
- Create: `plugin/ui/Surfaces.h`, `plugin/ui/Surfaces.cpp`
- Create: `tests/plugin/RenderTestHelpers.h`
- Modify: `CMakeLists.txt:9-19`, `plugin/CMakeLists.txt`
- Modify: `plugin/ui/DgLookAndFeel.h` (Palette)
- Modify: `plugin/ui/SlotEditor.cpp:109-112`, `plugin/ui/FxPanel.cpp:36-49`, `plugin/ui/PerformancePanel.cpp:21-24`
- Modify: `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp:69`
- Test: `tests/plugin/test_Look.cpp`

**Interfaces:**
- Consumes: `ui::font()` aus Task 1.
- Produces:
  - `colours::panelTop`, `panelBottom`, `highlight`, `shadow`, `groove`, `backgroundTop`, `backgroundBottom`, `padBase`
  - `void ui::drawPanelBody(juce::Graphics& g, juce::Rectangle<float> bounds)`
  - `class ui::PanelShadow { void render(juce::Graphics& g, juce::Rectangle<float> bounds); }`
  - `void ui::drawDivider(juce::Graphics& g, float x, float top, float bottom)`
  - `void ui::drawWindowBackground(juce::Graphics& g, juce::Rectangle<int> bounds)`
  - Test-Helfer `juce::Image dgtest::snapshot(juce::Component& c, float scale = 1.0f)` und `void dgtest::savePng(const juce::Image& img, const juce::String& name)`

- [ ] **Step 1: melatonin_blur einbinden**

`CMakeLists.txt`: nach `FetchContent_MakeAvailable(JUCE Catch2)` einfügen. Der Ordner muss `melatonin_blur` heißen, weil JUCE den Modulnamen aus dem Ordnernamen liest.

```cmake
FetchContent_Declare(melatonin_blur
    GIT_REPOSITORY https://github.com/sudara/melatonin_blur.git
    GIT_TAG v1.4
    GIT_SHALLOW ON
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/melatonin_blur)
FetchContent_MakeAvailable(melatonin_blur)
```

`plugin/CMakeLists.txt`: in `target_link_libraries(dg_plugin_shared INTERFACE ...)` nach `dg_fonts` ergänzen:

```cmake
    melatonin_blur
```

- [ ] **Step 2: Test-Helfer und fehlschlagende Tests schreiben**

`tests/plugin/RenderTestHelpers.h`:

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace dgtest {

inline juce::Image snapshot(juce::Component& c, float scale = 1.0f)
{
    return c.createComponentSnapshot(c.getLocalBounds(), true, scale);
}

// Schreibt das Bild nur, wenn DG_SNAPSHOT_DIR gesetzt ist (Sichtprüfung von Hand).
inline void savePng(const juce::Image& img, const juce::String& name)
{
    const auto dir = juce::SystemStats::getEnvironmentVariable("DG_SNAPSHOT_DIR", {});
    if (dir.isEmpty() || !img.isValid())
        return;
    const auto file = juce::File(dir).getChildFile(name + ".png");
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    if (auto out = file.createOutputStream())
        juce::PNGImageFormat().writeImageToStream(img, *out);
}

inline bool hasVisiblePixel(const juce::Image& img)
{
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt(x, y).getAlpha() > 0)
                return true;
    return false;
}

} // namespace dgtest
```

An `tests/plugin/test_Look.cpp` anhängen (Includes oben ergänzen: `#include "RenderTestHelpers.h"`, `#include "plugin/ui/Surfaces.h"`):

```cpp
TEST_CASE("a panel body is a vertical gradient with rounded corners", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Image img(juce::Image::ARGB, 100, 60, true);
    {
        juce::Graphics g(img);
        ui::drawPanelBody(g, img.getBounds().toFloat());
    }
    CHECK(img.getPixelAt(0, 0).getAlpha() == 0);         // Ecke bleibt frei
    CHECK(img.getPixelAt(50, 30).getAlpha() == 255);
    CHECK(img.getPixelAt(50, 8).getBrightness() > img.getPixelAt(50, 52).getBrightness());
}

TEST_CASE("a panel shadow reaches beyond the panel", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Image img(juce::Image::ARGB, 140, 100, true);
    ui::PanelShadow shadow;
    {
        juce::Graphics g(img);
        shadow.render(g, { 20.0f, 20.0f, 100.0f, 60.0f });
    }
    CHECK(img.getPixelAt(70, 84).getAlpha() > 0);  // unter dem Panel
    CHECK(img.getPixelAt(2, 2).getAlpha() == 0);   // weit weg: nichts
}

TEST_CASE("the editor paints at every window scale", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (const float scale : { 0.75f, 1.0f, 2.0f })
    {
        DubgefahrenProcessor p;
        p.clearSlot(2);
        p.setUiScale(scale);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        const auto img = dgtest::snapshot(*editor);
        REQUIRE(img.isValid());
        CHECK(img.getWidth() == juce::roundToInt(1000.0f * scale));
        // Punkt unten rechts im Slot-Editor-Panel (keine Controls) gegen den Fensterrand.
        const auto panel = img.getPixelAt(juce::roundToInt(980.0f * scale), juce::roundToInt(425.0f * scale));
        const auto window = img.getPixelAt(juce::roundToInt(4.0f * scale), juce::roundToInt(4.0f * scale));
        CHECK(panel != window);
        dgtest::savePng(img, "editor-" + juce::String(scale, 2));
    }
}
```

- [ ] **Step 3: Build ausführen und das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL, `plugin/ui/Surfaces.h` nicht gefunden. Der Configure-Schritt muss melatonin_blur nach `build/_deps/melatonin_blur` geholt haben.

- [ ] **Step 4: Palette erweitern**

`plugin/ui/DgLookAndFeel.h`, im Namespace `colours` nach `danger` ergänzen (`playing` bleibt bis Task 4):

```cpp
inline const juce::Colour backgroundTop { 0xff15181b };
inline const juce::Colour backgroundBottom { 0xff0f1113 };
inline const juce::Colour panelTop { 0xff1f2329 };
inline const juce::Colour panelBottom { 0xff181b20 };
inline const juce::Colour highlight { 0x12ffffff }; // Lichtkante: 7 % Weiß
inline const juce::Colour shadow { 0x99000000 };
inline const juce::Colour groove { 0xff0c0e10 };    // vertiefte Linien und Konturen
inline const juce::Colour padBase { 0xff4cd07d };
```

- [ ] **Step 5: `Surfaces` schreiben**

`plugin/ui/Surfaces.h`:

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "melatonin_blur/melatonin_blur.h"

namespace dg::ui {

inline constexpr float kPanelCorner = 8.0f;

// Körper eines Panels: Verlauf und Lichtkante oben. Der Schatten kommt von PanelShadow.
void drawPanelBody(juce::Graphics& g, juce::Rectangle<float> bounds);

// Weicher Schlagschatten eines Panels. Ragt über bounds hinaus und wird deshalb vom
// Elternteil unter dem Panel gezeichnet. Als Member halten: das Objekt cacht den Schatten.
class PanelShadow
{
public:
    void render(juce::Graphics& g, juce::Rectangle<float> bounds);

private:
    melatonin::DropShadow shadow_;
    bool configured_ = false;
};

// Senkrechter Gruppentrenner als Doppellinie (dunkel, daneben hell).
void drawDivider(juce::Graphics& g, float x, float top, float bottom);

void drawWindowBackground(juce::Graphics& g, juce::Rectangle<int> bounds);

} // namespace dg::ui
```

`plugin/ui/Surfaces.cpp`:

```cpp
#include "plugin/ui/Surfaces.h"
#include "plugin/ui/DgLookAndFeel.h"

namespace dg::ui {

void drawPanelBody(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (bounds.isEmpty())
        return;
    g.setGradientFill(juce::ColourGradient(colours::panelTop, 0.0f, bounds.getY(), colours::panelBottom, 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, kPanelCorner);
    g.setColour(colours::highlight);
    g.fillRect(juce::Rectangle<float>(bounds.getX() + kPanelCorner, bounds.getY(), bounds.getWidth() - 2.0f * kPanelCorner, 1.0f));
}

void PanelShadow::render(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (bounds.isEmpty())
        return;
    if (!configured_)
    {
        shadow_ = melatonin::DropShadow(colours::shadow, 10, { 0, 3 });
        configured_ = true;
    }
    juce::Path p;
    p.addRoundedRectangle(bounds, kPanelCorner);
    shadow_.render(g, p);
}

void drawDivider(juce::Graphics& g, float x, float top, float bottom)
{
    g.setColour(colours::groove);
    g.fillRect(juce::Rectangle<float>(x, top, 1.0f, bottom - top));
    g.setColour(colours::highlight);
    g.fillRect(juce::Rectangle<float>(x + 1.0f, top, 1.0f, bottom - top));
}

void drawWindowBackground(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setGradientFill(juce::ColourGradient(colours::backgroundTop, 0.0f, static_cast<float>(bounds.getY()), colours::backgroundBottom, 0.0f,
                                           static_cast<float>(bounds.getBottom()), false));
    g.fillRect(bounds);
}

} // namespace dg::ui
```

`plugin/CMakeLists.txt`: in `target_sources` nach den `Fonts`-Zeilen ergänzen:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/Surfaces.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/Surfaces.cpp
```

- [ ] **Step 6: Panels und Editor umstellen**

Alle drei Panel-Dateien bekommen `#include "plugin/ui/Surfaces.h"`.

`plugin/ui/SlotEditor.cpp`, in `paint` die zwei Zeilen

```cpp
    g.setColour(colours::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
```

ersetzen durch

```cpp
    drawPanelBody(g, getLocalBounds().toFloat());
```

Dieselbe Ersetzung in `plugin/ui/PerformancePanel.cpp` (`paint`) und `plugin/ui/FxPanel.cpp` (`paint`). In `FxPanel::paint` zusätzlich die zwei Trenner-Zeilen

```cpp
        g.setColour(colours::outline);
        g.drawVerticalLine(x - 4, 6.0f, static_cast<float>(getHeight() - 6));
```

ersetzen durch

```cpp
        drawDivider(g, static_cast<float>(x - 5), 6.0f, static_cast<float>(getHeight() - 6));
```

`plugin/PluginEditor.h`: Includes `#include <array>` und `#include "plugin/ui/Surfaces.h"` ergänzen, im privaten Teil nach `ui::PerformancePanel perf_;`:

```cpp
    std::array<ui::PanelShadow, 3> panelShadows_;
```

`plugin/PluginEditor.cpp`, `paint` ersetzen:

```cpp
void DubgefahrenEditor::paint(juce::Graphics& g)
{
    ui::drawWindowBackground(g, getLocalBounds());
    // Schatten ragen über ihre Komponenten hinaus, deshalb zeichnet sie der Editor darunter,
    // im Koordinatensystem der skalierten content_-Komponente.
    g.addTransform(content_.getTransform());
    panelShadows_[0].render(g, slotEditor_.getBounds().toFloat());
    panelShadows_[1].render(g, fx_.getBounds().toFloat());
    panelShadows_[2].render(g, perf_.getBounds().toFloat());
}
```

- [ ] **Step 7: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[look]"`
Expected: PASS, 7 Testfälle.

- [ ] **Step 8: Risiko „Schärfe bei skaliertem Fenster“ prüfen**

```powershell
$env:DG_SNAPSHOT_DIR = Join-Path $env:TEMP 'dg-snapshots'
build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "the editor paints at every window scale"
Remove-Item Env:DG_SNAPSHOT_DIR
```

Die drei Dateien `editor-0.75.png`, `editor-1.00.png`, `editor-2.00.png` in `%TEMP%\dg-snapshots` ansehen. Zu prüfen bei jeder Größe:
- Der Schatten jedes Panels liegt direkt unter dem Panel (leicht nach unten versetzt), nicht daneben.
- Der Schatten ist weich und an keiner Kante hart abgeschnitten.
- Schrift und Panel-Kanten sind scharf.

Sitzt der Schatten bei 0,75× oder 2× falsch: **stoppen und melden**, nicht weiterbauen. Das ist der in der Spec benannte Fall, für den eine andere Schattentechnik gewählt werden muss.

- [ ] **Step 9: Alle Tests, dann Commit**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

```bash
git add CMakeLists.txt plugin/CMakeLists.txt plugin/ui/Surfaces.h plugin/ui/Surfaces.cpp plugin/ui/DgLookAndFeel.h plugin/ui/SlotEditor.cpp plugin/ui/FxPanel.cpp plugin/ui/PerformancePanel.cpp plugin/PluginEditor.h plugin/PluginEditor.cpp tests/plugin/RenderTestHelpers.h tests/plugin/test_Look.cpp
git commit -m "feat(ui): panels with gradient body and soft shadow via melatonin_blur (#15)"
```

---

### Task 3: Helligkeitskurve der Pads

**Files:**
- Create: `plugin/ui/PadGlow.h`
- Create: `tests/plugin/test_PadGlow.cpp`
- Modify: `plugin/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `inline constexpr float dg::ui::kPadGlowHold = 0.7f`
  - `float dg::ui::advancePadGlow(float brightness, bool active, bool wasActive, float dtSeconds)`

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

`tests/plugin/test_PadGlow.cpp`:

```cpp
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include "plugin/ui/PadGlow.h"

using namespace dg::ui;
using Catch::Approx;

namespace {
constexpr float kTick = 1.0f / 30.0f;
} // namespace

TEST_CASE("a trigger sets full brightness", "[padglow]")
{
    CHECK(advancePadGlow(0.0f, true, false, kTick) == 1.0f);
    CHECK(advancePadGlow(0.3f, true, false, kTick) == 1.0f); // erneuter Trigger im Ausklingen
    CHECK(advancePadGlow(0.0f, true, false, 0.0f) == 1.0f);
}

TEST_CASE("a held pad falls to the hold level and stays there", "[padglow]")
{
    float b = 1.0f;
    float previous = b;
    for (int i = 0; i < 30; ++i) // 1 s
    {
        b = advancePadGlow(b, true, true, kTick);
        CHECK(b <= previous);
        CHECK(b >= kPadGlowHold);
        previous = b;
    }
    CHECK(b == kPadGlowHold); // eingerastet, nicht nur nahe dran
    CHECK(advancePadGlow(b, true, true, kTick) == kPadGlowHold);
}

TEST_CASE("the hold level is practically reached after 250 ms", "[padglow]")
{
    const float b = advancePadGlow(1.0f, true, true, 0.25f);
    CHECK(b == Approx(kPadGlowHold).margin(0.02));
}

TEST_CASE("a released pad fades to zero within 200 ms", "[padglow]")
{
    float b = kPadGlowHold;
    b = advancePadGlow(b, false, true, 0.1f);
    CHECK(b == Approx(0.2f).margin(0.001));
    b = advancePadGlow(b, false, false, 0.1f);
    CHECK(b == 0.0f);
    CHECK(advancePadGlow(1.0f, false, true, 0.2f) == 0.0f);
    CHECK(advancePadGlow(0.0f, false, false, kTick) == 0.0f);
}

TEST_CASE("huge, zero and negative time steps keep the brightness valid", "[padglow]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(advancePadGlow(1.0f, true, true, 3600.0f) == kPadGlowHold);
    CHECK(advancePadGlow(1.0f, false, true, 3600.0f) == 0.0f);
    CHECK(advancePadGlow(0.9f, true, true, 0.0f) == 0.9f);
    CHECK(advancePadGlow(0.9f, true, true, -1.0f) == 0.9f);
    CHECK(advancePadGlow(0.5f, false, false, -1.0f) == 0.5f);
    CHECK(advancePadGlow(0.9f, true, true, nan) == 0.9f);
    CHECK(advancePadGlow(nan, true, true, kTick) == kPadGlowHold);
    CHECK(advancePadGlow(nan, false, false, kTick) == 0.0f);
    CHECK(advancePadGlow(7.0f, false, false, 0.0f) == 1.0f);  // Eingabe wird auf 0…1 begrenzt
    CHECK(advancePadGlow(-3.0f, false, false, 0.0f) == 0.0f);
}

TEST_CASE("a pad that becomes active below the hold level rises to it", "[padglow]")
{
    // Kann nur auftreten, wenn wasActive schon gesetzt war (kein Trigger): nie über den Haltewert hinaus.
    float b = 0.2f;
    for (int i = 0; i < 30; ++i)
    {
        b = advancePadGlow(b, true, true, kTick);
        CHECK(b <= kPadGlowHold);
    }
    CHECK(b == kPadGlowHold);
}
```

In `tests/CMakeLists.txt` die Quellenliste von `DubgefahrenPluginTests` um `plugin/test_PadGlow.cpp` erweitern.

- [ ] **Step 2: Build ausführen und das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 configure`
Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL, `plugin/ui/PadGlow.h` nicht gefunden.

- [ ] **Step 3: `PadGlow.h` schreiben**

```cpp
#pragma once
#include <algorithm>
#include <cmath>

namespace dg::ui {

inline constexpr float kPadGlowHold = 0.7f;         // Helligkeit eines gehaltenen Pads
inline constexpr float kPadGlowTau = 0.08f;         // Zeitkonstante des Abfalls auf den Haltewert (s)
inline constexpr float kPadGlowFadeSeconds = 0.2f;  // Ausklingen von 1 auf 0 (s)
inline constexpr float kPadGlowSnap = 0.005f;       // ab diesem Abstand rastet der Haltewert ein

// Schreibt die Helligkeit (0…1) eines Pads um dtSeconds fort.
// active: das Pad klingt gerade; wasActive: Zustand im vorigen Schritt.
// Ungültige Eingaben (NaN, negatives dt, Helligkeit außerhalb 0…1) werden entschärft.
inline float advancePadGlow(float brightness, bool active, bool wasActive, float dtSeconds)
{
    const float dt = dtSeconds > 0.0f ? dtSeconds : 0.0f; // fängt auch NaN
    const float b = std::isfinite(brightness) ? std::clamp(brightness, 0.0f, 1.0f) : (active ? kPadGlowHold : 0.0f);

    if (active)
    {
        if (!wasActive)
            return 1.0f;
        const float next = kPadGlowHold + (b - kPadGlowHold) * std::exp(-dt / kPadGlowTau);
        return std::abs(next - kPadGlowHold) < kPadGlowSnap ? kPadGlowHold : next;
    }
    return std::max(0.0f, b - dt / kPadGlowFadeSeconds);
}

} // namespace dg::ui
```

`plugin/CMakeLists.txt`: in `target_sources` nach den `Surfaces`-Zeilen ergänzen:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/PadGlow.h
```

- [ ] **Step 4: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[padglow]"`
Expected: PASS, 6 Testfälle.

- [ ] **Step 5: Commit**

```bash
git add plugin/ui/PadGlow.h plugin/CMakeLists.txt tests/CMakeLists.txt tests/plugin/test_PadGlow.cpp
git commit -m "feat(ui): brightness curve for playing pads (#15)"
```

---

### Task 4: Pads – Körper, Glow, Animation, Legende

Dieser Task klärt auch das zweite Risiko der Spec: die Leistung bei 16 animierten Pads.

**Files:**
- Modify: `plugin/ui/PadGrid.h`, `plugin/ui/PadGrid.cpp`
- Modify: `plugin/ui/DgLookAndFeel.h` (`playing` entfernen, Pad-Farben)
- Modify: `plugin/PluginEditor.cpp` (Konstruktor, `paint`)
- Test: `tests/plugin/test_Look.cpp`

**Interfaces:**
- Consumes: `advancePadGlow`, `kPadGlowHold` (Task 3); `ui::font()` (Task 1); `colours::padBase`, `highlight`, `shadow`, `groove` (Task 2); `dgtest::snapshot`, `dgtest::savePng` (Task 2).
- Produces (in `PadGrid`):
  - `void paintGlows(juce::Graphics& g)` – Glow aller Pads in PadGrid-Koordinaten
  - `std::function<void(juce::Rectangle<int>)> onGlowChanged` – Bereich in PadGrid-Koordinaten
  - `float padBrightness(int slot) const`
  - `void setPadBaseColour(int slot, juce::Colour colour)` und `juce::Colour padBaseColour(int slot) const` (Vorbereitung für #14)
  - `static constexpr int kGlowReach = 24`

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

An `tests/plugin/test_Look.cpp` anhängen (Include oben ergänzen: `#include "plugin/ui/PadGrid.h"`, `#include "plugin/ui/PadGlow.h"`):

```cpp
TEST_CASE("a pad lights up on trigger and fades after release", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    grid.setSize(360, 384);

    CHECK(grid.padBrightness(0) == 0.0f);
    grid.setPadStates(1u, 0u, 0);
    CHECK(grid.padBrightness(0) == 1.0f);
    CHECK(grid.padBrightness(1) == 0.0f);

    juce::Thread::sleep(400);
    grid.setPadStates(1u, 0u, 0);
    CHECK(grid.padBrightness(0) == ui::kPadGlowHold);

    grid.setPadStates(0u, 0u, 0);
    juce::Thread::sleep(250);
    grid.setPadStates(0u, 0u, 0);
    CHECK(grid.padBrightness(0) == 0.0f);
}

TEST_CASE("an empty pad never lights up", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    p.clearSlot(2);
    ui::PadGrid grid(p);
    grid.setSize(360, 384);
    grid.setPadStates(1u << 2, 0u, 0);
    CHECK(grid.padBrightness(2) == 0.0f);

    // Ein leuchtendes Pad wird geleert: Es klingt aus, statt weiter zu leuchten.
    grid.setPadStates(1u << 5, 0u, 0);
    CHECK(grid.padBrightness(5) == 1.0f);
    p.clearSlot(5);
    grid.refreshNames();
    juce::Thread::sleep(250);
    grid.setPadStates(1u << 5, 0u, 0);
    CHECK(grid.padBrightness(5) == 0.0f);
}

TEST_CASE("a changing glow reports the pad area plus its reach", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    grid.setSize(360, 384);
    grid.setPadStates(0u, 0u, 0); // Ausgangszustand, Fokus auf Pad 1

    std::vector<juce::Rectangle<int>> areas;
    grid.onGlowChanged = [&areas](juce::Rectangle<int> a) { areas.push_back(a); };

    grid.setPadStates(1u << 3, 0u, 0);
    REQUIRE(areas.size() == 1);
    CHECK(areas[0] == grid.pad(3).getBounds().expanded(ui::PadGrid::kGlowReach));

    areas.clear();
    grid.setPadStates(1u << 3, 0u, 1); // Fokus wandert von Pad 1 zu Pad 2
    CHECK(areas.size() == 3);          // Pad 4 fällt auf den Haltewert, Pad 1 und 2 wechseln den Fokus-Glow
}

TEST_CASE("pads use the shared base colour until one is assigned", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    CHECK(grid.padBaseColour(0) == ui::colours::padBase);
    grid.setPadBaseColour(0, juce::Colours::red);
    CHECK(grid.padBaseColour(0) == juce::Colours::red);
    CHECK(grid.padBaseColour(1) == ui::colours::padBase);
}

TEST_CASE("pad glows are drawn outside the pads and only for lit or focused pads", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::PadGrid grid(p);
    grid.setSize(360, 384);

    const auto render = [&grid] {
        juce::Image img(juce::Image::ARGB, 360 + 2 * ui::PadGrid::kGlowReach, 384 + 2 * ui::PadGrid::kGlowReach, true);
        juce::Graphics g(img);
        g.setOrigin(ui::PadGrid::kGlowReach, ui::PadGrid::kGlowReach);
        grid.paintGlows(g);
        return img;
    };

    grid.setPadStates(0u, 0u, -1);
    CHECK_FALSE(dgtest::hasVisiblePixel(render()));

    grid.setPadStates(1u, 0u, -1); // Pad 1 liegt unten links
    const auto lit = render();
    CHECK(dgtest::hasVisiblePixel(lit));
    // Links neben dem Raster, auf Höhe von Pad 1: Glow ragt über das Raster hinaus.
    const auto padBounds = grid.pad(0).getBounds();
    CHECK(lit.getPixelAt(ui::PadGrid::kGlowReach - 3, ui::PadGrid::kGlowReach + padBounds.getCentreY()).getAlpha() > 0);
}

TEST_CASE("sixteen glowing pads paint within one timer tick", "[look][perf]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DubgefahrenProcessor p;
    ui::DgLookAndFeel lnf;
    ui::PadGrid grid(p);
    grid.setLookAndFeel(&lnf);
    grid.setSize(360, 384);

    juce::Image img(juce::Image::ARGB, 720, 768, true); // 2× Fenstergröße
    constexpr int frames = 60;
    const double start = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < frames; ++i)
    {
        grid.setPadStates(i % 8 < 4 ? 0xffffu : 0u, 0x00f0u, i % 16); // Trigger, Halten, Ausklingen im Wechsel
        juce::Graphics g(img);
        g.addTransform(juce::AffineTransform::scale(2.0f));
        grid.paintGlows(g);
        grid.paintEntireComponent(g, false);
    }
    const double perFrame = (juce::Time::getMillisecondCounterHiRes() - start) / frames;
    WARN("pad frame at 2x: " << perFrame << " ms");
    CHECK(perFrame < 33.0); // ein Timer-Tick bei 30 Hz
    dgtest::savePng(img, "pads-2.00");
    grid.setLookAndFeel(nullptr);
}
```

- [ ] **Step 2: Build ausführen und das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Expected: FAIL, `padBrightness`, `onGlowChanged`, `paintGlows`, `kGlowReach`, `padBaseColour` fehlen in `PadGrid`.

- [ ] **Step 3: Palette anpassen**

`plugin/ui/DgLookAndFeel.h`: die Zeile `inline const juce::Colour playing { 0xff4cd07d };` löschen und nach `padBase` ergänzen:

```cpp
inline const juce::Colour padTop { 0xff27302f };
inline const juce::Colour padBottom { 0xff161b1b };
inline const juce::Colour padEmpty { 0xff101214 };
inline const juce::Colour padTextLit { 0xff06240f }; // Text auf leuchtendem Pad
```

- [ ] **Step 4: `PadGrid.h` erweitern**

Im öffentlichen Teil von `PadGrid` ergänzen:

```cpp
    // So weit ragt der Glow höchstens über ein Pad hinaus (px im Basis-Layout).
    static constexpr int kGlowReach = 24;

    // Glow aller Pads in den Koordinaten dieses Rasters. Ragt über die Pads und das Raster
    // hinaus und wird deshalb vom Editor unter den Komponenten gezeichnet.
    void paintGlows(juce::Graphics& g);
    // Ein Glow hat sich geändert: Bereich (Raster-Koordinaten) neu zeichnen.
    std::function<void(juce::Rectangle<int>)> onGlowChanged;

    float padBrightness(int slot) const;
    // Grundfarbe eines Pads: ruhend gedämpft, spielend hell. Vorerst für alle gleich (siehe #14).
    void setPadBaseColour(int slot, juce::Colour colour);
    juce::Colour padBaseColour(int slot) const;
```

Im privaten Teil ergänzen:

```cpp
    void glowChanged(int slot);
    double lastTickMs_ = 0.0;
```

- [ ] **Step 5: `Pad` neu zeichnen**

`plugin/ui/PadGrid.cpp`: Includes ergänzen:

```cpp
#include "melatonin_blur/melatonin_blur.h"
#include "plugin/ui/Fonts.h"
#include "plugin/ui/PadGlow.h"
```

Vor der Klasse `Pad` im Namespace `dg::ui`:

```cpp
namespace {
constexpr float kPadInset = 5.0f;  // Abstand des Pad-Körpers zum Rand seiner Zelle: Platz für Schatten und Glow
constexpr float kPadCorner = 6.0f;
constexpr float kGlowAlpha = 0.75f; // Deckkraft des Glows bei voller Helligkeit
} // namespace
```

In der Klasse `Pad`: `setState`, `paint` und die Member ersetzen; `setContent`, `isEmpty`, `isSample`, `isMissing`, `mouseDown`, `mouseUp`, Konstruktor und Destruktor bleiben unverändert.

```cpp
    // Liefert true, wenn sich etwas geändert hat, das den Glow betrifft (Fokus).
    bool setState(bool latched, bool focus, bool selected)
    {
        if (latched == latched_ && focus == focus_ && selected == selected_)
            return false;
        const bool focusChanged = focus != focus_;
        latched_ = latched;
        focus_ = focus;
        selected_ = selected;
        repaint();
        return focusChanged;
    }

    // Schreibt die Helligkeit fort. Leere Pads und Pads mit fehlendem Sample leuchten nie.
    // Liefert true, wenn sich die Helligkeit geändert hat.
    bool advanceGlow(bool active, float dtSeconds)
    {
        const bool lit = active && !empty_ && !missing_;
        const float next = advancePadGlow(brightness_, lit, wasLit_, dtSeconds);
        wasLit_ = lit;
        if (next == brightness_)
            return false;
        brightness_ = next;
        repaint();
        return true;
    }

    float brightness() const { return brightness_; }
    juce::Colour baseColour() const { return baseColour_; }
    void setBaseColour(juce::Colour c)
    {
        if (c == baseColour_)
            return;
        baseColour_ = c;
        repaint();
    }

    juce::Rectangle<float> bodyBounds() const { return getLocalBounds().toFloat().reduced(kPadInset); }

    // Glow in den Koordinaten des Rasters.
    void paintGlow(juce::Graphics& g)
    {
        const auto r = bodyBounds().translated(static_cast<float>(getX()), static_cast<float>(getY()));
        if (r.isEmpty())
            return;
        juce::Path body;
        body.addRoundedRectangle(r, kPadCorner);
        if (!empty_ && brightness_ > 0.0f)
        {
            // Nur die Farbe ändert sich: der gecachte Blur bleibt gültig.
            playGlow_.setColor(baseColour_.withAlpha(kGlowAlpha * brightness_));
            playGlow_.render(g, body);
        }
        if (focus_)
            focusGlow_.render(g, body);
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = bodyBounds();
        if (r.isEmpty())
            return;
        juce::Path body;
        body.addRoundedRectangle(r, kPadCorner);

        if (empty_)
        {
            // Leeres Pad: vertieft, ohne Grundfarbe und ohne Schlagschatten.
            g.setColour(colours::padEmpty);
            g.fillPath(body);
            inset_.render(g, body);
        }
        else
        {
            drop_.render(g, body);
            g.setGradientFill(juce::ColourGradient(colours::padTop, 0.0f, r.getY(), colours::padBottom, 0.0f, r.getBottom(), false));
            g.fillPath(body);
            if (!missing_)
            {
                // Schimmer der Grundfarbe am unteren Rand.
                g.setGradientFill(juce::ColourGradient(baseColour_.withAlpha(0.0f), 0.0f, r.getCentreY(), baseColour_.withAlpha(0.35f), 0.0f,
                                                       r.getBottom(), false));
                g.fillPath(body);
            }
            if (brightness_ > 0.0f)
            {
                // Spielend: Körper in der Grundfarbe, oben heller.
                g.setGradientFill(juce::ColourGradient(baseColour_.brighter(0.7f).withAlpha(brightness_), 0.0f, r.getY(),
                                                       baseColour_.withAlpha(brightness_), 0.0f, r.getBottom(), false));
                g.fillPath(body);
            }
            g.setColour(colours::highlight.withMultipliedAlpha(1.0f + brightness_));
            g.fillRect(juce::Rectangle<float>(r.getX() + kPadCorner, r.getY() + 1.0f, r.getWidth() - 2.0f * kPadCorner, 1.0f));
        }

        // Farbe zeigt Fokus, Dicke zeigt Auswahl. Ohne beides nur die dunkle Kontur.
        if (focus_ || selected_)
        {
            g.setColour(focus_ ? colours::accent : colours::text.withAlpha(0.6f));
            g.drawRoundedRectangle(r, kPadCorner, selected_ ? 2.5f : 1.5f);
        }
        else
        {
            g.setColour(colours::groove);
            g.drawRoundedRectangle(r, kPadCorner, 1.0f);
        }

        const auto text = r.reduced(6.0f);
        const auto dim = colours::textDim.interpolatedWith(colours::padTextLit, brightness_);
        g.setColour(dim);
        g.setFont(font(11.0f));
        g.drawText(juce::String(slot_ + 1), text, juce::Justification::topLeft);
        // Fehlendes Sample: Name und Symbol in Warnfarbe, der Slot ist stumm.
        g.setColour(empty_ ? colours::textDim : (missing_ ? colours::warning : colours::text.interpolatedWith(colours::padTextLit, brightness_)));
        g.setFont(font(13.0f));
        g.drawFittedText(empty_ ? juce::String("Empty") : name_, text.toNearestInt(), juce::Justification::centred, 2);

        if (sample_ && !empty_)
        {
            g.setColour(missing_ ? colours::warning : dim);
            g.setFont(font(13.0f));
            g.drawText(u8("∿"), juce::Rectangle<float>(r.getRight() - 34.0f, r.getY() + 3.0f, 16.0f, 14.0f), juce::Justification::centred);
        }

        if (latched_)
        {
            juce::Path dot;
            dot.addEllipse(r.getRight() - 14.0f, r.getY() + 6.0f, 8.0f, 8.0f);
            dotGlow_.render(g, dot);
            g.setColour(colours::latched);
            g.fillPath(dot);
        }
    }
```

Member der Klasse `Pad` (ersetzt die bisherige Zeile mit den `bool`-Feldern):

```cpp
    PadGrid& owner_;
    const int slot_;
    juce::String name_;
    juce::Colour baseColour_ { colours::padBase };
    float brightness_ = 0.0f;
    bool wasLit_ = false;
    bool latched_ = false, focus_ = false, selected_ = false, held_ = false, empty_ = false, sample_ = false, missing_ = false;
    // Als Member: melatonin cacht den berechneten Schatten im Objekt.
    melatonin::DropShadow drop_ { colours::shadow, 4, { 0, 1 } };
    melatonin::InnerShadow inset_ { juce::Colours::black.withAlpha(0.8f), 6, { 0, 2 } };
    melatonin::DropShadow playGlow_ { colours::padBase, 14, { 0, 0 }, 2 };
    melatonin::DropShadow focusGlow_ { colours::accent.withAlpha(0.7f), 9, { 0, 0 }, 1 };
    melatonin::DropShadow dotGlow_ { colours::latched.withAlpha(0.9f), 5 };
```

Der Glow reicht 14 px (Radius) + 2 px (Spread) = 16 px über den Pad-Körper hinaus, also 11 px über die Zelle; `kGlowReach = 24` deckt das mit Reserve ab.

- [ ] **Step 6: `PadGrid` anpassen**

`setPadStates` ersetzen:

```cpp
void PadGrid::setPadStates(std::uint32_t active, std::uint32_t latched, int focus)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = lastTickMs_ > 0.0 ? static_cast<float>((now - lastTickMs_) / 1000.0) : 0.0f;
    lastTickMs_ = now;

    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& pad = *pads_[static_cast<std::size_t>(s)];
        const bool selected = pad.getProperties()["selected"];
        const bool focusChanged = pad.setState((latched >> s) & 1u, s == focus, selected);
        const bool glowMoved = pad.advanceGlow((active >> s) & 1u, dt);
        if (focusChanged || glowMoved)
            glowChanged(s);
    }
}
```

Neue Funktionen (nach `isEmpty`):

```cpp
void PadGrid::paintGlows(juce::Graphics& g)
{
    for (auto& p : pads_)
        p->paintGlow(g);
}

void PadGrid::glowChanged(int slot)
{
    if (onGlowChanged)
        onGlowChanged(pads_[static_cast<std::size_t>(slot)]->getBounds().expanded(kGlowReach));
}

float PadGrid::padBrightness(int slot) const { return pads_[static_cast<std::size_t>(slot)]->brightness(); }

void PadGrid::setPadBaseColour(int slot, juce::Colour colour)
{
    pads_[static_cast<std::size_t>(slot)]->setBaseColour(colour);
    glowChanged(slot);
}

juce::Colour PadGrid::padBaseColour(int slot) const { return pads_[static_cast<std::size_t>(slot)]->baseColour(); }
```

`setContent` in `Pad` ändert `empty_` und `missing_`; damit ein eben geleertes Pad seinen Glow verliert, in `PadGrid::refreshNames` nach der Schleife ergänzen:

```cpp
    if (onGlowChanged)
        onGlowChanged(getLocalBounds().expanded(kGlowReach));
```

Legende in `PadGrid::paint` ersetzen (gezeichnete Marken statt Schriftzeichen, damit nichts von der Glyphen-Abdeckung der Schrift abhängt):

```cpp
void PadGrid::paint(juce::Graphics& g)
{
    auto legend = getLocalBounds().removeFromBottom(20).toFloat().withTrimmedLeft(kPadInset);
    g.setFont(font(11.0f));
    const auto item = [&g, &legend](juce::Colour colour, const char* text, float width, bool diamond) {
        auto cell = legend.removeFromLeft(width);
        const auto mark = cell.removeFromLeft(12.0f).withSizeKeepingCentre(7.0f, 7.0f);
        g.setColour(colour);
        if (diamond)
        {
            juce::Path p;
            p.addQuadrilateral(mark.getCentreX(), mark.getY(), mark.getRight(), mark.getCentreY(), mark.getCentreX(), mark.getBottom(),
                               mark.getX(), mark.getCentreY());
            g.fillPath(p);
        }
        else
            g.fillEllipse(mark);
        g.setColour(colours::textDim);
        g.drawText(text, cell, juce::Justification::centredLeft);
    };
    item(colours::padBase, "playing", 80.0f, false);
    item(colours::accent, "focus", 80.0f, true);
    item(colours::latched, "latched", 90.0f, false);
}
```

- [ ] **Step 7: Editor zeichnet den Glow**

`plugin/PluginEditor.cpp`, im Konstruktor nach `pads_.onEmptyClick = ...;`:

```cpp
    pads_.onGlowChanged = [this](juce::Rectangle<int> area) { repaint(getLocalArea(&pads_, area)); };
```

In `paint` am Ende ergänzen:

```cpp
    g.setOrigin(pads_.getPosition());
    pads_.paintGlows(g);
```

- [ ] **Step 8: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[look]"`
Expected: PASS, 13 Testfälle. Die Warnung `pad frame at 2x: … ms` nennt die gemessene Zeit.

- [ ] **Step 9: Risiko „Leistung“ und Aussehen prüfen**

```powershell
$env:DG_SNAPSHOT_DIR = Join-Path $env:TEMP 'dg-snapshots'
build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[look]"
Remove-Item Env:DG_SNAPSHOT_DIR
```

- Zeit: Liegt `pad frame at 2x` über 33 ms, greift die Ausweichlösung der Spec: den Glow pro Pad einmal als `juce::Image` vorberechnen und mit `g.setOpacity(brightness)` zeichnen. **Dann stoppen und melden**, bevor weitergebaut wird.
- `pads-2.00.png` ansehen: Pads haben Körper mit Verlauf; leuchtende Pads sind in der Grundfarbe gefüllt; gelatchte Pads (5 bis 8) haben den blauen Punkt; der Fokus-Ring ist gelb.
- `editor-2.00.png` und `editor-0.75.png` ansehen: leeres Pad 3 wirkt vertieft; die Legende zeigt drei Marken mit Text.

- [ ] **Step 10: Alle Tests, dann Commit**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

```bash
git add plugin/ui/PadGrid.h plugin/ui/PadGrid.cpp plugin/ui/DgLookAndFeel.h plugin/PluginEditor.cpp tests/plugin/test_Look.cpp
git commit -m "feat(ui): pads with body, base colour and animated glow (#15)"
```

---

### Task 5: Regler, Buttons, Auswahlboxen, Schalter

**Files:**
- Modify: `plugin/ui/DgLookAndFeel.h`, `plugin/ui/DgLookAndFeel.cpp`
- Test: `tests/plugin/test_Look.cpp`

**Interfaces:**
- Consumes: `ui::font()` (Task 1); `colours::groove`, `highlight`, `shadow` (Task 2); `dgtest::hasVisiblePixel`, `dgtest::savePng` (Task 2).
- Produces: Überschreibungen `drawRotarySlider`, `drawButtonBackground`, `drawComboBox`, `drawToggleButton` in `DgLookAndFeel`.

- [ ] **Step 1: Die Tests schreiben**

An `tests/plugin/test_Look.cpp` anhängen:

```cpp
namespace {
juce::Image drawKnob(ui::DgLookAndFeel& lnf, juce::Slider& slider, int size, float pos)
{
    juce::Image img(juce::Image::ARGB, std::max(1, size), std::max(1, size), true);
    juce::Graphics g(img);
    lnf.drawRotarySlider(g, 0, 0, size, size, pos, juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, slider);
    return img;
}
} // namespace

TEST_CASE("controls draw nothing harmful into tiny or empty areas", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Slider slider;
    slider.setRange(0.0, 1.0);
    for (const int size : { 0, 1, 3, 8, 11 })
        CHECK_NOTHROW(drawKnob(lnf, slider, size, 0.5f));
    CHECK_FALSE(dgtest::hasVisiblePixel(drawKnob(lnf, slider, 3, 0.5f)));

    juce::Image img(juce::Image::ARGB, 8, 8, true);
    juce::Graphics g(img);
    juce::TextButton button("x");
    button.setSize(0, 0);
    CHECK_NOTHROW(lnf.drawButtonBackground(g, button, juce::Colours::grey, false, false));
    juce::ToggleButton toggle("x");
    toggle.setSize(0, 0);
    CHECK_NOTHROW(lnf.drawToggleButton(g, toggle, false, false));
    juce::ComboBox box;
    CHECK_NOTHROW(lnf.drawComboBox(g, 0, 0, false, 0, 0, 0, 0, box));
}

TEST_CASE("a knob shows its value arc in the accent colour and dims when disabled", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Slider slider;
    slider.setRange(0.0, 1.0);

    const auto countAccent = [](const juce::Image& img) {
        int n = 0;
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const auto c = img.getPixelAt(x, y);
                // Akzentgelb: viel Rot, mittleres Grün, wenig Blau.
                if (c.getAlpha() > 200 && c.getRed() > 200 && c.getGreen() > 140 && c.getBlue() < 110)
                    ++n;
            }
        return n;
    };

    const auto low = drawKnob(lnf, slider, 64, 0.1f);
    const auto high = drawKnob(lnf, slider, 64, 0.9f);
    CHECK(countAccent(high) > countAccent(low));
    CHECK(countAccent(low) > 0);

    slider.setEnabled(false);
    CHECK(countAccent(drawKnob(lnf, slider, 64, 0.9f)) == 0);
    dgtest::savePng(high, "knob-64");
}

TEST_CASE("a bipolar knob fills from its centre", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::Slider slider;
    slider.setRange(-1.0, 1.0);
    // In der Mitte ist der Wertebogen leer: kaum gelbe Pixel außer dem Glow-Ansatz.
    const auto centre = drawKnob(lnf, slider, 64, 0.5f);
    const auto right = drawKnob(lnf, slider, 64, 1.0f);
    int centreLeft = 0, rightLeft = 0;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 28; ++x) // linke Hälfte
        {
            const auto isAccent = [](juce::Colour c) { return c.getAlpha() > 200 && c.getRed() > 200 && c.getGreen() > 140 && c.getBlue() < 110; };
            centreLeft += isAccent(centre.getPixelAt(x, y)) ? 1 : 0;
            rightLeft += isAccent(right.getPixelAt(x, y)) ? 1 : 0;
        }
    CHECK(centreLeft == 0);
    CHECK(rightLeft == 0); // voll rechts: der Bogen läuft nur durch die rechte Hälfte
}

TEST_CASE("a toggle shows a lamp instead of a tick box", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::ToggleButton toggle("Sync");
    toggle.setLookAndFeel(&lnf);
    toggle.setSize(100, 24);

    const auto lamp = [&toggle] {
        const auto img = dgtest::snapshot(toggle);
        return img.getPixelAt(9, 12); // Mitte der Lampe
    };
    toggle.setToggleState(false, juce::dontSendNotification);
    const auto off = lamp();
    toggle.setToggleState(true, juce::dontSendNotification);
    const auto on = lamp();
    CHECK(on.getBrightness() > off.getBrightness() + 0.3f);
    CHECK(on.getRed() > 200); // Akzentfarbe
    toggle.setLookAndFeel(nullptr);
}

TEST_CASE("buttons and combo boxes have a lit top edge and look pressed when down", "[look]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ui::DgLookAndFeel lnf;
    juce::TextButton button("Rename");
    button.setSize(110, 28);

    const auto render = [&](bool down) {
        juce::Image img(juce::Image::ARGB, 110, 28, true);
        juce::Graphics g(img);
        lnf.drawButtonBackground(g, button, ui::colours::panel, false, down);
        return img;
    };
    const auto up = render(false);
    const auto down = render(true);
    CHECK(up.getPixelAt(55, 5).getBrightness() > up.getPixelAt(55, 22).getBrightness());     // oben heller
    CHECK(down.getPixelAt(55, 5).getBrightness() < down.getPixelAt(55, 22).getBrightness()); // gedrückt: umgekehrt

    juce::ComboBox box;
    box.setLookAndFeel(&lnf);
    box.setSize(100, 24);
    juce::Image img(juce::Image::ARGB, 100, 24, true);
    {
        juce::Graphics g(img);
        lnf.drawComboBox(g, 100, 24, false, 0, 0, 0, 0, box);
    }
    CHECK(img.getPixelAt(50, 5).getBrightness() > img.getPixelAt(50, 19).getBrightness());
    box.setLookAndFeel(nullptr);
}
```

- [ ] **Step 2: Build und Tests ausführen, das Scheitern sehen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[look]"`
Expected: FAIL in „a toggle shows a lamp …“ und „buttons and combo boxes have a lit top edge …“ (heute flach bzw. Häkchen). Die Regler-Tests können schon bestehen; sie sichern das Verhalten beim Umbau ab.

- [ ] **Step 3: Deklarationen ergänzen**

`plugin/ui/DgLookAndFeel.h`: Includes `#include <map>`, `#include <memory>`, `#include "melatonin_blur/melatonin_blur.h"` ergänzen; in der Palette nach `padTextLit`:

```cpp
inline const juce::Colour knobTop { 0xff3a4048 };
inline const juce::Colour knobBottom { 0xff1c2025 };
```

In der Klasse öffentlich ergänzen:

```cpp
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW,
                      int buttonH, juce::ComboBox& box) override;
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
```

Privat ergänzen:

```cpp
    // Erhabene Fläche für Buttons und Auswahlboxen: Verlauf, Kontur, Lichtkante; gedrückt eingelassen.
    static void drawRaisedBody(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour base, bool down, bool highlighted, bool enabled);
    melatonin::DropShadow& knobShadow(int diameter);

    // Ein Schatten pro Körper-Durchmesser, damit der Cache von melatonin bei gemischten Reglergrößen hält.
    std::map<int, std::unique_ptr<melatonin::DropShadow>> knobShadows_;
    melatonin::DropShadow lampGlow_ { colours::accent.withAlpha(0.8f), 4 };
    melatonin::InnerShadow lampInset_ { juce::Colours::black.withAlpha(0.7f), 2, { 0, 1 } };
```

- [ ] **Step 4: Regler neu zeichnen**

`plugin/ui/DgLookAndFeel.cpp`: `drawRotarySlider` ersetzen und `knobShadow` ergänzen.

```cpp
melatonin::DropShadow& DgLookAndFeel::knobShadow(int diameter)
{
    auto& slot = knobShadows_[diameter];
    if (slot == nullptr)
        slot = std::make_unique<melatonin::DropShadow>(colours::shadow, 3, juce::Point<int> { 0, 2 });
    return *slot;
}

void DgLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                     float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const float lineW = 3.0f;
    const float arcR = radius - lineW;
    if (arcR < 3.0f)
        return; // zu klein zum Zeichnen (erstes Layout, winzige Zelle)
    const auto centre = bounds.getCentre();
    const juce::PathStrokeType stroke(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    // Spur: vertieft und dunkel.
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(colours::groove);
    g.strokePath(track, stroke);

    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar
        ? rotaryStartAngle + static_cast<float>(slider.valueToProportionOfLength(0.0)) * (rotaryEndAngle - rotaryStartAngle)
        : rotaryStartAngle;

    if (std::abs(angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, std::min(from, angle), std::max(from, angle), true);
        if (slider.isEnabled())
        {
            // Schwacher Glow: zwei breitere, fast durchsichtige Striche. Bewusst ohne Blur, weil sich
            // der Bogen mit jedem Wert ändert und ein Blur deshalb nie aus dem Cache käme.
            g.setColour(colours::accent.withAlpha(0.08f));
            g.strokePath(value, juce::PathStrokeType(lineW + 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(colours::accent.withAlpha(0.16f));
            g.strokePath(value, juce::PathStrokeType(lineW + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour(slider.isEnabled() ? colours::accent : colours::textDim);
        g.strokePath(value, stroke);
    }

    // Körper mit Verlauf und kleinem Schatten, darauf der Zeiger.
    const float bodyR = arcR - 6.0f;
    if (bodyR < 4.0f)
        return;
    juce::Path body;
    body.addEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    knobShadow(juce::roundToInt(bodyR * 2.0f)).render(g, body);
    g.setGradientFill(juce::ColourGradient(colours::knobTop, centre.x, centre.y - bodyR * 0.6f, colours::knobBottom, centre.x,
                                           centre.y + bodyR, true));
    g.fillPath(body);
    g.setColour(colours::groove);
    g.strokePath(body, juce::PathStrokeType(1.0f));

    const juce::Point<float> dir(std::sin(angle), -std::cos(angle));
    g.setColour(slider.isEnabled() ? colours::text : colours::textDim);
    g.drawLine({ centre + dir * (bodyR * 0.35f), centre + dir * (bodyR * 0.85f) }, 2.0f);
}
```

`#include <cmath>` und `#include <algorithm>` am Dateianfang ergänzen.

- [ ] **Step 5: Buttons, Auswahlboxen, Schalter**

```cpp
void DgLookAndFeel::drawRaisedBody(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour base, bool down, bool highlighted,
                                   bool enabled)
{
    constexpr float corner = 5.0f;
    auto r = bounds.reduced(0.5f);
    r.removeFromBottom(1.0f); // Platz für die Schattenlinie
    if (r.getWidth() < 2.0f || r.getHeight() < 2.0f)
        return;
    const float alpha = enabled ? 1.0f : 0.5f;

    if (!down)
    {
        g.setColour(colours::shadow.withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(r.translated(0.0f, 1.0f), corner);
    }
    auto top = base.brighter(0.14f);
    auto bottom = base.darker(0.14f);
    if (down)
        std::swap(top, bottom);
    else if (highlighted)
    {
        top = top.brighter(0.06f);
        bottom = bottom.brighter(0.06f);
    }
    g.setGradientFill(juce::ColourGradient(top.withMultipliedAlpha(alpha), 0.0f, r.getY(), bottom.withMultipliedAlpha(alpha), 0.0f,
                                           r.getBottom(), false));
    g.fillRoundedRectangle(r, corner);
    g.setColour(colours::groove.withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(r, corner, 1.0f);
    if (!down && r.getWidth() > 2.0f * corner)
    {
        g.setColour(colours::highlight.withMultipliedAlpha(alpha));
        g.fillRect(juce::Rectangle<float>(r.getX() + corner, r.getY() + 1.0f, r.getWidth() - 2.0f * corner, 1.0f));
    }
}

void DgLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    drawRaisedBody(g, button.getLocalBounds().toFloat(), backgroundColour, shouldDrawButtonAsDown, shouldDrawButtonAsHighlighted,
                   button.isEnabled());
}

void DgLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int, int, int, int, juce::ComboBox& box)
{
    if (width < 4 || height < 4)
        return;
    drawRaisedBody(g, juce::Rectangle<int>(0, 0, width, height).toFloat(), box.findColour(juce::ComboBox::backgroundColourId),
                   isButtonDown, box.isMouseOver(true), box.isEnabled());
    if (width < 30)
        return;
    const float cx = static_cast<float>(width) - 14.0f;
    const float cy = static_cast<float>(height) * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
    arrow.lineTo(cx, cy + 2.5f);
    arrow.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId).withAlpha(box.isEnabled() ? 0.9f : 0.3f));
    g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void DgLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool, bool)
{
    const auto area = button.getLocalBounds();
    if (area.getHeight() < 6 || area.getWidth() < 6)
        return;
    // Zustand als kleine Leuchte statt als Häkchen.
    constexpr float lampSize = 10.0f;
    const juce::Rectangle<float> lampBounds(4.0f, (static_cast<float>(area.getHeight()) - lampSize) * 0.5f, lampSize, lampSize);
    juce::Path lamp;
    lamp.addEllipse(lampBounds);
    if (button.getToggleState())
    {
        if (button.isEnabled())
            lampGlow_.render(g, lamp);
        g.setColour(button.findColour(button.isEnabled() ? juce::ToggleButton::tickColourId : juce::ToggleButton::tickDisabledColourId));
        g.fillPath(lamp);
    }
    else
    {
        g.setColour(colours::groove);
        g.fillPath(lamp);
        lampInset_.render(g, lamp);
    }
    g.setColour(colours::outline);
    g.strokePath(lamp, juce::PathStrokeType(1.0f));

    g.setColour(button.findColour(juce::ToggleButton::textColourId).withAlpha(button.isEnabled() ? 1.0f : 0.5f));
    g.setFont(font(std::min(15.0f, static_cast<float>(area.getHeight()) * 0.75f)));
    g.drawFittedText(button.getButtonText(), area.withTrimmedLeft(static_cast<int>(lampSize) + 10).withTrimmedRight(2),
                     juce::Justification::centredLeft, 10);
}
```

- [ ] **Step 6: Bauen und Tests laufen lassen**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 build`
Run: `build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[look]"`
Expected: PASS, 18 Testfälle.

Schlägt „a bipolar knob fills from its centre“ wegen einzelner gelber Pixel des runden Strichendes fehl, den Zählbereich der linken Hälfte auf `x < 24` verkleinern; die Aussage des Tests (kein Bogen in der linken Hälfte) bleibt dieselbe.

- [ ] **Step 7: Aussehen prüfen**

```powershell
$env:DG_SNAPSHOT_DIR = Join-Path $env:TEMP 'dg-snapshots'
build\tests\DubgefahrenPluginTests_artefacts\Release\DubgefahrenPluginTests.exe "[look]"
Remove-Item Env:DG_SNAPSHOT_DIR
```

`editor-1.00.png`, `editor-0.75.png`, `editor-2.00.png` und `knob-64.png` ansehen:
- Regler: dunkle Spur, gelber Bogen, Körper mit Verlauf, heller Zeiger; Wert und Beschriftung lesbar.
- Buttons und Auswahlboxen: Verlauf, Lichtkante; Text nicht abgeschnitten.
- Schalter („Editor follows focus“, LFO-Sync): Lampe links, Text daneben, nichts überlappt.
- Titel „DUBGEFAHREN“ passt mit der größeren Laufweite in seine 220 px und stößt nicht an die CPU-Anzeige.

Ist der Titel abgeschnitten: die Laufweite in `plugin/PluginEditor.cpp` von `0.12f` auf `0.08f` senken und den Test „the embedded font keeps a label's letter spacing“ unverändert lassen (er nutzt seinen eigenen Wert).

- [ ] **Step 8: Alle Tests, dann Commit**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS.

```bash
git add plugin/ui/DgLookAndFeel.h plugin/ui/DgLookAndFeel.cpp tests/plugin/test_Look.cpp plugin/PluginEditor.cpp
git commit -m "feat(ui): knobs with body, raised buttons and lamp toggles (#15)"
```

---

### Task 6: Gesamtprüfung und Übergabe zur Abnahme

**Files:**
- Modify: `docs/superpowers/specs/2026-09-30-ui-restyle-design.md` (nur Status)

**Interfaces:**
- Consumes: alles aus Task 1 bis 5.

- [ ] **Step 1: Aufräum-Prüfungen**

Run: `git grep -n "colours::playing" -- plugin tests`
Expected: keine Treffer.

Run: `git grep -n "FontOptions(" -- plugin`
Expected: nur `plugin/ui/Fonts.cpp`.

Run: `git grep -n "setDefaultLookAndFeel\|getTypefaceForFont" -- plugin`
Expected: keine Treffer.

Run: `git diff main --stat -- engine`
Expected: leer (die Engine ist unberührt).

Run: `git diff main -- plugin | findstr /C:"setBounds" /C:"kRowHeight" /C:"kCellWidth" /C:"kTopOffset" /C:"kBaseWidth" /C:"kBaseHeight"`
Expected: keine Treffer (kein Layout geändert).

- [ ] **Step 2: Alle Tests und Validator**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 test`
Expected: alle Tests PASS (192 bestehende plus die neuen aus `[look]` und `[padglow]`).

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 validate`
Expected: VST3-Validator 47/47 bestanden; pluginval bestanden oder als „nicht gefunden – übersprungen“ gemeldet.

- [ ] **Step 3: Spec-Status setzen und committen**

In `docs/superpowers/specs/2026-09-30-ui-restyle-design.md` die Zeile `**Status:** Entwurf zur Freigabe` ersetzen durch `**Status:** Umgesetzt, Abnahme in Ableton offen`.

```bash
git add docs/superpowers/specs/2026-09-30-ui-restyle-design.md
git commit -m "docs: mark UI restyle spec as implemented (#15)"
```

- [ ] **Step 4: Übergabe zur Abnahme von Hand**

Das Bundle liegt unter `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3`. Installation durch Kopieren des Bundles nach `%CommonProgramFiles%\VST3` (Adminrechte); `build.ps1 install` nicht verwenden.

Checkliste für Ableton, dem Nutzer vorlegen:

1. Pads: ruhend, spielend, Fokus, ausgewählt, gelatcht, leer und „Sample fehlt“ sind erkennbar und voneinander unterscheidbar.
2. Ein Pad blitzt beim Trigger auf, hält eine etwas geringere Helligkeit und klingt beim Ende weich aus.
3. Der Glow eines Pads am Rand des Rasters (z. B. Pad 1, 4, 13, 16) ist nicht abgeschnitten.
4. Fenster auf kleinste und größte Größe ziehen: alles scharf und lesbar, Schatten sitzen richtig.
5. Viele Pads gleichzeitig spielen (z. B. 8 Noten halten): kein Ruckeln der UI.
6. Regler, Buttons, Auswahlboxen und Schalter lassen sich wie vorher bedienen; Popup-Menüs und der Rename-Dialog zeigen die neue Schrift.
7. Zeichen außerhalb der Schrift werden angezeigt: „∿“ auf Sample-Pads, „▾“ am Kit-Button, „…“ in „Add File…“.
8. Zwei Instanzen des Plugins öffnen, einen Editor schließen: der andere sieht unverändert aus.
