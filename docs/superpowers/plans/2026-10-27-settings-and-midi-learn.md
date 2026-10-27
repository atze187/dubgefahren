# Settings-Dialog, MIDI Learn und Pad-Profile – Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Jedes der 16 Pads bekommt per MIDI Learn eine eigene Note; Belegungen sind als Dateien austauschbare Profile; ein Settings-Dialog (Reiter MIDI und Kits) ersetzt den MIDI-Button und macht den Kit-Ordner im Plugin einstellbar.

**Architecture:** Die Umrechnung „erste Note + Offset“ (`translateNote`) wird durch `dg::NoteMap` ersetzt, eine lock-free lesbare Tabelle Note → Pad in der Engine-Schicht. `PadMapping` (prozessweit) hält die aktive `NoteMap`, den Ursprung und das aktive Profil; Profile liegen als JSON-Dateien im Ordner `Profiles` neben `settings.json`, das neue `UserSettings` verwaltet `profile` und `kitFolder`. Der Audio-Thread merkt sich je Note das ausgelöste Pad (`heldSlot_`), damit ein Note-Off nach einem Profilwechsel das richtige Pad freigibt, und reicht beim Lernen die erste Note-On lock-free an den Message-Thread weiter.

**Tech Stack:** C++20, JUCE 8.0.15, Catch2, CMake-Presets (`build.ps1` unter Windows).

**Spec:** `docs/superpowers/specs/2026-10-27-settings-and-midi-learn-design.md`

## Global Constraints

- Windows und macOS; kein Windows-spezifischer Code, Pfade über `juce::File::userApplicationDataDirectory`.
- Texte im Plugin sind Englisch, Code-Kommentare Deutsch (wie im Bestand), Doku (README/Wiki) Englisch.
- Keine Allokation und keine Sperren im Audio-Thread; die Roh-Byte-Auswertung der MIDI-Events (keine `juce::MidiMessage`) bleibt.
- Test-Builds (`DG_NO_USER_SETTINGS`) lesen und schreiben nie `%APPDATA%\Dubgefahren`; alle neuen Dateizugriffe sind mit einem Testordner prüfbar.
- Profile: eine Datei `<Name>.json` im Ordner `Profiles` neben `settings.json` (`%APPDATA%\Dubgefahren\Profiles`, macOS `~/Library/Application Support/Dubgefahren/Profiles`); Noten 0 bis 127 oder unbelegt; jede Note gehört höchstens einem Pad; `Default` ist nicht löschbar.
- Optik im Stil von `DgLookAndFeel` (Hintergrund `#1b1f24`, Akzent `#f2b134`, Pad-Grün `#4cd07d`).
- Jede Zuweisung wird sofort gespeichert (kein „Apply“). Kein fortlaufendes Weiterspringen beim Lernen. Keine Aktion „Fill from note…“, keine Import-/Export-Buttons.
- Release-Build `build.ps1 test -Config Release` bleibt am Ende jedes Tasks grün (aktuell 403 Tests).

## Review Focus

Eingaben und Zustände, die die Spezifikation nahelegt und die ein Nutzer am ehesten trifft (jede Zeile hat einen Test im genannten Task):

1. Eine von Dritten gelieferte Profildatei mit zu wenigen/zu vielen Noten, doppelten Noten, Noten außerhalb 0 bis 127, `null`/Text statt Zahl oder kaputtem JSON stürzt nichts ab und liefert eine nutzbare Belegung bzw. erscheint nicht in der Liste (Task 2).
2. Ein Note-Off, nachdem sich die Belegung geändert hat (Profilwechsel, Pad neu gelernt, Note neu zugeordnet), gibt das Pad frei, das das Note-On ausgelöst hat; es bleibt keine Stimme hängen (Task 4).
3. Zwei Instanzen im Host: Das Lernen fängt die Note nur in der Instanz des offenen Dialogs, ein Profilwechsel ist im anderen Editor nach dem nächsten Poll sichtbar (Task 5, 9).
4. `settings.json` ist kaputt oder nicht beschreibbar, oder der gespeicherte Kit-Ordner existiert nicht mehr: das Plugin fällt auf Bundle-Config bzw. Standardordner zurück, zeigt eine Warnung und stürzt nicht ab (Task 3, 6).
5. Profilnamen: Groß-/Kleinschreibung gilt als gleich („default“ ≙ „Default“), unzulässige Zeichen und Leerstring werden abgelehnt, das aktive Profil löschen oder umbenennen wählt bzw. behält `Default`, `Default` lässt sich weder löschen noch umbenennen (Task 2, 4, 8).

---

### Task 1: Branch und `NoteMap` (Engine)

**Files:**
- Create: `engine/NoteMap.h`, `engine/NoteMap.cpp`, `tests/test_NoteMap.cpp`
- Modify: `engine/CMakeLists.txt` (NoteMap.h/.cpp in `dg_engine`), `tests/CMakeLists.txt` (`test_NoteMap.cpp` in `DubgefahrenTests`)

**Interfaces:**
- Produces:
  ```cpp
  namespace dg {
  class NoteMap {                                   // engine/NoteMap.h, nur <array>/<atomic>/<cstdint>, kein JUCE
  public:
      NoteMap();                                    // alle Noten unbelegt
      int slotForNote(int note) const;              // 0..kNumSlots-1 oder -1; note außerhalb 0..127 → -1; relaxed atomic read, audio-thread-sicher
      int noteForSlot(int slot) const;              // 0..127 oder -1
      void assign(int slot, int note);              // note -1 gibt das Pad frei; sonst verliert der bisherige Besitzer der Note sie, die alte Note des Pads wird frei
      void setAll(const std::array<int, kNumSlots>& notes); // in Padreihenfolge via assign; ungültige Werte (-1, <0, >127) = unbelegt, bei Duplikaten behält das frühere Pad die Note
      std::array<int, kNumSlots> notes() const;
  };
  }
  ```

