# Sample-Bereiche: Start/End, Loop, Reverse – Design-Spezifikation

**Datum:** 2026-09-30
**Status:** Entwurf zur Freigabe
**Issue:** atze187/dubgefahren#11 (Voraussetzung für #17, Stretching)

## 1. Ziel

Ein Sample-Slot spielt nicht mehr zwingend die ganze Datei. Pro Slot lassen sich ein
Start- und ein Endpunkt setzen, ein hinterer Abschnitt loopen und die Wiedergabe
umkehren. Die Punkte werden in einer Wellenform-Anzeige mit ziehbaren Markern gesetzt.

**Erfolgskriterien:**

- Mit den Standardwerten klingt jeder bestehende Sample-Slot exakt wie bisher.
- Start, Loop-Start und End lassen sich in der Wellenform ziehen und automatisieren.
- Eine Schleife läuft mit X-Fade ohne Knacken an der Nahtstelle; bei 0 % ist der
  harte Schnitt hörbar.
- Reverse funktioniert mit und ohne Loop.
- Ein geloopter Sample-Slot lässt sich latchen: erste Note startet, zweite stoppt.
- Alte Kits (Version 1 bis 3) und alte Host-Projekte laden unverändert.

## 2. Umfang

**Enthalten:** sechs neue Slot-Parameter, Wiedergabe-Logik im `SamplePlayer`,
Latch für geloopte Sample-Slots, Kit-Format Version 4, Wellenform-Anzeige mit drei
Markern, neue Anordnung des Sample-Modus im Slot-Editor.

