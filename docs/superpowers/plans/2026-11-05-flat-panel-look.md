# Flacher Control-Panel-Look Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** dubgefahren bekommt den flachen LCD-Control-Panel-Look von dubcycling: Segment-Slider statt Drehregler, Monospace-Versalien, flache Flächen, flache Pads.

**Architecture:** `DgLookAndFeel`, `Surfaces`, `Fonts` und `Controls` bleiben die Stellen, an denen der Look zentral entschieden wird. Es ändern sich deren Inhalte, nicht die Aufteilung. `Knob` behält Namen und Schnittstelle (`slider`, `label`, `attach`), wird aber eine Zelle aus Beschriftung über einem horizontalen Segment-Slider mit LCD-Wert rechts. Panels ordnen ihre Zellen neu an; die Basisgrößen der Layouts wachsen nur so weit wie nötig.

**Tech Stack:** C++20, JUCE, Catch2 (Tests unter `tests/plugin`), CMake, `build.ps1`.

**Spec:** `docs/superpowers/specs/2026-11-05-flat-panel-look-design.md`
**Referenz-Code:** `E:\Repos\dubcycling\plugin\ui\PanelLook.h/.cpp` (Farben, Segmentmaße, `styleLcd`).

## Global Constraints

- Voraussetzung: `feature/settings-midi-learn` ist auf `main` gemergt. Arbeitsbranch `feature/flat-panel-look` von `main`.
- Monospace-Versalien über `juce::Font::getDefaultMonospacedFontName()`, keine eingebettete Schrift.
- Segment 5 px, Lücke 1 px, Balkenhöhe 8 px; Akzent `0xff5aa9e6`, LCD-Text `0xffffb347` auf `0xff0b0d0f`, Kante `0xff2a2e35` / `0xff3a3f47`, Fenster `0xff14161a`, Streifen `0xff191c21`, Button `0xff23272e`, Text `0xffd7dbe0`, gedämpft `0xff7b828d`.
- Keine Verläufe, Lichtkanten oder Schlagschatten; Eckenradius höchstens 2 px.
- Skalierung bleibt: festes Seitenverhältnis, Limits relativ zur Basisgröße (`setResizeLimits` und `setFixedAspectRatio` in `PluginEditor.cpp`).
- Basisgrößen dürfen nur wenig wachsen: Höhe höchstens +15 % gegenüber heute (`Edit` 640, `EditAdvanced` 744, `Live` 460), Breite unverändert (1200, 1200, 880), außer ein Task begründet es.
- Keine Änderung an Parametern, IDs, Wertebereichen, State oder DSP.
- Parameter-Attachments, Doppelklick-Reset, Mausrad und Fein-Modus der Slider bleiben wie heute.
- Tests: `.\build.ps1 test`. Vor dem Abschluss `.\build.ps1 validate` (Steinberg-Validator und pluginval, Strictness 5).
- Commit-Texte enden mit der Co-Authored-By-Zeile aus der Sitzung.

## Review Focus

- Pad-Zustände leer, belegt, getroffen, gewählt, gelatcht und Sample fehlt sind auf flachen Kacheln unterscheidbar (Task 5).
- Bipolare Slider (Pan, Tune) zeigen die Mitte und füllen von der Mitte aus (Task 2).
- Ein Slider mit Skew (Cutoff, Zeiten) und sehr kleine oder sehr große Werte zeigen einen LCD-Wert, der in die Anzeige passt (Task 2).
- Deaktivierte Slider (Stretch-Faktor, Grain, X-Fade) sind erkennbar abgeblendet (Task 2).
- Langer Text in Monospace-Versalien (Sample-Dateiname, Kit-Name, Pad-Name) wird gekürzt, nicht über den Rand gezeichnet (Tasks 4 und 5).

---

## File Structure

