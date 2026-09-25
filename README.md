# Dubgefahren

VST3-Instrument für Windows: 16 einzeln einstellbare Dubsirenen, spielbar über MIDI-Noten 36–51
(z. B. Intech Studio Grid BU16), mit gemeinsamer Dub-Effektkette (Drive, Filter, Tape-Delay, Federhall).

## Voraussetzungen

- Windows 10/11 x64
- Visual Studio 2026 mit „Desktopentwicklung mit C++" (bringt MSVC und CMake mit)
- Git

JUCE 8.0.15 und Catch2 werden beim ersten Configure automatisch geladen.

## Bauen und testen

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 all
```

Weitere Befehle: `configure`, `build`, `test`, `install`, `validate` (Option `-Config Debug|Release`, Standard Release).

Das Plugin liegt danach unter `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3`.

## Installieren

In einer **Administrator**-PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 install
```

Kopiert das Bundle nach `%CommonProgramFiles%\VST3`. Eine dort bereits vorhandene
`Dubgefahren.config.json` wird nicht überschrieben. Anderes Ziel: `cmake -DDG_VST3_INSTALL_DIR=<Ordner> build`.

## Config: Kit-Ordner festlegen

Datei: `%CommonProgramFiles%\VST3\Dubgefahren.vst3\Contents\Resources\Dubgefahren.config.json`
(Bearbeiten als Administrator).

```json
{ "kitFolder": "D:\\Musik\\Dubgefahren\\Kits" }
```

Umgebungsvariablen wie `%USERPROFILE%` sind erlaubt. Leer, fehlend oder ungültig → `Dokumente\Dubgefahren\Kits`.
Bei einer fehlerhaften Config zeigt das Plugin beim Öffnen von Kit-Menü, Import oder Export einen Hinweis.

## Kits

Kit-Menü → Werks-Kit oder Kits aus dem Kit-Ordner. Import/Export als `.dgkit` (JSON, 16 Slots, ohne Effekte).
Rechtsklick auf ein Pad: Kopieren, Einfügen, Auf Werkseinstellung zurücksetzen, Umbenennen.

## Validierung

`build.ps1 validate` baut Steinbergs VST3-Validator und prüft das Plugin. Für pluginval
`pluginval_Windows.zip` von https://github.com/Tracktion/pluginval/releases laden und
`pluginval.exe` nach `tools\bin\` legen.

## Abnahme-Checkliste (Ableton)

- [ ] Plugin erscheint als Instrument und lädt ohne Fehlermeldung.
- [ ] BU16-Pads 1–16 spielen die Slots 1–16; Pad-Anzeige im Plugin leuchtet mit.
- [ ] Gate: klingt nur solange gedrückt. Latch: an/aus per Tipp. One-Shot: feste Länge.
- [ ] Choke: Laser, Riser, Faller, Bleep, Zap, Drop würgen sich gegenseitig ohne Knacksen ab.
- [ ] Mehrere Sirenen klingen gleichzeitig.
- [ ] PO16 regelt die Effekte, TEK2 die Performance-Regler; Fokus/Alle verhält sich wie erwartet.
- [ ] Transport-Stopp mit allen drei „Latch bei Stopp"-Einstellungen geprüft.
- [ ] Panic stoppt alles, Delay-/Hall-Fahnen klingen aus.
- [ ] Set speichern, Live neu starten, Set laden: Einstellungen und Slot-Namen sind wieder da.
- [ ] Kit exportieren, Slots ändern, Kit importieren: Slots wiederhergestellt, Effekte unverändert.
- [ ] Fenster skalieren (75–200 %), Größe bleibt nach Neuladen erhalten.
- [ ] Delay-Feedback auf Maximum: Eigenoszillation ohne Übersteuern.