- [ ] **Step 1: Branch anlegen, Spec und Plan committen.** `git checkout -b feature/settings-midi-learn`, dann `git add docs/superpowers/specs/2026-10-27-settings-and-midi-learn-design.md docs/superpowers/plans/2026-10-27-settings-and-midi-learn.md` und committen (`docs: spec and plan for settings dialog, MIDI learn and pad profiles`).
- [ ] **Step 2: Failing tests in `tests/test_NoteMap.cpp`** (Tags `[notemap]`):
  - `"a new note map has no assignments"`: `slotForNote(n) == -1` für n = 0, 36, 127; `noteForSlot(s) == -1` für alle Slots.
  - `"assign links note and pad in both directions"`: `assign(2, 40)` → `slotForNote(40) == 2`, `noteForSlot(2) == 40`.
  - `"reassigning a pad frees its previous note"`: `assign(2, 40); assign(2, 41)` → `slotForNote(40) == -1`, `slotForNote(41) == 2`.
  - `"a note taken by another pad leaves its previous owner"`: `assign(0, 40); assign(5, 40)` → `slotForNote(40) == 5`, `noteForSlot(0) == -1`.
  - `"assign with -1 frees the pad"`: nach `assign(3, 50); assign(3, -1)` → `slotForNote(50) == -1`, `noteForSlot(3) == -1`.
  - `"out-of-range notes and slots are ignored"`: `slotForNote(-1) == -1`, `slotForNote(128) == -1`, `assign(16, 40)` und `assign(-1, 40)` ändern nichts (`notes()` unverändert), `assign(0, 128)` ändert nichts.
  - `"setAll applies a full profile and keeps the first pad on duplicates"`: `setAll({36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,36})` → `noteForSlot(15) == -1`, `slotForNote(36) == 0`; `notes()` gibt die 16 Werte zurück.
- [ ] **Step 3: Run `build.ps1 test -Config Release`; the new test file fails to compile (NoteMap missing).**
- [ ] **Step 4: Implement `NoteMap`** with `std::array<std::atomic<std::int8_t>, 128> slotOfNote_` and `std::array<std::atomic<std::int8_t>, kNumSlots> noteOfSlot_` (`kNumSlots` aus `engine/SlotParams.h`); Schreiben nur im Message-Thread (nicht synchronisiert untereinander), Lesen mit `memory_order_relaxed`.
- [ ] **Step 5: Run `build.ps1 test -Config Release`; all tests pass.**
- [ ] **Step 6: Commit** (`feat(engine): NoteMap, a lock-free note-to-pad table`).

---

### Task 2: `PadProfile` und `ProfileStore`

**Files:**
- Create: `plugin/PadProfile.h`, `plugin/PadProfile.cpp`, `tests/plugin/test_PadProfile.cpp`
- Modify: `plugin/CMakeLists.txt` (beide Quelldateien in `dg_plugin_shared`), `tests/CMakeLists.txt` (`plugin/test_PadProfile.cpp` in `DubgefahrenPluginTests`)

**Interfaces:**
- Consumes: `enum class PadOrigin { BottomLeft, TopLeft }` aus `plugin/PadMapping.h` (bleibt dort), `kNumSlots`, `kFirstNote`.
- Produces:
  ```cpp
  namespace dg {
  struct PadProfile {
      juce::String name;
      std::array<int, kNumSlots> notes;   // -1 = unbelegt
      PadOrigin origin = PadOrigin::BottomLeft;
      bool operator==(const PadProfile&) const = default;
  };
  PadProfile makeConsecutiveProfile(const juce::String& name, int firstNote, PadOrigin origin); // notes firstNote..firstNote+15, auf 0..127 begrenzt (Bereich überschritten → -1)
  juce::String padProfileToJson(const PadProfile& profile);                       // {"padOrigin":"bottomLeft"|"topLeft","notes":[36,37,…,null]} (Name steht nicht in der Datei)
  std::optional<PadProfile> parsePadProfile(const juce::String& name, const juce::String& json);
  // nullopt: kein JSON, kein Objekt, "notes" fehlt oder ist kein Array.
  // Sonst: notes aufgefüllt/gekürzt auf 16; jeder Eintrag, der keine ganze Zahl 0..127 ist (null, Text, Bool, Bruch, außerhalb) = -1; bei Duplikaten behält das frühere Pad die Note; padOrigin unbekannt/fehlend = BottomLeft.
  bool isValidProfileName(const juce::String& name); // getrimmt nicht leer, höchstens 64 Zeichen, juce::File::createLegalFileName(name) == name, nicht "." oder ".."

  class ProfileStore {
  public:
      explicit ProfileStore(juce::File folder);
      juce::File folder() const;
      juce::StringArray names() const;                         // Dateien *.json mit gültigem Inhalt, "Default" zuerst, Rest alphabetisch ohne Beachtung der Schreibweise
      bool contains(const juce::String& name) const;           // ohne Beachtung der Schreibweise
      std::optional<PadProfile> load(const juce::String& name) const;
      bool save(const PadProfile& profile);                    // legt den Ordner an; false bei ungültigem Namen oder Schreibfehler
      bool remove(const juce::String& name);
      bool rename(const juce::String& from, const juce::String& to); // false, wenn to ungültig oder (ohne Beachtung der Schreibweise) schon vorhanden und nicht dieselbe Datei
  };
  }
  ```

