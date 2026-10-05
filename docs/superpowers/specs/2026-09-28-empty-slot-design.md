# Leerer Slot mit Klangquellen-Auswahl – Design-Spezifikation

**Datum:** 2026-09-28
**Status:** Umgesetzt, in Ableton abgenommen (2026-09-30)
**Issue:** atze187/dubgefahren#7 (Voraussetzung für #8, Sample-Player)

## 1. Ziel

Ein Slot kann **leer** sein, also keine Klangquelle haben. Ein leerer Slot bleibt stumm.
Ein Klick auf ein leeres Pad öffnet eine Auswahl der Klangquelle („Synth“, „Sample“).
Später (#8) fällt ein Slot, dessen Audiodatei fehlt, in genau diesen Zustand zurück.

**Erfolgskriterien:**

- Slots lassen sich einzeln („Clear Slot“) oder alle auf einmal („New Empty Kit“) leeren.
- Ein leerer Slot erzeugt bei MIDI-Note und Pad-Klick keinen Ton, löst keine
  Choke-Gruppe aus und wird nicht gelatcht.
- Ein Klick auf ein leeres Pad öffnet das Auswahl-Popup; „Synth“ lädt die
  Factory-Sirene dieses Slots, „Sample“ ist ausgegraut.
- Leere Slots überstehen das Speichern/Laden des Host-Projekts sowie Kit-Export/-Import.
- Bestehende Projekte und Kit-Dateien (Version 1) laden unverändert als „alles Synth“.

## 2. Umfang

**Enthalten:** Datenmodell, Parameter, Engine-Verhalten, Kit-Format Version 2, UI (Pad,
Pad-Kontextmenü, Kit-Menü, Auswahl-Popup, Slot-Editor-Hinweis).

**Nicht enthalten:** Sample-Wiedergabe und alles zu Sample-Dateien (#8). „Sample“ ist
im Datenmodell bereits vorhanden, in der UI aber nicht wählbar.

## 3. Datenmodell (engine)

Neu in `engine/SlotParams.h`:

```cpp
enum class SourceType { Empty, Synth, Sample };
```

`SlotParams` erhält das Feld `SourceType source = SourceType::Synth;`.
Es ist **kein** `SlotField`: Es erscheint weder in der Feld-Tabelle noch im
`params`-Objekt der Kit-Datei, sondern wird überall gesondert behandelt (Parameter,
Kit-JSON, Engine). Weil es Teil von `SlotParams` ist, tragen Kopieren/Einfügen,
`setSlot` und `currentKit()` den Quellentyp automatisch mit.

Die Synth-Parameter eines leeren Slots bleiben erhalten (unsichtbar). Beim Wechsel
auf „Synth“ werden sie mit der Factory-Sirene des Slots überschrieben.

`makeFactoryKit()` bleibt unverändert (16 × Synth). Neu: `makeEmptyKit()` – alle 16 Slots
`Empty`, Namen leer, Synth-Parameter = Factory-Werte.

## 4. Parameter und Host-State (plugin)

- Pro Slot ein neuer `AudioParameterChoice` mit der ID `sNN_source`, dem Namen
  „SNN Source“ und den Einträgen „Empty“, „Synth“, „Sample“ (Default „Synth“).
  Er ist **nicht automatisierbar** (`withAutomatable(false)`).
- Der `ParameterID`-Versionshinweis der neuen Parameter ist **2**, alle bestehenden
  behalten 1. `kParameterVersion` wird dazu in zwei Konstanten aufgeteilt
  (bestehende Parameter / seit Version 2).
- Alte Host-Projekte kennen `sNN_source` nicht. APVTS legt fehlende Parameter bei
  `replaceState()` neu an und setzt sie dabei auf ihren Default „Synth“ – auch in einer
  Instanz, die vorher leere Slots hatte (`valueTreeChildAdded` → `setNewState`). Eine eigene
  Migration ist nicht nötig; ein Test sichert das Verhalten ab. `kStateVersion` bleibt 1.
- `readSlotFromParameters`, `writeSlotToParameters` und `ParamCache::read` lesen bzw.
  schreiben den Quellentyp mit.

## 5. Engine-Verhalten

- `Engine` leitet für jeden Slot ab, ob er spielbar ist (`source == Synth`). `Sample`
  wird bis #8 wie `Empty` behandelt.
- **NoteOn/PreviewOn** für einen nicht spielbaren Slot werden verworfen, bevor sie den
  `PadRouter` erreichen. Dadurch gibt es keinen Stimmenstart, keinen Choke und kein
  Latch. NoteOff/PreviewOff laufen weiter normal durch (harmlos).
- **Übergang auf nicht spielbar:** Zu Beginn jedes `process()` vergleicht die Engine
  den Quellentyp mit dem Stand des vorigen Blocks. Wurde ein Slot nicht spielbar, wird
  seine Stimme sofort gestoppt und der Router-Zustand des Slots (Latch, gehalten,
  One-Shot) zurückgesetzt. Dazu wird eine öffentliche Methode am `PadRouter` ergänzt,
  die intern `clearSlot` plus `killVoice` nutzt.

## 6. Kit-Format Version 2

```json
{ "format": "dubgefahren-kit", "version": 2,
  "slots": [
    { "source": "synth", "name": "Classic", "params": { ... } },
    { "source": "empty", "name": "" }
  ] }
```

- Schreiben: immer Version 2. Jeder Slot hat `source`. Bei `synth` gibt es `name`
  und `params` wie bisher, bei `empty` nur `name` (leer), ohne `params`.
- Lesen, Version 1: `source` fehlt, also `synth`. Die Validierung bleibt wie bisher.
- Lesen, Version 2: `source` muss `"synth"` oder `"empty"` sein, sonst Fehler
  „Slot N: unknown sound source“. Bei `empty` werden `params` ignoriert, falls
  vorhanden. Die Synth-Parameter werden dann auf die Factory-Werte des Slots gesetzt.
  `name` ist bei `empty` optional.
- `"sample"` wird erst mit #8 akzeptiert.
- Version > 2: Fehler wie bisher („newer version“).

## 7. Benutzeroberfläche

**Pad (`PadGrid::Pad`):**

- Ein leeres Pad zeigt statt des Namens „Empty“ in `colours::textDim`. Die Slot-Nummer
  bleibt, der Rahmen wird gedimmt.
- Ein Linksklick auf ein leeres Pad löst **keine Vorschau** aus. Er wählt den Slot
  (der Slot-Editor zeigt den Hinweis) und öffnet das Auswahl-Popup.
- Das PadGrid bekommt dafür einen neuen Callback `onEmptyClick(int slot)`. Den
  Quellentyp liest es über den Processor aus (neue Methode `slotSource(int)`).

**Auswahl-Popup (`DubgefahrenEditor::showSourceMenu`):**

- `juce::PopupMenu` mit denselben Optionen und demselben LookAndFeel wie das Kit-Menü,
  als Ziel-Komponente das angeklickte Pad.
- Einträge: „Synth“ (aktiv) und „Sample“ (ausgegraut). „Synth“ ruft
  `setSlot(slot, factory.slots[slot], factoryName)` auf.

**Pad-Kontextmenü:**

- Neuer Eintrag „Clear Slot“ (nur bei nicht leerem Slot aktiv). Er setzt
  `source = Empty` und den Namen auf leer, die Synth-Parameter bleiben unverändert.
- „Rename“ ist bei leerem Slot deaktiviert. „Copy“, „Paste“ und „Reset to Factory
  Default“ bleiben aktiv; Reset ergibt die Factory-Sirene (Synth).

**Kit-Menü:** neuer Eintrag „New Empty Kit“ direkt unter „Load Factory Kit“. Er ruft
`applyKit(makeEmptyKit())` auf.

**Slot-Editor:** Bei leerem Slot werden alle Regler und der Rename-Button ausgeblendet.
Stattdessen erscheint zentriert der Hinweis „Empty slot – click the pad to choose a
sound source“. Die Kopfzeile zeigt „Slot N“.

**Aktualisierung:** Quellenwechsel laufen über `setSlot`/`applyKit` und erhöhen
`stateGeneration`. Der bestehende Editor-Timer frischt damit Pads und Slot-Editor auf,
auch nach einem Host-Restore.

## 8. Tests

- **engine:**
  - NoteOn und Preview auf einem leeren Slot erzeugen Stille, `activeMask` bleibt 0.
  - Ein leerer Slot in einer Choke-Gruppe beendet klingende Nachbarn nicht.
  - Wird ein spielender bzw. gelatchter Slot geleert, ist die Stimme im nächsten Block
    aus und das Latch-Bit gelöscht.
  - `Sample` verhält sich wie `Empty`.
  - `makeEmptyKit()`: alle Slots `Empty`.
- **KitFile:**
  - Roundtrip mit gemischten Slots.
  - Datei der Version 1 lädt als alles Synth.
  - Unbekannter `source` ergibt einen Fehler.
  - `empty` ohne `params` ist gültig.
  - Version 3 wird abgelehnt (der bestehende Test wird von 2 auf 3 angepasst).
- **PluginProcessor:**
  - State-Roundtrip mit leeren Slots.
  - Ein State ohne `sNN_source`-Eintrag (altes Projekt) ergibt Synth, auch wenn die
    Instanz vorher leere Slots hatte.
  - `setSlot` bzw. `applyKit` setzen den Quellentyp.
- **Editor:** leerer Slot zeigt den Hinweis und blendet die Regler aus; das Kit-Menü
  und das Pad-Kontextmenü enthalten die neuen Einträge, soweit sich das über die
  bestehenden Editor-Tests prüfen lässt.
- **Manuell:** VST3-Validator, Laden eines alten Projekts in Ableton, Popup-Look.
