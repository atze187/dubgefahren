# Delay-Filter im Feedback mit LFO – Design-Spezifikation

**Datum:** 2026-10-01
**Status:** Umgesetzt, Abnahme in Ableton offen
**Issue:** atze187/dubgefahren#20 (vor #17, Stretching)

## 1. Ziel

Der globale Filter und der Tone-Regler des Delays entfallen. Stattdessen sitzt ein
Filter im Feedback-Weg des Tape-Delays. Er hat vier Typen (Lowpass, Bandpass,
Highpass, Notch) und einen LFO, der Cutoff und Resonanz moduliert. Jede Wiederholung
läuft erneut durch den Filter; das ergibt die typischen Dub-Echos, die mit jedem
Durchlauf dunkler, dünner oder bewegter werden.

**Erfolgskriterien:**

- Das erste Echo ist bereits gefiltert, jede Wiederholung stärker.
- LP, BP, HP und Notch lassen sich wählen; ein Wechsel im laufenden Signal knackt nicht.
- Mit LFO-Tiefen von 0 klingt der Delay wie ein statischer Filter im Feedback.
- Der LFO läuft frei (Hz) oder tempo-synchron und moduliert Cutoff (Oktaven) und
  Resonanz getrennt.
- Bei Feedback 1,1 und Resonanz 1 bleibt der Ausgang begrenzt und endlich.
- Der Dry-Weg und der Reverb werden nicht mehr gefiltert.
- Alte Host-Projekte laden ohne Fehler; die entfernten Parameter werden ignoriert.

## 2. Umfang

**Enthalten:** Filter im Delay-Loop (neuer Notch-Typ, Typ-Umschalter), LFO, neue
Parameter, Entfernen des globalen Filters und von Delay Tone, neues FX-Panel
(16 Zellen), Anpassung von Tests und README.

**Nicht enthalten:** Filter-Flankensteilheit über 12 dB, Hüllkurven-Modulation des
Filters, ein zweiter LFO, Ping-Pong oder freie Delay-Zeit (spätere Themen).
Drive bleibt unverändert global vor den Wegen.

## 3. Signalweg

```
Pad-Summe → Drive → Main ─────────────────────────┐
                  → Send → Delay[ Puffer → Filter → Wet ] → Reverb → Send-Summe
                                         └→ tanh(Feedback) → zurück in den Puffer
Main + Send-Summe → Master → Limiter
```

- Im Delay-Loop liest der Lesekopf den Puffer (mit Wow wie bisher), danach läuft das
  Signal durch den Filter. Das gefilterte Signal ist der Wet-Ausgang und füttert
  über `tanh(feedback * wet)` den Puffer.
- `FxChain` besitzt keine `SvFilter` mehr; Main und Send laufen nach Drive direkt
  weiter.

## 4. Filter

`SvFilter` (TPT, 12 dB) wird erweitert:

- Typen als `enum class FilterType { Lowpass, Bandpass, Highpass, Notch }`.
  Notch = LP + HP des Zustandsvariablen-Filters.
- Die stufenlose LP→BP→HP-Überblendung wird durch eine Überblendung zwischen den
  Typgewichten ersetzt: Bei einem Typwechsel gleiten die Gewichte in etwa 10 ms zum
  neuen Typ, damit es nicht knackt.
- Cutoff und Resonanz werden alle 8 Abtastwerte neu gesetzt (Ein-Pol-Glättung, ca. 5 ms).
  Die Koeffizienten werden dabei exakt (mit `tan()`) berechnet und dazwischen nicht
  interpoliert; die CPU-Last wird gemessen (Abschnitt 8).
- Die Resonanz wirkt wie bisher als Q-Faktor (0,5 bis 20).

## 5. LFO

- Wiederverwendung von `Lfo` und `LfoShape` (Square, Triangle, SawUp, SawDown,
  SampleHold) sowie `lfoRateFromSync` und `SyncDivision` aus dem Slot-LFO.
- Rate: frei 0,05 bis 40 Hz oder per Sync-Teilung zum Host-Tempo.
- Der LFO-Wert liegt in [0, 1] und wird zentriert (`2 * v - 1`), damit er um den
  eingestellten Wert schwingt.
- **Cutoff:** `cutoff * 2^(depthOct * c)` mit `c` in [-1, 1], Tiefe 0 bis 4 Oktaven;
  das Ergebnis wird auf 20 Hz bis 0,49 · Samplerate begrenzt.
- **Resonanz:** `res + depth * c`, auf [0, 1] begrenzt, Tiefe 0 bis 1.
- Der LFO läuft pro Sample im Delay und setzt sich bei einem Reset zurück. Er
  synchronisiert nicht mit der Host-Transportposition, nur mit dem Tempo.

## 6. Parameter