- `plugin/ui/Fonts.h/.cpp`: Schrift-Zugang. Wird Monospace; `EmbeddedFonts` entfällt.
- `plugin/ui/DgLookAndFeel.h/.cpp`: Palette (`colours`) und Zeichnen aller Standard-Widgets, neu: flache Buttons, Combo, Toggle, Popup, Segment-Slider, LCD-Textbox.
- `plugin/ui/Surfaces.h/.cpp`: flacher Panel-Körper, Trenner, Fensterhintergrund; `PanelShadow` entfällt.
- `plugin/ui/Controls.h/.cpp`: `Knob` (Segment-Slider-Zelle), `Choice`, `Toggle`, neu `Segmented` (Dreifach-Schalter für den Filtertyp).
- `plugin/ui/LivePanel`, `PerformancePanel`, `AdvancedPanel`, `SlotEditor`, `PadGrid`, `WaveformView`, `CpuMeter`, `SettingsDialog`, `plugin/PluginEditor.*`: Layout und Farben.
- `plugin/CMakeLists.txt`, `CMakeLists.txt`: `dg_fonts` und `melatonin_blur` entfallen in Task 7.
- Tests: `tests/plugin/test_Look.cpp`, `test_Editor.cpp`, `test_PadColours.cpp`, `test_CpuMeter.cpp`, `test_Waveform.cpp`, neu `test_Controls.cpp` (in `tests/CMakeLists.txt` eintragen).

---

### Task 1: Palette, Monospace-Schrift und flache Standard-Widgets

**Files:**
- Modify: `plugin/ui/Fonts.h/.cpp`, `plugin/ui/DgLookAndFeel.h/.cpp`, `plugin/ui/Surfaces.h/.cpp`, `plugin/PluginEditor.cpp` (nur `paint`)
- Test: `tests/plugin/test_Look.cpp`

**Interfaces:**
- Produces: `juce::FontOptions ui::font(float height, bool bold = false)` liefert Monospace; `juce::Font ui::withEmbeddedTypeface(const juce::Font&)` bleibt als Name erhalten und setzt Monospace (so ändern sich Aufrufer nicht). Neue Farben in `ui::colours` nach der Tabelle in den Global Constraints: `window`, `strip`, `lcd`, `edge`, `edgeLight`, `button`, `text`, `textDim`, `amber`, `accent` (= Blau), `warning`, `danger`. Alte Verlaufsfarben (`panelTop`, `panelBottom`, `backgroundTop`, `backgroundBottom`, `highlight`, `shadow`, `knobTop`, `knobBottom`, `groove`) entfallen. `drawPanelBody(g, bounds)` und `drawWindowBackground(g, bounds)` malen flache Flächen mit 1-px-Kante. `drawDivider(g, x, top, bottom)` malt eine einzelne Linie.

- [ ] **Step 1: Tests umschreiben.** In `test_Look.cpp`: die Inter-Tests werden `"fonts are monospace"` (Name des Typefaces gleich `juce::Font::getDefaultMonospacedFontName()`-Typeface; `lnf.getLabelFont`, `getTextButtonFont`, `getComboBoxFont`, `getPopupMenuFont`, Alert-Fonts liefern Monospace; Höhe und Kerning-Faktor bleiben erhalten). Test `"the embedded font survives closing another editor"` entfällt. `"a panel body is a vertical gradient..."` wird `"a panel body is flat with a 1 px edge"`: Pixel (50,8) und (50,52) gleich, Pixel (0,30) hat `colours::edge`-Farbe, Mitte ist `colours::strip`. `"a panel shadow reaches beyond the panel"` entfällt.
- [ ] **Step 2: Tests laufen lassen, Fehlschlag prüfen.** `.\build.ps1 test` — die neuen Prüfungen scheitern.
- [ ] **Step 3: Umsetzen.** `Fonts.cpp` ohne `DgFonts.h`-Include; Monospace wie `panel::mono` in dubcycling. `DgLookAndFeel`: Konstruktor setzt die Farben neu; `drawButtonBackground` wie `PanelLook::drawButtonBackground`; `drawComboBox` flach mit Kante und kleinem Pfeil; `drawToggleButton` bleibt eine kleine Lampe, aber ohne Glow-/Inset-Schatten (Quadrat 10 px, aktiv in `colours::amber`); `drawRotarySlider` entfällt, `knobShadow`, `lampGlow_`, `lampInset_`, `knobShadows_` entfallen. Der Editor-`paint` zeichnet nur noch `drawWindowBackground` und `pads_.paintGlows` (Glow-Verhalten in Task 5 klären).
- [ ] **Step 4: Tests laufen lassen.** `.\build.ps1 test` — Erwartung: grün. Hinweis: `melatonin_blur` bleibt bis Task 7 gelinkt, solange `PadGrid` es nutzt.
- [ ] **Step 5: Commit** `feat(ui): flat palette, monospace font and flat standard widgets`.