- [ ] **Step 1: Failing tests in `tests/plugin/test_PadProfile.cpp`** (Tag `[padprofile]`, Testordner per `juce::File::createTempFile` und am Ende gelöscht):
  - `"consecutive profile maps firstNote upwards and clips at 127"`: `makeConsecutiveProfile("x", 36, BottomLeft).notes[15] == 51`; mit `firstNote 120`: `notes[7] == 127`, `notes[8] == -1`.
  - `"a profile round-trips through JSON"`: `parsePadProfile("A", padProfileToJson(p)) == p` für ein Profil mit TopLeft, einer Lücke (`-1`) und Noten 0 und 127; der JSON-Text enthält `"topLeft"` und `null`.
  - `"third-party files are repaired, not rejected"` (Review Focus 1): Tabelle aus JSON-Texten → erwartete `notes`: 3 Einträge `[1,2,3]` → `{1,2,3,-1,…}`; 20 Einträge → die ersten 16; `[36,36,38]` → `{36,-1,38,-1,…}`; `[128,-1,"a",null,true,1.5,36]` → `{-1,-1,-1,-1,-1,-1,36,-1,…}`; fehlendes/unbekanntes `padOrigin` → BottomLeft.
  - `"unusable files yield nullopt"`: `""`, `"not json"`, `"[1,2]"`, `"{}"`, `{"notes":5}` → `std::nullopt`.
  - `"profile names"`: gültig: `"Launchpad Mini"`, `"Default"`, `"BU16 (alt)"`; ungültig: `""`, `"  "`, `"a/b"`, `"a\\b"`, `"a:b"`, `"."`, `".."`, 65 Zeichen.
  - `"the store saves, lists and loads profiles"`: nach `save` zweier Profile "Zeta" und "alpha" und "Default" ist `names() == {"Default","alpha","Zeta"}`; `load("alpha")` gibt es zurück; `contains("ALPHA")` ist true; eine Datei `broken.json` mit Text `"{"` und eine Datei `readme.txt` im Ordner erscheinen nicht in `names()`.
  - `"rename and remove"`: `rename("alpha","beta")` → `names()` enthält "beta" statt "alpha"; `rename("beta","ZETA")` false (vorhanden, andere Datei); `rename("beta","Beta")` true (nur Schreibweise); `rename("beta","a/b")` false; `remove("beta")` true, danach `load("beta") == nullopt`; `remove("nope")` false.
  - `"the store creates a missing folder on save and tolerates a missing folder on read"`: `names()` auf nicht vorhandenem Ordner → leer; nach `save` existiert der Ordner.
- [ ] **Step 2: Run the build; tests fail to compile.**
- [ ] **Step 3: Implement** die Funktionen in `plugin/PadProfile.cpp`; JSON über `juce::JSON`/`juce::DynamicObject` wie `plugin/PadMapping.cpp`; Dateiname = `<Name>.json`.
- [ ] **Step 4: Run `build.ps1 test -Config Release`; all pass.**
- [ ] **Step 5: Commit** (`feat(plugin): pad profiles and the profile store`).

---

### Task 3: `UserSettings` (`settings.json`)

