"""Generate the P01 C4 reference audio, contact trace, plots, and convergence table."""

from __future__ import annotations

import csv
import json
import math
from pathlib import Path
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly

ROOT = Path(__file__).resolve().parents[3]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.analyzer.reference_solver.solver import (
    Hammer,
    String,
    modal_velocity_at,
    simulate_contact,
)


OUT = ROOT / "experiments" / "P01"
PARAMS_PATH = OUT / "reference_parameters.json"
FS = 48_000


def load_parameters() -> tuple[dict, float, float, float, float, float]:
    data = json.loads(PARAMS_PATH.read_text(encoding="utf-8"))
    target_f0 = data["target_f0_hz"]
    length = data["string"]["length_m"]
    density = data["string"]["linear_density_kg_m"]
    B = data["string"]["inharmonicity_B"]
    calculated_tension = density * (2.0 * length * target_f0) ** 2 / (1.0 + B)
    tension = data["string"]["tension_N"]
    bending = data["string"]["bending_stiffness_N_m2"]
    if not math.isclose(tension, calculated_tension, rel_tol=1e-12):
        raise ValueError("stored tension_N does not satisfy its declared tuning rule")
    calculated_bending = B * tension * length**2 / math.pi**2
    if not math.isclose(bending, calculated_bending, rel_tol=1e-12):
        raise ValueError("stored bending stiffness does not match the declared B")
    if data["integrator"]["audio_sample_rate_hz"] != FS:
        raise ValueError("script sample rate differs from reference_parameters.json")
    return data, target_f0, length, density, tension, bending


def make_string(params: dict, modes: int, tension: float, bending: float) -> String:
    values = params["string"]
    return String(
        length_m=values["length_m"],
        linear_density_kg_m=values["linear_density_kg_m"],
        tension_N=tension,
        bending_stiffness_N_m2=bending,
        modes=modes,
        damping_ratio=values["modal_damping_ratio"],
    )


def make_hammer(params: dict, velocity_m_s: float) -> Hammer:
    values = params["hammer"]
    return Hammer(
        mass_kg=values["mass_kg"],
        stiffness_N_m_p=values["contact_stiffness_N_m_p"],
        exponent_p=values["contact_exponent_p"],
        strike_position_m=params["string"]["length_m"] * values["strike_position_fraction"],
        initial_velocity_m_s=velocity_m_s,
    )


def integrate(params: dict, modes: int, oversample: int, velocity: float, tension: float, bending: float):
    string = make_string(params, modes, tension, bending)
    hammer = make_hammer(params, velocity)
    result = simulate_contact(
        string,
        hammer,
        max_step_s=1.0 / (FS * oversample),
        rtol=params["integrator"]["relative_tolerance"],
        atol=params["integrator"]["absolute_tolerance"],
        max_contact_s=params["integrator"]["max_contact_duration_s"],
    )
    if not result.had_contact:
        raise RuntimeError("positive mapped velocity unexpectedly produced no contact")
    return result


def render_point_velocity(result, sample_rate: int, duration_s: float, position_m: float) -> np.ndarray:
    """Sample the continuous mechanical velocity, with bounded working memory."""
    count = int(round(sample_rate * duration_s))
    values = np.empty(count, dtype=np.float64)
    block = 4096
    for start in range(0, count, block):
        stop = min(count, start + block)
        times = np.arange(start, stop, dtype=np.float64) / sample_rate
        states = result.state_at(times)
        values[start:stop] = modal_velocity_at(result.string, states, position_m)
    return values


def render_anti_aliased(
    result, oversample: int, duration_s: float, readout_position_fraction: float
) -> np.ndarray:
    high_rate = FS * oversample
    readout_m = readout_position_fraction * result.string.length_m
    raw = render_point_velocity(result, high_rate, duration_s, readout_m)
    return resample_poly(raw, up=1, down=oversample, window=("kaiser", 8.6))