### Task 2: Segment-Slider-Zelle (`Knob`), `Choice`, `Toggle`, `Segmented`

**Files:**
- Modify: `plugin/ui/Controls.h/.cpp`, `plugin/ui/DgLookAndFeel.h/.cpp`
- Create: `tests/plugin/test_Controls.cpp`

**Interfaces:**
- Produces: `Knob` bleibt `Knob(const juce::String& label)`, `attach(apvts, paramId)`, Member `slider` (jetzt `LinearHorizontal`, `TextBoxRight`, Box 44×18) und `label`. `Knob::resized()` setzt Beschriftung 12 px hoch oben, darunter den Slider über die volle Zellbreite. Neu: `class Segmented final : public juce::Component` mit `Segmented(const juce::String& label)`, `attach(apvts, paramId)` für einen `AudioParameterChoice` (Einträge als Segmente, aktives Segment `colours::amber` auf `colours::button`). `DgLookAndFeel::drawLinearSlider` zeichnet für `LinearHorizontal` Segmente wie `PanelLook::drawLinearSlider`; bei `getMinimum() < 0 && getMaximum() > 0` füllt es von der Mitte aus und malt eine Mittelmarke (1 px `colours::edgeLight`, volle Balkenhöhe). Deaktivierte Slider malen alle Segmente gedämpft (`colours::edge`, Füllung `colours::textDim`). `getSliderThumbRadius` liefert 0. `Slider::textBox*ColourId` gelten für das LCD (Bernstein auf `colours::lcd`, Kante `colours::edge`).
- Consumes: Task 1 (Palette, `font`).

- [ ] **Step 1: Failing Tests in `test_Controls.cpp`.** (a) `"segment slider fills proportionally"`: `Knob`, Bereich 0..1, Wert 0.5, Größe 200×34, in ein `juce::Image` gerendert (`createComponentSnapshot`); Anzahl gefüllter Segmente (Pixel in `colours::accent` entlang der Balkenmitte) liegt bei Wert 0.5 etwa halb so hoch wie bei 1.0 (±1 Segment) und ist bei 0 gleich 0. (b) `"a bipolar slider fills from the centre"`: Bereich −1..1, Wert 0 → kein Segment gefüllt, Wert 0.5 → nur Segmente rechts der Mitte gefüllt, Wert −0.5 → nur links. (c) `"a disabled slider is dimmed"`: gefüllter Slider mit `setEnabled(false)` enthält kein Pixel in `colours::accent`. (d) `"the LCD text fits for extreme values"`: `slider.textFromValueFunction` für 0.0, 0.001, 12000.5, −1.0 liefert Text, dessen Breite in der LCD-Schrift ≤ Textboxbreite − 8 px ist. (e) `"segmented selects the choice and follows the parameter"`: an `DubgefahrenProcessor`-APVTS mit `pid::filterType` angehängt, Klick auf Segment 2 setzt den Parameter auf Index 2, Parameteränderung von außen ändert das aktive Segment.
- [ ] **Step 2: Fehlschlag prüfen.** `.\build.ps1 test` — neue Tests scheitern bzw. bauen nicht.
- [ ] **Step 3: Umsetzen** nach dem Interfaces-Block. `Knob::attach` behält die Dezimalstellen-Regel (2 Stellen, 3 für Einheit `s`) und das "kein −0.00". `Choice` bleibt eine Combo-Box unter einer Beschriftung, Höhe der Box 22 px. `Toggle` bleibt `juce::ToggleButton` mit Lampe, Höhe 22 px.
- [ ] **Step 4: Tests laufen lassen.** Erwartung: grün (auch die bestehenden Editor-Tests, die `Knob::slider` ansprechen).
- [ ] **Step 5: Commit** `feat(ui): segment slider cell, flat choice and toggle, segmented filter switch`.

