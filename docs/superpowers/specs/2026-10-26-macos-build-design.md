# macOS-Build – Design-Spezifikation

**Datum:** 2026-10-26
**Status:** Entwurf, wartet auf Freigabe
**Vorgeschichte:** Plan vom 2026-10-02 (damals zurückgestellt), am 2026-10-26 wieder aufgenommen

## 1. Hintergrund und Ziel

Dubgefahren gibt es bisher nur als Windows-VST3. Mac-Nutzer sollen dasselbe Plugin als VST3 in
Ableton Live (und anderen DAWs) laden können.

Rahmenbedingungen: Es steht kein Mac zum Testen zur Verfügung und es gibt keinen Apple-Developer-
Account. Der Mac-Build entsteht deshalb ausschließlich auf GitHub Actions, ist ad-hoc signiert und
nicht notarisiert, und wird klar als experimentell gekennzeichnet.

**Erfolgskriterien:**

- Der Job `macos` baut das Plugin als Universal-Binary (arm64 + x86_64), alle Unit- und Plugin-Tests
  bestehen, ebenso der Steinberg-VST3-Validator und pluginval (Strictness 5).
- Ein Release enthält zusätzlich `Dubgefahren-vX.Y.Z-macos.zip`, wenn der Mac-Job erfolgreich war.
- Schlägt der Mac-Job fehl, wird der Windows-Release trotzdem veröffentlicht.
- Windows-Build und -Verhalten bleiben unverändert.
- Das Plugin ist nicht auf einem echten Mac geprüft; das steht in Release-Text, README und Wiki.

## 2. Umfang

**Enthalten:** `CMakePresets.json` für Windows und macOS, Umbau von `build.ps1` auf die Presets,
macOS-Installpfad, plattformneutrale Tests, Job `macos` und gemeinsamer Job `publish` in
`release.yml`, README und Wiki.

**Nicht enthalten:** AU-Format, Notarisierung und Developer-ID-Signierung, `build.sh`, iOS/iPadOS,
Linux, Tests auf echter Mac-Hardware.

## 3. Entscheidungen

- **Nur VST3.** Live unterstützt VST3 auf dem Mac, ein AU bringt hier keinen Nutzen.
- **Universal-Binary** `arm64;x86_64`, **Deployment-Target macOS 11.0** (älteste Version mit
  Apple Silicon; Live 12 verlangt ohnehin macOS 11).
- **CMake-Presets** als gemeinsame Build-Beschreibung für lokal und CI. `build.ps1` bleibt als
  Wrapper (gleiche Befehle `configure/build/test/install/validate/all`); ein `build.sh` gibt es nicht.
- **Validierung vor Veröffentlichung:** Das Mac-Zip entsteht nur, wenn Tests, Validator und
  pluginval bestehen.
- **Release-Struktur (Variante A):** `windows` und `macos` bauen parallel und laden nur Artifacts
  hoch; ein dritter Job `publish` veröffentlicht, was vorhanden ist. Kein Wettlauf beim Anlegen des
  Releases, der Warnhinweis steht an einer Stelle.
- **Keine neue Config-Syntax.** `%NAME%` bleibt; auf dem Mac funktioniert z. B. `%HOME%`.

## 4. Build: CMake-Presets

`CMakePresets.json` im Repo-Root (Version 6 oder höher, passend zu CMake ≥ 3.25):

- Configure-Presets `windows-x64` (Visual Studio, Architektur x64) und `macos-universal` (Ninja,
  `CMAKE_OSX_ARCHITECTURES=arm64;x86_64`, `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`). Jedes Preset hat eine
  `condition` auf seine Plattform, sodass es auf der anderen nicht angeboten wird.
- Build- und Test-Presets je Plattform und Konfiguration (Release; Debug bleibt lokal möglich).
- Eigene Presets für `tools/vst3validator` (nativ, nicht Universal).
- Build-Verzeichnis bleibt `build/` (Windows) bzw. `build-macos/` (macOS), damit sich beide nicht
  überschreiben, wenn jemand das Repo teilt.
- `build.ps1` ruft `cmake --preset windows-x64` usw. auf; Ausgabepfade und Befehle bleiben gleich.
- `DG_VST3_INSTALL_DIR`: Windows wie bisher `%CommonProgramFiles%/VST3`; macOS
  `~/Library/Audio/Plug-Ins/VST3` (kein Admin nötig). `cmake/InstallBundle.cmake` ist
  plattformneutral und bleibt, ebenso die Regel „vorhandene Config nicht überschreiben“.

