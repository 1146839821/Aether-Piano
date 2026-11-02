"""Compare real-time C4 waveguide renders with the P01 modal reference."""

from __future__ import annotations

import csv
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.io import wavfile
from scipy.signal import correlate

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.analyzer.reference_solver.run_experiments import (  # noqa: E402
    FS,
    integrate,
    load_parameters,
    render_anti_aliased,
)

OUT = ROOT / "experiments" / "P02"
VELOCITIES = (24, 80, 120)
TARGET_F0 = 261.625565


def float_audio(path: Path) -> tuple[int, np.ndarray]:
    rate, data = wavfile.read(path)
    signal = np.asarray(data, dtype=np.float64)
    if signal.ndim == 2:
        signal = np.mean(signal, axis=1)
    if not np.all(np.isfinite(signal)):
        raise ValueError(f"non-finite sample in {path}")
    return int(rate), signal


def peak_frequency(signal: np.ndarray, rate: int, expected: float) -> float:
    start, stop = int(0.15 * rate), min(signal.size, int(0.65 * rate))
    frame = signal[start:stop]
    frame = frame - np.mean(frame)
    window = np.hanning(frame.size)
    nfft = 1 << max(19, int(math.ceil(math.log2(frame.size * 16))))
    spectrum = np.abs(np.fft.rfft(frame * window, n=nfft))
    frequencies = np.fft.rfftfreq(nfft, 1.0 / rate)
    band = np.flatnonzero((frequencies >= expected * 0.985) & (frequencies <= expected * 1.015))
    index = int(band[np.argmax(spectrum[band])])
    if index <= 0 or index >= spectrum.size - 1:
        return float(frequencies[index])
    left, center, right = np.log(np.maximum(spectrum[index - 1:index + 2], 1.0e-300))
    delta = 0.5 * (left - right) / (left - 2.0 * center + right)
    return float((index + delta) * rate / nfft)


def spectral_centroid(signal: np.ndarray, rate: int, begin: float, end: float) -> float:
    frame = signal[int(begin * rate):int(end * rate)]
    windowed = frame * np.hanning(frame.size)
    spectrum = np.abs(np.fft.rfft(windowed))
    frequencies = np.fft.rfftfreq(frame.size, 1.0 / rate)
    return float(np.sum(frequencies * spectrum) / max(np.sum(spectrum), 1.0e-300))


def fit_fundamental_t60(signal: np.ndarray, rate: int, f0: float) -> tuple[float, float]:
    frame_size = int(0.40 * rate)
    hop = int(0.25 * rate)
    omega = 2.0 * np.pi * f0 / rate
    offsets = np.arange(frame_size, dtype=np.float64)
    window = np.hanning(frame_size)
    reference = window * np.exp(-1j * omega * offsets)
    times, levels = [], []
    for start in range(int(0.50 * rate), min(signal.size - frame_size, int(5.0 * rate)), hop):
        segment = signal[start:start + frame_size]
        magnitude = 2.0 * abs(np.dot(segment, reference)) / np.sum(window)
        if magnitude > 1.0e-12:
            times.append((start + frame_size / 2.0) / rate)
            levels.append(20.0 * np.log10(magnitude))
    if len(times) < 4:
        return float("nan"), float("nan")
    slope, intercept = np.polyfit(np.asarray(times), np.asarray(levels), 1)
    return float(-60.0 / slope) if slope < 0.0 else float("inf"), float(slope)