**Files:**
- Create: `plugin/UserSettings.h`, `plugin/UserSettings.cpp`, `tests/plugin/test_UserSettings.cpp`
- Modify: `plugin/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `PadMappingSettings` und `parsePadMappingSettings(const juce::String&)` aus `plugin/PadMapping.h` (bleiben als Leser der Altwerte `firstNote`/`padOrigin`).
- Produces:
  ```cpp
  namespace dg {
  class UserSettings {
  public:
      explicit UserSettings(juce::File file);       // leere Datei (juce::File()) = nur im Speicher, nichts auf der Platte
      static UserSettings& instance();              // Datei = userSettingsFile()
      juce::String profile() const;                 // "" wenn nicht gesetzt
      void setProfile(const juce::String& name);
      juce::String kitFolder() const;               // "" wenn nicht gesetzt
      void setKitFolder(const juce::String& path);  // "" entfernt den Schlüssel
      std::optional<PadMappingSettings> legacyPad() const; // nur wenn firstNote oder padOrigin in der Datei stehen
  };
  juce::File userSettingsFile();   // %APPDATA%\Dubgefahren\settings.json; in Test-Builds (DG_NO_USER_SETTINGS) juce::File()
  }
  ```
  `padMappingFile()` in `PadMapping.h` entfällt (Task 4 entfernt es); `userSettingsFile()` ersetzt es.

- [ ] **Step 1: Failing tests** (Tag `[usersettings]`, Temp-Datei):
  - `"missing file gives empty values"`: `profile() == ""`, `kitFolder() == ""`, `legacyPad() == nullopt`.
  - `"values round-trip through the file and keep the folder creation"`: Datei in `nested/settings.json`; `setProfile("Launchpad")`, `setKitFolder("C:/k")` → eine neue `UserSettings`-Instanz auf derselben Datei liefert beide Werte; der Ordner `nested` wurde angelegt.
  - `"saving keeps no legacy keys but reading still sees them"`: Datei mit `{"firstNote":32,"padOrigin":"topLeft"}` → `legacyPad() == PadMappingSettings{32, TopLeft}`; nach `setProfile("x")` enthält der Dateitext weder `firstNote` noch `padOrigin`, `legacyPad()` ist danach `nullopt`.
  - `"a broken file is treated as empty and does not crash"` (Review Focus 4): Dateiinhalt `"{ kaputt"`, `"[1]"`, leer → alle Werte leer; ein anschließendes `setProfile("a")` überschreibt die Datei mit gültigem JSON.
  - `"an unwritable path does not crash"`: `UserSettings` auf Pfad, dessen Elternordner eine Datei ist → `setProfile("a")` wirft nicht, `profile() == "a"` (im Speicher).
  - `"in test builds the default file is empty"`: `userSettingsFile() == juce::File()`.
- [ ] **Step 2: Build fails to compile.**
- [ ] **Step 3: Implement** mit `juce::JSON` wie `PadMapping.cpp`; Werte im Speicher halten, bei jedem Setter speichern, Schreibfehler nur per `DBG` melden.
- [ ] **Step 4: `build.ps1 test -Config Release` grün.**
- [ ] **Step 5: Commit** (`feat(plugin): UserSettings for profile and kit folder`).

---

### Task 4: `PadMapping` auf Profile umstellen und Audio-Pfad mit `NoteMap`

**Files:**
- Modify: `plugin/PadMapping.h`, `plugin/PadMapping.cpp`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `engine/PadRouter.h`, `engine/PadRouter.cpp`, `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`, `tests/plugin/PadMappingTestHelpers.h`, `tests/plugin/test_PadMapping.cpp`, `tests/plugin/test_PluginProcessor.cpp`, `tests/plugin/test_Editor.cpp`, `tests/test_PadRouter.cpp`

**Interfaces:**
- Consumes: `NoteMap` (Task 1), `PadProfile`/`ProfileStore`/`makeConsecutiveProfile` (Task 2), `UserSettings` (Task 3).
- Produces:
  ```cpp
  class PadMapping {                              // plugin/PadMapping.h, prozessweit, Setter nur im Message-Thread
  public:
      PadMapping(juce::File profilesFolder, UserSettings& settings);   // Testnaht
      static PadMapping& instance();              // echter Ordner (padProfilesFolder()), UserSettings::instance()
      const NoteMap& noteMap() const noexcept;
      PadOrigin origin() const;
      PadProfile profile() const;                 // Schnappschuss des aktiven Profils
      juce::String activeProfile() const;
      int revision() const;                       // +1 bei jeder Änderung (Auswahl, Note, Ursprung, Anlegen, Umbenennen, Löschen)
      juce::StringArray profileNames() const;     // liest den Ordner neu ein
      juce::File profilesFolder() const;
      bool selectProfile(const juce::String& name);       // false, wenn nicht ladbar; speichert die Auswahl in UserSettings
      juce::String createProfile();               // legt "New profile" (bei Bedarf "New profile 2", …) wie "Default" belegt an, wählt es, liefert den Namen
      bool renameActiveProfile(const juce::String& newName); // false bei ungültigem/vorhandenem Namen oder aktivem "Default"
      bool deleteActiveProfile();                 // false bei "Default"; wählt danach "Default"
      void setPadNote(int slot, int note);        // -1 = unbelegt; NoteMap::assign semantics; speichert das aktive Profil
      void setOrigin(PadOrigin origin);           // speichert das aktive Profil
  };
  juce::File padProfilesFolder();                 // <settings.json-Ordner>/Profiles; in Test-Builds ein pro Prozess einmaliger Temp-Ordner, der beim Beenden gelöscht wird
  ```
  Entfällt: `kDefaultPadFirstNote`, `firstNote()`, `setFirstNote`, `parsePadFirstNote`, `padMappingFile`, `loadPadMappingSettings`, `savePadMappingSettings`, `padMappingSettingsToJson`, `translateNote` (samt Tests). Bleibt: `PadOrigin`, `PadMappingSettings`, `parsePadMappingSettings` (nur als Leser der Altwerte), `kMaxPadFirstNote`.
  `dgtest::ScopedPadMapping` (Testhelper) merkt sich aktives Profil, Noten und Ursprung der Singleton-Instanz und stellt sie wieder her; Profile, die im Scope neu entstanden sind, löscht er.

- [ ] **Step 1: Failing tests, `tests/plugin/test_PadMapping.cpp` ersetzen** (Tag `[padmapping]`, jeder Test mit eigenem Temp-Ordner und eigenem `UserSettings(juce::File())`):
  - `"first start creates Default from the legacy settings"`: Ordner existiert nicht, Altwerte `{32, TopLeft}` → `profileNames() == {"Default"}`, `profile().notes[0] == 32`, `notes[15] == 47`, `origin() == TopLeft`, `activeProfile() == "Default"`, `noteMap().slotForNote(32) == 0`.
  - `"first start without legacy settings gives 36 and bottom left"`: `notes[0] == 36`, `notes[15] == 51`, `BottomLeft`.
  - `"an existing profile folder ignores legacy settings"`: Ordner mit einer Datei `Default.json` (Noten ab 50) und Altwerte 32 → `notes[0] == 50`.
  - `"the saved selection is restored and falls back to Default"`: `UserSettings.setProfile("A")` mit vorhandener Datei A → `activeProfile() == "A"`; mit nicht vorhandener Datei → `"Default"`.
  - `"a missing or broken Default file is recreated with the standard assignment"`: `Default.json` gelöscht bzw. mit `"{"` überschrieben → nach Konstruktion `profile().notes[0] == 36`.
  - `"setPadNote persists, frees the previous owner and updates the NoteMap"`: `setPadNote(1, 60)` → `noteMap().slotForNote(60) == 1`; eine neue `PadMapping`-Instanz auf demselben Ordner liefert dasselbe; `setPadNote(2, 60)` → Pad 1 `noteForSlot(1) == -1`; `setPadNote(2, -1)` → frei; `revision()` steigt bei jeder Änderung.
  - `"setOrigin persists in the profile"`.
  - `"create, select, rename and delete"` (Review Focus 5): `createProfile()` zweimal → `"New profile"`, `"New profile 2"`, aktiv ist das zuletzt angelegte; `renameActiveProfile("Launchpad")` true, Datei umbenannt, Auswahl in UserSettings folgt; `renameActiveProfile("default")` false; `renameActiveProfile("a/b")` false; `selectProfile("Default")` und `renameActiveProfile("x")` false; `deleteActiveProfile()` auf "Default" false; `selectProfile("Launchpad")`, `deleteActiveProfile()` true → aktiv `"Default"`, Datei weg; `selectProfile("nope")` false und aktiv bleibt.
  - `"in test builds the singleton uses a temporary profile folder"`: `PadMapping::instance().profilesFolder()` liegt unter `juce::File::getSpecialLocation(tempDirectory)`.
- [ ] **Step 2: Failing processor tests, `tests/plugin/test_PluginProcessor.cpp`** (die drei Tests auf `setFirstNote` und der Test zu Note-Off ersetzen; Helfer `prepare`, `noteMessage`, `processBlocks` wie dort):
  - `"each pad is triggered by its own note"`: `setPadNote(0, 40)`, `setPadNote(1, 50)` → Note 40 setzt Bit 0, Note 50 setzt Bit 1, Note 36 (vorher Pad 1, jetzt unbelegt) löst nichts aus.
  - `"with the default profile nothing changes"`: Noten 35 und 52 nichts, 36 → Pad 1, 51 → Pad 16.
  - `"a change of the assignment takes effect for the next block"`: wie der bisherige Test, mit `setPadNote`.
  - `"note off releases the pad the note on triggered, even after the assignment changed"` (Review Focus 2): Note 40 auf Pad 1 → Note-On; danach `setPadNote(0,-1)`, `setPadNote(4, 40)`; Note-Off 40 → nach 60 Blöcken ist Bit 0 frei und Bit 4 nie gesetzt.
  - `"note off after switching the profile releases the original pad"`: gleiche Idee mit `selectProfile`.
  - `"a note off without a note on is ignored"`: Note-Off 36 ohne vorheriges Note-On ändert `activeMask()` nicht (0).
- [ ] **Step 3: Run the build; fails to compile / tests fail.**
- [ ] **Step 4: Implement.** `PadMapping` hält `NoteMap noteMap_`, `std::atomic<int> origin_`, `PadProfile active_` (Message-Thread), `ProfileStore store_`, `std::atomic<int> revision_`; Konstruktor legt wie in Task-4-Tests beschrieben an. `PluginProcessor`: `std::array<std::int8_t, 128> heldSlot_` (alle -1; in `prepareToPlay` und `releaseResources` zurücksetzen); Note-On: `slot = noteMap.slotForNote(note)`, bei `-1` überspringen, sonst `heldSlot_[note] = slot` und Event `NoteOn` mit `kFirstNote + slot`; Note-Off (auch Note-On mit Velocity 0): `slot = heldSlot_[note]`, bei `-1` überspringen, sonst zurücksetzen und `NoteOff` mit `kFirstNote + slot`. Aus Editor und Editor-Tests entfallen der Punkt „Pad 1 note“ im MIDI-Menü (`kMidiFirstNote`), `showFirstNoteDialog`, `applyFirstNoteText` und ihre Tests; der Punkt „Pad 1 at top left“ bleibt bis Task 7 und ruft `PadMapping::setOrigin`; der Test `"the MIDI menu shows …"` prüft nur noch diesen einen Punkt.
- [ ] **Step 5: Run `build.ps1 test -Config Release`; all pass.**
- [ ] **Step 6: Commit** (`feat(plugin): pad profiles drive the note-to-pad mapping`).

---

### Task 5: Lern-Erfassung im Processor

**Files:**
- Modify: `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `tests/plugin/test_PluginProcessor.cpp`