### Task 3: Live-, Performance- und Advanced-Panel neu anordnen, Basisgrößen

**Files:**
- Modify: `plugin/ui/LivePanel.h/.cpp`, `PerformancePanel.h/.cpp`, `AdvancedPanel.h/.cpp`, `plugin/PluginEditor.h/.cpp` (`baseSize`, `layoutContent`)
- Test: `tests/plugin/test_Editor.cpp`

**Interfaces:**
- Consumes: `Knob`, `Choice`, `Toggle`, `Segmented` aus Task 2. `LivePanel::filterType_` wird `Segmented` (Beschriftung "Filter"), Einträge LP, BP, HP.
- Produces: unveränderte `controlBounds()` der Panels (die Liste der Bounds, getestet in `test_Editor.cpp`); neue `DubgefahrenEditor::baseSize(Layout)`-Werte, vom Builder zu bestimmen innerhalb der Grenzen der Global Constraints.

- [ ] **Step 1: Tests anpassen.** `"the editor has three layouts with fixed base sizes"` bekommt die neuen Werte. `"the effects bar is split into..."` und `"in every layout all visible controls lie inside their panel without overlaps"` bleiben ungeändert und müssen mit den neuen Layouts grün sein. Neu: `"every slider cell is wide enough for its LCD"` — für alle `controlBounds()` der drei Panels gilt Breite ≥ 120 und Höhe ≥ 30.
- [ ] **Step 2: Fehlschlag prüfen.** `.\build.ps1 test`.
- [ ] **Step 3: Umsetzen.** Zellen sind Zeilen aus 130×34-Zellen (Beschriftung 12 + Slider 22). Live-Panel in `Edit`: Gruppen DUB FX (Space, Grit, Throw), FILTER (Cutoff, Reso, Filter-Schalter), DELAY (Zeit), MASTER als Spalten nebeneinander mit je drei Zeilen; in `Live` (Breite 880) zwei Gruppen-Zeilen. Performance-Panel: vier Slider plus die zwei Choices. Advanced-Panel: Gruppen DRIVE, DELAY, PHASER, REVERB in zwei Zeilen. `baseSize` so wählen, dass nichts überlappt; die Höhe der Panels ergibt sich aus der Zahl der Zeilen. Die Panel-Titel (`drawText`) kommen als 11-px-Monospace-Versalien in `colours::textDim`; Trenner über `drawDivider`.
- [ ] **Step 4: Tests laufen lassen**, danach Sichtprüfung im Standalone oder Host: Layout `Edit`, `EditAdvanced`, `Live` bei Skalierung 0.75, 1.0 und 2.0 (`"the editor paints at every window scale"` läuft schon automatisch).
- [ ] **Step 5: Commit** `feat(ui): slider rows in the live, performance and advanced panels`.

### Task 4: Slot-Editor

**Files:**
- Modify: `plugin/ui/SlotEditor.h/.cpp`
- Test: `tests/plugin/test_Editor.cpp`

**Interfaces:**
- Consumes: `Knob`, `Choice`, `Toggle` (Task 2), neue Basisgrößen (Task 3).
- Produces: `SlotEditor` passt in den von Task 3 gelassenen Platz (`slotEditor_.setBounds(r)` in `layoutContent`).

