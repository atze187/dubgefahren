# Vintage-Stretch für Samples (S950-Charakter) – Design-Spezifikation

**Datum:** 2026-10-12
**Status:** Umgesetzt, Abnahme in Ableton offen
**Issue:** atze187/dubgefahren#17 (baut auf #11 auf)

## 1. Hintergrund und Ziel

Ein Sample-Slot soll in der Zeit gedehnt oder gestaucht werden können, ohne die Tonhöhe zu
ändern, mit dem Klang des klassischen zyklischen Stretch der Akai S950/S1000: Das Sample wird in
kurze Stücke geschnitten, die wiederholt oder übersprungen und mit kurzen Crossfades verbunden
werden. Die metallischen, leicht robotischen Nähte gehören zum Klang; ein sauberer moderner
Algorithmus ist ausdrücklich nicht das Ziel.

Der typische Einsatz ist die Tempoanpassung eines Breaks (z. B. 130 auf 170 BPM, Faktor etwa
0,76), also Faktoren zwischen 50 % und 200 %. Darüber hinaus dehnt der Stretch kurze Samples
bis auf das Achtfache zu langen Flächen und Drones (bis 800 %).

**Erfolgskriterien:**

- Ein Stretch-Faktor ändert die Dauer eines Samples, nicht seine Tonhöhe.
- Tune und Performance-Pitch ändern bei aktivem Stretch nur die Tonhöhe, nicht die Dauer.
- Der Charakter lässt sich über die Stückgröße (Grain) einstellen.
- Ein Slot mit Stretch **Aus** rendert bitgleich wie bisher; alte Kits und Projekte klingen
  unverändert.
- Der Stretch wirkt wahlweise auf den ganzen Bereich oder nur auf die Loop-Region.

## 2. Umfang

**Enthalten:** drei neue Slot-Parameter, die Klasse `GrainStretch`, Anbindung im `SamplePlayer`,
Bedienelemente im Slot-Editor, Kit-Format Version 5, Tests, README.

**Nicht enthalten:** Tempo-Sync mit dem Host (Originaltempo und Ziel-BPM), Faktoren über 800 %,
Tonhöhenkorrektur nach klassischen Algorithmen (WSOLA, Phase-Vocoder), Zeichnung der Stückgrenzen
in der Wellenform, Stretch für Synth-Slots, ein Stretch-Regler in der Live-Ansicht.

## 3. Entscheidungen

- **Echtzeit im Player** statt einmal gerenderter Puffer: Ein gerenderter Puffer würde mit Tune
  wieder seine Dauer ändern; die unabhängige Dauer wäre nicht erreichbar.
- **Unabhängige Dauer:** Bei aktivem Stretch bestimmt allein der Faktor die Dauer.
- **Ein Parameter für Ein/Aus und Ziel** (Aus, Alles, Loop), damit der Schalter keinen
  unstetigen Sprung bei Faktor 100 % erzeugt. Aus bleibt der Ausgangszustand.
- **Eigener Grain-Regler**, weil die Stückgröße den Charakter bestimmt.
- **Faktor ohne Host-Sync** (Option 1 der Brainstorming-Frage); ein BPM-Feld wäre ein späterer
  Ausbau.

## 4. Parameter

Drei neue Slot-Felder über die Feld-Tabelle (`SlotField`/`FieldSpec`) mit dem
`ParameterID`-Versionshinweis **5**, am Ende der Reihenfolge angefügt (die Reihenfolge ist Teil des
Host-States). Sie sind automatisierbar und stehen im Kit. Sie gelten nur für Sample-Slots.

| Feld (`SlotField`) | Schlüssel | Art | Bereich | Standard | Einheit |
|---|---|---|---|---|---|
| `StretchMode` | `stretch` | Choice | Aus, Alles, Loop | Aus | – |
| `StretchFactor` | `stretchPct` | Float | 50 … 800, Mitte 100 | 100 | % |
| `StretchGrain` | `grainMs` | Float | 5 … 100, Mitte 20 | 20 | ms |

