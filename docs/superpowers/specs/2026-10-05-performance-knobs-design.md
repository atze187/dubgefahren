# Performance-Knobs Space, Grit, Throw und Live-Ansicht – Design-Spezifikation

**Datum:** 2026-10-05
**Status:** Umgesetzt, Abnahme in Ableton offen
**Teilprojekt 5 von 5** der Überarbeitung des Effekt-Charakters

## 1. Hintergrund und Ziel

Die Effekt-Teilprojekte 1 bis 3 (Tape-Delay, Sättigung, Phaser) sind gebaut und abgenommen. Beim
Live-Test zeigte sich: Die Parameter eines Pads braucht man nur zum Bearbeiten, live stören der
Slot-Editor und die vielen Einzelregler. Gewünscht sind wenige Performance-Knobs, die mehrere
Parameter sinnvoll koppeln, und eine kompakte **Live-Ansicht** mit deutlich kleinerem Fenster.

**Erfolgskriterien:**

- Drei neue Knobs **Space**, **Grit** und **Throw** koppeln die Effekt-Einzelwerte musikalisch.
- Die Knobs sind ein **Aufschlag** auf die Einzelwerte: Bei Knob = 0 klingt es genau wie die
  Einzelregler (bestehende Projekte klingen unverändert).
- Die Einzelregler bleiben automatierbare Host-Parameter und wandern in einen aufklappbaren
  Advanced-Bereich.
- Ein Umschalter im Header wechselt zwischen **Live** (kompaktes Fenster, 880 × 460) und **Edit**
  (volle Ansicht).

## 2. Umfang

**Enthalten:** drei Host-Parameter, Rechenregel in der Engine, Throw-Aufschlag auf den FX-Send,
Live-/Edit-Ansicht, Live-Streifen, Advanced-Streifen, drei Fenstergrößen, Zustand im Projekt.

**Nicht enthalten:** einstellbare Kurven oder Zuordnung der Knobs im Plugin, MIDI-Learn, tempo-
synchroner Throw, Hochformat-Layout, ein zusätzlicher Throw-Taster.

## 3. Entscheidungen

- **Aufschlag auf die Einzelwerte** (nicht Ersetzen, nicht Schreiben in die Einzelregler): Die
  Einzelwerte legen den Grundklang fest, die Knobs drücken ihn nach oben.
- **Throw ist ein normaler Knob**, der stehen bleibt (keine Rückfederung, kein Taster). Ein federnder
  Controller-Fader macht daraus den kurzen Wurf.
- **Live-Ansicht** zeigt zusätzlich Master, Filter (Cutoff, Reso, Typ) und die Delay-Zeit.

## 4. Parameter und Rechenregel

Drei neue automatierbare Host-Parameter, 0 bis 1, Standard 0: `Space`, `Grit`, `Throw` (IDs
`fxSpace`, `fxGrit`, `fxThrow`, Versionshinweis 5). Der Plugin-Zustand alter Projekte kennt sie nicht und
lädt sie mit 0; das gilt auch, wenn der Zustand in eine laufende Instanz geladen wird (Parameter, die im
Zustand fehlen, werden auf ihren Default zurückgesetzt). Die Parameter erscheinen bei den globalen Effekt-Parametern (insgesamt dann
26 globale und 442 Parameter).

Eine neue, reine und JUCE-freie Funktion `applyMacros(const FxParams&, const MacroParams&)`
(`engine/Macros.h/.cpp`) rechnet die Einzelwerte plus Aufschläge zu den wirksamen Werten um. Die
Einzelparameter selbst werden nie verändert. Alle Faktoren und Obergrenzen stehen als benannte
Werte oben in `Macros.cpp`. Die Kurven sind zunächst linear.

| Knob | Wirkt auf | Aufschlag bei Knob = 1 (Obergrenze) |
|---|---|---|
| **Space** | Delay Mix | +0,35 (bis 1) |
| | Delay Feedback | +0,30 (bis 0,95) |
| | Reverb Mix | +0,40 (bis 1) |
| | Reverb Decay | +0,30 (bis 1) |
| **Grit** | Drive | +0,70 (bis 1) |
| | Delay Wow | +0,50 (bis 1) |
| | Delay Tone | −0,30 (nicht unter 0), also dunkler |
| | Phaser Mix | +0,60 (bis 1) |
| | Phaser Depth | +0,30 (bis 1) |
| **Throw** | FX-Send aller Slots | +1 (bis 1, Abschnitt 5) |
| | Delay Feedback | +0,50 (bis 1,10) |
| | Delay Mix | +0,50 (bis 1) |

Regeln:

- Aufschläge wachsen linear mit dem Knob (Wert × Faktor). Mehrere Knobs addieren sich, danach wird
  begrenzt.
- **Nie unter dem Grundwert:** Liegt ein Einzelwert schon über der Obergrenze (zum Beispiel Feedback
  1,05 bei Space), wird er nicht abgesenkt: wirksam = max(Grundwert, min(Grundwert + Aufschlag, Grenze)).
- Space begrenzt das Feedback bei 0,95: nur Throw darf bis zur Selbstoszillation (1,10).
- Der Klang bei Knob = 0 ist bit-genau der bisherige.

## 5. Einbau in die Engine

- `GlobalParams` (`engine/Engine.h`) bekommt `MacroParams macros` (space, grit, throw).
- `FxChain` übernimmt die Knobs mit den Effektparametern, glättet sie pro Block mit 20 ms und ruft pro
  Block `applyMacros` auf. Die wirksamen Werte gehen an die bestehenden Stufen, deren Einzelwerte ihre
  eigenen Pro-Sample-Glätter behalten (zwei kaskadierte Glätter, unhörbar).