**Interfaces:**
- Consumes: `heldSlot_`-Pfad aus Task 4.
- Produces (Message-Thread-API des Processors):
  ```cpp
  void armMidiLearn();                    // die nächste Note-On wird erfasst und nicht gespielt
  void disarmMidiLearn();                 // verwirft Bereitschaft und eine noch nicht abgeholte Note
  bool midiLearnArmed() const;
  std::optional<int> takeLearnedNote();   // holt die erfasste Note (0..127) ab; danach nullopt
  ```
  Audio-Thread: `std::atomic<bool> learnArmed_`, `std::atomic<int> learnedNote_` (-1 = keine); erste Note-On bei `learnArmed_.exchange(false)` → `learnedNote_` setzen, Event nicht erzeugen, `heldSlot_` nicht setzen.

- [ ] **Step 1: Failing tests** (Tag `[plugin][learn]`):
  - `"an armed processor captures the next note on and does not play it"`: mit Default-Profil `armMidiLearn()`, Note-On 40 (Pad 5 hat Note 40) → `activeMask() == 0`, `takeLearnedNote() == 40`, zweites `takeLearnedNote() == nullopt`, `midiLearnArmed()` false.
  - `"the note off of a captured note is ignored"`: nach der Erfassung Note-Off 40 → `activeMask() == 0`, danach löst Note-On 40 Pad 5 normal aus (Bit 4) und Note-Off gibt es frei.
  - `"only the first note on is captured"`: zwei Note-Ons 40 und 41 im selben Block → `takeLearnedNote() == 40`, Note 41 löst sein Pad aus.
  - `"note offs and other MIDI do not satisfy learning"`: Note-Off, Controller, SysEx (20 Bytes) bei armed → `midiLearnArmed()` bleibt true, `takeLearnedNote() == nullopt`.
  - `"disarm discards the pending capture"`: `armMidiLearn()`, Note-On 40, `disarmMidiLearn()` → `takeLearnedNote() == nullopt`.
  - `"learning is per instance"` (Review Focus 3): Processor A armed, B nicht; Note-On 40 an beide → A: nichts gespielt und erfasst, B: Pad 5 gespielt, `B.takeLearnedNote() == nullopt`.
  - `"notes beyond the mapped range can be learned"`: Note-On 5 und 127 werden erfasst (auch wenn ihnen kein Pad gehört).
- [ ] **Step 2: Build fails.**
- [ ] **Step 3: Implement** wie in den Interfaces; kein `new`, keine Sperre im Audio-Thread.
- [ ] **Step 4: `build.ps1 test -Config Release` grün.**
- [ ] **Step 5: Commit** (`feat(plugin): capture the next note on for MIDI learn`).

---

### Task 6: Kit-Ordner aus den Benutzereinstellungen

**Files:**
- Modify: `plugin/Config.h`, `plugin/Config.cpp`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `tests/plugin/test_Config.cpp`, `tests/plugin/test_PluginProcessor.cpp`

**Interfaces:**
- Consumes: `UserSettings::instance()` (Task 3).
- Produces:
  ```cpp
  // plugin/Config.h – ersetzt die alte Signatur
  KitFolderInfo resolveKitFolder(const juce::String& userKitFolder, const juce::File& configFile, const juce::File& defaultFolder);
  // Reihenfolge: userKitFolder (nach expandEnvironmentVariables; absolut und vorhanden) → Bundle-Config (bisherige Logik) → defaultFolder.
  // Ein gesetzter, aber unbrauchbarer userKitFolder erzeugt die Warnung "The kit folder in your settings does not exist: <Pfad>" (bzw. "… is not an absolute path: <Pfad>") und fällt auf die nächste Stufe zurück; Warnungen der Config-Stufe bleiben unverändert und haben Vorrang nur, wenn userKitFolder leer ist.

  // DubgefahrenProcessor (Message-Thread)
  void setKitFolder(const juce::File& folder);   // speichert in UserSettings, löst neu auf, aktualisiert kitFolder_
  void useDefaultKitFolder();                    // entfernt die Einstellung, löst neu auf
  ```