In `SlotParams`: `stretchMode` (Enum `StretchMode { Off, All, Loop }`), `stretchPct`, `grainMs`
(float). Das sind 3 × 16 = 48 neue Host-Parameter.

Der Faktor ist die Dauer relativ zum Original: 200 % heißt doppelt so lang, 800 % achtmal so lang,
50 % halb so lang.

## 5. Verhalten

- **Aus:** Der `SamplePlayer` läuft exakt wie bisher; `GrainStretch` wird nicht berührt.
- **Alles:** Der gesamte Bereich Start bis Ende läuft gestretcht.
- **Loop:** Von Start bis zum Marker L läuft das Sample normal, ab L läuft die Loop-Region
  gestretcht. Der Modus wirkt nur, wenn Loop an ist; ohne Loop verhält sich der Slot wie bei Aus.
  Die Bedienelemente zeigen einen Hinweis.
- **Reverse:** Die Quellposition wandert rückwärts, die Stücke werden ebenfalls rückwärts gelesen
  (wie die bestehende Reverse-Richtung).
- **Tune und Performance-Pitch:** wirken nur auf die Lesegeschwindigkeit der Stücke, also nur auf
  die Tonhöhe.
- **Loop-X-Fade:** Bei Stretch (Alles oder Loop) mit eingeschaltetem Loop gilt der Regler nicht; die
  Stück-Crossfades übernehmen seine Rolle.
- **Trigger-Modi, Choke, Hüllkurve:** unverändert; ein Latch auf einem geloopten, gestretchten Slot
  funktioniert wie bei einem geloopten Slot ohne Stretch.

## 6. Engine

### 6.1 Klasse `GrainStretch`

Neue, eigenständige Klasse in `engine/GrainStretch.h/.cpp`. Eingänge: die Sample-Daten, ein
Leseschritt (Tonhöhe), die Parameter (Faktor, Grain), die Richtung und das Segment, in dem die
Quellposition kreist (Bereich oder Loop-Segment). Ausgang: ein Wert pro Ausgabesample. Sie ist
ohne `SamplePlayer` testbar und alloziert nichts im Audio-Thread.

Ablauf pro Ausgabesample:

- Eine **Quellposition** läuft mit `Quellrate / Faktor` durch das Material; sie bestimmt, wo ein
  neues Stück beginnt. Der Faktor wird wie der Performance-Pitch mit einem Einpol-Filter geglättet.
- Alle `Grain` Millisekunden startet ein neues Stück an der aktuellen Quellposition. Das
  vorherige Stück läuft für ein Viertel der Stückdauer weiter; beide werden linear übergeblendet.
- Jedes Stück liest mit der Tonhöhe aus Tune und Performance-Pitch (Catmull-Rom wie bisher).
- Bei Faktor über 100 % wird Material wiederholt, darunter wird es übersprungen.
- Eine Änderung von Grain gilt ab dem nächsten Stück, eine Änderung des Faktors wirkt geglättet
  sofort.

### 6.2 Anbindung im `SamplePlayer`

- Bei Stretch Aus bleibt der bestehende Pfad unverändert.
- Modus Alles mit Loop: Die Quellposition kreist im Loop-Segment, die Stücke lesen mit Modulo darin.
- Modus Loop: Der Player läuft normal bis zur Position L; dort übernimmt `GrainStretch` mit
  Quellposition `seg.lo`, das erste Stück blendet ein.
- Bereichsende ohne Loop: Die Ausblendung am Ende läuft über die Restdauer der Quellposition (in
  Ausgabesamples), wie bisher.
- Retrigger und Abschalten des Stretch während des Spielens: Die alten Stücke laufen in einer zweiten
  `GrainStretch`-Instanz mit dem bestehenden 5-ms-Fade weiter aus, damit das Signal stetig bleibt.
- Ein Sprung der Position am Schleifenende (Marker unter der Stimme verschoben) erzeugt bei Stretch
  keinen zweiten Kopf, weil die Stücke unabhängig von der Quellposition weiterlesen.