- [ ] **Step 1: Test.** Neu `"slot editor cells fit the panel in synth and sample mode"`: in beiden Modi liegen alle sichtbaren Zellen (`Knob`, `Choice`, `Toggle`, Wellenform, Hinweis) im Panel, überlappen nicht und sind ≥ 120 × 30 groß (Zugriff wie vorhandene Tests über Bounds der Komponenten).
- [ ] **Step 2: Fehlschlag prüfen.** `.\build.ps1 test`.
- [ ] **Step 3: Umsetzen.** `kRowHeight`, `kCellWidth`, `kTopOffset`, `kRowLabelWidth` neu setzen (Zellen mindestens 130×34, Raster mit 5 Spalten wie heute, 4 Reihen). Zeilenbeschriftungen (`kRowNames`, `kSampleRowNames`) in Versalien; die Kopfzeile (`header_`) und Dateinamen werden mit `drawFittedText`/`Label::setMinimumHorizontalScale` gekürzt statt überzulaufen. Der Hinweis `stretchHint_` behält seinen Platz.
- [ ] **Step 4: Tests laufen lassen**; Sichtprüfung Synth- und Sample-Slot mit langem Sample-Namen.
- [ ] **Step 5: Commit** `feat(ui): flat slot editor with slider cells`.

### Task 5: Pads

**Files:**
- Modify: `plugin/ui/PadGrid.cpp/.h` (Pad-Komponente `paint`, `paintGlow`, `paintGlows`, Legende), `plugin/ui/PadGlow.h` (nur falls nötig), `plugin/PluginEditor.cpp` (Glow-Repaint)
- Test: `tests/plugin/test_Look.cpp`, `tests/plugin/test_PadColours.cpp`, `tests/plugin/test_PadGlow.cpp`

**Interfaces:**
- Consumes: Palette aus Task 1; `padBaseColour(slot)`, `padLitTextColour(slot)`, `padBrightness(slot)` bleiben unverändert (sie sind getestet).
- Produces: dieselbe öffentliche Schnittstelle von `PadGrid`.

- [ ] **Step 1: Tests.** `"pad states are distinguishable"` in `test_Look.cpp`: ein `PadGrid` mit Pads in den Zuständen leer, belegt, getroffen (Helligkeit 1), gewählt, gelatcht und Sample-fehlt wird gerendert; die Kachelmitten und Balkenbereiche unterscheiden sich paarweise in der Farbe (Abstand der RGB-Werte > 24). `"a triggered pad is filled with its colour"`: Helligkeit 1 → Kachelmitte hat die Pad-Farbe `padBaseColour(slot)`. Die bestehenden `PadGlow`- und `PadColours`-Tests müssen unverändert grün bleiben.
- [ ] **Step 2: Fehlschlag prüfen.** `.\build.ps1 test`.
- [ ] **Step 3: Umsetzen.** Kachel: `colours::strip` mit Kante `colours::edge`; belegtes Pad hat einen 3-px-Balken unten in der Pad-Farbe; Helligkeit blendet die Füllung von Kachelfarbe zur Pad-Farbe (ohne Glow-Bild und ohne Blur, `glowImage_` entfällt). Gewählt (Fokus): Kante `colours::accent`; gelatcht: Punkt wie heute; Sample fehlt: Warnmarke in `colours::warning`. Pad-Name in Monospace-Versalien, gekürzt. Legende unter dem Raster behält ihre Marken und ihre Texte in Versalien. `onGlowChanged` zeichnet nur die Kachel neu (keine `kGlowReach`-Erweiterung mehr), `paintGlows` im Editor-`paint` entfällt.
- [ ] **Step 4: Tests laufen lassen**; Sichtprüfung mit allen acht Pad-Farben aus #14 und Pad-Ursprung oben/unten links.
- [ ] **Step 5: Commit** `feat(ui): flat pad tiles with colour bar and fill on hit`.

### Task 6: Wellenform, CPU-Meter, Settings-Dialog, Kopfzeile

**Files:**
- Modify: `plugin/ui/WaveformView.cpp`, `plugin/ui/CpuMeter.cpp`, `plugin/ui/SettingsDialog.cpp`, `plugin/PluginEditor.cpp` (Kopfzeile), `plugin/ui/PadColours.cpp` (nur falls Farben aus alter Palette)
- Test: `tests/plugin/test_Waveform.cpp`, `tests/plugin/test_CpuMeter.cpp`, `tests/plugin/test_SettingsDialog.cpp`

