# Settings-Dialog, MIDI Learn und Pad-Profile – Design-Spezifikation

**Datum:** 2026-10-27
**Status:** Entwurf, wartet auf Freigabe
**Vorgeschichte:** Teil der Vorbereitung auf Version 1.0 („Benutzerführung“). Zwei weitere Themen aus
demselben Gespräch sind **nicht** Teil dieser Spezifikation und werden getrennt besprochen: die
Kopfleiste/Buttons (Optik) und das Kit-Handling beim ersten Sample („vorher exportieren“).

## 1. Hintergrund und Ziel

Die Zuordnung von MIDI-Noten zu den 16 Pads ist heute eine einzige globale Einstellung aus „Note
von Pad 1“ (Standard 36) und „Ursprung“ (Pad 1 unten oder oben links), erreichbar über den Button
**MIDI** in der Kopfleiste und ein Zahlenfeld. Sie setzt voraus, dass der Controller seine Pads
fortlaufend nummeriert. Nach Tests mit mehreren Controllern ist das zu starr; außerdem müsste man die
Zahl für jeden Controller von Hand nachschlagen.

Der Kit-Ordner ist heute nur über die Datei `Dubgefahren.config.json` **im Plugin-Bundle** einstellbar
(unter `Program Files` mit Administratorrechten). Im Plugin gibt es dafür keine Stelle.

**Ziel:**

- Der Nutzer belegt jedes Pad per **MIDI Learn** mit einer beliebigen Note (Controller-Pad oder Taste
  eines Keyboards, beides sind einfach Noten).
- Belegungen sind **Profile**, die sich schnell wechseln und als Datei an andere weitergeben lassen.
- Ein neuer **Settings-Dialog** mit den Reitern *MIDI* und *Kits* ersetzt den MIDI-Button und macht
  den Kit-Ordner im Plugin sichtbar und änderbar.

**Erfolgskriterien:**

- Ein Nutzer richtet einen neuen Controller in unter einer Minute ein: Dialog öffnen, Pad im Dialog
  anklicken, Pad am Controller drücken, wiederholen; die Belegung bleibt nach Neustart des Hosts erhalten.
- Ein bereits eingerichtetes Profil lässt sich mit zwei Klicks wieder auswählen.
- Eine Profildatei, die ein Kollege in den Profilordner legt, erscheint nach dem Öffnen des Dialogs in
  der Liste und funktioniert unverändert.
- Wer bisher die alte Einstellung (Note von Pad 1, Ursprung) benutzt hat, hört keinen Unterschied.
- Der Kit-Ordner lässt sich im Plugin anzeigen und ändern, ohne Administratorrechte.

## 2. Umfang

**Enthalten:** Settings-Dialog mit Reitern *MIDI* und *Kits*; MIDI Learn im Dialog; Profile (Datei pro
Profil in einem Ordner); Ersatz der Umrechnung „erste Note + Offset“ durch eine Nachschlagetabelle;
Verlagerung der Kit-Ordner-Einstellung in die Benutzereinstellungen; Anpassung von README und Wiki
(Englisch wie bisher); Tests.

**Nicht enthalten (Nicht-Ziele):**

- Automatische Erkennung des Controllers. Ein VST3-Plugin erfährt vom Host nicht, welches Gerät
  angeschlossen ist; das Profil wird von Hand gewählt.
- MIDI Learn für Regler (CC), Parameter oder Automation. Gelernt werden nur Noten für die 16 Pads.
- Mehrere Noten pro Pad, Velocity-Kurven, Kanalfilter.
- Eigene Import- und Export-Buttons für Profile (Weitergabe läuft über die Datei).
- Umbau der Kopfleiste über den Ersatz des MIDI-Buttons hinaus; Kit-Handling im Hauptfenster.
- Eine Aktion „Fill from note…“ (der Nutzer hat sie ausdrücklich abgelehnt: Einrichtung ist einmaliger Aufwand).

**Anti-Ziele (wäre ein Fehlschlag, auch wenn es technisch läuft):**