- Ohne Loop halten die Stücke an den Bereichsgrenzen ihren letzten Wert, bis die Ausblendung am Ende
  greift.
- Mit jedem neuen Stück laufen alle älteren innerhalb der neuen Überblendzeit aus; es leben höchstens
  zwei Stücke, und die Gewichte bleiben auch bei wechselnder Stückgröße stetig.
- Kosten: zwei zusätzliche Interpolationen pro Sample und Slot (vier kurz beim Retrigger), keine
  Speicherzuteilung.

## 7. Kit und Plugin-Zustand

- Kit-Format **Version 5**; die drei Schlüssel kommen dazu. Kits der Versionen 1 bis 4 laden mit
  den Standardwerten (Aus, 100 %, 20 ms).
- Alte Host-Projekte ohne die neuen Parameter laden mit den Standardwerten; der bestehende Test für
  fehlende Parameter deckt das Verfahren ab und wird um die neuen Felder erweitert.
- Beim Wechsel oder Zurücksetzen des Samples werden die Stretch-Felder mit den Bereichsfeldern
  zurückgesetzt. Die Funktion `resetSampleRegionFields` heißt dann `resetSampleFields`.

## 8. Oberfläche

Im Slot-Editor des Sample-Slots kommt neben Loop, Reverse und X-Fade eine Gruppe **Stretch** hinzu:
ein Auswahlfeld (Aus, Alles, Loop) und die Regler Faktor und Grain.

- Faktor und Grain sind deaktiviert, solange der Modus Aus ist.
- Im Modus Loop sind sie nur bedienbar, wenn Loop an ist; sonst steht der Hinweis „needs Loop“ dabei.
- Der X-Fade-Regler ist bei Stretch (Alles oder Loop) mit eingeschaltetem Loop deaktiviert.
- Anordnung im Sample-Modus: Reihe `SAMPLE/STRETCH` mit Tune, Stretch, Factor, Grain und Hinweis,
  Reihe `LOOP/TRIG` (Loop, Reverse, X-Fade, Mode, Choke), Reihe `AMP/MIX` (Attack, Release, Volume,
  Pan, FX-Send).
- Die Live-Ansicht bleibt unverändert.

## 9. Betroffene Dateien

`engine/GrainStretch.{h,cpp}` (neu), `engine/SamplePlayer.{h,cpp}`, `engine/SlotParams.h`,
`engine/SlotFields.{h,cpp}`, `plugin/KitFile.cpp`, `plugin/ui/SlotEditor.{h,cpp}`,
`README.md`, `docs/superpowers/` (Spec, Plan), Tests unter `tests/`.

## 10. Tests

- **Engine, Lesepositionen:** Rampendaten (`samples[i] = i * 1e-4`), damit der Ausgabewert die
  Leseposition verrät.
- **Engine, Dauer und Tonhöhe:** Sinusdaten bei Faktor 0,5, 1 und 2; die Tonhöhe bleibt, die
  Gesamtdauer skaliert mit dem Faktor.
- **Engine, Aus:** Mit Modus Aus rendert der Player bitgleich wie ohne die neuen Felder.
- **Engine, Randfälle:** Loop mit Reverse, Modus Loop ohne Loop, Retrigger mitten im Stretch, Wechsel
  von Grain und Faktor beim Spielen, Sample kürzer als ein Stück, Stretch zusammen mit Tune und
  Performance-Pitch.
- **Plugin:** Die Felder erscheinen als Host-Parameter; ein alter State setzt sie auf die
  Standardwerte; Kit speichern und laden (Version 5) sowie Laden von Version 4.

## 11. Abnahme

Per Gehör in Ableton: Charakter bei Grain 10, 20 und 60 ms, Tempoanpassung eines Breaks von 130 auf
170 BPM, Zusammenspiel mit Loop und Reverse. Faktor- und Grain-Bereich lassen sich anhand des Gehörs
nachstellen.