## 5. Code-Anpassungen

- `plugin/Config.cpp`, `pluginConfigFile()`: funktioniert auf macOS unverändert
  (`Contents/MacOS/<Name>` → zwei Ebenen hoch → `Contents/Resources`). Nur der Kommentar nennt
  künftig beide Layouts.
- `resources/Dubgefahren.config.json`: `_note` nennt Windows- und macOS-Beispiel
  (`%USERPROFILE%` bzw. `%HOME%`) und den Standardordner `Documents/Dubgefahren/Kits`.
- Tests: `test_Config.cpp` wählt die Umgebungsvariable plattformabhängig (`TEMP` bzw. `HOME`) und
  bildet Pfade mit dem Plattform-Trenner. Die Traversal-Tests (`..\Dub\horn.wav`) bleiben auf allen
  Plattformen gültig; ist die Logik in `KitFile`/`Samples` nicht plattformneutral, wird sie
  angepasst. Weitere Fehler zeigt erst der CI-Lauf; sie werden iterativ behoben.
- MSVC-spezifische Compiler-Flags sind bereits abgesichert; JUCE bindet die macOS-Frameworks selbst.

## 6. CI: `release.yml`

- **`windows`:** wie bisher (Build, Tests, Validierung, Paketierung), aber auf Presets umgestellt;
  lädt `Dubgefahren-<version>-win64.zip` als Artifact hoch, veröffentlicht selbst nichts mehr.
- **`macos`** (`macos-latest`, Timeout 90 min):
  1. Configure, Build, Test mit `macos-universal`.
  2. pluginval 1.0.4 (`pluginval_macOS.zip`) laden; Steinberg-Validator bauen; beide gegen
     `Dubgefahren.vst3` laufen lassen.
  3. `codesign --force --deep -s -` auf dem Bundle (ad-hoc, auch für die x86_64-Hälfte).
  4. Zip mit `ditto -c -k --keepParent` (erhält Bundle-Struktur und Signatur), Inhalt
     `Dubgefahren.vst3`, README, LICENSE; als `Dubgefahren-<version>-macos` hochladen.
- **`publish`** (nur bei Tag-Pushes; `needs: [windows, macos]`; `if: always() &&
  needs.windows.result == 'success'`): lädt vorhandene Artifacts und veröffentlicht sie mit
  `softprops/action-gh-release`. Liegt ein Mac-Zip vor, hängt `publish` den Hinweis an die Notes:
  *macOS build is experimental and untested, not notarized. After copying the bundle to
  `~/Library/Audio/Plug-Ins/VST3`, remove the quarantine with `xattr -cr Dubgefahren.vst3`.*
- Test des Workflows ohne Veröffentlichung per `workflow_dispatch` auf dem Feature-Branch
  (`publish` läuft nur bei Tags).

## 7. Doku (Englisch wie bisher)

- README: Plattformzeile („Windows x64; macOS experimental“), macOS-Installation inkl. `xattr -cr`,
  Build-Anleitung mit Presets, Config-Beispiele für beide Plattformen, Release-Beschreibung (zwei
  Zips).
- Wiki: Abschnitt zu macOS (Installation, bekannte Einschränkungen: ungetestet, nicht notarisiert,
  kein AU).

## 8. Risiken

- Kein Mac: Fehler, die nur zur Laufzeit in Ableton auftreten (Oberfläche, Dateidialoge, Pfade),
  zeigen weder Tests noch Validatoren. Gegenmaßnahme: Kennzeichnung „experimentell, ungetestet“.
- Gatekeeper: Ohne Notarisierung blockiert macOS das Plugin, bis die Quarantäne entfernt ist.
  Gegenmaßnahme: Anleitung in Release-Text und README.
- Der CI-Lauf braucht vermutlich mehrere Iterationen, bis die Tests auf macOS bestehen.
- Neue Runner-Images können die Toolchain ändern (`macos-latest` wandert); bei Bedarf Version
  festpinnen.

## 9. Folgearbeiten (nicht Teil dieser Spec)

- Notarisierung und Developer-ID-Signierung, falls ein Apple-Developer-Account vorhanden ist.
- AU-Format, falls Bedarf entsteht.
- Rückmeldung eines echten Mac-Nutzers einholen und die Kennzeichnung „ungetestet“ danach anpassen.
