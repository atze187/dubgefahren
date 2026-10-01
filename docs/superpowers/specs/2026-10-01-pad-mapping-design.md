# Pad-Belegung: einstellbare Pad-1-Note und Raster-Ursprung – Design-Spezifikation

**Datum:** 2026-10-01
**Status:** Entwurf, Review offen
**Issue:** noch keines angelegt

## 1. Ziel

Das Plugin geht davon aus, dass Note 36 das erste Pad auslöst und Pad 1 im Raster unten links
liegt. Das Intech Grid BU16 sendet seine Tasten aber zeilenweise von oben links ab Note 32. Mit
diesem Controller bleibt die oberste Reihe stumm, die Pads 13 bis 16 sind nicht erreichbar, und die
Anordnung auf dem Bildschirm spiegelt die Tasten nicht. Die Belegung wird einstellbar, damit das
Plugin zum Controller passt.

Gemessen am Controller (Position = Taste, Zahl = getriggertes Pad):

| Reihe | Tasten von links nach rechts |
|---|---|
| oben | keine Reaktion (Noten 32–35) |
| zweite | Pad 1, 2, 3, 4 (Noten 36–39) |
| dritte | Pad 5, 6, 7, 8 |
| unten | Pad 9, 10, 11, 12 |

**Erfolgskriterien:**

- Mit der Einstellung „Pad 1 = Note 32“ und „Pad 1 oben links“ löst jede der 16 Tasten des BU16 das Pad
  an derselben Stelle auf dem Bildschirm aus.
- Mit den Standardwerten (Note 36, unten links) verhält sich das Plugin exakt wie bisher.
- Die Einstellung gilt für alle Projekte und alle gleichzeitig offenen Instanzen.
- Eine fehlende oder kaputte Einstellungsdatei führt zu den Standardwerten, nie zu einem Fehler.

## 2. Umfang

**Enthalten:** zwei Einstellungen, ihre Speicherung, die Übersetzung der Noten im Plugin, das
Raster in beiden Ursprüngen, ein Menü „MIDI“ in der Kopfzeile, README.

**Nicht enthalten:** MIDI-Learn (Taste drücken, um Pad 1 festzulegen), Zuordnung einzelner Noten
auf beliebige Pads, ein Kanalfilter, eine Anzeige der Notennummer auf den Pads, Spiegelung nach
rechts oder eine Spalten-Reihen-Vertauschung.

## 3. Einstellungen

| Einstellung | Bereich | Standard |
|---|---|---|
| Note für Pad 1 | 0 bis 112 (damit Pad 16 höchstens Note 127 ist) | 36 |
| Pad 1 oben links | an / aus | aus (Pad 1 unten links) |

Mit Pad 1 unten links laufen die Pads 1 bis 4 in der untersten Reihe von links nach rechts, die
nächsten vier darüber usw. (wie bisher). Mit Pad 1 oben links laufen sie in der obersten Reihe von
links nach rechts, die nächsten vier darunter usw.; die Spalten bleiben gleich, nur die Reihen
werden vertikal gespiegelt. Das entspricht der Reihenfolge, in der das BU16 seine Noten sendet.

## 4. Speicherung