**Nicht enthalten:** Ping-Pong-Schleifen, Einrasten der Marker (z. B. an
Nulldurchgängen), eine Positionsanzeige (laufender Strich) in der Wellenform, Zoom in
der Wellenform, Stretching (#17), Farbzuweisung pro Pad (#14).

## 3. Parameter

Sechs neue Slot-Felder, alle über die Feld-Tabelle (`SlotField`/`FieldSpec`) mit dem
`ParameterID`-Versionshinweis **4**. Sie sind automatisierbar und stehen im Kit.

| Feld (`SlotField`) | Schlüssel | Art | Bereich | Standard | Einheit |
|---|---|---|---|---|---|
| `SampleStart` | `smpStart` | Float | 0 … 1 | 0 | – |
| `LoopStart` | `loopStart` | Float | 0 … 1 | 0 | – |
| `SampleEnd` | `smpEnd` | Float | 0 … 1 | 1 | – |
| `Loop` | `loop` | Bool | – | aus | – |
| `Reverse` | `reverse` | Bool | – | aus | – |
| `LoopXfade` | `xfade` | Float | 0 … 50 | 5 | % |

Start, Loop-Start und End sind Anteile der Sample-Länge. X-Fade ist ein Anteil der
Länge des geloopten Abschnitts.

In `SlotParams`: `sampleStart`, `loopStart`, `sampleEnd` (float), `loop`, `reverse`
(bool), `loopXfadePct` (float). Die Felder gelten nur für Sample-Slots; bei
Synth-Slots bleiben sie erhalten, wirken aber nicht.

## 4. Bereiche

### 4.1 Auflösen der Marker

Eine reine Funktion in `engine/SampleRegion.h` rechnet die drei Anteile in Positionen
um. Engine und Wellenform benutzen dieselbe Funktion.

```cpp
struct SampleRegion
{
    double start = 0.0;     // erste gespielte Position (in Samples der Datei)
    double loopStart = 0.0; // Marker L
    double end = 0.0;       // Position hinter dem letzten gespielten Sample
};

inline constexpr double kMinRegionSamples = 16.0;

SampleRegion resolveSampleRegion(float start01, float loopStart01, float end01, std::size_t numSamples);
```

Regeln, in dieser Reihenfolge (`n` = Anzahl Samples, `m` = min(`kMinRegionSamples`, `n`)):

1. Alle Anteile werden auf 0 … 1 begrenzt; nicht endliche Werte gelten als ihr Standard.
2. `end = max(end01 · n, m)`, begrenzt auf `n`.
3. `start = min(start01 · n, end − m)`.
4. `loopStart = clamp(loopStart01 · n, start, end)`.

Damit gilt immer `0 ≤ start ≤ loopStart ≤ end ≤ n` und `end − start ≥ m`. Die
Parameter selbst werden dabei nie verändert.

### 4.2 Gelooptes Segment

| Richtung | Segment | Rücksprung |
|---|---|---|
| vorwärts | `loopStart … end` | von `end` nach `loopStart` |
| Reverse | `start … loopStart` | von `start` nach `loopStart` |

Ist das Segment kürzer als `m`, kreist stattdessen der ganze Bereich
(`start … end`). Das ist der Fall bei Loop-Start = End (vorwärts) und bei
Loop-Start = Start (Reverse), also auch bei den Standardwerten mit Reverse.

### 4.3 Abspielen

| | Loop aus | Loop an |
|---|---|---|
| vorwärts | `start` → `end`, dann Ende | `start` → `end`, dann immer wieder das Segment |
| Reverse | `end` → `start`, dann Ende | `end` → `start`, dann immer wieder das Segment |

- Bei Reverse beginnt die Wiedergabe am letzten Sample des Bereichs und läuft
  rückwärts. Tune und Performance-Pitch bestimmen wie bisher die Schrittweite, nur
  mit umgekehrtem Vorzeichen.
- Ohne Schleife wird am Ende des Bereichs wie bisher über 2 ms linear ausgeblendet
  (vorwärts vor `end`, rückwärts vor `start`).
- „Loop an“ gilt für die Stimme nur, wenn das Feld `Loop` an ist **und** der
  Trigger-Modus nicht One Shot ist.

### 4.4 X-Fade

Der Crossfade wird aus der Position berechnet und braucht keinen eigenen Zustand;
der zweite Lesekopf bleibt dem Retrigger vorbehalten.

- Länge: `xf = LoopXfade / 100 · Segmentlänge`.
- Vorwärts: Im letzten Stück der Länge `xf` vor `end` wird linear in das Material
  übergeblendet, das im Abstand einer Segmentlänge davor liegt, also in das Stück
  direkt vor dem Schleifenanfang. Am Sprung ist das Signal dadurch stetig.
  `xf` wird auf das vorhandene Material begrenzt: `xf ≤ Schleifenanfang` (Abstand
  zum Dateianfang). Material vor dem Start-Marker darf dafür gelesen werden.
- Reverse: gespiegelt. Im letzten Stück vor dem unteren Segmentende wird in das
  Material direkt hinter dem oberen Segmentende übergeblendet; `xf` ist auf den
  Abstand zum Dateiende begrenzt.
- `xf = 0` ergibt einen harten Schnitt.

### 4.5 Änderungen während des Spielens

Der Bereich wird in jedem Block neu aufgelöst; Marker, Loop, Reverse und X-Fade
wirken sofort.

- Liegt die Position nach einer Änderung hinter dem Ende des Bereichs (in
  Laufrichtung), gilt das Ende als erreicht: mit Schleife Sprung an den
  Schleifenanfang, ohne Schleife Ende der Stimme. In beiden Fällen läuft die alte
  Position als zweiter Lesekopf 5 ms aus (dieselbe Überblendung wie beim Retrigger),
  damit es nicht knackt.
- Liegt die Position vor dem Anfang des Bereichs, spielt die Stimme weiter und
  läuft von selbst in den Bereich hinein.
- Wird Reverse während des Spielens umgeschaltet, kehrt die Stimme an ihrer
  aktuellen Position die Richtung um.

## 5. Trigger-Modi bei Sample-Slots

- **Gate:** spielt, solange die Note gehalten wird; beim Loslassen Ausklang mit
  Release. Eine Schleife läuft während des Ausklangs weiter.
- **One Shot:** spielt den Bereich einmal bis zu seinem Ende. Loop wird ignoriert.
- **Latch:** gilt nur, wenn `Loop` an ist: erste Note startet, zweite stoppt (wie bei
  Synth-Slots). Ist `Loop` aus, wirkt Latch wie Gate (wie bisher).

Folgen für die Engine:

- Beim Aufbau der Trigger-Einstellungen bleibt Latch für einen Sample-Slot Latch,
  wenn `Loop` an ist; sonst wird es wie bisher zu Gate.
- Wird `Loop` ausgeschaltet, während ein Slot gelatcht ist, spielt die Stimme bis zum
  Ende des Bereichs und endet. `PadRouter::advance` löscht den Latch-Zustand eines
  Slots, dessen Stimme nicht mehr aktiv ist, damit der Punkt auf dem Pad erlischt.

Der Mausklick auf ein Pad bleibt eine Gate-Vorschau (unverändert).

## 6. Kit-Datei und Host-State

- Kit-Format **Version 4**. Die sechs Felder stehen mit ihren Schlüsseln im
  `params`-Objekt des Slots.
- Kits der Versionen 1 bis 3 laden unverändert; fehlende Felder erhalten ihre
  Standardwerte. Ein Kit der Version 4 lässt sich mit einer älteren Plugin-Version
  nicht öffnen (bestehende Meldung „created with a newer version“).
- Alte Host-Projekte kennen die neuen Parameter nicht; APVTS setzt sie beim
  Wiederherstellen auf ihre Standardwerte.
- Kopieren/Einfügen, Reset und `currentKit()` tragen die Felder über die
  Feld-Tabelle automatisch mit. „Reset to Factory Default“ und der Wechsel der
  Quelle auf ein anderes Sample setzen die sechs Felder auf ihre Standardwerte.

## 7. UI

### 7.1 Wellenform (`plugin/ui/WaveformView.h/.cpp`, neu)

- Der Processor gibt dem Editor die geladenen Daten eines Slots heraus
  (`std::shared_ptr<const SampleData> slotSampleData(int slot) const`, nur auf dem
  Message-Thread).
- Spitzenwerte: eine reine Funktion berechnet aus den Samples pro Pixelspalte Minimum
  und Maximum. Sie läuft einmal pro Sample und Breite, nicht bei jedem Zeichnen.
- Aussehen im Look von #15: vertiefte Fläche wie ein leeres Pad, Kurve in der
  Pad-Grundfarbe. Der Teil außerhalb von Start … End ist abgedunkelt. Ist `Loop` an,
  ist das geloopte Segment (Abschnitt 4.2, abhängig von Reverse) blau getönt.
- Marker: **S** und **E** in Textfarbe, **L** in Blau (`colours::latched`); jeweils
  eine senkrechte Linie mit einem Griff am oberen Rand. **L** ist nur sichtbar, wenn
  `Loop` an ist.
- Ziehen: Ein Zug ist eine Host-Geste (`beginChangeGesture` … `endChangeGesture`)
  auf dem zugehörigen Parameter. Ein Marker bleibt am Nachbarn hängen, statt ihn zu
  überholen: S ≤ L ≤ E, und zwischen S und E bleibt mindestens die Mindestlänge aus
  Abschnitt 4.1. Liegen zwei Griffe übereinander, greift der Klick den Marker, der
  sich in Richtung der Mausbewegung noch bewegen lässt; bei Gleichstand gilt die
  Reihenfolge L, E, S.
- Die Anzeige liest die Marker aus den Parametern und folgt damit auch Automation
  und Slot-Wechsel.
- Ohne Daten (Sample fehlt oder lädt noch) zeigt der Streifen statt der Kurve den
  Text „No sample loaded“ bzw. bei fehlender Datei „Sample missing“; Marker sind
  dann nicht ziehbar.

### 7.2 Slot-Editor im Sample-Modus

Vier Reihen wie im Synth-Modus:

| Reihe | Inhalt |
|---|---|
| 1 | Wellenform über die volle Breite |
| 2 `SAMPLE / AMP` | Tune, Attack, Release |
| 3 `LOOP / TRIG` | Loop (Schalter), Reverse (Schalter), X-Fade (Regler), Mode, Choke |
| 4 `MIX` | Volume, Pan, FX-Send |

- Der Eintrag „Latch“ im Modus ist bei Sample-Slots genau dann wählbar, wenn `Loop`
  an ist.
- Synth-Modus und leerer Slot bleiben unverändert.

## 8. Risiken

Werden im Plan als frühe Schritte geprüft.

- **Knacken beim Verschieben der Marker während des Spielens.** Abschnitt 4.5 sieht
  dafür die 5-ms-Überblendung vor; zu prüfen ist, ob sie bei schnellem Ziehen (viele
  Sprünge hintereinander) reicht.
- **Retrigger mitten im Crossfade.** Der Crossfade ist zustandslos, der zweite
  Lesekopf bleibt dem Retrigger. Zu prüfen ist, dass beide zusammen kein Knacken und
  keine Pegelüberhöhung ergeben.

## 9. Tests und Abnahme

**Automatisch**

- `resolveSampleRegion`: Standardwerte ergeben die ganze Datei; vertauschte und
  identische Marker; Werte außerhalb 0 … 1 und nicht endliche Werte; Datei kürzer als
  die Mindestlänge; Datei mit einem Sample.
- `SamplePlayer`: Start und End begrenzen die Wiedergabe; Standardwerte ergeben
  dieselbe Ausgabe wie bisher; Schleife vorwärts und rückwärts trifft die Segmente
  aus Abschnitt 4.2, auch den Rückfall auf den ganzen Bereich; One Shot ignoriert
  Loop; X-Fade 0 % springt hart, größere Werte ergeben an der Nahtstelle keinen
  Sprung; zu wenig Material vor dem Schleifenanfang; Marker werden während des
  Spielens verschoben (Position danach innerhalb, hinter dem Ende, vor dem Anfang);
  Reverse wird während des Spielens umgeschaltet; Tune und Reverse zusammen;
  Retrigger mitten im Crossfade.
- Engine und Router: Latch bei Sample-Slots nur mit Loop; zweite Note stoppt; Loop
  wird ausgeschaltet, während ein Pad gelatcht ist, und der Latch-Zustand erlischt.
- Plugin: Kit Version 4 schreiben und lesen; Version 3 laden; alter Host-State lädt
  mit Standardwerten; Kopieren/Einfügen und Reset.
- UI: Spitzenwerte für leere, sehr kurze und lange Daten; ein Marker-Zug schreibt den
  Parameter und bleibt am Nachbarn hängen; der Latch-Eintrag folgt dem
  Loop-Schalter; der Editor zeichnet im Sample-Modus bei 0,75×, 1× und 2×
  fehlerfrei, mit und ohne Daten.
- Alle bestehenden Tests und der Validator bleiben grün.

**Von Hand in Ableton**

- Schleife knackfrei mit X-Fade; hörbarer Schnitt bei 0 %.
- Reverse mit und ohne Loop.
- Latch auf einem geloopten Sample: erste Note startet, zweite stoppt; der blaue
  Punkt erscheint und erlischt.
- Marker ziehen während des Spielens, ohne Knacken.
- Automation der Marker und von X-Fade.
- Ein Projekt und ein Kit aus Version 0.4.0 klingen unverändert.