- **Throw und FX-Send:** Der FX-Send steckt pro Slot in `engine/Engine.cpp` (`targetSend`). Dort kommt
  ein globaler Aufschlag dazu: `Send = clamp(Slot-Send + Throw, 0, 1)`. Der Throw-Aufschlag läuft durch den
  bestehenden Send-Glätter pro Slot (20 ms).

## 6. Ansichten und Oberfläche

**Bausteine:**

- **Live-Streifen** (in beiden Ansichten sichtbar): große Knobs Space, Grit, Throw; Filter (Cutoff, Reso,
  Typ); Delay-Zeit; Master.
- **Performance-Leiste:** unverändert (Pitch, Rate, Depth, Sweep, Target, Latch on Stop).
- **Advanced-Streifen** (nur Edit, aufklappbar): Drive; Delay (Feedback, Tone, Wow, Mix);
  Phaser (Rate, Depth, Mix); Reverb (Decay, Tone, Mix).
- **Slot-Editor:** nur Edit.

**Umschalten:**

- Ein Taster im Header wechselt zwischen **Live** und **Edit**. In Edit gibt es im Header einen
  Taster **Advanced ▾/▸**, der den Advanced-Streifen auf- und zuklappt.
- Header in Live: Titel, CPU-Anzeige, Kit-Menü, Umschalter, PANIC. Die Einrichtungs-Dinge (MIDI-Menü,
  Import, Export, "Editor follows focus") gibt es nur in Edit.
- Beide Zustände (Ansicht, Advanced auf/zu) liegen im Plugin-Zustand (wie die Fensterskalierung, keine
  Host-Parameter). Alte Projekte öffnen in **Edit mit aufgeklapptem Advanced**.

**Fenstergrößen** (Basisgröße; die Skalierung 75 bis 200 % bleibt und gilt für alle Layouts):

| Ansicht | Basisgröße |
|---|---|
| Edit mit Advanced | 1200 × 744 (Live-Streifen als zusätzliche Zeile) |
| Edit ohne Advanced | 1200 × 640 |
| Live | 880 × 460 |

In Live: links die Pads (wie bisher), rechts in Zeilen: die drei großen Knobs; dann Cutoff, Reso, Typ,
Delay-Zeit und Master; dann das Performance-Panel (Pitch, Rate, Depth, Sweep in einer Zeile, darunter
Target und Latch on Stop). Die endgültigen
Maße legt der Plan fest. Das Seitenverhältnis ist pro Layout fest; beim Umschalten ändert sich die
Fenstergröße. Der Pad-Klick (Gate-Vorschau, Kontextmenü, leere Pads) und der Fokus-Slot funktionieren in
Live unverändert.

## 7. Betroffene Dateien

- Neu: `engine/Macros.h`, `engine/Macros.cpp` (in `engine/CMakeLists.txt` eintragen),
  `tests/test_Macros.cpp` (in `tests/CMakeLists.txt` eintragen).
- Engine: `engine/Engine.h`, `engine/Engine.cpp`, `engine/FxChain.h`, `engine/FxChain.cpp`.
- Plugin: `plugin/ParameterLayout.h/.cpp`, `plugin/PluginProcessor.h/.cpp`, `plugin/PluginEditor.h/.cpp`,
  `plugin/ui/LivePanel.h/.cpp` (neu), `plugin/ui/AdvancedPanel.h/.cpp` (umbenannt aus `FxPanel`),
  `plugin/ui/PerformancePanel.h/.cpp`
  (Anordnung anpassbar).
- Tests: `tests/test_FxChain.cpp`, `tests/test_Engine.cpp`, `tests/plugin/test_PluginProcessor.cpp`,
  `tests/plugin/test_Editor.cpp`, `tests/plugin/test_Look.cpp`.
- Doku: README und Wiki (User Interface, Effects, MIDI and Host Integration, Home, Quick Start,
  Performance Controls).

## 8. Tests

1. **Knobs auf 0:** `applyMacros` liefert die Einzelwerte unverändert.
2. **Aufschläge:** Space, Grit und Throw erhöhen genau die Werte aus der Tabelle; Space begrenzt das
   Feedback bei 0,95, Throw erlaubt bis 1,10; Tone wird dunkler, aber nie unter 0.
3. **Nie unter dem Grundwert:** Liegt ein Einzelwert über der Obergrenze, senkt der Knob ihn nicht ab.
4. **Throw und Send:** Der Aufschlag verschiebt den Slot-Anteil in den Send-Bus, begrenzt auf 100 %.
5. **Extremwerte:** Alle Knobs auf 1 mit Extremeinstellungen bleiben endlich und unter dem Limiter-Pegel.
6. **Glättung:** Ein Sprung eines Knobs erzeugt keinen Klick im Ausgang.
7. **Plugin:** Die drei Parameter existieren (Standard 0, automatierbar, 442 Parameter); Live/Edit und
   Advanced überleben das Speichern; alte Zustände laden in Edit mit Advanced und Knobs auf 0.
8. **Editor:** Pro Layout stimmt die Fenstergröße (Basis × Skalierung), das Seitenverhältnis ist fest,
   alle Live-Regler liegen vollständig im Fenster und überlappen nicht; in Live sind Slot-Editor und
   Advanced ausgeblendet; die Render-Tests laufen für alle Layouts bei 0,75, 1 und 2.
9. **Regression:** Alle bestehenden Tests laufen weiter; die Editor-Tests passen zu den neuen Größen.

## 9. Abnahme

Die Beurteilung (Kurven, Stärke der Aufschläge, Gefühl der Live-Ansicht) erfolgt per Gehör und im
Spiel in Ableton. Die Faktoren stehen als benannte Werte an einer Stelle und lassen sich leicht
nachstellen.
