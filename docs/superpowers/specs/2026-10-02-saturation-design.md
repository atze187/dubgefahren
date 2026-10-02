# Saturation (weich bis hart, ADAA) – Design-Spezifikation

**Datum:** 2026-10-02
**Status:** Spec, Review und Plan offen
**Teilprojekt 2 von 5** der Überarbeitung des Effekt-Charakters

## 1. Hintergrund und Ziel

Teilprojekt 1 (Tape-Delay, PR #23) hat dem Delay den RE-201-Charakter gegeben. Teilprojekt 2
ersetzt das einfache Drive in `engine/Drive.h`. Es ist rein symmetrisch (`tanh`, nur ungerade
Obertöne), hat keine Frequenzabhängigkeit, läuft ohne Anti-Aliasing (bei hohem Drive klappen
Obertöne ins Hörbare zurück und klingen digital) und wird durch den Pegelausgleich `1/sqrt(g)`
bei starkem Drive leiser statt wärmer.

Die neue Sättigung ist bei wenig Drive weich und warm, wird ab der Mitte zunehmend härter und
läuft über denselben Drive-Regler. Das passt zum späteren **Grit**-Knob (Teilprojekt 5).

**Erfolgskriterien:**

- Drive 0 ist bit-genau transparent.
- Niedriger Drive klingt warm (gerade Obertöne), hoher Drive hart und bissig.
- Der Pegel wächst mit dem Drive um ca. 6 dB (Sinus bei −12 dBFS), statt wie bisher zu sinken.
- Aliasing ist hörbar geringer als beim bisherigen Ansatz, ohne Latenz.
- Bestehende Projekte und Kits laden unverändert (klingen allerdings anders).

## 2. Umfang

**Enthalten:** neue Klasse `Saturator` (Kennlinien, ADAA, DC-Blocker, Pegel), Einbau in
`FxChain` an der bisherigen Stelle (vor dem Filter, auf Main und Send).

**Nicht enthalten:** UI-Änderungen, der Grit-Knob, klassisches Oversampling, ein Tone-Regler,
neue Parameter.

## 3. Ansatz

Zwei Kennlinien (weich und hart), über Drive überblendet, mit Anti-Aliasing über die Methode der
Stammfunktion (ADAA, 1. Ordnung). Verworfen wurden klassisches 2x-/4x-Oversampling (Latenz, die
auch bei Drive 0 konstant sein müsste; mehr CPU und Code) und eine einzelne Kennlinie mit
variablem Exponenten (keine geschlossene Stammfunktion, bräuchte Oversampling).

## 4. Kennlinie und Pegel

`Saturator` ersetzt die freie Funktion `driveSample` und hält Zustand pro Signalweg. `FxChain`
besitzt vier Instanzen (MainL, MainR, SendL, SendR). Die Drive-Glättung in `FxChain` bleibt;
`Saturator` bekommt den geglätteten Wert `d` (0 bis 1).

Aus `d` abgeleitet:

- **Vorverstärkung:** `g = 10^(1,4·d)`, von 1 bis ca. 25 (+28 dB).
- **Härte-Anteil:** `m = smoothstep(0,3 … 1,0 | d)`; unter `d = 0,3` rein weich, bei `d = 1` rein hart.
- **Bias:** fester Wert `b = 0,1` für gerade Obertöne.

Pro Sample:

1. `u = g·x + b`
2. weich `S(u) = tanh(u) − tanh(b)`, hart `H(u) = clamp(u, −1, 1) − clamp(b, −1, 1)`,
   jeweils per ADAA berechnet (Abschnitt 5)
3. `y = (1−m)·S + m·H`
4. Pegel: `y · g^(−β)`
5. DC-Blocker bei ca. 10 Hz
6. Mischung wie bisher: `out = x + min(1, 10·d)·(y − x)`. Bei `d = 0` ist das Signal damit
   bit-genau unverändert.

**Pegelverhalten (teilweise ausgeglichen):** `β` wird so abgestimmt, dass ein Sinus bei −12 dBFS
zwischen `d = 0` und `d = 1` um ca. +6 dB lauter wird. Die Abstimmung erfolgt per Messung im Test.
Die Spitzen fängt weiter der Limiter am Ende der Kette ab. `β` und die übrigen Klangkonstanten
stehen als benannte Werte oben in `Saturator.cpp`.

## 5. ADAA

Erste Ordnung: `y[n] = (F(u[n]) − F(u[n−1])) / (u[n] − u[n−1])` mit der Stammfunktion `F`.

- **Weich:** `F(u) = ln(cosh u)`, numerisch stabil als `|u| − ln 2 + log1p(e^(−2|u|))`.
- **Hart:** `F(u) = u²/2` für `|u| ≤ 1`, sonst `|u| − 0,5` (mit passendem Vorzeichen), stetig.
- **Kleine Schritte:** bei `|u[n] − u[n−1]| < 1e-6` wird `f((u[n] + u[n−1]) / 2)` verwendet.
- **Genauigkeit:** Die ADAA-Rechnung läuft in `double`.
- **Zustand:** letzter Wert `u[n−1]` und die zwei Stammfunktionswerte daran; pro Sample wird
  nur ein `ln cosh` berechnet.
- **Zeitvarianter Drive:** `g` ändert sich durch die Glättung pro Sample; `u` enthält den
  aktuellen Wert, ADAA bleibt anwendbar.
- **Reset:** `reset()` löscht den Zustand; das erste Sample danach nutzt direkt `f(u)`.
  `FxChain::reset()` setzt alle vier `Saturator` zurück (Absicherung bei NaN/Inf am Ausgang).

## 6. Betroffene Dateien

- Neu: `engine/Saturator.h`, `engine/Saturator.cpp` (in `engine/CMakeLists.txt` eintragen).
- Geändert: `engine/FxChain.h`, `engine/FxChain.cpp` (vier `Saturator` statt `driveSample`,
  `prepare` und `reset` ergänzt).
- Entfällt: `engine/Drive.h`.
- Tests: neu `tests/test_Saturator.cpp` (in `tests/CMakeLists.txt` eintragen); der Drive-Test
  in `tests/test_FxUnits.cpp` wandert dorthin.
- Unverändert: Parameter `drive`, UI, Kits und Presets.

## 7. Tests

1. Bei `d = 0` ist der Ausgang bit-genau gleich dem Eingang.
2. Bei `d = 1` bleibt der Ausgang bei beliebig großem Eingang endlich und begrenzt.
3. Der Ausgang wächst monoton mit dem Eingangspegel.
4. **Pegel:** Ein Sinus bei −12 dBFS wird zwischen `d = 0` und `d = 1` um 6 dB ± 1,5 dB lauter.
5. **Charakter:** Bei `d = 0,3` ist die 2. Harmonische deutlich vorhanden (gerade Obertöne);
   bei `d = 1` ist die 3. Harmonische stärker als bei `d = 0,3`.
6. **Kein Gleichanteil:** Der Mittelwert eines Sinus bei `d = 1` liegt unter 0,01.
7. **Aliasing:** Bei einem 9-kHz-Sinus und `d = 1` hat ADAA mindestens 6 dB weniger Energie auf
   nicht-harmonischen Frequenzen als dieselbe Kennlinie ohne ADAA (naive Referenz im Test).
8. **Robustheit:** stabil bei 44,1 und 96 kHz; ein Sweep von `d` über die Zeit erzeugt keine
   Sprünge im Ausgang; konstantes Eingangssignal (auch Stille) erzeugt keine NaN-Werte.
9. **Regression:** Die bestehenden `FxChain`-Tests laufen weiter, auch der mit Extremwerten
   unter der Limiter-Grenze.

## 8. Abnahme

Die Charakterbeurteilung erfolgt per Gehör in Ableton. Die Konstanten (`β`, Bias, Härte-Bereich,
Maximalverstärkung) sind leicht nachjustierbar angelegt. Nach dem Bau gibt es ein Urteil, bevor
die Zahlen festgeschrieben werden.
