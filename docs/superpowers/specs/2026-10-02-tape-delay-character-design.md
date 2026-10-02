# Tape-Delay-Charakter (RE-201) – Design-Spezifikation

**Datum:** 2026-10-02
**Status:** Spec, Review und Plan offen
**Teilprojekt 1 von 5** der Überarbeitung des Effekt-Charakters

## 1. Hintergrund und Ziel

Eine längere Session in Ableton hat gezeigt, dass die Effektkette allgemein noch steril klingt.
Die Überarbeitung ist in fünf Teilprojekte zerlegt, jedes mit eigener Spec, eigenem Plan
und eigener Umsetzung:

1. **Delay-Charakter** (diese Spec)
2. Saturation (ersetzt das einfache `tanh`-Drive in `engine/Drive.h`)
3. Phaser (neuer Effekt)
4. Reverb-Charakter
5. Performance-Knobs **Space**, **Grit**, **Throw** samt UI mit einklappbarem Advanced-Bereich
   (zuletzt, weil die Kurven erst nach den Effekten sinnvoll festzulegen sind)

Dieses Teilprojekt gibt dem Delay den Charakter des **Roland RE-201 Space Echo**: warm,
weich, leicht unregelmäßig. Das Benidub Digital Echo bleibt als Modus-Wunsch außerhalb des Umfangs.

**Erfolgskriterien:**

- Das Delay klingt hörbar wärmer und lebendiger als bisher; jede Wiederholung dunkelt leicht ab
  und wird durch die Sättigung weicher.
- Die Tonhöhe schwankt unregelmäßig und wiederholt sich nicht periodisch.
- Bei Feedback bis 110 % bleibt der Ausgang endlich und begrenzt.
- Bei normalem Feedback ist kein Dauerzischen hörbar.
- Bestehende Projekte und Kits laden unverändert (klingen allerdings anders).

## 2. Umfang

**Enthalten:**

1. Tape-Klang im Loop: Sättigung, sanfter Höhenverlust pro Wiederholung, Bass-Bump, Hochpass.
2. Unregelmäßige Tonhöhenschwankung: Wow und Flutter mit Zufallsanteil.
3. Leises Bandrauschen im Loop.

**Nicht enthalten:**

- Mehrfach-Köpfe (kommt in einem späteren Projekt).
- Benidub-/Digital-Modus.
- Ping-Pong und Stereo-Breite über die kleine Modulationsversetzung hinaus.
- Der Feedback-Filter aus PR #21 (bleibt ruhen, hat sich eventuell erledigt).
- Neue Parameter oder UI-Änderungen.

## 3. Ansatz

Die bestehende Struktur von `engine/TapeDelay` wird schrittweise ausgebaut. Verworfen wurden
ein physikalisches Bandmodell (Hysterese; hohe CPU-Last, schwer abzustimmen) und gemessene
Impulsantworten des Originals (Lizenz- und Datenfragen, Zeitvariation geht verloren).

## 4. Signalweg im Loop (pro Kanal)

Eingabe, Zeit-Glättung beim Wechsel der Division und die Parameter bleiben unverändert.

1. **Lesen** mit kubischer Interpolation (4 Punkte) statt linearer Interpolation.
2. **Hochpass** bei ca. 100 Hz, damit sich im Loop kein Bass aufstaut.
3. **Bass-Bump:** sanftes Low-Shelf bei ca. 120 Hz, etwa +2 dB.
4. **Tiefpass** über den bestehenden Tone-Regler (500 Hz bis 12 kHz), ergänzt um eine feste
   leichte Höhenabsenkung bei 9 bis 10 kHz.
5. **Sättigung:** weicher `tanh` mit kleiner Asymmetrie (gerade Obertöne) und DC-Blocker.
   Sie begrenzt wie bisher auch bei 110 % Feedback.
6. **Schreiben:** Eingang plus Feedback-Anteil plus leises Bandrauschen (ca. −75 dBFS),
   das im Loop mit umläuft.

Es gibt keine neuen Bedienelemente. Time, Feedback, Tone, Wow und Mix bleiben bestehen.

**Risiko Rauschen:** Es darf bei geringem Feedback nicht als Dauerzischen hörbar sein. Der Pegel
wird so gewählt, dass es erst bei hohem Feedback hörbar wird, und im Test gemessen.

## 5. Modulation (Wow und Flutter)

1. **Wow (langsam):** der bestehende 0,55-Hz-Sinus plus ein unregelmäßiger Anteil aus
   tiefpassgefiltertem Zufall (Random Walk, ca. 0,2 bis 1 Hz).
2. **Flutter (schnell):** Sinus bei 6 bis 9 Hz mit leicht schwankender Frequenz und Amplitude,
   dazu ein kleiner Jitter-Anteil aus gefiltertem Rauschen.
3. **Kopplung:** Der bestehende **Wow-Regler** skaliert beide Anteile gemeinsam; Flutter mit
   kleinerem Faktor.
4. **Stereo:** Die Modulation ist für L und R fast gleich, mit kleinem festem Versatz.
5. **Zufall:** Fester Startwert pro Instanz, damit Tests reproduzierbar sind. Der Generator
   wird bei `reset()` nicht zurückgesetzt.
6. **Tiefe:** Das heutige Maximum wird nicht überschritten. Deutliches Wabern gibt es erst
   bei Wow ganz oben.

## 6. Betroffene Dateien

- `engine/TapeDelay.h`, `engine/TapeDelay.cpp`: neue Loop-Kette und Modulation.
- `engine/DspMath.h`: falls nötig kubische Interpolation und DC-Blocker.
- `engine/FxChain.cpp`, `engine/FxParams.h`: unverändert (gleiche `setParams`-Schnittstelle).
- Keine Änderung an UI, gespeicherten Kits oder Presets.

## 7. Tests (`tests/test_TapeDelay.cpp`)

1. **Stabilität:** Bei Feedback 1,1 und Dauerrauschen als Eingang bleibt der Ausgang endlich
   und begrenzt, auch nach mehreren Sekunden.
2. **Höhenverlust:** Ein Impuls verliert mit jeder Wiederholung messbar Höhenenergie.
3. **Kein Gleichanteil:** Der DC-Blocker verhindert einen Offset trotz asymmetrischer Sättigung.
4. **Rauschpegel:** Ohne Eingang und mit Feedback 0,45 bleibt das Rauschen unter einer
   festgelegten Schwelle; bei Feedback 1,1 ist es hörbar, aber begrenzt.
5. **Modulation:** Bei Wow 0 ist die Zeit konstant; bei Wow 1 schwankt sie innerhalb der
   Grenzen, und der Verlauf wiederholt sich nicht periodisch.
6. **Regression:** Die bestehenden Tests zu Division, Glide und Mix laufen weiter.

## 8. Abnahme

Die Charakterbeurteilung erfolgt per Gehör in Ableton. Die Konstanten (Bump, Sättigung,
Rauschen, Modulationstiefe) sind leicht nachjustierbar angelegt. Nach dem Bau gibt es ein Urteil,
bevor die Zahlen festgeschrieben werden.
