"""Measure the C4 WAV exported by the installed MuseScore VST3 host test."""

from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np
from scipy.io import wavfile

root = Path(__file__).resolve().parents[2]
path = root / "experiments/P02/MuseScore-VST3-C4-playback.wav"
rate, audio = wavfile.read(path)
floating = np.asarray(audio, dtype=np.float64)
mono = np.mean(floating, axis=1) if floating.ndim == 2 else floating
window = int(0.10 * rate)
hop = int(0.02 * rate)
levels = np.array([
    np.sqrt(np.mean(mono[start:start + window] ** 2))
    for start in range(0, mono.size - window, hop)
])
active = np.flatnonzero(levels > 1.0e-4)
if not active.size:
    raise SystemExit("no active MuseScore VST3 audio above 1e-4 RMS")

active_start = int(active[0] * hop)
active_end = int(active[-1] * hop + window)
analysis_start = active_start
analysis_end = min(mono.size, analysis_start + int(0.5 * rate))
segment = mono[analysis_start:analysis_end]
nfft = 1 << 20
spectrum = np.abs(np.fft.rfft(segment * np.hanning(segment.size), n=nfft))
frequencies = np.fft.rfftfreq(nfft, 1.0 / rate)
band = np.flatnonzero((frequencies >= 250.0) & (frequencies <= 275.0))
index = int(band[np.argmax(spectrum[band])])
left, center, right = np.log(np.maximum(spectrum[index - 1:index + 2], 1.0e-300))
offset = 0.5 * (left - right) / (left - 2.0 * center + right)
f0 = float((index + offset) * rate / nfft)
peak = float(np.max(np.abs(floating)))
full_duration = mono.size / rate
trim_frames = min(mono.size, int(3.0 * rate))
stats = {
    "host": "MuseScore Studio VST3 export",
    "sample_rate_hz": int(rate),
    "channels": 1 if floating.ndim == 1 else int(floating.shape[1]),
    "raw_duration_s": full_duration,
    "trimmed_duration_s": trim_frames / rate,
    "active_start_s": active_start / rate,
    "active_end_s": active_end / rate,
    "peak_absolute": peak,
    "rms_raw": float(np.sqrt(np.mean(floating**2))),
    "active_f0_hz": f0,
    "active_f0_error_cents": 1200.0 * math.log2(f0 / 261.625565),
    "non_finite_samples": int((~np.isfinite(floating)).sum()),
}
if stats["non_finite_samples"] != 0 or peak <= 0.01:
    raise SystemExit(f"host output validation failed: {stats}")

# Keep the note and short damper tail while avoiding a long mostly-silent host file.
wavfile.write(path, rate, audio[:trim_frames])
(root / "experiments/P02/musescore-host-validation.json").write_text(
    json.dumps(stats, indent=2) + "\n", encoding="utf-8"
)
print(json.dumps(stats, sort_keys=True))
