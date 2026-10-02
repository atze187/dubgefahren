# Phaser auf den Delay-Rückläufern – Design-Spezifikation

**Datum:** 2026-10-02
**Status:** Spec, Review und Plan offen
**Teilprojekt 3 von 5** der Überarbeitung des Effekt-Charakters

## 1. Hintergrund und Ziel

Teilprojekt 1 (Tape-Delay, PR #23) und 2 (Sättigung, PR #24) sind abgenommen. Teilprojekt 3 fügt
einen sanften Phaser hinzu. Er liegt **nur auf den Delay-Rückläufern**: die Echos klingen
phasig, das trockene Signal bleibt sauber (klassischer Dub-Kniff mit Phaser auf dem Echo-Return).
Der Charakter ist ein Phase-90-artiger 4-Stufen-Phaser mit freiem, langsamem Sweep. Später
(Teilprojekt 5) steuert der **Grit**-Knob seine Parameter mit.

**Erfolgskriterien:**

- Die Echos bekommen einen hörbaren, sanften Phasing-Sweep; Dry und Main bleiben unberührt.
- Mit Standard-Mix 0 ist der Klang bestehender Projekte und Kits unverändert.
- Bei `mix = 0` ist der Phaser bit-genau transparent.
- Auch bei maximaler Einstellung bleibt der Phaser stabil (kein Eigenschwingen).

## 2. Umfang

**Enthalten:** neue Klasse `Phaser`, Einbau in `FxChain`, drei Parameter (Rate, Depth, Mix) in
`FxParams` und im Plugin, eine Regler-Gruppe PHASER im `FxPanel` (vorübergehend).

**Nicht enthalten:** Tempo-Synchronisation, 8 Stufen, Typ-Umschaltung, der Grit-Knob, der
Advanced-Bereich.

## 3. Ansatz

Vier Allpässe 1. Ordnung (Phase-90-Aufbau), LFO-moduliert, mit weichem Feedback. Verworfen wurden
zwei Allpässe 2. Ordnung (schärfere Notches, eher Studio-Klang, mehr Aufwand) und modulierte
Delay-Ketten (kammartig, passt nicht zu "sanft").

## 4. Phaser

Neue Klasse `Phaser` in `engine/Phaser.h` und `engine/Phaser.cpp`. Sie bearbeitet das
Stereo-Signal der Delay-Rückläufer.

**Pro Kanal:**

- **Vier Allpässe 1. Ordnung** hintereinander, alle mit derselben Grenzfrequenz `fc`. Koeffizient
  `a = (tan(π·fc/sr) − 1) / (tan(π·fc/sr) + 1)`. Notches entstehen bei ca. 0,41·fc und 2,41·fc.
- **Mischung:** `y = (1 − 0,5·mix)·in + 0,5·mix·ap`. Bei `mix = 1` ist das die klassische 50/50-
  Mischung, bei `mix = 0` ist `y` bit-genau gleich dem Eingang.
- **Feedback:** Der Ausgang des letzten Allpasses geht über `tanh` zurück an den Eingang,
  `fb = 0,15 + 0,35·depth`. Sanft und weit von der Eigenschwingung entfernt.

**Sweep:**

- Ein Sinus-LFO fährt `fc` exponentiell um die Mitte von 700 Hz mit der Breite
  `±(0,5 + 2,0·depth)` Oktaven (bei `depth = 1` ca. 120 Hz bis 4 kHz).
- Der rechte Kanal läuft 90° versetzt (Stereo-Breite).
- Der LFO läuft frei in Hz, nicht tempo-synchron.

**Parameter:**

| Parameter | Bereich | Standard |
|---|---|---|
| Rate | 0,05 bis 3 Hz | 0,4 Hz |
| Depth | 0 bis 1 | 0,5 |
| Mix | 0 bis 1 | **0** |

Alle Konstanten (Mitte, Breite, Feedback-Bereich, Stereo-Versatz) stehen als benannte Werte oben
in `Phaser.cpp`.

## 5. Einbau

**Engine:**

- `FxParams` bekommt `phaserRate`, `phaserDepth`, `phaserMix` (Standard 0,4 / 0,5 / 0).
- `FxChain` besitzt einen `Phaser`, der in `prepare` und `reset` mitläuft.
- In `process` bearbeitet er die Delay-Rückläufer `dl` und `dr`, bevor sie mit `delayMix` in den
  Send gemischt werden. Dry und Main bleiben unberührt. Depth und Mix werden wie die übrigen
  Regler mit 20 ms geglättet.

**Plugin:**

- Drei neue Host-Parameter in `ParameterLayout` (automatierbar), mit den Standardwerten oben.
  Alte Kits und Projekte laden unverändert (Standard-Mix 0).
- **UI (vorübergehend):** neue Gruppe **PHASER** mit drei Reglern (Rate, Depth, Mix) im
  bestehenden `FxPanel` zwischen DELAY und REVERB. Das Panel wird um drei Zellen (216 px)
  breiter; die Basisbreite des Editors wächst entsprechend (von 1000 auf 1200 px). Mit dem
  Grit-Knob (Teilprojekt 5) wandern die drei Regler in den Advanced-Bereich.

## 6. Betroffene Dateien

- Neu: `engine/Phaser.h`, `engine/Phaser.cpp` (in `engine/CMakeLists.txt` eintragen),
  `tests/test_Phaser.cpp` (in `tests/CMakeLists.txt` eintragen).
- Geändert: `engine/FxParams.h`, `engine/FxChain.h`, `engine/FxChain.cpp`,
  `plugin/ParameterLayout.h`, `plugin/ParameterLayout.cpp`, `plugin/ui/FxPanel.h`,
  `plugin/ui/FxPanel.cpp`, `plugin/PluginEditor.h` (Basisbreite).
- Tests: `tests/test_FxChain.cpp`, `tests/plugin/test_PluginProcessor.cpp` und gegebenenfalls
  `tests/plugin/test_Editor.cpp`.

## 7. Tests

1. Bei `mix = 0` ist der Ausgang bit-genau gleich dem Eingang.
2. **Notch:** Mit Rauschen als Eingang entstehen bei `fc = 700 Hz` Einkerbungen bei ca. 290 Hz
   und 1,7 kHz, mindestens 10 dB tiefer als in der Umgebung.
3. **Sweep:** Die Notch-Lage bewegt sich mit dem LFO (zwei Zeitpunkte, deutlich verschiedene Lage).
4. **Stabilität:** Bei Depth 1, Mix 1 und 30 s Rauschen bleibt der Ausgang endlich und begrenzt,
   auch bei 44,1 und 96 kHz.
5. L und R unterscheiden sich durch den 90°-Versatz.
6. **Nur Rückläufer:** In `FxChain` ändert der Phaser den Main-Bus nie, auch bei Mix 1.
7. **Reset:** Nach NaN im Eingang erholt sich die Kette; der Phaser-Zustand wird zurückgesetzt.
8. Alle drei Parameter existieren, haben die Standardwerte und überleben das Speichern des
   Plugin-Zustands.
9. **Regression:** Alle bestehenden Tests laufen weiter; die Editor-Tests passen zur neuen Breite.

## 8. Abnahme

Die Charakterbeurteilung erfolgt per Gehör in Ableton. Die Konstanten sind leicht nachjustierbar
angelegt.