- Ein Pad löst nach dem Lernen weiter die alte Note aus, oder eine Note löst zwei Pads aus.
- Hängende Noten: Ein Note-Off geht beim Lernen oder Profilwechsel verloren und lässt eine Stimme stehen.
- Allokation oder Sperren im Audio-Thread.
- Bestehende Nutzer verlieren ihre Belegung beim Update.
- Verhalten, das nur unter Windows funktioniert (macOS-Build ist Teil des Produkts).

## 3. Entscheidungen des Nutzers

- **Lernen im Dialog (Option A):** Das Pad-Raster im Dialog ist die Lernfläche. Pad im Dialog anklicken,
  es zeigt „press…“, dann Pad/Taste am Controller drücken. Danach wählt der Nutzer das nächste Pad
  selbst; es gibt kein automatisches Weiterspringen.
- **Profile** sind Dateien in einem Ordner; Weitergabe durch Kopieren der Datei. Dazu **ein** Button
  „Open profile folder“, der den Ordner im Dateimanager öffnet. Keine Import-/Export-Buttons.
- **Settings-Dialog** mit den Reitern *MIDI* und *Kits*; der MIDI-Button der Kopfleiste entfällt, ein
  Button „Settings“ nimmt seinen Platz ein.
- **Kein „Pad 1 note“-Feld** und keine „Fill from note…“-Aktion mehr.
- **Der Ursprung** (Pad 1 oben/unten links) bleibt, gehört ins Profil und bestimmt nur, wie das Raster
  (im Dialog und im Hauptfenster) gezeichnet wird, nicht welche Note ein Pad auslöst.
- Das Wechseln des Profils geschieht von Hand, die Auswahl gilt für alle Instanzen im Host (wie die
  heutige Belegung).

## 4. Verhalten

### 4.1 Settings-Dialog

- Öffnet über den Button **Settings** in der Kopfleiste (im Edit-Modus, an der Stelle des bisherigen
  MIDI-Buttons). Im Live-Modus bleibt die Kopfleiste wie heute.
- Zwei Reiter: **MIDI** und **Kits**. Schließen mit „Close“ oder Esc (außer beim Lernen, siehe 4.3).
- Der Dialog ist **modal** (Entscheidung des Nutzers: Einstellungen ändert man nicht im Live-Betrieb).
  Das Lernen findet im Dialog statt, das Hauptfenster muss dafür nicht bedienbar sein. Die
  Audioverarbeitung und das Auslösen der Pads laufen weiter, nur die Oberfläche des Hauptfensters ist gesperrt.
- Texte im Plugin sind Englisch (wie die übrige Oberfläche). Optik im bestehenden Stil
  (`DgLookAndFeel`: Hintergrund `#1b1f24`, Akzent `#f2b134`, Pad-Grün).

### 4.2 Reiter MIDI

- Oben die **Profilliste** (Auswahlfeld) mit den Aktionen *New*, *Rename*, *Delete* und *Open profile folder*.
- Darunter das **4×4-Raster** der Pads. Jedes Feld zeigt die Padnummer und die zugewiesene Note, ein
  unbelegtes Pad zeigt einen Strich. Der **Ursprung** wird mit einem Schalter im Reiter eingestellt
  („Pad 1 at top left“) und das Raster folgt ihm sofort.
- Die Notenanzeige als Zahl (0 bis 127). Ob zusätzlich der Notenname erscheint, entscheidet der Bauende.
- Eine Zeile mit dem Hinweis „Click a pad, then press a pad or key on your controller.“

### 4.3 MIDI Learn

- Klick auf ein Pad im Raster setzt es in den Zustand **lernend** (deutliche Markierung, „press…“).
  Es lernt immer höchstens ein Pad.
- Die nächste **Note-On** (egal auf welchem Kanal), die der Prozessor empfängt, wird diesem Pad zugewiesen.
  Der Lernzustand endet danach.
- **Konflikt:** Hat die Note schon ein anderes Pad, behält das lernende Pad sie und das andere wird
  unbelegt (jede Note gehört höchstens einem Pad). *(Eigene Entscheidung, noch nicht vom Nutzer bestätigt.)*
- **Abbruch** des Lernens: Esc, erneuter Klick auf das lernende Pad, Klick auf ein anderes Pad (dieses
  lernt dann), Wechsel des Reiters oder Schließen des Dialogs. Esc beendet nur das Lernen und schließt
  den Dialog dabei nicht. *(Eigene Entscheidung, noch nicht vom Nutzer bestätigt.)*