- Datei `settings.json` in `%APPDATA%\Dubgefahren\` (juce `userApplicationDataDirectory`), Inhalt:
  `{ "firstNote": 32, "padOrigin": "topLeft" }`. Erlaubte Werte für `padOrigin`: `"bottomLeft"` und
  `"topLeft"`.
- Beim ersten Zugriff in einem Prozess wird die Datei gelesen. Fehlt sie, ist kein Objekt darin,
  fehlt ein Schlüssel oder liegt ein Wert außerhalb des Bereichs, gilt für diesen Wert der
  Standard; die übrigen Werte bleiben erhalten.
- Alle Instanzen im Prozess teilen sich ein Objekt (`PadMapping`, Singleton). Die Werte liegen als
  atomare Ganzzahlen darin; der Audio-Thread liest sie ohne Sperre.
- Eine Änderung wird sofort im Objekt gesetzt und danach in die Datei geschrieben (Message-Thread).
  Schlägt das Schreiben fehl, bleibt der neue Wert für die laufende Sitzung gültig, und das MIDI-Menü
  zeigt keine Fehlermeldung; der Fehler wird nur protokolliert (`DBG`).
- Das Projekt (Host-Zustand) und die Kits enthalten die Einstellung nicht.

## 5. Übersetzung der Noten

- In `DubgefahrenProcessor::processBlock`, vor `engine_.process`: Für jede Note-An-/Note-Aus-Nachricht
  gilt `slot = note - firstNote`. Liegt `slot` in 0 bis 15, geht ein Ereignis mit der Note
  `kFirstNote + slot` an die Engine; sonst wird die Nachricht verworfen.
- Die Engine bleibt unverändert: Sie kennt weiter nur Note 36 bis 51. Mit den Standardwerten ist die
  Übersetzung die Identität.
- Die Übersetzung steht in einer eigenen, JUCE-freien Funktion
  `int translateNote(int note, int firstNote)` (liefert die Engine-Note oder −1), damit sie
  ohne Plugin testbar ist.
- Ändert sich die Einstellung, während eine Taste gedrückt ist, passt das spätere Note-Aus unter
  Umständen nicht mehr zum Note-An. Ein Gate-Pad kann dann bis zum nächsten Druck weiterklingen;
  der Panic-Button beendet es. Das wird nicht gesondert behandelt.

## 6. Oberfläche

- Ein Button **MIDI** in der Kopfzeile (neben den vorhandenen Buttons) öffnet ein Menü im Look des
  Plugins (`styled`-Menü wie die anderen Menüs) mit zwei Einträgen:
  - **Pad 1 note: N …** öffnet einen kleinen Dialog (im Look des Plugins, wie `createDialog`) mit
    einem Zahlenfeld, „OK“ und „Cancel“. Gültig sind ganze Zahlen von 0 bis 112; „OK“ ist bei
    einer ungültigen Eingabe nicht auswählbar.
  - **Pad 1 at top left** ist ein Häkchen-Eintrag.
- Nach einer Änderung legt `PadGrid` das Raster neu an; ein offener Editor wird sofort
  aktualisiert, auch wenn die Änderung in einer anderen Instanz stattfand (der Editor fragt die
  Einstellung beim nächsten Timer-Schritt ab, 20 Hz wie bisher).
- Die Beschriftung der Pads (Slot-Nummer) bleibt unverändert; die Nummer folgt dem Slot, nicht der
  Position.

## 7. Tests

- **Übersetzung:** `translateNote` mit Basis 36 ist die Identität für 36 bis 51 und −1 sonst; mit Basis
  32 ergeben 32 bis 47 die Engine-Noten 36 bis 51, 31 und 48 ergeben −1; mit Basis 112 ergibt 127 die
  Engine-Note 51 und 128 ergibt −1.
- **Plugin:** Mit Basis 32 löst Note 32 den Slot 1 aus und Note 47 den Slot 16; Note 36 löst Slot 5
  aus. Mit der Basis 36 bleibt das Verhalten wie bisher (Note 36 → Slot 1, Note 35 nichts).
- **Raster:** Mit Ursprung unten links liegt Pad 1 in der untersten Reihe und Pad 16 oben rechts; mit
  Ursprung oben links liegt Pad 1 oben links und Pad 16 unten rechts; die Spaltenreihenfolge ist in
  beiden gleich.
- **Einstellungen:** Laden von gültigem Inhalt; fehlende Datei; kein JSON; JSON ohne Objekt;
  fehlender Schlüssel (anderer Wert bleibt erhalten); Wert außerhalb des Bereichs; unbekannter
  `padOrigin`-Wert; Speichern und erneutes Laden ergibt dieselben Werte.
- **Abnahme in Ableton:** Mit dem BU16 und den Einstellungen 32 / oben links löst jede Taste das Pad
  an der Stelle aus, an der sie auf dem Controller sitzt (README-Checkliste).

## 8. Dokumentation

- README: Die Zeile „playable via MIDI notes 36–51“ nennt die einstellbare Pad-1-Note (Standard 36);
  die Checkliste bekommt den Hinweis auf die Einstellung „32 / top left“ für das BU16.

## 9. Risiken

- Die Datei liegt im Benutzerprofil. Auf Systemen ohne Schreibrecht (selten) bleibt die Einstellung
  nur in der Sitzung gültig.
- Wer die Einstellung ändert, hört ein anderes Pad auf eine Taste als in einem früheren Projekt
  vorgesehen; das ist beabsichtigt, weil die Einstellung den Controller beschreibt, nicht das
  Projekt.
