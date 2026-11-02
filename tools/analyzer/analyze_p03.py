"""Render and measure all 88 estimated keys plus P03 MIDI performance scenarios."""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path
import subprocess
import tempfile

import numpy as np
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "experiments" / "P03"
FIRST_NOTE = 21
LAST_NOTE = 108
A4_HZ = 440.0


def midi_frequency(note: int) -> float:
    return A4_HZ * 2.0 ** ((note - 69) / 12.0)


def note_name(note: int) -> str:
    names = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    return f"{names[note % 12]}{note // 12 - 1}"


def estimate_frequency(signal: np.ndarray, sample_rate: int, expected_hz: float) -> float:
    frame = np.asarray(signal, dtype=np.float64)
    frame -= np.mean(frame)
    frame *= np.hanning(frame.size)
    nfft = 1 << max(20, int(math.ceil(math.log2(frame.size * 16))))
    spectrum = np.abs(np.fft.rfft(frame, n=nfft))
    frequencies = np.fft.rfftfreq(nfft, 1.0 / sample_rate)
    band = np.flatnonzero(
        (frequencies >= expected_hz * 0.97) & (frequencies <= expected_hz * 1.03)
    )
    index = int(band[np.argmax(spectrum[band])])
    if index <= 0 or index >= spectrum.size - 1:
        return float(frequencies[index])
    left, center, right = np.log(np.maximum(spectrum[index - 1:index + 2], 1.0e-300))
    denominator = left - 2.0 * center + right
    offset = 0.5 * (left - right) / denominator if denominator else 0.0
    return float((index + offset) * sample_rate / nfft)


def read_wav(path: Path) -> tuple[int, np.ndarray]:
    sample_rate, audio = wavfile.read(path)
    floating = np.asarray(audio, dtype=np.float64)
    if floating.ndim == 2:
        mono = np.mean(floating, axis=1)
    else:
        mono = floating
    if not np.all(np.isfinite(floating)):
        raise ValueError(f"non-finite WAV samples in {path}")
    return int(sample_rate), mono


def render(renderer: Path, arguments: list[str]) -> dict[str, str]:
    result = subprocess.run(
        [str(renderer), *arguments], cwd=ROOT, capture_output=True, text=True, check=True
    )
    fields: dict[str, str] = {}
    for token in result.stdout.split():
        if "=" in token:
            key, value = token.split("=", 1)
            fields[key] = value
    return fields


def analyze_notes(renderer: Path) -> list[dict[str, float | int | str]]:
    rows: list[dict[str, float | int | str]] = []
    with tempfile.TemporaryDirectory(prefix="aether_p03_notes_") as temporary:
        wav_path = Path(temporary) / "note.wav"
        configurations = (
            (44100, 0.0), (48000, 0.0), (96000, 0.0),
            (192000, 0.0), (48000, 0.25),
        )
        for sample_rate, stretch in configurations:
            for note in range(FIRST_NOTE, LAST_NOTE + 1):
                fields = render(renderer, [
                    "--scenario", "note", "--note", str(note), "--velocity", "80",
                    "--sample-rate", str(sample_rate), "--stretch", str(stretch),
                    "--duration", "2.0", "--note-off", "1.5", "--output", str(wav_path),
                ])
                rate, mono = read_wav(wav_path)
                if rate != sample_rate:
                    raise ValueError(f"expected {sample_rate} Hz, got {rate} for MIDI {note}")
                start = int(0.25 * rate)
                stop = int(1.25 * rate)
                target = float(fields["target_f0_hz"])
                expected_target = midi_frequency(note) * 2.0 ** (
                    stretch * ((note - 69) / 12.0) * abs((note - 69) / 12.0) / 1200.0
                )
                segment = mono[start:stop]
                measured = estimate_frequency(segment.copy(), rate, target)
                error_cents = 1200.0 * math.log2(measured / target)
                peak = float(np.max(np.abs(mono)))
                attack_rms = float(np.sqrt(np.mean(mono[int(0.02 * rate):int(0.25 * rate)] ** 2)))
                row: dict[str, float | int | str] = {
                    "midi_note": note,
                    "name": note_name(note),
                    "sample_rate_hz": sample_rate,
                    "stretch_cents_per_octave_squared": stretch,
                    "target_f0_hz": target,
                    "expected_f0_hz": expected_target,
                    "measured_f0_hz": measured,
                    "error_cents": error_cents,
                    "inharmonicity_B": float(fields["inharmonicity_B"]),
                    "fundamental_t60_s": float(fields["fundamental_t60_s"]),
                    "damper_t60_s": float(fields["damper_t60_s"]),
                    "hammer_mass_kg": float(fields["hammer_mass_kg"]),
                    "hammer_stiffness_N_m_p": float(fields["hammer_stiffness_N_m_p"]),
                    "strike_position_fraction": float(fields["strike_position_fraction"]),
                    "readout_position_fraction": float(fields["readout_position_fraction"]),
                    "peak_velocity_m_s": peak,
                    "attack_rms_m_s": attack_rms,
                    "non_finite_samples": 0,
                    "non_finite_outputs": int(fields["non_finite_outputs"]),
                    "calibration_status": fields["calibration_status"],
                    "source_ref": fields["source_ref"],
                }
                if abs(target / expected_target - 1.0) > 1.0e-10:
                    raise ValueError(f"tuning curve target failed for MIDI {note}: {row}")
                if abs(error_cents) > 5.0 or peak <= 1.0e-9:
                    raise ValueError(f"pitch/energy regression failed for MIDI {note}: {row}")
                if row["non_finite_outputs"] != 0:
                    raise ValueError(f"non-finite engine output for MIDI {note}: {row}")
                rows.append(row)
    return rows


