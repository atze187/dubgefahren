# Dubgefahren

VST3 instrument for Windows and (experimental) macOS: 16 individually adjustable dub sirens, playable via MIDI notes you can learn per pad (default 36–51)
(e.g. Intech Studio Grid BU16), with a shared dub effects chain (drive, filter, tape delay with phaser on the echoes, spring reverb).

*This is an experimental project to evaluate Claude. 
I do not contribute to code while still reviewing Claude-written code. I do however propose features.*

## Download

Prebuilt binaries are attached to the [GitHub releases](https://github.com/atze187/dubgefahren/releases)
(`Dubgefahren-vX.Y.Z-win64.zip`). Unzip and copy the `Dubgefahren.vst3` folder to
`C:\Program Files\Common Files\VST3` (administrator rights required), then rescan plugins in your DAW.
The binaries are not code-signed, so Windows may show a SmartScreen warning.

**macOS (experimental):** `Dubgefahren-vX.Y.Z-macos.zip` is a universal VST3 (Apple Silicon and Intel, macOS 11 or newer).
It is built and validated on GitHub Actions but has not been tested on real hardware, and it is not notarized.
Copy `Dubgefahren.vst3` to `~/Library/Audio/Plug-Ins/VST3`, remove the quarantine flag with
`xattr -cr ~/Library/Audio/Plug-Ins/VST3/Dubgefahren.vst3` and rescan plugins in your DAW. There is no AU version.

## Requirements (building from source)

- Windows 10/11 x64, Visual Studio 2026 with the "Desktop development with C++" workload (includes MSVC and CMake), or
- macOS 11 or newer with Xcode command line tools, CMake 3.25+ and Ninja (only built and checked on GitHub Actions so far)
- Git

JUCE 8.0.15 and Catch2 are downloaded automatically on the first configure.

## Build and test

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 all
```

Other commands: `configure`, `build`, `test`, `install`, `validate` (option `-Config Debug|Release`, default Release).
`build.ps1` is a thin wrapper around the CMake presets in `CMakePresets.json`; you can call them directly:

```
cmake --preset windows-x64            # Windows
cmake --build --preset windows-release --parallel
ctest --preset windows-release

cmake --preset macos-universal        # macOS (universal arm64 + x86_64, deployment target 11.0)
cmake --build --preset macos-release --parallel
ctest --preset macos-release
```

The plugin is then located at `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3`
(macOS: `build-macos/plugin/Dubgefahren_artefacts/Release/VST3/Dubgefahren.vst3`).

## Install

In an **administrator** PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 install
```

Copies the bundle to `%CommonProgramFiles%\VST3`. An existing `Dubgefahren.config.json` there
is not overwritten. Different target: `cmake -DDG_VST3_INSTALL_DIR=<folder> build`.
On macOS the default target is `~/Library/Audio/Plug-Ins/VST3` (no administrator rights needed):
`cmake --install build-macos`.

## Config: setting the kit folder (fallback)

The kit folder is normally set in **Settings → Kits**, and that choice wins. Without it the plugin reads
this config file instead.

File: `%CommonProgramFiles%\VST3\Dubgefahren.vst3\Contents\Resources\Dubgefahren.config.json`
(edit as administrator). On macOS: `~/Library/Audio/Plug-Ins/VST3/Dubgefahren.vst3/Contents/Resources/Dubgefahren.config.json`.

```json
{ "kitFolder": "D:\\Music\\Dubgefahren\\Kits" }
```

On macOS use a path such as `"%HOME%/Music/Dubgefahren/Kits"`.
Environment variables such as `%USERPROFILE%` (Windows) or `%HOME%` (macOS) are allowed. Empty, missing or invalid → `Documents/Dubgefahren/Kits`.
If the config is invalid, the plugin shows a notice when the kit menu, import or export is opened.

## Settings

The **Settings** button in the header opens a window with two tabs. It is modal: audio and MIDI keep
playing, but the plugin window waits until you close it. Everything takes effect immediately and is stored
per user in `%APPDATA%\Dubgefahren\settings.json` (macOS: `~/Library/Application Support/Dubgefahren`),
shared by every project and instance.

- **MIDI**: pad profiles, MIDI learn and the orientation of the pad grid (below).
- **Kits**: the kit folder. *Change…* picks a folder, *Open folder* shows it, *Use default* goes back to
  `Documents/Dubgefahren/Kits`. If the folder in your settings is gone, the plugin falls back to the
  config file below (or the default folder) and shows a warning here.

## MIDI learn and profiles

Each of the 16 pads has its own MIDI note. By default pad 1 is note 36, pad 2 is 37 and so on, with
pad 1 at the bottom left of the pad grid.

To assign pads to your controller: open **Settings → MIDI**, click a pad in the grid (it shows
*press…*), then press a pad on the controller or a key on the keyboard. Choose the next pad by hand,
there is no auto-advance. Clicking the pad again or pressing Esc cancels. If the note already belongs to
another pad, it moves to the pad you are learning; that pad becomes unassigned. Every assignment is saved
at once. The **Pad 1 at top left** switch only changes how the grid is drawn, so it matches your controller.

The assignment lives in a **profile**. *Default* is always there and cannot be renamed or deleted.
*New* creates a profile (a copy of *Default*), *Rename* and *Delete* work on the selected one. Profiles
are plain JSON files in `%APPDATA%\Dubgefahren\Profiles` (*Open profile folder* shows the folder):

```json
{ "padOrigin": "topLeft", "notes": [32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47] }
```

`null` in `notes` leaves a pad without a note. To share a profile with a colleague who has the same
controller, send them the file and have them drop it into their profile folder; it shows up in the list
the next time you open the tab. A plugin cannot tell which controller is connected, so you pick the
profile yourself. Settings written by older versions (*Pad 1 note*, *Pad 1 at top left*) become the
*Default* profile on the first start.

## Effects

All sirens share one effects chain. Each slot's *FX Send* sets how much of it feeds the send bus.

Three performance knobs sit on top of the effects: **Space** (longer, wider echoes and reverb), **Grit**
(saturation, tape wobble, darker echoes, phaser) and **Throw** (the dub throw: more FX send and a delay that
can run into self-oscillation). They add to the values of the individual controls: at 0 the sound is exactly
what the individual controls say. Drive, the Delay *Feedback*, *Tone*, *Wow* and *Mix* controls, the Phaser
and the Reverb live in the collapsible *Advanced* strip in the Edit view; Filter, Delay *Time* and Master
stay visible in both views.

- **Drive**: saturation in front of the filter, on both buses. Soft and warm in the lower half of the
  knob, increasingly hard in the upper half. The level rises with the knob for moderate signals; very loud
  signals get quieter at the top. Highs are slightly softened as soon as Drive is above zero; at 0 the
  signal passes through untouched.
- **Filter**: cutoff, resonance and a type slider (LP · BP · HP) that morphs continuously from low-pass through band-pass to high-pass; on both buses.
- **Delay** (send bus): tape echo in the style of a Roland RE-201 Space Echo. *Time* is tempo-synced
  (1/16 triplet up to 1 bar), *Feedback* goes up to 110 % (self-oscillation), *Tone* darkens the repeats,
  *Wow* adds irregular pitch drift and flutter, *Mix* sets the level. Every repeat gets a little darker and
  softer, and a faint tape noise runs in the loop.
- **Phaser** (delay returns only): soft four-stage phaser on the echoes, so the dry signal stays clean.
  *Rate* 0.05 to 3 Hz, *Depth*, *Mix* (default 0 = off).
- **Reverb** (send bus): spring reverb with *Decay*, *Tone* and *Mix*.
- **Master**: output level, followed by a peak limiter at -0.3 dBFS.

## Live and Edit view

The *Live* button in the header switches to a compact view with the pads, Space, Grit, Throw, Filter, Delay
time, Master and the performance controls (window about 880 × 460). *Edit* shows everything, including the
slot editor and the *Advanced* strip. The choice is saved with the project.

The interface is a flat control panel in a monospace font with upper-case labels. Parameters are segment
sliders: drag the bar, and the value shows in the LCD field next to it. Pads are flat tiles with a colour bar
at the bottom; the tile fills with the pad colour while the pad plays, and a blue frame marks the focused or
selected pad. The font is the system monospace font, so the exact look differs slightly between Windows and
macOS.

## Kits

Kit menu → factory kit, a new empty kit, or kits from the kit folder. Import/export as `.dgkit`
(JSON, 16 slots, without effects).
Right-click a pad: copy, paste, reset to factory settings, rename, clear, colour.
Pads with a sound source can be given one of eight colours (Green, Turquoise, Blue, Violet, Pink, Red, Orange, Yellow) via right-click → Colour, e.g. to mark groups of sounds. The colour shows as a bar along the bottom edge of the pad; while the pad plays, the whole tile fills with it, and the sample waveform uses it too. Copy/Paste carries the colour; Clear Slot and Reset to Factory Default set it back to green. Colours are saved in the project and in kits (kit format version 6; older kits load all green). A latched pad shows a light dot with a dark ring in its top right corner.
An empty pad is silent; click it to choose a sound source: Synth, or Sample.
Samples (WAV, AIFF, MP3, FLAC, up to 60 s, played in mono) live in a folder named like the kit,
next to the `.dgkit` file (`Kits\Dub.dgkit` → `Kits\Dub\`). "Add File…" copies a file into that
folder; exporting to a new kit copies the used samples along. Samples need a kit file, so export
a new kit once before adding samples. A missing sample mutes its pad (shown in orange) and shows a notice;
the pad keeps its sample and plays again once the file is back (reopen the set or reload the kit).

Sample pads have a vintage time-stretch in the style of the Akai S950: the sample is cut into short grains that are repeated or skipped and joined with short crossfades. Stretch can be Off, All (everything between start and end) or Loop (the beginning plays normally, only the loop region is stretched; needs Loop on). Factor sets the duration (50-800 %, the pitch stays the same; Tune and the performance pitch only change the pitch), Grain sets the grain size (5-100 ms): short grains sound metallic and rough, long ones softer with audible repeats. With Stretch on and Loop on, the Loop X-Fade has no effect.

## Validation

`build.ps1 validate` builds Steinberg's VST3 validator and checks the plugin. For pluginval, download
`pluginval_Windows.zip` from https://github.com/Tracktion/pluginval/releases and place
`pluginval.exe` in `tools\bin\`. On macOS the same checks run in the `macos` job of the release workflow.

## Releases

Pushing a tag `v*` (e.g. `v0.1.0`) runs the GitHub Actions workflow `.github/workflows/release.yml`.
Two jobs run in parallel, `windows` and `macos`; each builds, runs the tests, the Steinberg validator and pluginval
(strictness 5) and uploads its zip as a build artifact. A third job, `publish`, attaches the zips of all successful
jobs to the GitHub release for that tag. If the macOS job fails, the Windows release is still published (without
the macOS zip). The workflow can also be started manually (Actions tab → "Build and release") to get the zips as
build artifacts without publishing a release.

## License

Dubgefahren is licensed under the GNU Affero General Public License v3.0 — see [LICENSE](LICENSE).

Third-party components:

- [JUCE](https://juce.com) 8 — used under the AGPLv3.
- [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) (Steinberg Media Technologies) — MIT license; included in the plugin
  via JUCE's VST3 wrapper and used to build the validator tool. VST is a registered trademark of Steinberg Media Technologies GmbH.
- [Catch2](https://github.com/catchorg/Catch2) — Boost Software License 1.0; used for tests only, not part of the plugin.