- [ ] **Step 1: Failing tests.** In `test_Config.cpp` alle bestehenden Aufrufe auf die neue Signatur mit leerem `userKitFolder` umstellen; neu (Tag `[config]`):
  - `"the user setting wins over the bundle config"`: beide Ordner vorhanden → `info.folder == userFolder`, `warning` leer.
  - `"an empty user setting uses the bundle config, then the default"`.
  - `"an unusable user setting warns and falls back"` (Review Focus 4): `userKitFolder = <nicht vorhanden>` mit gültiger Config → `info.folder == configFolder`, `warning.contains("settings")`; relativer Pfad → Warnung "not an absolute path"; ohne Config → Standardordner.
  - `"environment variables are expanded in the user setting"` (z. B. `%DG_TEST_DIR%`, vorhandene Hilfsfunktion `setTestEnv`).
  In `test_PluginProcessor.cpp`: `"setKitFolder changes the kit folder and persists it in the user settings"` (`kitFolder().folder == neuer Ordner`, `UserSettings::instance().kitFolder()` gleich dem Pfad, Aufräumen mit `useDefaultKitFolder()`), `"useDefaultKitFolder returns to the default folder"` (`folder == defaultKitFolder()`), `"setKitFolder with a missing folder keeps the previous one and sets a warning"`.
- [ ] **Step 2: Build fails.**
- [ ] **Step 3: Implement.** Der Konstruktor ruft `resolveKitFolder(UserSettings::instance().kitFolder(), pluginConfigFile(), defaultKitFolder())`. `Dubgefahren.config.json` (Bundle) und README bleiben unverändert als Rückfall.
- [ ] **Step 4: `build.ps1 test -Config Release` grün.**
- [ ] **Step 5: Commit** (`feat(plugin): kit folder from the user settings with bundle config as fallback`).

---

### Task 7: Settings-Dialog, Settings-Button, Reiter Kits, Ursprung

**Files:**
- Create: `plugin/ui/SettingsDialog.h`, `plugin/ui/SettingsDialog.cpp`, `tests/plugin/test_SettingsDialog.cpp`
- Modify: `plugin/CMakeLists.txt`, `tests/CMakeLists.txt`, `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`, `tests/plugin/test_Editor.cpp`

**Interfaces:**
- Consumes: `DubgefahrenProcessor::kitFolder()/setKitFolder/useDefaultKitFolder` (Task 6), `PadMapping::instance().origin()/setOrigin` (Task 4).
- Produces:
  ```cpp
  namespace dg::ui {
  class SettingsDialog final : public juce::Component, private juce::Timer {
  public:
      struct Host {                                              // vom Editor bereitgestellt (nutzt createDialog für den Look)
          std::function<void(const juce::String& title, const juce::String& initial, std::function<void(const juce::String&)> onOk)> promptText;
          std::function<void(const juce::String& title, const juce::String& text, std::function<void()> onYes)> confirm;
          std::function<void()> close;
      };
      enum class Tab { Midi, Kits };
      SettingsDialog(DubgefahrenProcessor& proc, Host host);
      ~SettingsDialog() override;                                 // ruft stopLearning() (ab Task 9)
      void showTab(Tab tab);
      Tab currentTab() const;
      // Kits
      juce::String kitFolderText() const;                         // voller Pfad des aktuellen Kit-Ordners
      juce::String kitWarningText() const;                        // leer, wenn keine Warnung
      void applyKitFolder(const juce::File& folder);              // von "Change…" nach der Auswahl aufgerufen
      void useDefaultKitFolderClicked();
      void openKitFolderClicked();                                // folder.startAsProcess()
      // MIDI
      bool originTopLeft() const;
      void setOriginTopLeft(bool topLeft);                        // Schalter "Pad 1 at top left"
  };
  }
  // DubgefahrenEditor
  void showSettings();                       // öffnet den Dialog modal (juce::DialogWindow mit enterModalState, im Editor gehalten); ist er schon offen, passiert nichts
  ui::SettingsDialog* settingsDialog() const; // nullptr, wenn geschlossen
  ```
  Der Editor ersetzt `midiButton_` ("MIDI ▾") durch `settingsButton_` ("Settings") an derselben Stelle (nur im Edit-Modus sichtbar, wie bisher) und entfernt `buildMidiMenu`, `applyMidiMenuResult`, `showMidiMenu`, `MidiMenuId`. Das Fenster wird im Destruktor vor `lnf_` geschlossen (modalen Zustand verlassen, dann löschen). Esc und Fensterkreuz schließen den Dialog. Die Aufforderungen aus `Host::promptText`/`confirm` sind weitere modale `AlertWindow`s über dem Dialog.

- [ ] **Step 1: Failing tests** (Tags `[settings][editor]`, `juce::ScopedJuceInitialiser_GUI`):
  - `"the settings button replaces the MIDI button and opens the dialog modally"`: Editor erzeugen, `settingsDialog() == nullptr`; `showSettings()` → nicht null, `currentTab() == Tab::Midi`, das Fenster ist im modalen Zustand (`juce::Component::isCurrentlyModal()`); zweiter Aufruf erzeugt keinen zweiten Dialog; im Live-Modus ist der Settings-Button wie der alte MIDI-Button unsichtbar.
  - `"the kits tab shows the folder and the warning"`: `showTab(Kits)`; `kitFolderText() == proc.kitFolder().folder.getFullPathName()`; mit unbrauchbarer Einstellung zeigt `kitWarningText()` die Warnung.
  - `"changing the kit folder applies immediately"`: `applyKitFolder(tempDir)` → `proc.kitFolder().folder == tempDir`, `kitFolderText()` aktualisiert; `useDefaultKitFolderClicked()` → `defaultKitFolder()`; Aufräumen.
  - `"the origin switch moves the pad grid"`: `setOriginTopLeft(true)` → `PadMapping::instance().origin() == TopLeft`, `e->padPosition(0).y < e->padPosition(4).y`; zurück auf false.
  - `"closing the dialog and destroying the editor with an open dialog is safe"`.
  - Der alte Menü-Test in `test_Editor.cpp` entfällt.
