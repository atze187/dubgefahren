# macOS Build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship an experimental, ad-hoc signed, universal macOS VST3 as part of the normal release, without touching Windows behaviour.

**Architecture:** `CMakePresets.json` becomes the shared build description for local and CI; `build.ps1` delegates to it. `release.yml` splits into `windows`, `macos` (both only build and upload artifacts) and a `publish` job that releases whatever succeeded.

**Tech Stack:** CMake ≥ 3.25 presets (version 6), Ninja on macOS, JUCE 8.0.15, Catch2, GitHub Actions (`macos-latest`), pluginval 1.0.4, `codesign`, `ditto`.

**Spec:** `docs/superpowers/specs/2026-10-26-macos-build-design.md`

## Global Constraints

- Format VST3 only (no AU); universal `arm64;x86_64`; `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`.
- No Mac is available: macOS work is only verified through GitHub Actions runs on the feature branch (`workflow_dispatch`); `publish` must never release outside tag pushes.
- Windows build output stays at `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3`; `build.ps1` commands (`configure/build/test/install/validate/all`, `-Config Debug|Release`) stay unchanged.
- The macOS zip is only produced if tests, Steinberg validator and pluginval (strictness 5) pass; a failing `macos` job must not block the Windows release.
- Zip name `Dubgefahren-vX.Y.Z-macos.zip` (non-tag runs: `dev-<sha7>`), contents `Dubgefahren.vst3`, `README.md`, `LICENSE`; created with `ditto -c -k --keepParent`, bundle signed with `codesign --force --deep -s -`.
- Config syntax stays `%NAME%`; macOS install default `~/Library/Audio/Plug-Ins/VST3`.
- README, wiki, PR text in English; commit messages end with the attribution line from the session reminder.

## Review Focus

- Windows regression: `build.ps1 all` and `validate` produce the same bundle at the same path as before (Task 1 verification).
- Config folder given as `%VAR%/sub` must expand on POSIX paths too, with a platform-neutral test that sets its own variable (Task 2).
- Universal binary really contains both architectures (`lipo -archs` check in Task 3).
- Zipped bundle keeps a valid signature after unzip (`codesign --verify --deep --strict` on the unzipped copy in Task 3).
- Mac job red must still let `publish` run for a successful Windows job (observed in a dispatch run, Task 3).

---

### Task 1: CMake presets and `build.ps1` on presets

**Files:**
- Create: `CMakePresets.json`, `tools/vst3validator/CMakePresets.json`
- Modify: `build.ps1`, `.gitignore` (add `build-macos/`)

**Interfaces:**
- Produces: configure presets `windows-x64` (binaryDir `${sourceDir}/build`, `architecture` x64/set, no generator so the installed Visual Studio is picked as today) and `macos-universal` (Ninja, binaryDir `${sourceDir}/build-macos`, cache `CMAKE_BUILD_TYPE=Release`, `CMAKE_OSX_ARCHITECTURES=arm64;x86_64`, `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`); each with a `condition` on `${hostSystemName}` (`Windows` / `Darwin`). Build and test presets `windows-release`, `windows-debug` (configuration Release/Debug) and `macos-release`; test presets set `output.outputOnFailure`. In `tools/vst3validator`: configure presets `validator-windows` (binaryDir `${sourceDir}/../../build/vst3validator`, architecture x64) and `validator-macos` (Ninja, binaryDir `${sourceDir}/../../build-macos/vst3validator`, Release), build presets of the same names with target `validator`.

