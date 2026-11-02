# P00 Phase Report

- Phase: P00 — project foundation and buildable silent plugin
- Date: 2026-11-02
- Owner: Codex
- Commit / branch: P00 deliverable commit on `master` (this report is included in the commit; use `git rev-parse HEAD` for its exact ID).
- Environment: see [`environment.md`](environment.md); Apple M4 arm64, macOS 27.0.1, Apple Clang 21.0.0, CMake 4.4.0, Ninja, Xcode 27.2, JUCE 9.0.5 pinned.

## Delivered

- C++20 `PianoDSP` static library with `prepare`, `process`, `reset` and event sample-offset validation. It deliberately emits zero-valued stereo output; no piano acoustics are implemented in P00.
- JUCE synth plugin targets for AU, VST3, and Standalone. JUCE is pinned as a submodule at `d60b7a78030661574a615b586d9325ba7a7fa7dd`.
- `offline_render` writes stereo 48 kHz, 32-bit IEEE-float WAV silence for a requested duration.
- CTest checks zero-block and buffer behavior, zero output, repeat prepare/reset, block bounds, and valid/invalid ordered sample offsets.
- Build guide, third-party license ledger, basic CI workflow, P00 ADR, and environment/test logs.

## Commands and results

| Command / check | Result | Evidence |
|---|---|---|
| `cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64` | PASS | [`configure-debug.log`](configure-debug.log) |
| `cmake --build build/debug --clean-first --parallel 4` | PASS | [`build-debug.log`](build-debug.log) |
| `ctest --test-dir build/debug --output-on-failure` | PASS, 1/1 | [`ctest-debug.log`](ctest-debug.log) |
| `cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64` | PASS | [`configure-release.log`](configure-release.log) |
| `cmake --build build/release --clean-first --parallel 4` | PASS | [`build-release.log`](build-release.log) |
| `ctest --test-dir build/release --output-on-failure` | PASS, 1/1 | [`ctest-release.log`](ctest-release.log) |
| `build/release/offline_render --output experiments/P00/silence.wav --seconds 1` | PASS | [`offline-render.log`](offline-render.log), [`silence.wav`](silence.wav) |
| WAV binary validation | PASS: RIFF, IEEE float format 3, 2 channels, 48,000 Hz, 48,000 frames/channel, 384,044 bytes, all samples zero | [`wav-validation.log`](wav-validation.log); SHA-256 `e95a5af23ccc7c2372216dd545d332b327153809e41fd56af852446a65e64b69` |
| AU host validation: `auval -v aumu AeP0 Aeth` | PASS; AU validation succeeded, including host callbacks, supported formats, render tests at varied block sizes/sample rates, and MIDI | [`auval.log`](auval.log) |

The AU was copied to `~/Library/Audio/Plug-Ins/Components/Aether Piano.component` and AudioComponentRegistrar was restarted for discovery. The first configure attempt failed because the project declared only CXX while JUCE requires C; the project language list was corrected to C and CXX. The first VST3 build hit JUCE's VST2 replacement parameter compatibility guard; this new, unreleased product disables `JUCE_VST3_CAN_REPLACE_VST2`, then all formats built successfully. These were corrected during P00.

## Acoustic and performance results

- Acoustic response: N/A; output is intentionally silent and no physical model is present.
- Audio artifact: one second of digital silence. This is a format/renderer check, not an acoustic result.
- Performance profile, callback allocation probe, DAW load, pluginval, and Steinberg validator: NOT RUN; P00 has no DSP workload and no DAW/pluginval/Steinberg Validator installation. AU render and host callback checks did run through `auval`.

## Risks and phase gate

- JUCE dual-license terms require project-specific review before redistribution; see `THIRD_PARTY_LICENSES.md`.
- The JUCE process adapter clears audio and currently ignores MIDI events. The sample-offset function validates event ordering/bounds but does not schedule or render events.
- `xcrun --show-sdk-version` did not resolve the selected Command Line Tools SDK path even though CMake/Xcode plugin builds succeeded; repair SDK selection before depending on SDK-specific automation.
- The repository began with project documents untracked and no prior commit; the P00 deliverable commit establishes the initial tracked baseline.
- Windows build commands and CI were authored but not executed on this macOS host.

**Phase gate: PASS.** Debug and Release builds, CTest, WAV format validation, and Apple's AU validation all pass. P01 may begin. Host validation covers AU only; VST3 host scanning, Windows builds, real-time profiling, and acoustic behavior remain future phase work.