- Beim Lernen **löst die gedrückte Note keinen Klang aus** und erreicht die Engine nicht. *(Eigene
  Entscheidung, siehe Abschnitt 8.)* Ein zugehöriges Note-Off darf keine Stimme hängen lassen.
- Eine Taste am Keyboard wird genauso gelernt wie ein Controller-Pad.
- Jede Zuweisung wird **sofort** im Profil gespeichert (kein „Apply“).

### 4.4 Profile

- Ein Profil ist eine benannte Belegung: Name, 16 Noten (Pad 1 bis 16, jede 0 bis 127 oder unbelegt)
  und Ursprung.
- Eine Datei pro Profil: `<Name>.json` im Ordner `Profiles` neben der bisherigen `settings.json`
  (`%APPDATA%\Dubgefahren\Profiles` unter Windows, `~/Library/Application Support/Dubgefahren/Profiles`
  unter macOS). Der Name ist der Dateiname ohne Endung; unzulässige Zeichen werden beim Anlegen und
  Umbenennen verweigert oder ersetzt.
- Die Liste liest den Ordner beim Öffnen des Dialogs neu ein; eine vom Kollegen hineinkopierte Datei
  erscheint dann ohne Neustart. Eine ungültige oder unlesbare Datei erscheint nicht in der Liste und
  stürzt nichts ab.
- **Standardprofil „Default“:** Existiert der Profilordner noch nicht, wird er angelegt und ein Profil
  „Default“ aus der bisherigen Einstellung erzeugt (16 fortlaufende Noten ab der alten ersten Note,
  alter Ursprung; fehlt `settings.json`, ab Note 36, unten links). `Default` ist nicht löschbar; fehlt
  die Datei später, wird sie wieder angelegt.
- Das **aktive Profil** (Name) steht in `settings.json`. Ist es beim Start nicht auffindbar, gilt `Default`.
- *New* legt ein Profil mit einem eindeutigen Namen an (Standard: „New profile“, bei Bedarf nummeriert),
  belegt wie `Default`, und wählt es aus. *Rename* ändert Name und Dateiname, *Delete* löscht die Datei
  nach Rückfrage und wählt `Default`.
- Die Auswahl gilt prozessweit für alle Instanzen (wie die heutige `PadMapping`).

### 4.5 Reiter Kits

- Zeigt den aktuellen **Kit-Ordner** (voller Pfad) und eine Warnzeile, wenn er unbrauchbar ist
  (bestehende Logik von `resolveKitFolder`).
- Aktionen: **Change…** (Ordnerauswahl), **Open folder** (im Dateimanager) und **Use default**
  (setzt die Einstellung zurück auf `Documents/Dubgefahren/Kits`).
- Eine Änderung wirkt sofort für Kit-Menü, Import und Export. Vorhandene Dateien werden nicht verschoben.
- Die Einstellung steht künftig in den Benutzereinstellungen (`settings.json`, Schlüssel `kitFolder`).
  Eine vorhandene Angabe in `Dubgefahren.config.json` im Bundle gilt weiter als Rückfall, wenn in
  `settings.json` nichts steht; damit ändert sich für bestehende Installationen nichts.

## 5. Technische Rahmenbedingungen

- **Engine:** Die Engine kennt weiter nur die Noten 36 bis 51 als Pad 1 bis 16 (`translateNote`).
  Die Umrechnung „Basisnote + Offset“ wird durch eine Tabelle Note → Pad ersetzt (128 Einträge, im
  Audio-Thread ohne Sperren lesbar, nur im Message-Thread beschrieben, ohne Allokation).
  `translateNote` und seine Tests werden entsprechend umgestellt oder ersetzt.
- **Audio-Thread:** Auswertung der Roh-Bytes ohne `juce::MidiMessage` bleibt (keine Allokation bei SysEx).
  Für das Lernen reicht der Audio-Thread die empfangene Note lock-free an den Message-Thread weiter
  (z. B. atomare Variable oder FIFO) und unterdrückt solange das Auslösen.
- **Mehrere Instanzen:** Belegung und aktives Profil sind prozessweit; das **Lernen** gilt aber nur für
  die Instanz, deren Dialog es angestoßen hat und deren Prozessor die MIDI-Note empfängt.