- [ ] **Step 1: Write both preset files** (`"version": 6`, `cmakeMinimumRequired` 3.25.0) with exactly the presets above.
- [ ] **Step 2: Rewrite `Do-Configure`, `Do-Build`, `Do-Test`, `Do-Validate` in `build.ps1`** to call `cmake --preset windows-x64`, `cmake --build --preset windows-<config lower-case> --parallel`, `ctest --preset windows-<config lower-case>`, and for the validator `Push-Location tools\vst3validator` + `cmake --preset validator-windows` / `cmake --build --preset validator-windows`. Drop the `-A x64` arguments; keep `Find-CMake`, `Do-Install`, the bundle path and the pluginval block as they are.
- [ ] **Step 3: Verify Windows is unchanged.** Run `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 all` in a fresh checkout state (delete `build\` first). Expected: configure, build and all tests pass, and `build\plugin\Dubgefahren_artefacts\Release\VST3\Dubgefahren.vst3` exists.
- [ ] **Step 4: Run `build.ps1 validate`.** Expected: Steinberg validator passes (pluginval too when `tools\bin\pluginval.exe` exists).
- [ ] **Step 5: Commit** `build: add CMake presets and build windows through them`.

### Task 2: Platform-neutral tests, install path, config notes

**Files:**
- Modify: `tests/plugin/test_Config.cpp`, `plugin/CMakeLists.txt` (`DG_VST3_INSTALL_DIR` default), `plugin/Config.cpp` (comment in `pluginConfigFile()`), `resources/Dubgefahren.config.json` (`_note`)
- Test: `tests/plugin/test_Config.cpp`

**Interfaces:**
- Consumes: `expandEnvironmentVariables(const juce::String&)`, `resolveKitFolder(const juce::File&, const juce::File&)` (unchanged).
- Produces: test helper `setTestEnv(const char* name, const juce::String& value)` in the anonymous namespace (`_putenv_s` on Windows, `setenv(..., 1)` elsewhere).

- [ ] **Step 1: Rewrite the test `environment variables are expanded`** so it no longer depends on `TEMP`: it sets `DG_TEST_DIR` to `tmp.dir`'s parent path via `setTestEnv`, then checks `expandEnvironmentVariables("%DG_TEST_DIR%" + sep + "x") == value + sep + "x"` with `sep = juce::File::getSeparatorString()`, still checks the unknown-variable, `a%%b` and no-percent cases (use `sep` instead of `"\\"`), and replaces the conditional block by an unconditional config check: `writeConfig(cfg, "%DG_TEST_DIR%" + sep + tmp.dir.getFileName())` resolves to `tmp.dir` with an empty warning.
- [ ] **Step 2: Run** `build.ps1 test`. Expected: PASS (the test now exercises the same path on Windows and macOS).
- [ ] **Step 3: Set the `DG_VST3_INSTALL_DIR` default** in `plugin/CMakeLists.txt` to `$ENV{HOME}/Library/Audio/Plug-Ins/VST3` when `APPLE`, else the current `$ENV{CommonProgramFiles}/VST3`.
- [ ] **Step 4: Update comments/notes.** `pluginConfigFile()` comment names both layouts (`Contents/x86_64-win/<Name>.vst3` and `Contents/MacOS/<Name>`); `_note` in `resources/Dubgefahren.config.json` gives a Windows example (`%USERPROFILE%`) and a macOS example (`%HOME%/Music/Dubgefahren/Kits`) and states the default `Documents/Dubgefahren/Kits`.
- [ ] **Step 5: Run `build.ps1 all`.** Expected: all tests pass.
- [ ] **Step 6: Commit** `feat: make config tests and install path platform-neutral`.

### Task 3: `release.yml` with `windows`, `macos`, `publish`

**Files:**
- Modify: `.github/workflows/release.yml`

**Interfaces:**
- Consumes: presets from Task 1 (`macos-universal`, `macos-release`, `validator-macos`).
- Produces: artifacts `Dubgefahren-<version>-win64` and `Dubgefahren-<version>-macos` (each one zip); `publish` job releasing `dist/*.zip`.

- [ ] **Step 1: Edit job `windows`**: use the presets through `build.ps1` as today, keep all steps, delete the `Publish GitHub release` step (it moves to `publish`).
- [ ] **Step 2: Add job `macos`** (`runs-on: macos-latest`, `timeout-minutes: 90`) with these steps: checkout; configure/build/test with `cmake --preset macos-universal`, `cmake --build --preset macos-release --parallel`, `ctest --preset macos-release` (install Ninja with `brew install ninja` only if `command -v ninja` fails); `lipo -archs` on `build-macos/plugin/Dubgefahren_artefacts/Release/VST3/Dubgefahren.vst3/Contents/MacOS/Dubgefahren` must print both `x86_64` and `arm64` (fail otherwise); build the Steinberg validator with `validator-macos` presets and run it on the bundle; download `pluginval_macOS.zip` from pluginval v1.0.4 and run `pluginval.app/Contents/MacOS/pluginval --strictness-level 5 --validate-in-process --validate <bundle>`; `codesign --force --deep -s - <bundle>`; package with `ditto -c -k --keepParent` into `Dubgefahren-<version>-macos.zip` together with README and LICENSE (staging folder like the Windows job); unzip the result into a fresh temp folder and run `codesign --verify --deep --strict` on the unzipped bundle; upload the artifact.
- [ ] **Step 3: Add job `publish`** (`needs: [windows, macos]`, `if: always() && needs.windows.result == 'success' && github.ref_type == 'tag'` for the release steps only; the job itself uses `if: always() && needs.windows.result == 'success'` so a dispatch run still proves the condition): `actions/download-artifact` (latest major, check with `gh api repos/actions/download-artifact/releases/latest`) with `pattern: Dubgefahren-*`, `merge-multiple: true`, `path: dist`; a step writing `notes.md` with the experimental/xattr warning (spec §6 text) only when a `dist/*macos.zip` exists; `softprops/action-gh-release@v3` with `files: dist/*.zip`, `body_path: notes.md` (only when the file exists), `generate_release_notes: true`, restricted by `if: github.ref_type == 'tag'`.
- [ ] **Step 4: Push the branch and start the workflow** (`gh workflow run release.yml --ref feature/macos-build`). Ask the user before pushing. Iterate on build/test/validator failures from the log until the `macos` job is green; fix portability problems in code and tests in the same branch, one commit per fix.
- [ ] **Step 5: Verify the failure path.** In the first run where `macos` is red (or by temporarily adding `exit 1` to a macOS step in a throwaway commit), confirm `publish` still starts and ends green while `macos` is red; remove the throwaway commit afterwards.
- [ ] **Step 6: Verify the green path.** Expected: `windows`, `macos`, `publish` green; the `Dubgefahren-dev-<sha7>-macos` artifact exists and `publish` created no release (not a tag).
- [ ] **Step 7: Commit** `ci: build macOS universal VST3 and publish both zips in one job`.

### Task 4: Documentation

**Files:**
- Modify: `README.md`; wiki (clone `https://github.com/atze187/dubgefahren.wiki.git` into the scratchpad directory, never into the repo)

- [ ] **Step 1: README.** Platform line (“VST3 instrument for Windows and (experimental) macOS”), download section for `Dubgefahren-vX.Y.Z-macos.zip` with copy target `~/Library/Audio/Plug-Ins/VST3`, `xattr -cr Dubgefahren.vst3`, “untested, not notarized”; requirements for building on macOS; build/test commands with the presets (`cmake --preset windows-x64` / `macos-universal`); config example for `%HOME%`; validation and release sections mention the second zip and the `publish` job.
- [ ] **Step 2: Wiki.** Add macOS section (installation, `xattr -cr`, limitations: untested, not notarized, no AU) in the page that holds installation/configuration (`Kits-and-Configuration` or `Home`), and update the platform statement on `Home`.
- [ ] **Step 3: Commit** `docs: describe the macOS build and presets`; commit and push the wiki after the PR is merged (wiki is public immediately).

### Task 5: Pull request

- [ ] **Step 1: Run `build.ps1 all` and `build.ps1 validate` once more** and confirm the latest `release.yml` dispatch run on the branch is green.
- [ ] **Step 2: Zip the VST3 folder** as `dubgefahren.vst3.feature-macos-build.zip` into `C:\Users\atzet\OneDrive\Dokumente\musik\dubgefahren` (standing rule after green tests).
- [ ] **Step 3: Open the PR** to `main` in English; list deferred points (notarization, AU, real-Mac feedback, pinning `macos-latest`); end with the attribution line from the session reminder. Release (version bump, tag) is a separate step after merge.
