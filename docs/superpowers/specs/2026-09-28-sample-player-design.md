# Sample-Player als Klangquelle pro Slot – Design-Spezifikation

**Datum:** 2026-09-28
**Status:** Entwurf zur Freigabe
**Issue:** atze187/dubgefahren#8 (baut auf #7 auf, Branch gestapelt auf `feature/empty-slot`)

## 1. Ziel

Neben der Sirene kann ein Slot ein **Sample** abspielen. Samples gehören zu einem Kit: Sie
liegen im Ordner `<Kit>/` neben der Datei `<Kit>.dgkit`. Fehlt eine Sample-Datei, wird der
Slot leer (#7).

**Erfolgskriterien:**

- Im Quellen-Popup eines leeren Pads lässt sich ein Sample aus `<Kit>/` wählen oder per
  „Add File…“ hinzufügen, wobei die Datei in `<Kit>/` kopiert wird.
- WAV, AIFF, MP3 und FLAC werden abgespielt, in Gate oder One Shot, mit Tune, Attack,
  Release, Choke, Volume, Pan und FX-Send.
- Sample-Slots überstehen Host-Projekt speichern/laden sowie Kit-Export/-Import. Beim
  Export in ein neues Kit werden die Samples mitkopiert.
- Fehlende oder unlesbare Samples führen zu einem leeren Slot und einer Meldung, nie zu
  einem Absturz oder Aussetzer.
- Kein Datei-I/O, keine Allokation und keine Locks im Audio-Thread.

## 2. Umfang

**Enthalten:** alles in den Abschnitten 3–8.

**Nicht enthalten (spätere Ausbaustufe, eigenes Issue):** Sample-Start/-Ende, Loop,
Reverse. Außerdem: echtes Stereo (Stereo-Dateien werden zu Mono gemischt), Streaming von
der Platte, Factory-Samples, Aufräumen ungenutzter Dateien in `<Kit>/`.

## 3. Datenmodell und State

### 3.1 Quellentyp

- `hasSound(SourceType::Sample)` liefert jetzt `true`. Die Engine klingt für einen
  Sample-Slot nur, wenn zusätzlich Sample-Daten geladen sind (Abschnitt 4).
- Die Reihenfolge `Empty = 0, Synth = 1, Sample = 2` bleibt (Host-State).

### 3.2 Tune

- Neues `SlotField::Tune`: Schlüssel `tune`, Name „Tune“, Float −24 … +24 Halbtöne,
  Default 0, linear, Einheit `st`. Es ist automatisierbar, Parameter `sNN_tune`.
- Der Versionshinweis des neuen Parameters ist **3** (bestehende 1, Quelle 2). Dafür bekommt
  `FieldSpec` ein Feld `versionHint`.
- Die Sirene ignoriert Tune.
- Kit-Dateien älterer Versionen haben kein `tune` und bekommen daher den Default. Alte
  Host-Projekte bekommen ihn über APVTS ebenfalls (wie bei #7).

### 3.3 Sample-Referenz und Kit-Datei im Host-State

Beides liegt als Nicht-Parameter-State im APVTS-ValueTree:

- `kitFile`: Property am Wurzelknoten, absoluter Pfad der zuletzt geladenen, importierten
  oder exportierten `.dgkit`. Leer, wenn keine Kit-Datei zugeordnet ist.
- Kindknoten `SLOTSAMPLES` mit Properties `s1` … `s16`: Dateiname relativ zu `<Kit>/`,
  z. B. `horn.wav`. Leer, wenn der Slot kein Sample hat.

Regeln:

- „Load Factory Kit“ und „New Empty Kit“ leeren `kitFile` und alle Sample-Referenzen.
- Laden oder Importieren von `X.dgkit` setzt `kitFile = X.dgkit`.
- Exportieren nach `Y.dgkit` setzt `kitFile = Y.dgkit` (Abschnitt 7).
- `applyKit` bekommt die zugehörige Kit-Datei als zweites Argument
  (`applyKit(const Kit&, const juce::File& kitFile)`; leere Datei = keine Kit-Datei) und setzt
  damit `kitFile` und den Sample-Ordner in einem Schritt.
- `Kit` (engine) bekommt `std::array<std::string, kNumSlots> samples` (UTF-8, relativer
  Dateiname), damit Kopieren/Einfügen, `currentKit()` und `applyKit()` die Referenz mittragen.
- Der Sample-Ordner ist `kitFile.getParentDirectory().getChildFile(kitFile.getFileNameWithoutExtension())`.

### 3.4 Kit-Format Version 3

```json
{ "source": "sample", "name": "Horn", "sample": "horn.wav", "params": { ... } }
```

- Schreiben: immer Version 3. Ein Sample-Slot hat `source: "sample"`, `name`, `sample` und
  `params` (alle Felder inkl. `tune`). Synth- und Empty-Slots bleiben wie in Version 2.
- Lesen: Die Versionen 1 und 2 laden wie bisher. In Version 3 ist `"sample"` als `source`
  erlaubt und erfordert einen nicht leeren String `sample` ohne `/`, `\`, `:`, ohne `..` und ohne
  führende/abschließende Leerzeichen (`isValidSampleFileName`), sonst Fehler „Slot N: invalid sample file name.“ Version > 3 wird abgelehnt.
- In Version 2 bleibt `"sample"` weiterhin ein Fehler („unknown sound source“).

## 4. Engine (JUCE-frei)

### 4.1 SampleData

```cpp
struct SampleData
{
    std::vector<float> samples; // Mono
    double sampleRate = 44100.0;
};
```

Die Daten sind nach dem Erzeugen unveränderlich und werden als
`std::shared_ptr<const SampleData>` geteilt.

### 4.2 SamplePlayer : SoundSource

- `start`: Position 0, `Envelope::noteOn(attackS)`.
- `release`: `Envelope::noteOff(releaseS)`.
- `kill`: Kill-Fade der Envelope (5 ms, wie bei der Sirene).
- `render`: kubische (Catmull-Rom-)Interpolation. Die Schrittweite pro Ausgabe-Sample ist
  `data.sampleRate / hostRate × 2^((tune + perfPitch) / 12)`. Erreicht die Position das
  Ende, wird die Stimme sofort inaktiv; der Rest des Puffers ist 0.
- Ohne Daten (Nullzeiger) startet der Player nicht und bleibt inaktiv.
- Von den Performance-Offsets wirkt nur `pitchSemis`, geglättet wie bei der Sirene (20 ms).

### 4.3 Engine-Anbindung

- `EngineParams` bekommt pro Slot `const SampleData* sample` (nicht besitzend). Der
  Processor garantiert, dass die Daten leben, solange die Engine sie sehen kann
  (Abschnitt 5.3).
- Pro Slot gibt es eine `SirenVoice` und einen `SamplePlayer`. `Engine::Bank::startVoice`
  startet je nach `source` die passende Stimme und stoppt die jeweils andere hart.
  `release`, `kill`, `isVoiceActive` und `isVoiceReleasing` wirken auf die aktive Stimme.
  `renderSubSegment` rendert beide, von denen höchstens eine aktiv ist.
- Ein Slot ist spielbar (`hasSound_`), wenn `source == Synth`, oder wenn
  `source == Sample && sample != nullptr`.
- Ändert sich der `sample`-Zeiger eines Slots gegenüber dem vorigen Block, während die
  Sample-Stimme aktiv ist, wird der Slot hart gestoppt (`PadRouter::killSlot`), genau wie
  beim Übergang auf nicht spielbar. Der Player hält selbst keinen Besitz an den Daten, sondern
  liest pro Block aus `EngineParams`.

### 4.4 Trigger-Modi beim Sample

- **One Shot:** spielt bis zum Sample-Ende. `TriggerSettings` bekommt dafür
  `bool untilEnd`. Ist es gesetzt, startet `PadRouter` keinen Längen-Timer. Die Engine setzt
  `untilEnd` für Sample-Slots im Modus One Shot.
- **Gate:** Loslassen startet den Release. Am Sample-Ende ist die Stimme zu Ende.
- **Latch:** wird für Sample-Slots wie Gate behandelt. Die Engine übergibt dem Router
  `Gate` als Modus.

## 5. Laden (plugin)

### 5.1 SampleLoader

- Dekodiert mit `juce::AudioFormatManager` (`registerBasicFormats()`: WAV, AIFF, FLAC,
  unter Windows MP3 über Media Foundation) auf einem eigenen `juce::ThreadPool` mit einem
  Thread.
- Mischt alle Kanäle zu Mono (Mittelwert).
- Lehnt Dateien über 60 s ab („longer than 60 seconds“), ebenso Dateien, die sich nicht
  öffnen oder lesen lassen („file not found“ / „unsupported or damaged file“).
- Jeder Auftrag trägt Slot und eine Auftragsnummer. Das Ergebnis (Daten oder Fehlertext)
  landet in einer mit `juce::CriticalSection` geschützten Warteschlange. Diese Sperre wird
  nur auf dem Message-Thread und im Loader-Thread genommen, nie im Audio-Thread.

### 5.2 Wann geladen wird

- Beim Wählen eines Samples im Popup oder im Slot-Editor.
- Bei `applyKit` bzw. beim Laden oder Importieren eines Kits: für jeden Sample-Slot.
- Bei `setStateInformation`: für jeden Sample-Slot, relativ zum gespeicherten `kitFile`.

Solange geladen wird, ist der Slot stumm (`sample == nullptr`). Ein neuer Auftrag für
denselben Slot macht ältere Aufträge ungültig (Auftragsnummer).

### 5.3 Übernahme und Lebensdauer

- Der Processor besitzt pro Slot `std::shared_ptr<const SampleData>` und veröffentlicht den
  Rohzeiger über `std::atomic<const SampleData*>`. `ParamCache::read` bzw. `processBlock`
  liest ihn mit `acquire` in `EngineParams`.
- Ein `juce::Timer` des Processors (Message-Thread, 20 Hz) übernimmt fertige Ergebnisse,
  tauscht die Zeiger und legt ersetzte Daten in eine Garbage-Liste.
- Die Garbage-Liste wird frühestens freigegeben, wenn nach dem Tausch ein vollständiger
  `processBlock` gelaufen ist. Dazu zählt `processBlock` einen atomaren Blockzähler hoch.
  Ist kein Audio aktiv (keine `prepareToPlay` bzw. `releaseResources` gelaufen), wird sofort
  freigegeben.
- `waitForSampleLoads()` (für Tests) wartet auf alle offenen Aufträge und übernimmt die
  Ergebnisse, ohne Message-Loop.

### 5.4 Fehlerfälle

- Scheitert das Laden, gilt: Der Slot wird leer (Quelle `Empty`), die Sample-Referenz wird
  gelöscht, der Name bleibt. Der Fehler kommt als Eintrag
  `"Slot N: horn.wav – <Grund>"` in eine Problemliste des Processors, und `stateGeneration`
  steigt.
- Der Editor zeigt die Problemliste einmal als Meldung „Some samples could not be loaded“
  und leert sie danach. Ist kein Editor offen, erscheint die Meldung beim nächsten Öffnen.
- Ist `kitFile` leer, der State aber enthält Sample-Slots (nur durch manipulierte Daten
  möglich), gilt jeder dieser Slots als fehlend.

## 6. Benutzeroberfläche

### 6.1 Quellen-Popup (leeres Pad)

- „Synth“.
- „Sample ▸“ mit den Audiodateien aus `<Kit>/` (Endungen `.wav`, `.aif`, `.aiff`, `.mp3`,
  `.flac`, ohne Groß-/Kleinschreibung), alphabetisch sortiert. Ist der Ordner leer oder fehlt,
  steht dort ausgegraut „(no samples in kit folder)“. Nach einem Trenner folgt „Add File…“.
- Ohne `kitFile` ist das Untermenü ersetzt durch den ausgegrauten Eintrag
  „Sample (export the kit first)“.
- **„Add File…“:** Ein Dateidialog filtert auf die fünf Endungen. Die gewählte Datei wird
  nach `<Kit>/` kopiert; der Ordner wird bei Bedarf angelegt. Existiert dort schon eine Datei
  gleichen Namens, wird sie nicht überschrieben, sondern verwendet, wenn Inhalt und Größe
  gleich sind, sonst unter `name (2).wav` usw. abgelegt. Danach wird die Datei gewählt.
- **Wählen eines Samples** setzt `source = Sample`, die Sample-Referenz und den Namen
  (Dateiname ohne Endung, gekürzt auf 32 Zeichen). Beim Wechsel von Empty oder Synth auf
  Sample werden Tune 0, Attack 0, Release 0,05 s und Mode One Shot gesetzt. Choke, Volume,
  Pan und FX-Send bleiben. Beim Tausch eines Samples in einem Sample-Slot bleiben alle Regler.

### 6.2 Slot-Editor (Sample-Slot)

- Kopfzeile „Slot N · Name“ mit dem Rename-Button.
- Darunter die Schaltfläche mit dem Dateinamen. Sie öffnet dasselbe Sample-Menü
  (Dateiliste plus „Add File…“) zum Tauschen.
- Regler: Tune, Attack, Release, Mode, Choke, Volume, Pan, FX-Send. Im Mode-Feld ist
  „Latch“ deaktiviert.
- Die Sirenen-Regler sind ausgeblendet.

### 6.3 Pad

Ein Sample-Slot zeigt seinen Namen wie ein Synth-Slot, dazu ein kleines Wellen-Symbol
(„∿“) oben rechts, links neben dem Latch-Punkt.

### 6.4 Pad-Kontextmenü

Unverändert. „Reset to Factory Default“ macht auch aus einem Sample-Slot die
Factory-Sirene (Sample-Referenz leer). „Clear Slot“ leert auch die Sample-Referenz.

## 7. Import und Export

- **Laden/Import** von `X.dgkit`: `kitFile = X.dgkit`, dann `applyKit` mit Laden der
  Samples aus `X/`.
- **Export nach `Y.dgkit`:**
  - Ist `Y.dgkit` gleich `kitFile`, wird nur die Kit-Datei geschrieben.
  - Sonst werden alle von Sample-Slots genutzten Dateien aus `<altes Kit>/` nach `Y/`
    kopiert (Ordner bei Bedarf anlegen, gleichnamige Dateien überschreiben). Danach
    `kitFile = Y.dgkit`.
  - Schlägt eine Kopie fehl (Quelle fehlt, Ziel gesperrt), wird die Kit-Datei trotzdem
    geschrieben und der Export meldet „Some samples could not be copied“ mit der Liste.
- Nicht benutzte Dateien in Kit-Ordnern werden nie gelöscht.

## 8. Tests

**engine:**
- `SamplePlayer`:
  - Tonhöhe stimmt bei gleicher Samplerate, bei Datei 44,1 kHz / Host 48 kHz, bei Tune
    ±12 und mit Perf-Pitch (Nulldurchgänge eines Test-Sinus).
  - Die Stimme wird am Sample-Ende inaktiv.
  - Ohne Daten kein Start.
- Engine:
  - Ein Sample-Slot startet den `SamplePlayer`, ein Synth-Slot die `SirenVoice`.
  - Ein Sample-Slot ohne Daten bleibt stumm.
  - One Shot spielt bis zum Ende, auch über die frühere Längeneinstellung hinaus.
  - Gate endet beim Loslassen mit Release. Latch verhält sich wie Gate.
  - Ein Wechsel des Sample-Zeigers stoppt die klingende Stimme.
  - Choke funktioniert zwischen Sirene und Sample.
- Kit: `samples`-Array, Factory- und Empty-Kit ohne Samples.

**plugin:**
- `SampleLoader`:
  - lädt WAV, AIFF und FLAC (im Test mit JUCE erzeugt);
  - MP3-Format ist registriert (Laden nur manuell prüfbar);
  - Stereo wird zu Mono gemischt;
  - über 60 s, fehlende Datei und kaputte Datei liefern Fehler.
- Processor:
  - Sample wählen, laden, spielen (processBlock liefert Signal).
  - Fehlschlag macht den Slot leer, behält den Namen und füllt die Problemliste.
  - State-Roundtrip mit `kitFile` und Sample-Referenzen.
  - `applyKit(factory, {})` leert `kitFile` und alle Sample-Referenzen.
  - Garbage wird erst nach einem `processBlock` freigegeben.
- KitFile: Version-3-Roundtrip, Versionen 1/2 laden, ungültige Dateinamen werden abgelehnt,
  Version 4 wird abgelehnt.
- Export kopiert Samples in den neuen Ordner und meldet fehlende Quellen.
- Add File: Kopie in `<Kit>/`, Namenskonflikt ergibt `name (2).wav`.
- Editor: Popup ohne `kitFile` ausgegraut, mit `kitFile` Dateiliste; der Slot-Editor zeigt
  die Sample-Regler; die Problemliste wird als Meldung angezeigt.

**Manuell (Ableton):** echte Samples inkl. MP3, Set speichern und neu laden, Kit-Ordner
verschieben (Samples fehlen, Meldung), Export in neues Kit und Import auf einem zweiten Pfad.