def estimate_peak_hz(signal: np.ndarray, sample_rate: int, low_hz: float, high_hz: float) -> float:
    segment = np.asarray(signal, dtype=np.float64)
    segment = segment - np.mean(segment)
    window = np.hanning(segment.size)
    nfft = 1 << max(18, int(math.ceil(math.log2(segment.size * 8))))
    spectrum = np.abs(np.fft.rfft(segment * window, n=nfft))
    frequencies = np.fft.rfftfreq(nfft, 1.0 / sample_rate)
    band = np.flatnonzero((frequencies >= low_hz) & (frequencies <= high_hz))
    idx = int(band[np.argmax(spectrum[band])])
    if idx <= 0 or idx >= spectrum.size - 1:
        return float(frequencies[idx])
    a, b, c = np.log(np.maximum(spectrum[idx - 1 : idx + 2], np.finfo(float).tiny))
    offset = 0.5 * (a - c) / (a - 2.0 * b + c)
    return float((idx + offset) * sample_rate / nfft)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    params, target_f0, length, density, tension, bending = load_parameters()
    B = params["string"]["inharmonicity_B"]
    velocity_map = params["midi_velocity_to_hammer_speed_m_s"]
    params["rendered_artifacts"] = []
    resolution_levels = tuple(
        tuple(level)
        for level in params["integrator"]["resolution_levels_modes_oversample"]
    )
    modes_for_audio = params["string"]["modes_used_for_audio"]
    readout_fraction = params["render"]["readout_position_fraction"]
    duration_s = params["render"]["duration_s"]
    medium_speed = velocity_map["64"]
    convergence_results = []
    convergence_audio = []

    for modes, oversample in resolution_levels:
        result = integrate(params, modes, oversample, medium_speed, tension, bending)
        audio = render_anti_aliased(result, oversample, 0.5, readout_fraction)
        f1 = estimate_peak_hz(audio, FS, target_f0 - 12.0, target_f0 + 12.0)
        f2 = estimate_peak_hz(audio, FS, 2.0 * target_f0 - 12.0, 2.0 * target_f0 + 12.0)
        convergence_results.append((modes, oversample, result, audio, f1, f2))
        convergence_audio.append(audio)

    finest = convergence_audio[-1]
    finest_f1 = convergence_results[-1][4]
    rows = []
    for modes, oversample, result, audio, f1, f2 in convergence_results:
        shared = min(audio.size, finest.size)
        reference_rms = math.sqrt(float(np.mean(finest[:shared] ** 2)))
        difference_rms = math.sqrt(float(np.mean((audio[:shared] - finest[:shared]) ** 2)))
        rows.append(
            {
                "modes": modes,
                "oversample": oversample,
                "max_step_us": 1.0e6 / (FS * oversample),
                "contact_ms": 1.0e3 * result.release_time_s,
                "peak_force_N": result.peak_force_N,
                "impulse_N_s": result.impulse_N_s,
                "hammer_momentum_change_N_s": result.hammer_momentum_change_N_s,
                "impulse_momentum_error_N_s": result.impulse_momentum_error_N_s,
                "f1_estimate_hz": f1,
                "f1_delta_vs_finest_hz": f1 - finest_f1,
                "f1_error_cents": 1200.0 * math.log2(f1 / target_f0),
                "inharmonicity_from_f2": f2 / (2.0 * f1) - 1.0,
                "waveform_relative_rms_error_vs_finest": difference_rms / max(reference_rms, 1e-30),
                "energy_balance_error_J": result.max_energy_balance_error_J,
            }
        )

    with (OUT / "convergence.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    (OUT / "convergence.json").write_text(
        json.dumps(rows, indent=2) + "\n", encoding="utf-8"
    )

    fine_result = convergence_results[-1][2]
    contact_times = np.arange(
        0.0,
        fine_result.release_time_s + 0.25 / (FS * resolution_levels[-1][1]),
        1.0 / (FS * resolution_levels[-1][1]),
    )
    contact_forces = fine_result.force_at(contact_times)
    np.savetxt(
        OUT / "c4_velocity64_contact_force.csv",
        np.column_stack((contact_times, contact_forces)),
        delimiter=",",
        header="time_s,contact_force_N",
        comments="",
    )
    plt.figure(figsize=(8.5, 4.5))
    plt.plot(contact_times * 1e3, contact_forces, color="#1c5b8f", linewidth=1.5)
    plt.xlabel("Time (ms)")
    plt.ylabel("Hammer contact force (N)")
    plt.title("C4, MIDI velocity 64: nonlinear hammer/string contact")
    plt.grid(True, alpha=0.25)
    plt.tight_layout()
    plt.savefig(OUT / "contact_force.png", dpi=160)
    plt.close()

    for midi_velocity, speed in velocity_map.items():
        audio_oversample = resolution_levels[-1][1]
        result = integrate(params, modes_for_audio, audio_oversample, speed, tension, bending)
        audio = render_anti_aliased(result, audio_oversample, duration_s, readout_fraction)
        audio = audio[: int(FS * duration_s)].astype(np.float32)
        filename = f"c4_midi{int(midi_velocity):03d}_string_velocity.wav"
        wavfile.write(OUT / filename, FS, audio)
        params["rendered_artifacts"].append(
            {
                "file": filename,
                "midi_velocity": int(midi_velocity),
                "initial_hammer_speed_m_s": speed,
                "quantity": "string transverse point velocity",
                "units": "m/s",
                "sample_rate_hz": FS,
                "peak_absolute_value_m_s": float(np.max(np.abs(audio))),
                "normalization": "none",
            }
        )
        if int(midi_velocity) == 64:
            spectrogram_signal = audio.astype(np.float64)
            nperseg = 4096
            hop = 1024
            frames = np.lib.stride_tricks.sliding_window_view(spectrogram_signal, nperseg)[::hop]
            window = np.hanning(nperseg)
            stft = np.fft.rfft(frames * window[None, :], axis=1)
            magnitude = np.abs(stft).T / np.sum(window)
            frequencies = np.fft.rfftfreq(nperseg, 1.0 / FS)
            times = (np.arange(frames.shape[0]) * hop + nperseg / 2) / FS
            db = 20.0 * np.log10(np.maximum(magnitude, 1e-12))
            plt.figure(figsize=(9.0, 5.0))
            mesh = plt.pcolormesh(times, frequencies, db, shading="auto", cmap="magma", vmin=-120, vmax=-30)
            plt.ylim(0, 6000)
            plt.xlabel("Time (s)")
            plt.ylabel("Frequency (Hz)")
            plt.title("C4 string velocity spectrogram (dB re 1 m/s; not acoustic SPL)")
            plt.colorbar(mesh, label="dB re 1 m/s")
            plt.tight_layout()
            plt.savefig(OUT / "c4_velocity64_spectrogram.png", dpi=160)
            plt.close()

    PARAMS_PATH.write_text(json.dumps(params, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    plt.figure(figsize=(8.5, 4.5))
    levels = [f"M={row['modes']}, dt={row['max_step_us']:.2f} µs" for row in rows]
    plt.plot(levels, [row["peak_force_N"] for row in rows], "o-", label="Peak force (N)")
    plt.ylabel("Peak force (N)")
    plt.xlabel("Modal/time resolution")
    plt.title("P01 convergence: peak contact force")
    plt.grid(True, alpha=0.25)
    plt.tight_layout()
    plt.savefig(OUT / "convergence.png", dpi=160)
    plt.close()

    print(f"target_f0_hz={target_f0:.9f}")
    print(f"ideal_string_f0_same_tension_hz={math.sqrt(tension / density) / (2.0 * length):.9f}")
    print(f"stiff_string_f1_hz={fine_result.string.angular_frequencies()[0] / (2.0 * math.pi):.9f}")
    print(f"inharmonicity_B={B:.9g}")
    print(f"theoretical_f2_over_2f1_minus_1={math.sqrt((1.0 + 4.0 * B) / (1.0 + B)) - 1.0:.9g}")
    print(f"fine_release_ms={fine_result.release_time_s * 1e3:.9f}")
    print(f"fine_peak_force_N={fine_result.peak_force_N:.9f}")
    print(f"fine_impulse_N_s={fine_result.impulse_N_s:.9g}")
    print(f"fine_hammer_momentum_change_N_s={fine_result.hammer_momentum_change_N_s:.9g}")
    print(f"fine_impulse_momentum_error_N_s={fine_result.impulse_momentum_error_N_s:.9g}")
    print(f"fine_initial_energy_J={fine_result.initial_energy_J:.9g}")
    print(f"fine_energy_balance_error_J={fine_result.max_energy_balance_error_J:.9g}")
    print("convergence_rows=")
    for row in rows:
        print(json.dumps(row, sort_keys=True))
    print("artifacts=" + ",".join(sorted(p.name for p in OUT.iterdir() if p.is_file())))


if __name__ == "__main__":
    main()
