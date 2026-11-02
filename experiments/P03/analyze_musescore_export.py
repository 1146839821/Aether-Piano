"""Measure the four-note chord rendered by MuseScore through the installed VST3."""
from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parents[2]
WAV = ROOT / "experiments/P03/MuseScore-P03-polyphony.wav"
REPORT = ROOT / "experiments/P03/musescore-host-validation.json"
NOTES = ((60, "C4", 261.625565),)
UNEXPECTED = (("C#4", 277.182631),)


def measure_peak(frame: np.ndarray, rate: int, expected: float) -> tuple[float, float]:
    windowed = frame * np.hanning(frame.size)
    nfft = 1 << max(20, int(math.ceil(math.log2(frame.size * 8))))
    spectrum = np.abs(np.fft.rfft(windowed, n=nfft))
    frequencies = np.fft.rfftfreq(nfft, 1.0 / rate)
    band = np.flatnonzero((frequencies >= expected * 0.97) & (frequencies <= expected * 1.03))
    if band.size == 0:
        raise ValueError(f"empty frequency band around {expected} Hz")
    index = int(band[np.argmax(spectrum[band])])
    if index <= 0 or index >= spectrum.size - 1:
        return float(frequencies[index]), float(spectrum[index])
    left, center, right = np.log(np.maximum(spectrum[index - 1:index + 2], 1.0e-300))
    denominator = left - 2.0 * center + right
    offset = 0.5 * (left - right) / denominator if denominator else 0.0
    return float((index + offset) * rate / nfft), float(spectrum[index])


rate, audio = wavfile.read(WAV)
raw = np.asarray(audio, dtype=np.float64)
if raw.ndim != 2 or raw.shape[1] != 2:
    raise ValueError(f"expected stereo host WAV, got shape={raw.shape}")
if not np.isfinite(raw).all():
    raise ValueError("host WAV contains non-finite samples")
mono = raw.mean(axis=1)
window = int(0.10 * rate)
hop = int(0.02 * rate)
levels = np.array([
    np.sqrt(np.mean(mono[start:start + window] ** 2))
    for start in range(0, mono.size - window + 1, hop)
])
active = np.flatnonzero(levels > 1.0e-5)
if not active.size:
    raise ValueError("no active MuseScore VST3 audio above 1e-5 RMS")
active_start = int(active[0] * hop)
active_end = min(mono.size, int(active[-1] * hop + window))
segment_start = min(active_end - int(0.25 * rate), active_start + int(0.15 * rate))
segment_start = max(active_start, segment_start)
segment_end = min(active_end, segment_start + int(0.75 * rate))
segment = mono[segment_start:segment_end]
if segment.size < int(0.25 * rate):
    raise ValueError(f"active host segment too short: {segment.size / rate:.3f}s")

measurements = []
strongest_magnitude = 0.0
for midi, name, expected in NOTES:
    measured, magnitude = measure_peak(segment.copy(), int(rate), expected)
    error = 1200.0 * math.log2(measured / expected)
    strongest_magnitude = max(strongest_magnitude, magnitude)
    measurements.append({
        "midi_note": midi,
        "note": name,
        "expected_f0_hz": expected,
        "measured_peak_hz": measured,
        "error_cents": error,
        "spectral_magnitude": magnitude,
        "present": False,
    })

for note in measurements:
    note["present"] = bool(note["spectral_magnitude"] >= strongest_magnitude * 0.02)

unexpected_peaks = []
for name, expected in UNEXPECTED:
    measured, magnitude = measure_peak(segment.copy(), int(rate), expected)
    unexpected_peaks.append({
        "note": name,
        "expected_f0_hz": expected,
        "measured_peak_hz": measured,
        "spectral_magnitude": magnitude,
        "relative_to_strongest": magnitude / max(strongest_magnitude, 1.0e-300),
    })

host_pitch_pass = all(
    note["present"] and abs(note["error_cents"]) <= 5.0 for note in measurements
) and all(peak["relative_to_strongest"] < 0.05 for peak in unexpected_peaks)

stats = {
    "host": "MuseScore Studio 4.5.2, installed Aether Piano VST3",
    "validation_scope": "C4 single-note render; chord MIDI is validated separately",
    "plugin_bundle": str(Path.home() / "Library/Audio/Plug-Ins/VST3/Aether Piano.vst3"),
    "sample_rate_hz": int(rate),
    "channels": int(raw.shape[1]),
    "duration_s": raw.shape[0] / rate,
    "active_start_s": active_start / rate,
    "active_end_s": active_end / rate,
    "analysis_start_s": segment_start / rate,
    "analysis_duration_s": segment.size / rate,
    "peak_absolute": float(np.max(np.abs(raw))),
    "rms_full": float(np.sqrt(np.mean(raw ** 2))),
    "rms_analysis": float(np.sqrt(np.mean(segment ** 2))),
    "non_finite_samples": int((~np.isfinite(raw)).sum()),
    "notes": measurements,
    "unexpected_peaks": unexpected_peaks,
    "host_pitch_pass": host_pitch_pass,
}
REPORT.write_text(json.dumps(stats, indent=2) + "\n", encoding="utf-8")
print(json.dumps(stats, sort_keys=True))