- [ ] **Step 2: Build fails.**
- [ ] **Step 3: Implement** `SettingsDialog` mit zwei Reitern (JUCE `TabbedComponent` oder zwei Schaltflächen mit eigenem Zeichnen im `DgLookAndFeel`-Stil, Entscheidung des Bauenden), Kits-Reiter mit Pfad-Label, Warnzeile, Buttons "Change…" (asynchroner `juce::FileChooser`, Rückruf ruft `applyKitFolder`), "Open folder", "Use default"; MIDI-Reiter vorerst nur mit dem Schalter "Pad 1 at top left" und "Close". `Host` im Editor aus `createDialog` ableiten (Vorbild `renameSlot`).
- [ ] **Step 4: `build.ps1 test -Config Release` grün; Dialog zusätzlich als Snapshot mit `dgtest::savePng` ansehen (`DG_SNAPSHOT_DIR`).**
- [ ] **Step 5: Commit** (`feat(ui): settings dialog with kits tab replaces the MIDI menu`).

---

### Task 8: Reiter MIDI – Profilverwaltung

**Files:**
- Modify: `plugin/ui/SettingsDialog.h`, `plugin/ui/SettingsDialog.cpp`, `tests/plugin/test_SettingsDialog.cpp`

**Interfaces:**
- Consumes: `PadMapping` (Task 4), `Host::promptText/confirm` (Task 7).
- Produces (zusätzlich zu Task 7):
  ```cpp
  juce::StringArray profileNames() const;          // Inhalt der Auswahlliste (liest den Ordner beim Öffnen und bei jedem Aufruf neu)
  juce::String selectedProfile() const;
  void selectProfile(const juce::String& name);
  void newProfileClicked();                        // PadMapping::createProfile()
  void renameProfileClicked();                     // promptText mit dem aktuellen Namen; Ergebnis → PadMapping::renameActiveProfile, bei false zeigt ein Hinweis-Label im Dialog "Name not allowed or already taken" (kein weiterer Dialog)
  void deleteProfileClicked();                     // confirm "Delete profile <name>?" → deleteActiveProfile
  void openProfileFolderClicked();                 // PadMapping::profilesFolder().startAsProcess()
  bool renameEnabled() const; bool deleteEnabled() const; // beide false bei "Default"
  ```
- [ ] **Step 1: Failing tests:**
  - `"the profile list shows the profiles and the active one"`: Default + zwei Profile → `profileNames()` entspricht `PadMapping::profileNames()`, `selectedProfile() == activeProfile()`.
  - `"a profile file copied into the folder shows up when the list is read again"`: Datei `Friend.json` in `PadMapping::profilesFolder()` schreiben → `profileNames()` enthält "Friend"; `selectProfile("Friend")` wählt es, `PadMapping::noteMap()` folgt den Noten der Datei.
  - `"new, rename and delete update the list and the selection"`: `newProfileClicked()` → neuer Name aktiv und in der Liste; `renameProfileClicked()` mit einem Host, dessen `promptText` sofort "Launchpad" liefert, → Liste enthält "Launchpad"; mit "default" → Name unverändert und Hinweistext gesetzt; `deleteProfileClicked()` mit Host, dessen `confirm` sofort bestätigt, → "Default" aktiv; mit Host, der nicht bestätigt, bleibt alles.
  - `"Default cannot be renamed or deleted"`: `renameEnabled()` und `deleteEnabled()` false, wenn "Default" aktiv ist.
  - Aufräumen über `dgtest::ScopedPadMapping`.
- [ ] **Step 2: Build fails / tests fail.**
- [ ] **Step 3: Implement** Auswahlfeld (`juce::ComboBox`) mit Buttons "New", "Rename", "Delete", "Open profile folder" oberhalb des Schalters aus Task 7; Liste beim Zeigen des Reiters und nach jeder Aktion neu einlesen.
- [ ] **Step 4: `build.ps1 test -Config Release` grün; Snapshot ansehen.**
- [ ] **Step 5: Commit** (`feat(ui): profile selector with new, rename, delete and open folder`).

---

### Task 9: Reiter MIDI – Pad-Raster und MIDI Learn

**Files:**
- Modify: `plugin/ui/SettingsDialog.h`, `plugin/ui/SettingsDialog.cpp`, `plugin/PluginEditor.cpp`, `tests/plugin/test_SettingsDialog.cpp`

**Interfaces:**
- Consumes: `armMidiLearn/disarmMidiLearn/takeLearnedNote` (Task 5), `PadMapping::setPadNote/revision/origin` (Task 4).
- Produces:
  ```cpp
  void startLearning(int slot);             // Klick auf ein Pad im Raster; ein anderes lernendes Pad wird dabei abgebrochen; ein Klick auf das lernende Pad bricht ab
  void stopLearning();                      // disarmMidiLearn(); wird auch bei Reiterwechsel, Schließen und Zerstörung aufgerufen
  int learningSlot() const;                 // -1 = keines
  juce::String padCellText(int slot) const; // "<Padnummer>\n<Note>" bzw. "<Padnummer>\n-" ; lernend: "<Padnummer>\npress…"
  juce::Point<int> padCellPosition(int slot) const;  // folgt dem Ursprung (Pad 1 oben/unten links)
  void pollLearning();                      // vom 30-Hz-Timer aufgerufen, für Tests öffentlich: holt eine erfasste Note ab, ruft PadMapping::setPadNote und beendet das Lernen
  bool keyPressed(const juce::KeyPress&) override; // Esc beendet nur das Lernen (true); sonst false, damit der Dialog schließt
  ```
  Das Raster zeichnet sich neu, wenn sich `PadMapping::revision()` ändert (auch durch einen anderen Editor).
