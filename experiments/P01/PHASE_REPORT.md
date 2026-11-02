# P01 Phase Report — Offline Hammer/String Reference

- Phase: P01 — offline physical reference model
- Date: 2026-11-02
- Owner: Codex
- Commit / branch: P01 deliverable commit on `master`; use `git rev-parse HEAD` for the exact ID.
- Environment: Apple M4 arm64, macOS 27.0.1, Python 3.14.6, NumPy 2.5.4, SciPy 1.18.1, Matplotlib 3.11.2; see [`environment.md`](environment.md).

## Delivered

- Offline modal stiff-string solver with a lumped nonlinear hammer, DOP853 integration, first-separation event, dense output and analytic damped free-string continuation. Code: `tools/analyzer/reference_solver/`.
- SI/sign, boundary, energy, dispersion and output definitions: [`model_assumptions.md`](model_assumptions.md) and [`hammer_string_reference_interface.md`](hammer_string_reference_interface.md).
- Literature review and source notes: [`literature_notes.md`](literature_notes.md). All modeled parameters and the MIDI-to-hammer-speed map are explicitly classified as `synthetic_research`.
- Three unnormalized 48 kHz float WAV files, contact-force CSV, contact/convergence plots, spectrogram, and machine-readable convergence results.
- Six Python tests and a CTest wrapper. No plugin or real-time DSP code changed.

## Commands and results

| Command / check | Result | Evidence |
|---|---|---|
| `uv pip install --python .venv/bin/python -r requirements-p01.txt` | PASS | Environment pins in `requirements-p01.txt`; versions recorded in [`environment.md`](environment.md) |
| `.venv/bin/python -m unittest discover -s tests -p test_reference_solver.py` | PASS, 6 tests | [`python-tests.log`](python-tests.log) |
| `cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 -DPython3_EXECUTABLE="$PWD/.venv/bin/python"` | PASS | [`configure-debug.log`](configure-debug.log) |
| `cmake --build build/debug --parallel 4` | PASS; Ninja reported no pending compile work | [`build-debug.log`](build-debug.log) |
| `ctest --test-dir build/debug --output-on-failure` | PASS, 2/2 | [`ctest-debug.log`](ctest-debug.log) |
| Release configure/build with the same options and `CMAKE_BUILD_TYPE=Release` | PASS; no pending compile work | [`configure-release.log`](configure-release.log), [`build-release.log`](build-release.log) |
| `ctest --test-dir build/release --output-on-failure` | PASS, 2/2 | [`ctest-release.log`](ctest-release.log) |
| `.venv/bin/python tools/analyzer/reference_solver/run_experiments.py` | PASS | [`solver-run.log`](solver-run.log) |
| WAV structure, finite samples and checksums | PASS | [`wav-validation.log`](wav-validation.log) |

During development, an early energy-diagnostic smoke run exposed a mode/time-array broadcasting error, which was corrected. An early unit run also exposed an absolute readout coordinate outside the 0.65 m string; the readout was changed to the declared fraction `0.92 L`. A subsequent test incorrectly required a spectral estimator's tiny target-frequency bias to decrease monotonically; the convergence test now compares each estimate to the finest-resolution estimate and applies an explicit 0.01 Hz target tolerance. Direct script execution initially lacked the repository root on `sys.path`; the runner now inserts its own root. All final commands above were rerun after these fixes.

## Acoustic and numerical results

Synthetic C4 uses MIDI 60, target `f1=261.625565 Hz`, `L=0.65 m`, `mu=0.0062 kg/m`, `B=0.0003`, derived `T=716.982381 N`, and `EI=0.009207818 N m²`. At the same tension the ideal-string fundamental is `261.586330 Hz`; the stiff-string fundamental is tuned to the target. The predicted ratio `f2/(2 f1)-1` is `0.0004497639`; the finest rendered proxy estimates `f1=261.625550 Hz` and ratio `0.0004498166`. There is no real-piano measurement in this repository, so measured-reference error is N/A.

For MIDI velocity 64 mapped to synthetic hammer speed `0.8 m/s`, initial hammer energy is `0.00256 J`. At 96 modes and `1.302083 us` maximum step: contact lasts `3.412524 ms`, dense-output peak force is `5.873124 N`, impulse and hammer momentum change are each `0.0112891703 N s`, and their difference is `6.77e-17 N s`. Maximum energy-balance residual is `5.03e-14 J`.

| Modes / oversample | Max step (us) | Peak force (N) | Contact (ms) | Estimated f1 (Hz) | Δf1 vs. finest (Hz) | Waveform relative RMS error vs. finest | Energy residual (J) |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 24 / 4 | 5.2083 | 5.948167 | 3.393854 | 261.625550 | 1.94e-8 | 7.7583% | 8.55e-13 |
| 48 / 8 | 2.6042 | 5.889315 | 3.408053 | 261.625550 | 4.86e-9 | 1.6085% | 2.03e-13 |
| 96 / 16 | 1.3021 | 5.873124 | 3.412524 | 261.625550 | 0 | 0% (reference) | 5.03e-14 |

The velocity table is MIDI 24 → 0.3 m/s, 64 → 0.8 m/s, 112 → 1.6 m/s. Each WAV contains transverse string point velocity at `0.92 L` in m/s; these files are mechanical reference signals, not acoustic-pressure recordings. No per-file normalization was applied. Artifacts: [`c4_midi024_string_velocity.wav`](c4_midi024_string_velocity.wav), [`c4_midi064_string_velocity.wav`](c4_midi064_string_velocity.wav), [`c4_midi112_string_velocity.wav`](c4_midi112_string_velocity.wav), [`c4_velocity64_contact_force.csv`](c4_velocity64_contact_force.csv), [`contact_force.png`](contact_force.png), [`convergence.png`](convergence.png), [`c4_velocity64_spectrogram.png`](c4_velocity64_spectrogram.png).

## RT constraints, approximations, and gate

Real-time allocation, lock and CPU profiling are not applicable to this offline Python solver; the plugin callback was not changed. Plugin validators were not rerun because P01 adds no plugin code; P00's AU and MuseScore VST3 host checks remain separately documented. Main approximations are the truncated sine basis, first-contact-only hammer action, memoryless felt power law, constant synthetic modal damping, and absence of bridge, soundboard, acoustic radiation, measured piano parameters and real MIDI action calibration.

**Phase gate: PASS.** The 24/48/96-mode, 4x/8x/16x study shows decreasing force, contact-time and waveform error; the fundamental and inharmonicity estimates remain within the stated target tolerance. Energy/momentum checks, finite-state/extreme-input tests, Python tests and Debug/Release CTest pass. P02 may begin after this deliverable is reviewed/committed.
