# Dubgefahren

VST3 instrument for Windows: 16 individually adjustable dub sirens, playable via MIDI notes 36–51
(e.g. Intech Studio Grid BU16), with a shared dub effects chain (drive, filter, tape delay, spring reverb).

## Download

Prebuilt binaries are attached to the [GitHub releases](https://github.com/atze187/dubgefahren/releases)
(`Dubgefahren-vX.Y.Z-win64.zip`). Unzip and copy the `Dubgefahren.vst3` folder to
`C:\Program Files\Common Files\VST3` (administrator rights required), then rescan plugins in your DAW.
The binaries are not code-signed, so Windows may show a SmartScreen warning.

## Requirements (building from source)

- Windows 10/11 x64
- Visual Studio 2026 with the "Desktop development with C++" workload (includes MSVC and CMake)
- Git

JUCE 8.0.15 and Catch2 are downloaded automatically on the first configure.

## Build and test

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 all
```

Other commands: `configure`, `build`, `test`, `install`, `validate` (option `-Config Debug|Release`, default Release).

The plugin is then located at `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3`.

## Install

In an **administrator** PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 install
```

Copies the bundle to `%CommonProgramFiles%\VST3`. An existing `Dubgefahren.config.json` there
is not overwritten. Different target: `cmake -DDG_VST3_INSTALL_DIR=<folder> build`.

## Config: setting the kit folder

File: `%CommonProgramFiles%\VST3\Dubgefahren.vst3\Contents\Resources\Dubgefahren.config.json`
(edit as administrator).

```json
{ "kitFolder": "D:\\Music\\Dubgefahren\\Kits" }
```

Environment variables such as `%USERPROFILE%` are allowed. Empty, missing or invalid → `Documents\Dubgefahren\Kits`.
If the config is invalid, the plugin shows a notice when the kit menu, import or export is opened.

## Kits

Kit menu → factory kit ("Werks-Kit") or kits from the kit folder. Import/export as `.dgkit` (JSON, 16 slots, without effects).
Right-click a pad: copy, paste, reset to factory settings, rename.

## Validation

`build.ps1 validate` builds Steinberg's VST3 validator and checks the plugin. For pluginval, download
`pluginval_Windows.zip` from https://github.com/Tracktion/pluginval/releases and place
`pluginval.exe` in `tools\bin\`.

## Releases

Pushing a tag `v*` (e.g. `v0.1.0`) runs the GitHub Actions workflow `.github/workflows/release.yml` on Windows:
build, tests, Steinberg validator and pluginval (strictness 5). If everything passes, the zip is attached
to a GitHub release for that tag. The workflow can also be started manually (Actions tab → "Build and release")
to get the zip as a build artifact without publishing a release.

## License

Dubgefahren is licensed under the GNU Affero General Public License v3.0 — see [LICENSE](LICENSE).

Third-party components:

- [JUCE](https://juce.com) 8 — used under the AGPLv3.
- [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) (Steinberg Media Technologies) — MIT license; included in the plugin
  via JUCE's VST3 wrapper and used to build the validator tool. VST is a registered trademark of Steinberg Media Technologies GmbH.
- [Catch2](https://github.com/catchorg/Catch2) — Boost Software License 1.0; used for tests only, not part of the plugin.

## Acceptance checklist (Ableton)

- [ ] Plugin appears as an instrument and loads without error messages.
- [ ] BU16 pads 1–16 play slots 1–16; the pad display in the plugin lights up accordingly.
- [ ] Gate: sounds only while held. Latch: on/off per tap. One-shot: fixed length.
- [ ] Choke: Laser, Riser, Faller, Bleep, Zap and Drop cut each other off without clicks.
- [ ] Several sirens can sound at the same time.
- [ ] PO16 controls the effects, TEK2 the performance controls; focus/all ("Fokus"/"Alle") behaves as expected.
- [ ] Transport stop tested with all three "latch on stop" ("Latch bei Stopp") settings.
- [ ] Panic stops everything; delay/reverb tails ring out.
- [ ] Save the set, restart Live, load the set: settings and slot names are restored.
- [ ] Export a kit, change slots, import the kit: slots restored, effects unchanged.
- [ ] Scale the window (75–200 %); the size is kept after reloading.
- [ ] Delay feedback at maximum: self-oscillation without clipping.
