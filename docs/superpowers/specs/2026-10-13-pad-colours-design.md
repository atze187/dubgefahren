# Pad-Farben – Design-Spezifikation

**Datum:** 2026-10-13
**Status:** Entwurf zur Freigabe
**Issue:** atze187/dubgefahren#14 (baut auf dem UI-Restyle #15 auf)

## 1. Hintergrund und Ziel

Pads mit einer Klangquelle sollen eine Farbe bekommen, damit sich im Live-Set wenige Gruppen
(z. B. Sirenen, Riser/Drops, Vocals) auf einen Blick unterscheiden lassen. Eine Gruppe entsteht
dadurch, dass mehrere Pads dieselbe Farbe haben; es gibt keine eigenen Gruppenobjekte.

Seit dem UI-Restyle zeichnet `PadGrid` jedes Pad um eine Grundfarbe (`setPadBaseColour`); alle Pads
nutzen bisher dieselbe (`colours::padBase`, Grün). Dieses Issue ergänzt die Farbe pro Slot, ihre
Speicherung und die Auswahl.

Zusätzlich wird die Latch-Anzeige kontrastreicher: Der blaue Punkt ist schon auf dem heutigen Grün
bei ungünstigem Licht schlecht zu sehen und wäre auf einem blauen Pad unsichtbar.

**Erfolgskriterien:**

- Jeder Slot mit Klangquelle kann eine von acht Farben bekommen; die Farbe bleibt im Ableton-Set und
  im Kit erhalten.
- Pads zeigen ihre Farbe gedimmt im Ruhezustand und leuchtend mit Glow beim Spielen.
- Latch-Punkt und Pad-Schrift sind auf jeder Farbe, gedimmt wie leuchtend, gut lesbar
  (Kontrast mindestens 3:1).
- Bestehende Kits und Projekte laden unverändert (alle Pads in der Standardfarbe).

## 2. Umfang

**Enthalten:** Farbindex pro Slot in `Kit` und Plugin-State, Kit-Format Version 6, Palette mit acht
Farben, Untermenü „Colour“ im Pad-Kontextmenü, Einfärben von Pad und Sample-Wellenform, neue
Latch-Anzeige, Kontrasttests, README und Wiki.

**Nicht enthalten:** benannte Farbgruppen, freie Farbwahl, Farbe als Host-Parameter, Weitergabe der
Farben an Controller-LEDs, Farbe in der Live-Ansicht (sie zeigt keine Pads).

## 3. Entscheidungen

- **Farbe direkt pro Pad** statt benannter Gruppen oder Kopplung an die Choke-Gruppe (Option 1 des
  Brainstormings). Benannte Gruppen lassen sich später darauf aufbauen.
- **Feste Palette mit acht Farben**, damit sie im gedimmten Zustand sicher unterscheidbar bleiben.
- **Index statt RGB** in Kit und State: Die Farbtöne können später angepasst werden, ohne
  gespeicherte Daten zu ändern.
- **Kein Host-Parameter:** Die Farbe ist Metadatum wie der Slot-Name.

## 4. Palette

Die Palette liegt in `plugin/ui/DgLookAndFeel.h` (Namespace `dg::ui`). Pro Eintrag: Name (englisch,
für das Menü), Grundfarbe, Schriftfarbe auf leuchtendem Pad (sehr dunkle Variante des Farbtons).

| Index | Name | Hinweis |
|---|---|---|
| 0 | Green | heutiges `padBase`, Standard |
| 1 | Turquoise | |
| 2 | Blue | |
| 3 | Violet | |
| 4 | Pink | |
| 5 | Red | |
| 6 | Orange | deutlich vom Fokus-Gelb (`accent`) abgesetzt |
| 7 | Yellow | |

Die genauen RGB-Werte werden bei der Umsetzung festgelegt und über Render-Snapshots sowie die
Kontrasttests (Abschnitt 8) abgestimmt. `kNumPadColours = 8`; Indizes außerhalb 0 … 7 gelten als 0.

## 5. Daten und Speicherung

- **Engine:** `Kit` bekommt `std::array<int, kNumSlots> colours {}` (Standard 0). Die Engine
  verwendet den Wert nicht; er wird nur mitgetragen.