def aligned_transient_metrics(reference: np.ndarray, waveguide: np.ndarray, rate: int) -> dict[str, float]:
    count = min(reference.size, waveguide.size, int(0.15 * rate))
    ref = reference[:count] - np.mean(reference[:count])
    actual = waveguide[:count] - np.mean(waveguide[:count])
    corr = correlate(actual, ref, mode="full", method="fft")
    center = count - 1
    radius = int(0.010 * rate)
    lag = int(np.argmax(corr[center - radius:center + radius + 1]) - radius)
    ref_start = max(0, -lag)
    actual_start = max(0, lag)
    size = min(count - ref_start, count - actual_start)
    a = reference[ref_start:ref_start + size]
    b = waveguide[actual_start:actual_start + size]
    ref_rms = float(np.sqrt(np.mean(a * a)))
    error_rms = float(np.sqrt(np.mean((a - b) ** 2)))
    corr_value = float(np.corrcoef(a, b)[0, 1]) if size else float("nan")
    return {
        "best_alignment_ms": 1000.0 * lag / rate,
        "aligned_relative_rms_error": error_rms / max(ref_rms, 1.0e-30),
        "aligned_correlation": corr_value,
    }


def render_diagnostics(velocity: int) -> dict[str, float]:
    fields: dict[str, float] = {}
    path = OUT / f"offline_velocity{velocity:03d}.log"
    for token in path.read_text(encoding="utf-8").split():
        if "=" in token:
            key, value = token.split("=", 1)
            try:
                fields[key] = float(value)
            except ValueError:
                pass
    return fields