- **Persistenz:** `settings.json` bekommt die Schlüssel `profile` und `kitFolder`; die bisherigen
  Schlüssel `firstNote` und `padOrigin` bleiben lesbar (für die Migration), werden aber nicht mehr
  geschrieben oder ausgewertet, sobald ein Profilordner existiert.
- **Test-Builds:** Wie bisher keine Zugriffe auf Benutzerverzeichnisse (`DG_NO_USER_SETTINGS`); alle
  neuen Dateizugriffe müssen mit einem Testordner statt `%APPDATA%` prüfbar sein.
- **Plattformen:** Windows und macOS (Pfade über `juce::File::userApplicationDataDirectory`, kein
  Windows-spezifischer Code).

## 6. Tests

Mindestens (Engine/Plugin, Catch2 wie bisher):

- Nachschlagetabelle: jede belegte Note löst genau ihr Pad aus, unbelegte Noten nichts; Wechsel des
  Profils ändert das Verhalten ohne Neustart.
- Konflikt beim Lernen: die Note geht an das lernende Pad, das andere wird unbelegt.
- Lernen unterdrückt das Auslösen; ein späteres Note-Off lässt keine Stimme hängen.
- Profil: Lesen/Schreiben, ungültige Datei wird übersprungen, Name mit unzulässigen Zeichen, Umbenennen,
  Löschen, Rückfall auf `Default`.
- Migration: aus `settings.json` mit `firstNote`/`padOrigin` entsteht `Default` mit den 16 erwarteten
  Noten; ohne Datei die Standardwerte (36, unten links).
- Kit-Ordner: Vorrang `settings.json` vor Bundle-Config vor Standardordner; „Use default“.
- Editor: Dialog öffnet, Reiter, Lernzustand wechselt und endet bei Esc, Raster folgt dem Ursprung.

Der Dialog selbst kann von Hand in Live geprüft werden; die Abnahme auf echter Hardware (Controller,
Keyboard) macht der Nutzer.

## 7. Risiken

- Ein gelerntes Note-On wird unterdrückt, das dazugehörige Note-Off kommt aber nach der Zuweisung an
  und würde auf die neue Zuordnung treffen. Die Engine darf dadurch keine Stimme hängen lassen oder
  falsch freigeben.
- Mehrere Instanzen im Host teilen Belegung und Profil; ein Profilwechsel in einer Instanz wirkt in
  allen, auch bei laufenden Noten.
- Profildateien von Dritten sind Eingaben von außen: fehlerhafte Inhalte (falsche Länge, Noten
  außerhalb 0 bis 127, doppelte Noten) müssen robust behandelt werden.
- macOS ist ungetestet auf echter Hardware (siehe v0.10.0); Pfade und Dateimanager-Aufruf sind dort
  nur über den CI-Build geprüft.

## 8. Offene Entscheidungen des Bauenden / eigene Entscheidungen

- *Eigene Entscheidung:* Beim Lernen wird die gedrückte Note nicht an die Engine gereicht.
- *Eigene Entscheidung:* `Default` ist nicht löschbar und wird bei Bedarf wieder angelegt.
- *Eigene Entscheidung:* Profildateien liegen im Ordner `Profiles` neben `settings.json`; die
  Dateistruktur (`name` aus Dateiname, `notes`: Liste mit 16 Einträgen, `padOrigin`) ist dem Bauenden
  überlassen, solange sie von Hand lesbar bleibt (JSON).
- *Eigene Entscheidung:* Der Kit-Ordner wandert in `settings.json`; die Bundle-Config bleibt Rückfall.
- Dem Bauenden überlassen: Aufteilung in Klassen und Dateien, Darstellung der
  Notennamen, Dateinamen-Bereinigung, genaue Technik der Weitergabe an den Message-Thread, Layout im
  Detail.

## 9. Folgearbeiten (nicht Teil dieser Spezifikation)

- Optik der Kopfleiste (Buttons „weniger schick“).
- Kit-Handling beim ersten Sample („vorher exportieren“).
- Wiki-Seite zu Profilen und Kit-Ordner, README-Abschnitt; Release-Notiz für 1.0.
- Rückmeldung eines echten Mac-Nutzers.