- **Plugin-State:** Ein neuer Kindknoten `SLOTCOLOURS` neben `SLOTNAMES`, Eigenschaften `c1` … `c16`
  (int). Fehlt der Knoten oder ein Eintrag (ältere Projekte), gilt 0. API im Processor:
  `int slotColour(int slot) const` und `void setSlotColour(int slot, int colour)` (klemmt auf
  0 … 7, erhöht `stateGeneration`). `currentKit()` und `applyKit()` übernehmen die Farben.
- **Kit-Datei:** Format **Version 6**; jeder Slot bekommt den Schlüssel `"colour"` (int, neben
  `"name"`). Kits der Versionen 1 bis 5 laden mit Farbe 0. Fehlt der Schlüssel, ist er kein Zahlwert
  oder liegt er außerhalb 0 … 7, gilt 0 (kein Ladefehler).

## 6. Verhalten

- **Leere Pads** werden immer im neutralen Leer-Look gezeichnet. Wird ein Pad geleert, wird seine
  Farbe auf 0 zurückgesetzt.
- **Kopieren/Einfügen** übernimmt die Farbe mit.
- **Auf Werkseinstellung zurücksetzen** setzt die Farbe auf 0.
- **Werks-Kit und leeres Kit:** alle Farben 0.
- **Sample ersetzen** lässt die Farbe unverändert.

## 7. Oberfläche

- **Kontextmenü:** neues Untermenü **„Colour“** mit acht Einträgen (Farbfeld und Name); die aktuelle
  Farbe ist abgehakt. Bei leeren Pads ist das Untermenü deaktiviert.
- **PadGrid:** Bei jeder Aktualisierung (Zustandswechsel über `stateGeneration`) setzt der Editor pro
  Pad die Grundfarbe aus der Palette über `setPadBaseColour`. Ruhezustand (Schimmer der Grundfarbe
  am unteren Rand), Leuchtverlauf und Glow nutzen die Grundfarbe wie bisher; ob die acht Farben im
  Ruhezustand gut genug unterscheidbar sind, entscheidet der Nutzer bei der Abnahme (eine stärkere
  Ruhefarbe wäre ein Folgepunkt); die Schrift auf leuchtenden Pads nutzt die Schriftfarbe des
  Paletteneintrags statt `colours::padTextLit`.
- **Latch-Anzeige:** heller Punkt (fast weiß) mit dunklem Ring (fast schwarz, etwa 1,5 px),
  Durchmesser etwa 10 px statt 8 px. Gleiches Symbol in der Legende unter den Pads.
- **Fokus-Rahmen** bleibt gelb (`accent`).
- **Wellenform:** `WaveformView` zeichnet die Wellenform eines Sample-Slots in der Grundfarbe des
  Slots statt in `colours::padBase`.

## 8. Tests

- **Kit:** Round-Trip mit Farben; Kits der Version 5 laden mit Farbe 0; fehlender, nicht numerischer
  und außerhalb liegender `"colour"`-Wert ergibt 0.
- **State:** Round-Trip der Farben; ein State ohne `SLOTCOLOURS` lädt mit 0; `setSlotColour` klemmt
  und erhöht `stateGeneration`.
- **Bedienlogik:** Kopieren/Einfügen übernimmt die Farbe; Zurücksetzen und Leeren setzen sie auf 0.
- **Kontrast:** Für jede Palettenfarbe liegt der Kontrast (WCAG-Formel) bei mindestens 3:1, und zwar
  - zwischen Latch-Punkt (der bessere Wert aus Kern und Ring) und den Flächen **gedimmt**
    (Mittelwert aus `padTop` und `padBottom`), **leuchtend oben** (`Grundfarbe.brighter(0.7)`) und
    **leuchtend unten** (Grundfarbe);
  - zwischen der Schrift auf leuchtendem Pad (Schriftfarbe des Paletteneintrags) und den Flächen
    leuchtend oben und leuchtend unten.
  Die gedimmte Schrift ist farbunabhängig und bleibt wie bisher.
- **Render-Snapshots:** alle acht Farben gedimmt und leuchtend, mit Latch-Punkt, als PNG über
  `DG_SNAPSHOT_DIR` zur Sichtprüfung.
- **Abschluss:** volle Suite, VST3-Validator, pluginval; manuelle Abnahme in Ableton durch den Nutzer.