**Interfaces:**
- Consumes: Palette aus Task 1, `panel`-artige LCD-Hilfe: `void ui::styleLcd(juce::Label&, juce::Colour text, juce::Justification = centred)` (neu in `DgLookAndFeel.h`, wie dubcycling).

- [ ] **Step 1: Tests.** `test_CpuMeter.cpp`: Farbschwellen (gedämpft, Warnung, Gefahr) bleiben, Textformat `CPU 12 %` bleibt; Erwartung auf die neuen Farben umstellen. `test_Waveform.cpp` und `test_SettingsDialog.cpp`: bestehende Verhaltens-Tests bleiben unverändert grün. Neu `"the waveform keeps marker and region colours"`: Start-/Ende-/Loop-Markerfarben und Regionsfarben aus #11 sind unverändert (Pixel an der Markerlinie).
- [ ] **Step 2: Fehlschlag prüfen** (nur der CPU-Meter-Test scheitert zunächst).
- [ ] **Step 3: Umsetzen.** Wellenform: Hintergrund `colours::window`, Kante 1 px, Spur in `colours::accent`/Text; Marker und Regionen behalten ihre Farben. CPU-Meter als LCD (`styleLcd`). Settings-Dialog: alle `colours::`-Verweise auf die neue Palette, Pad-Raster im Dialog als flache Kacheln wie in Task 5, Eingabefelder und Buttons flach. Kopfzeile: Titel in Versalien, Buttons flach, Texte der Buttons in Versalien (`KIT`, `SETTINGS`, `PANIC`, `LIVE`/`EDIT`, `ADVANCED`).
- [ ] **Step 4: Tests laufen lassen**; Sichtprüfung des Settings-Dialogs mit aktivem MIDI-Learn.
- [ ] **Step 5: Commit** `feat(ui): flat waveform, LCD cpu meter, settings dialog and header`.

### Task 7: Aufräumen und Dokumentation

**Files:**
- Modify: `plugin/CMakeLists.txt`, `CMakeLists.txt`, `README.md`, ggf. `tools/`, `.github/workflows/release.yml`, Wiki-Quellen im Repo
- Delete: `plugin/assets/fonts/` (Inter und `OFL.txt`), `plugin/ui/Fonts.cpp`-Reste, ungenutzte Konstanten in `PadGlow.h`

- [ ] **Step 1: Nutzung prüfen.** `Grep` nach `melatonin`, `dg_fonts`, `DgFonts`, `Inter`, `OFL`, `kGlowReach`, `knobTop`, `PanelShadow` im ganzen Repo (ohne `build/`): Erwartung nach dem Entfernen, dass kein Treffer mehr im Code, in CMake und in Release-Skripten bleibt. Prüfen, ob `OFL.txt` im ausgelieferten Zip/Installer erwartet wird (`CMakeLists.txt`, `cmake/`, `tests/InstallBundleTest.cmake`) und die Erwartung anpassen.
- [ ] **Step 2: Entfernen.** `juce_add_binary_data(dg_fonts ...)` und die Link-Einträge `dg_fonts`, `melatonin_blur`; `FetchContent` für `melatonin_blur`; Font-Dateien; `PanelShadow`, `panelShadows_` im Editor.
- [ ] **Step 3: Volle Prüfung.** `.\build.ps1 test` grün; `.\build.ps1 validate` grün (Steinberg-Validator und pluginval Strictness 5); Sichtprüfung der drei Layouts bei 0.75, 1.0, 2.0 unter Windows.
- [ ] **Step 4: Dokumentation.** `README.md` und das Wiki (Bedienung: Regler sind jetzt Slider, Filtertyp-Schalter, neues Aussehen der Pads); Screenshots ersetzen.
- [ ] **Step 5: Plattform-Prüfung.** macOS- und Linux-Build in CI laufen lassen (Branch pushen nur nach Absprache); Beschriftungen auf Abschneiden prüfen, manuelle Sichtprüfung auf macOS und Linux ist Teil der Abnahme.
- [ ] **Step 6: Commit** `chore(ui): drop embedded Inter and melatonin_blur`, danach PR (Beschreibung auf Englisch, mit zurückgestellten Punkten).