def analyze_scenarios(renderer: Path) -> list[dict[str, str]]:
    scenario_rows: list[dict[str, str]] = []
    for name in ("scale", "arpeggio", "chord", "retrigger"):
        wave = OUT / f"{name}.wav"
        events = OUT / f"{name}-events.csv"
        fields = render(renderer, [
            "--scenario", name, "--velocity", "80", "--sample-rate", "48000",
            "--output", str(wave), "--events", str(events),
        ])
        rate, mono = read_wav(wave)
        if rate != 48000 or mono.size == 0:
            raise ValueError(f"invalid scenario WAV: {name}")
        if int(fields["non_finite_outputs"]) != 0:
            raise ValueError(f"non-finite output in scenario {name}: {fields}")
        if int(fields["active_voices_at_end"]) != 0:
            raise ValueError(f"released voices were not reclaimed in scenario {name}: {fields}")
        if int(fields["accepted_note_ons"]) == 0 or float(fields["peak_output_velocity_m_s"]) <= 0.0:
            raise ValueError(f"empty scenario output: {name}")
        if name == "chord" and int(fields["peak_active_voices"]) != 4:
            raise ValueError(f"four-note chord did not activate four voices: {fields}")
        if name == "retrigger" and int(fields["peak_active_voices"]) != 1:
            raise ValueError(f"same-pitch retrigger did not reuse one voice: {fields}")
        scenario_rows.append(fields)
    return scenario_rows


def analyze_inharmonicity(renderer: Path) -> list[dict[str, float | int]]:
    rows: list[dict[str, float | int]] = []
    with tempfile.TemporaryDirectory(prefix="aether_p03_inharmonicity_") as temporary:
        wav_path = Path(temporary) / "note.wav"
        for note in (21, 60, 108):
            measured_by_scale: dict[int, tuple[float, float]] = {}
            for scale in (0, 1):
                fields = render(renderer, [
                    "--scenario", "note", "--note", str(note), "--velocity", "80",
                    "--sample-rate", "48000", "--duration", "2.0", "--note-off", "1.5",
                    "--inharmonicity-scale", str(scale), "--output", str(wav_path),
                ])
                rate, mono = read_wav(wav_path)
                segment = mono[int(0.25 * rate):int(1.25 * rate)].copy()
                measured = estimate_frequency(segment, rate, midi_frequency(note))
                target = float(fields["target_f0_hz"])
                B = float(fields["inharmonicity_B"])
                if abs(1200.0 * math.log2(measured / target)) > 5.0:
                    raise ValueError(f"f0 tuning failed for MIDI {note}, B scale {scale}")
                if int(fields["non_finite_outputs"]) != 0:
                    raise ValueError(f"non-finite output for MIDI {note}, B scale {scale}")
                measured_by_scale[scale] = (measured, B)
            f0_zero, B_zero = measured_by_scale[0]
            f0_base, B_base = measured_by_scale[1]
            delta_cents = 1200.0 * math.log2(f0_base / f0_zero)
            if abs(delta_cents) > 0.1 or B_zero != 0.0 or B_base <= 0.0:
                raise ValueError(
                    f"inharmonicity changed fundamental for MIDI {note}: {delta_cents:.4f} cents"
                )
            rows.append({
                "midi_note": note,
                "f0_B_zero_hz": f0_zero,
                "f0_B_estimated_hz": f0_base,
                "delta_cents": delta_cents,
                "B_zero": B_zero,
                "B_estimated": B_base,
            })
    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    args = parser.parse_args()
    renderer = args.renderer.resolve()
    OUT.mkdir(parents=True, exist_ok=True)

    note_rows = analyze_notes(renderer)
    with (OUT / "note-pitch-summary.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(note_rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(note_rows)

    scenario_rows = analyze_scenarios(renderer)
    inharmonicity_rows = analyze_inharmonicity(renderer)
    with (OUT / "inharmonicity-tuning.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(inharmonicity_rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(inharmonicity_rows)
    (OUT / "scenario-results.json").write_text(
        json.dumps(scenario_rows, indent=2) + "\n", encoding="utf-8"
    )
    print(f"note_measurements={len(note_rows)} note_pitch_configs=5 "
          f"pitch_error_cents_min={min(r['error_cents'] for r in note_rows):.4f} "
          f"pitch_error_cents_max={max(r['error_cents'] for r in note_rows):.4f}")
    for sample_rate, stretch in ((44100, 0.0), (48000, 0.0), (96000, 0.0),
                                 (192000, 0.0), (48000, 0.25)):
        group = [
            row["error_cents"] for row in note_rows
            if row["sample_rate_hz"] == sample_rate
            and row["stretch_cents_per_octave_squared"] == stretch
        ]
        print(f"sample_rate_hz={sample_rate} stretch={stretch} notes={len(group)} "
              f"pitch_error_cents_min={min(group):.4f} "
              f"pitch_error_cents_max={max(group):.4f}")
    for result in scenario_rows:
        print(" ".join(f"{key}={value}" for key, value in result.items()))
    print(f"inharmonicity_variants={len(inharmonicity_rows)} "
          f"max_abs_delta_cents={max(abs(r['delta_cents']) for r in inharmonicity_rows):.6f}")
    print(f"pitch_summary={OUT / 'note-pitch-summary.csv'}")
    print(f"scenario_results={OUT / 'scenario-results.json'}")


if __name__ == "__main__":
    main()