**Entfallen (4):** `fltCutoff`, `fltRes`, `fltType`, `dlyTone`.

**Neu (9):**

| ID | Anzeige | Bereich | Standard |
|---|---|---|---|
| `dlyFltType` | Delay Filter Type | LP, BP, HP, Notch | LP |
| `dlyFltCutoff` | Delay Filter Cutoff | 20 bis 20000 Hz | 2400 Hz |
| `dlyFltRes` | Delay Filter Resonance | 0 bis 1 | 0,1 |
| `dlyLfoShape` | Delay LFO Shape | 5 Formen | Triangle |
| `dlyLfoRate` | Delay LFO Rate | 0,05 bis 40 Hz | 0,5 Hz |
| `dlyLfoSync` | Delay LFO Sync | aus / an | aus |
| `dlyLfoSyncDiv` | Delay LFO Sync Rate | Slot-Teilungen | 1/4 |
| `dlyLfoCutDepth` | Delay LFO Cutoff Depth | 0 bis 4 Oktaven | 0 |
| `dlyLfoResDepth` | Delay LFO Resonance Depth | 0 bis 1 | 0 |

Die Zahl der Host-Parameter steigt von 436 auf 441. Der Cutoff-Regler nutzt wie
bisher eine logarithmische Skala. `FxParams` bekommt die entsprechenden Felder;
`delayTone`, `cutoffHz`, `resonance` und `filterType` werden entfernt.

**Migration:** Kit-Dateien enthalten keine FX-Parameter, es ändert sich nichts am
Kit-Format. Ein älterer Host-Zustand enthält die entfernten Parameter noch; APVTS
ignoriert sie, die neuen Parameter fallen auf ihre Standardwerte.

## 7. Oberfläche

Das FX-Panel ist eine Reihe aus Zellen. Bisher 13 Zellen à 72 px; neu 16 Zellen von
etwa 60 px Breite (976 px verfügbar):

| Gruppe | Zellen |
|---|---|
| DRIVE | Drive |
| DELAY | Time, Feedback, Wow, Mix |
| FILTER | Type, Cutoff, Res |
| LFO | Shape, Rate/Sync, Cutoff Depth, Res Depth |
| REVERB | Decay, Tone, Mix |
| MASTER | Master |

- Die Rate-Zelle zeigt bei ausgeschaltetem Sync Hertz und bei eingeschaltetem Sync die
  Teilung; der kleine Sync-Schalter sitzt darunter.
- Der Typ ist eine ComboBox (LP, BP, HP, Notch).
- Zwei Reihen werden verworfen, weil das Slot-Panel mit 376 von 384 px schon voll ist.

## 8. Tests

- **SvFilter:** Notch dämpft am Cutoff stark und lässt entfernte Frequenzen durch;
  LP, BP, HP wie bisher; ein Typwechsel erzeugt keinen Sprung über einer Schwelle.
- **TapeDelay:** Mit Tiefpass verliert jede Wiederholung Höhen; das erste Echo ist
  gefiltert; Feedback 1,1 mit Resonanz 1 bleibt endlich; mit Tiefe 0 entspricht der
  Ausgang dem statischen Filter.
- **LFO im Delay:** Cutoff-Modulation schwingt zentriert; Sync folgt dem Tempo;
  Resonanz bleibt in [0, 1].
- **FxChain:** Der Dry-Weg besteht nur aus Drive und Master; veraltete Tests zum
  globalen Filter in `test_FxChain` und `test_FxUnits` werden angepasst oder entfernt.
- **Plugin:** 441 Parameter mit den neuen Standardwerten; ein Host-Zustand mit den
  entfernten Parametern lädt fehlerfrei; Render-Test des FX-Panels bei 0,75×, 1× und
  2× ohne abgeschnittene Zellen; CPU-Last: 10 s Audio durch die FX-Kette mit allen
  Modulationen in unter 2 s (Release; gemessen: etwa 0,09 s).
- **Abnahme in Ableton:** Echos klingen mit allen vier Typen plausibel; LFO auf
  Cutoff und Resonanz hörbar; Sync folgt dem Tempo; Selbstoszillation bei Resonanz 1
  und Feedback 1,1 bleibt kontrolliert.

## 9. Risiken

- Ein Filter mit hoher Resonanz in der Feedback-Schleife kann selbst schwingen.
  `tanh` und der Master-Limiter begrenzen das.
- Das Klangbild ändert sich bewusst: Der Dry-Weg und der Reverb werden nicht mehr
  gefiltert, und ohne Tone klingt der Delay anders als vorher. Alte Projekte klingen
  deshalb nicht identisch.
- Die Koeffizienten werden nur alle 8 Abtastwerte neu berechnet; die CPU-Last ist
  gemessen und liegt bei rund einem Prozent der Echtzeit.
