# Dubgefahren

VST3 instrument for Windows: 16 individually adjustable dub sirens, playable via 16 consecutive MIDI notes (default 36–51, adjustable)
(e.g. Intech Studio Grid BU16), with a shared dub effects chain (drive, filter, tape delay with phaser on the echoes, spring reverb).

*This is an experimental project to evaluate Claude. 
I do not contribute to code while still reviewing Claude-written code. I do however propose features.*

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

## MIDI mapping

The 16 pads answer to 16 consecutive MIDI notes. By default pad 1 is note 36 and sits at the bottom left
of the pad grid. The **MIDI** menu in the header changes both: *Pad 1 note* sets the note of the first
pad, *Pad 1 at top left* flips the grid so that pad 1 is at the top left. The setting is stored per user in
`%APPDATA%\Dubgefahren\settings.json` and applies to every project and instance.

The Intech Grid BU16 sends its buttons row by row from the top left starting at note 32: set
*Pad 1 note* to 32 and enable *Pad 1 at top left* to make the grid match the controller.

## Effects

All sirens share one effects chain. Each slot's *FX Send* sets how much of it feeds the send bus.

- **Drive**: saturation in front of the filter, on both buses. Soft and warm in the lower half of the
  knob, increasingly hard in the upper half. The level rises with the knob for moderate signals; very loud
  signals get quieter at the top. Highs are slightly softened as soon as Drive is above zero; at 0 the
  signal passes through untouched.
- **Filter**: cutoff, resonance and a type knob that morphs low-pass, band-pass and high-pass; on both buses.
- **Delay** (send bus): tape echo in the style of a Roland RE-201 Space Echo. *Time* is tempo-synced
  (1/16 triplet up to 1 bar), *Feedback* goes up to 110 % (self-oscillation), *Tone* darkens the repeats,
  *Wow* adds irregular pitch drift and flutter, *Mix* sets the level. Every repeat gets a little darker and
  softer, and a faint tape noise runs in the loop.
- **Phaser** (delay returns only): soft four-stage phaser on the echoes, so the dry signal stays clean.
  *Rate* 0.05 to 3 Hz, *Depth*, *Mix* (default 0 = off).
- **Reverb** (send bus): spring reverb with *Decay*, *Tone* and *Mix*.
- **Master**: output level, followed by a peak limiter at -0.3 dBFS.

## Kits

Kit menu → factory kit, a new empty kit, or kits from the kit folder. Import/export as `.dgkit`
(JSON, 16 slots, without effects).
Right-click a pad: copy, paste, reset to factory settings, rename, clear.
An empty pad is silent; click it to choose a sound source: Synth, or Sample.
Samples (WAV, AIFF, MP3, FLAC, up to 60 s, played in mono) live in a folder named like the kit,
next to the `.dgkit` file (`Kits\Dub.dgkit` → `Kits\Dub\`). "Add File…" copies a file into that
folder; exporting to a new kit copies the used samples along. Samples need a kit file, so export
a new kit once before adding samples. A missing sample mutes its pad (shown in orange) and shows a notice;
the pad keeps its sample and plays again once the file is back (reopen the set or reload the kit).

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
- [ ] BU16 (MIDI menu: pad 1 note 32, pad 1 at top left) pads 1–16 play slots 1–16; each button lights up the pad at the same position in the plugin.
- [ ] Gate: sounds only while held. Latch: on/off per tap. One-shot: fixed length.
- [ ] Choke: Laser, Riser, Faller, Bleep, Zap and Drop cut each other off without clicks.
- [ ] Several sirens can sound at the same time.
- [ ] PO16 controls the effects, TEK2 the performance controls; "Perf Target" focus/all behaves as expected.
- [ ] Transport stop tested with all three "Latch on Stop" settings.
- [ ] Panic stops everything; delay/reverb tails ring out.
- [ ] Save the set, restart Live, load the set: settings and slot names are restored.
- [ ] Export a kit, change slots, import the kit: slots restored, effects unchanged.
- [ ] Scale the window (75–200 %); the size is kept after reloading.
- [ ] Delay feedback at maximum: self-oscillation without clipping.
- [ ] Delay: warm tape character, every repeat a little darker, a slight irregular wobble with Wow, no audible hiss at Feedback 0.45.
- [ ] Drive: 0 changes nothing; soft and warm at low settings, harder and biting towards the top; no aliasing fizz on high notes.
- [ ] Phaser: Mix up on the delay returns gives a soft sweeping phase on the echoes only; the dry signal stays clean; Mix 0 sounds exactly like before.