- [ ] **Step 1: Failing tests:**
  - `"a learning pad shows press and takes the next note"`: `startLearning(3)` → `learningSlot() == 3`, `proc.midiLearnArmed()`, `padCellText(3)` enthält "press"; Note-On 55 über `proc.processBlock`, dann `pollLearning()` → `PadMapping::noteMap().noteForSlot(3) == 55`, `learningSlot() == -1`, `padCellText(3)` enthält "55", `!proc.midiLearnArmed()`.
  - `"learning does not advance to the next pad"`: nach der Zuweisung bleibt `learningSlot() == -1` und `proc.midiLearnArmed()` false.
  - `"a note that another pad owns moves to the learning pad"`: Pad 1 hat Note 36; `startLearning(5)`, Note-On 36, `pollLearning()` → `noteForSlot(5) == 36`, `padCellText(0)` endet mit "-".
  - `"clicking the learning pad again, another pad, Esc, a tab switch, closing and destruction stop or switch learning"`: je ein Check; Esc über `keyPressed(KeyPress::escapeKey)` liefert true, wenn gelernt wurde, und false, wenn nicht; nach Reiterwechsel/Schließen/Zerstörung `!proc.midiLearnArmed()`.
  - `"the grid follows the origin"`: `padCellPosition(0).y > padCellPosition(4).y` bei BottomLeft, umgekehrt bei TopLeft (Schalter aus Task 7).
  - `"the dialog shows changes made elsewhere"` (Review Focus 3): zweiter Editor ändert per `PadMapping::instance().setPadNote(7, 70)`; nach `pollLearning()`-unabhängigem Refresh des ersten Dialogs (Timer-Methode `refreshFromMapping()` öffentlich für Tests) zeigt `padCellText(7)` "70".
  - `"the Note-Off of a learned note does not leave a voice stuck"`: nach dem Lernen von Note 55 für Pad 4 sendet der Test Note-Off 55 → `activeMask() == 0`.
- [ ] **Step 2: Build fails / tests fail.**
- [ ] **Step 3: Implement** 4×4-Raster aus 16 Zellen im Pad-Stil (grün für belegt, Akzentrahmen für lernend, wie im Mockup), `juce::Timer` mit 30 Hz ruft `pollLearning()` und prüft `revision()`. `SettingsDialog::~SettingsDialog` und `showTab` rufen `stopLearning()`. Der Editor-Destruktor schließt das Fenster vor dem Processor-Zugriff.
- [ ] **Step 4: `build.ps1 test -Config Release` grün; Snapshot ansehen; ZIP-Datei für die Handabnahme wie üblich ablegen (`dubgefahren.vst3.feature-settings-midi-learn.zip`).**
- [ ] **Step 5: Commit** (`feat(ui): pad grid with MIDI learn in the settings dialog`).

---

### Task 10: Dokumentation

**Files:**
- Modify: `README.md`, `docs/superpowers/specs/2026-10-27-settings-and-midi-learn-design.md` (Status), Wiki-Seiten `Kits-and-Configuration`, `Home`, `Installation` (separates Repo im Scratchpad)

- [ ] **Step 1: README (Englisch):** Abschnitt „Settings“ (Dialog, Reiter), „MIDI learn and profiles“ (Ablauf: Settings → Pad anklicken → Pad/Taste drücken; Profil wechseln; Profilordner und Datei weitergeben; `Default`; Profile werden nicht automatisch erkannt) und Kit-Ordner jetzt im Plugin einstellbar (`Dubgefahren.config.json` bleibt Rückfall). Den bisherigen MIDI-Menü-/„Pad 1 note“-Abschnitt entfernen.
- [ ] **Step 2: Spec-Status** auf „Akzeptiert, umgesetzt“ setzen; die Eigenentscheidungen aus Abschnitt 8, die der Nutzer im Review bestätigt hat, als bestätigt vermerken (nur mit seinem ausdrücklichen Ja).
- [ ] **Step 3: Wiki** in den Seiten oben anpassen (Englisch); erst nach dem Merge pushen.
- [ ] **Step 4: `build.ps1 test -Config Release` grün, Commit** (`docs: settings dialog, MIDI learn and profiles`).

---

## Self-Review

- **Spec-Abdeckung:** Dialog/Reiter (Task 7–9), Settings-Button statt MIDI-Button (7), Lernen im Dialog mit „press…“, kein Weiterspringen, Abbruchwege (9), Konfliktregel (1, 9), Note-On unterdrückt und Note-Off ohne hängende Stimme (4, 5), Profile als Dateien, Default, Anlegen/Umbenennen/Löschen/Ordner öffnen (2, 4, 8), Ordner neu einlesen (8), Ursprung im Profil (2, 4, 7), Migration aus alter Einstellung (4), Kit-Ordner Change/Open/Use default und Vorrang (6, 7), Nachschlagetabelle statt `translateNote` (1, 4), mehrere Instanzen (5, 9), Test-Builds ohne `%APPDATA%` (3, 4), Doku (10). Nicht gebaut, wie vom Nutzer gewünscht: Fill from note, Import/Export-Buttons, CC-Learn, Controller-Erkennung.
- **Typkonsistenz:** `PadProfile::notes` ist überall `std::array<int, kNumSlots>` mit `-1` für unbelegt; `NoteMap::assign(slot, note)` und `PadMapping::setPadNote(slot, note)` haben dieselbe Reihenfolge; `SettingsDialog::startLearning(int slot)`/`learningSlot()` verwenden Slot 0 bis 15; Tabs heißen in Task 7 und 9 `Tab::Midi`/`Tab::Kits`.
- **Entscheidungen dieses Plans, die über die Spec hinausgehen:** Profilformat `{"padOrigin": …, "notes": […]}` mit `null` für unbelegt; `Default` ist auch nicht umbenennbar; Namensregeln über `createLegalFileName`; `heldSlot_` im Audio-Thread; `PadMapping`-Testnaht über Konstruktor mit Ordner und `UserSettings`; Temp-Ordner für Profile in Test-Builds.