def generate_renders(executable: Path) -> None:
    jobs = []
    for velocity in VELOCITIES:
        jobs.append((f"c4_midi{velocity:03d}_string_velocity.wav", 1, 48000, velocity, 0.8,
                     f"offline_velocity{velocity:03d}.log", None))
        jobs.append((f"c4_decay_midi{velocity:03d}.wav", 6, 48000, velocity, 5.8,
                     f"decay_velocity{velocity:03d}.log", None))
    for rate in (44100, 96000):
        jobs.append((f"c4_rate{rate}.wav", 1, rate, 80, 0.8,
                     f"offline_rate{rate}.log", None))
    jobs.append(("c4_dispersion_B0.wav", 1, 48000, 80, 0.8, "offline_dispersion_B0.log", 0.0))
    for output, seconds, rate, velocity, note_off, log_name, inharmonicity in jobs:
        command = [
            str(executable), "--output", str(OUT / output), "--seconds", str(seconds),
            "--sample-rate", str(rate), "--velocity", str(velocity), "--note-off", str(note_off),
        ]
        if inharmonicity is not None:
            command.extend(("--inharmonicity", str(inharmonicity)))
        completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=True)
        (OUT / log_name).write_text(completed.stdout, encoding="utf-8")
        if completed.stderr:
            (OUT / f"{Path(log_name).stem}.stderr.log").write_text(
                completed.stderr, encoding="utf-8"
            )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--render", type=Path, help="run offline_render before analysis")
    arguments = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    if arguments.render is not None:
        generate_renders(arguments.render.resolve())
    parameters, _, _, _, tension, bending = load_parameters()
    string_parameters = parameters["string"]
    results: list[dict[str, float | int | str]] = []
    comparison_audio: dict[tuple[str, int], np.ndarray] = {}
    reference_diagnostics: dict[int, dict[str, float]] = {}

    for velocity in VELOCITIES:
        wave_rate, wave = float_audio(OUT / f"c4_midi{velocity:03d}_string_velocity.wav")
        if wave_rate != FS:
            raise ValueError(f"expected {FS} Hz waveguide WAV, got {wave_rate} Hz")
        hammer_speed = 0.3 + (velocity - 24) * (0.5 / 40.0) if velocity <= 64 else (
            0.8 + (velocity - 64) * (0.8 / 48.0) if velocity <= 112 else 1.6 + (velocity - 112) * (0.3 / 15.0)
        )
        reference_result = integrate(parameters, 96, 16, hammer_speed, tension, bending)
        ref = render_anti_aliased(
            reference_result, 16, 0.75, parameters["render"]["readout_position_fraction"]
        )
        comparison_audio[("waveguide", velocity)] = wave
        comparison_audio[("reference", velocity)] = ref
        diagnostics = render_diagnostics(velocity)

        f0 = peak_frequency(wave, wave_rate, TARGET_F0)
        partials: dict[str, float] = {}
        for harmonic in range(1, 7):
            frequency = peak_frequency(wave, wave_rate, harmonic * TARGET_F0)
            partials[f"partial_{harmonic}_hz"] = frequency
        ratios = {
            f"partial_{n}_inharmonicity": partials[f"partial_{n}_hz"] / (n * f0) - 1.0
            for n in range(2, 7)
        }
        decay_rate, decay_signal = float_audio(OUT / f"c4_decay_midi{velocity:03d}.wav")
        if decay_rate != wave_rate:
            raise ValueError("decay render sample rate differs from the feature render")
        t60, decay_slope = fit_fundamental_t60(decay_signal, decay_rate, f0)
        ref_f0 = peak_frequency(ref, FS, TARGET_F0)
        onset_metrics = aligned_transient_metrics(ref, wave, FS)
        row: dict[str, float | int | str] = {
            "midi_velocity": velocity,
            "hammer_speed_m_s": hammer_speed,
            "waveguide_f0_hz": f0,
            "waveguide_f0_error_cents": 1200.0 * math.log2(f0 / TARGET_F0),
            "reference_f0_hz": ref_f0,
            "reference_f2_inharmonicity": peak_frequency(ref, FS, 2 * TARGET_F0) / (2 * ref_f0) - 1.0,
            "waveguide_fundamental_t60_extrapolated_s": t60,
            "waveguide_fundamental_decay_slope_db_s": decay_slope,
            "waveguide_attack_centroid_hz": spectral_centroid(wave, FS, 0.005, 0.080),
            "reference_attack_centroid_hz": spectral_centroid(ref, FS, 0.005, 0.080),
            "waveguide_attack_rms_m_s": float(np.sqrt(np.mean(wave[:int(0.080 * FS)] ** 2))),
            "waveguide_late_rms_m_s": float(np.sqrt(np.mean(wave[int(0.50 * FS):int(0.70 * FS)] ** 2))),
            "waveguide_peak_m_s": float(np.max(np.abs(wave))),
            "reference_peak_force_N": float(reference_result.peak_force_N),
            "reference_impulse_N_s": float(reference_result.impulse_N_s),
            "waveguide_initial_hammer_energy_J": diagnostics["initial_hammer_energy_J"],
            "waveguide_peak_force_N": diagnostics["peak_force_N"],
            "waveguide_impulse_N_s": diagnostics["hammer_impulse_N_s"],
            "waveguide_momentum_error_N_s": abs(
                diagnostics["hammer_impulse_N_s"] - diagnostics["hammer_momentum_change_N_s"]
            ),
            "waveguide_contact_duration_ms": 1000.0 * diagnostics["contact_duration_s"],
            "waveguide_non_finite_states": int(diagnostics["non_finite_states"]),
            **partials,
            **ratios,
            **onset_metrics,
        }
        results.append(row)
        reference_diagnostics[velocity] = {
            "peak_force_N": float(reference_result.peak_force_N),
            "impulse_N_s": float(reference_result.impulse_N_s),
            "contact_duration_ms": 1000.0 * float(reference_result.release_time_s),
            "max_energy_balance_error_J": float(reference_result.max_energy_balance_error_J),
        }

    # The matching engine diagnostics are emitted by offline_render beside the
    # WAV runs and incorporated into this machine-readable result by the shell.
    fields = list(results[0])
    with (OUT / "feature_results.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(results)
    sample_rate_rows: list[dict[str, float | int]] = []
    for rate in (44100, 48000, 96000):
        if rate == 48000:
            signal = comparison_audio[("waveguide", 80)]
            log_path = OUT / "offline_velocity080.log"
        else:
            loaded_rate, signal = float_audio(OUT / f"c4_rate{rate}.wav")
            if loaded_rate != rate:
                raise ValueError(f"expected {rate} Hz rate-check WAV, got {loaded_rate} Hz")
            log_path = OUT / f"offline_rate{rate}.log"
        fields: dict[str, float] = {}
        for token in log_path.read_text(encoding="utf-8").split():
            if "=" in token:
                key, value = token.split("=", 1)
                try:
                    fields[key] = float(value)
                except ValueError:
                    pass
        measured = peak_frequency(signal, rate, TARGET_F0)
        sample_rate_rows.append({
            "sample_rate_hz": rate,
            "f0_hz": measured,
            "f0_error_cents": 1200.0 * math.log2(measured / TARGET_F0),
            "peak_m_s": float(np.max(np.abs(signal))),
            "non_finite_states": int(fields["non_finite_states"]),
        })
    with (OUT / "sample_rate_results.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(sample_rate_rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(sample_rate_rows)

    f2_target = math.sqrt((1.0 + 4.0 * parameters["string"]["inharmonicity_B"])
                          / (1.0 + parameters["string"]["inharmonicity_B"]))
    failures = []
    for row in results:
        if abs(float(row["waveguide_f0_error_cents"])) > 0.05:
            failures.append(f"velocity {row['midi_velocity']}: f0 error exceeds 0.05 cents")
        if abs(float(row["partial_2_inharmonicity"]) - (f2_target - 1.0)) > 2.0e-5:
            failures.append(f"velocity {row['midi_velocity']}: f2 dispersion exceeds tolerance")
        if abs(float(row["waveguide_fundamental_t60_extrapolated_s"]) - 210.110086) > 2.5:
            failures.append(f"velocity {row['midi_velocity']}: T60 differs by more than 2.5 s")
        if float(row["waveguide_momentum_error_N_s"]) > 1.0e-6:
            failures.append(f"velocity {row['midi_velocity']}: hammer impulse does not close")
        if int(row["waveguide_non_finite_states"]) != 0:
            failures.append(f"velocity {row['midi_velocity']}: non-finite state was observed")
        if float(row["aligned_correlation"]) < 0.90:
            failures.append(f"velocity {row['midi_velocity']}: attack correlation vs P01 is below 0.90")
    centroids = [float(row["waveguide_attack_centroid_hz"]) for row in results]
    if not (centroids[0] < centroids[1] < centroids[2]):
        failures.append("attack spectral centroid does not rise with MIDI velocity")
    for row in sample_rate_rows:
        if abs(float(row["f0_error_cents"])) > 0.05 or int(row["non_finite_states"]) != 0:
            failures.append(f"sample rate {row['sample_rate_hz']}: tuning or finite-state regression")

    stiff_signal = comparison_audio[("waveguide", 80)]
    b0_rate, b0_signal = float_audio(OUT / "c4_dispersion_B0.wav")
    if b0_rate != FS:
        raise ValueError("B=0 comparison render sample rate differs from 48 kHz")
    stiff_f0 = peak_frequency(stiff_signal, FS, TARGET_F0)
    b0_f0 = peak_frequency(b0_signal, FS, TARGET_F0)
    dispersion_rows: list[dict[str, float | int]] = []
    observed_shifts = []
    for harmonic in range(2, 7):
        b0_frequency = peak_frequency(b0_signal, FS, harmonic * TARGET_F0)
        stiff_frequency = peak_frequency(stiff_signal, FS, harmonic * TARGET_F0)
        expected = harmonic * TARGET_F0 * math.sqrt(
            (1.0 + parameters["string"]["inharmonicity_B"] * harmonic**2)
            / (1.0 + parameters["string"]["inharmonicity_B"])
        )
        shift = stiff_frequency - b0_frequency
        observed_shifts.append(shift)
        dispersion_rows.append({
            "harmonic": harmonic,
            "B0_frequency_hz": b0_frequency,
            "B_positive_frequency_hz": stiff_frequency,
            "B0_error_from_harmonic_hz": b0_frequency - harmonic * b0_f0,
            "positive_B_expected_hz": expected,
            "positive_B_error_hz": stiff_frequency - expected,
            "observed_B_shift_hz": shift,
        })
    with (OUT / "dispersion_results.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(dispersion_rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(dispersion_rows)
    if abs(stiff_f0 - TARGET_F0) > 0.01 or abs(b0_f0 - TARGET_F0) > 0.01:
        failures.append("B=0 and B>0 configurations do not preserve the tuned fundamental")
    if any(abs(float(row["B0_error_from_harmonic_hz"])) > 0.15 for row in dispersion_rows):
        failures.append("B=0 partials deviate from harmonic spacing")
    if any(abs(float(row["positive_B_error_hz"])) > 1.5 for row in dispersion_rows):
        failures.append("B>0 partial dispersion exceeds 1.5 Hz at partials 2..6")
    if any(shift <= 0.0 for shift in observed_shifts) or any(
        right <= left for left, right in zip(observed_shifts, observed_shifts[1:])
    ):
        failures.append("inharmonicity shift does not rise with partial number")
    (OUT / "reference_diagnostics.json").write_text(
        json.dumps(reference_diagnostics, indent=2) + "\n", encoding="utf-8"
    )

    fig, axes = plt.subplots(2, 1, figsize=(10, 7), constrained_layout=True)
    for kind, color in (("reference", "#597b9b"), ("waveguide", "#d17637")):
        audio = comparison_audio[(kind, 80)]
        frame = audio[: int(0.20 * FS)]
        nfft = 1 << 19
        spectrum = np.abs(np.fft.rfft(frame * np.hanning(frame.size), n=nfft))
        frequencies = np.fft.rfftfreq(nfft, 1.0 / FS)
        band = frequencies <= 6.0 * TARGET_F0
        axes[0].plot(frequencies[band], 20.0 * np.log10(np.maximum(spectrum[band], 1.0e-12)),
                     label=kind, color=color)
    axes[0].set(title="C4 velocity 80 spectrum (first 200 ms)", xlabel="Frequency (Hz)",
                ylabel="Level (dB re 1 m/s)")
    axes[0].legend()
    axes[0].grid(alpha=0.25)
    time_ms = np.arange(int(0.025 * FS)) * 1000.0 / FS
    axes[1].plot(time_ms, comparison_audio[("reference", 80)][:time_ms.size],
                 label="P01 modal reference", color="#597b9b")
    axes[1].plot(time_ms, comparison_audio[("waveguide", 80)][:time_ms.size],
                 label="P02 waveguide", color="#d17637", alpha=0.85)
    axes[1].set(title="C4 velocity 80 attack", xlabel="Time (ms)", ylabel="Point velocity (m/s)")
    axes[1].legend()
    axes[1].grid(alpha=0.25)
    fig.savefig(OUT / "feature_comparison.png", dpi=160)
    plt.close(fig)

    print(f"analyzed_velocities={','.join(map(str, VELOCITIES))}")
    print(f"features={OUT / 'feature_results.csv'}")
    print(f"reference_diagnostics={OUT / 'reference_diagnostics.json'}")
    print(f"sample_rates={OUT / 'sample_rate_results.csv'}")
    print(f"dispersion={OUT / 'dispersion_results.csv'}")
    print(f"plot={OUT / 'feature_comparison.png'}")
    for row in results:
        print(
            f"velocity={row['midi_velocity']} f0_hz={row['waveguide_f0_hz']:.6f} "
            f"f2_inharm={row['partial_2_inharmonicity']:.8f} "
            f"centroid_hz={row['waveguide_attack_centroid_hz']:.2f} "
            f"T60_extrap_s={row['waveguide_fundamental_t60_extrapolated_s']:.3f}"
        )
    for row in sample_rate_rows:
        print(f"sample_rate={row['sample_rate_hz']} f0_hz={row['f0_hz']:.6f} "
              f"cents={row['f0_error_cents']:.4f} non_finite={row['non_finite_states']}")
    for row in dispersion_rows:
        print(f"partial={row['harmonic']} B0_hz={row['B0_frequency_hz']:.4f} "
              f"B_positive_hz={row['B_positive_frequency_hz']:.4f} "
              f"expected_hz={row['positive_B_expected_hz']:.4f}")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        raise SystemExit(1)
    print("regression=PASS")


if __name__ == "__main__":
    main()
