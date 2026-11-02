#!/usr/bin/env python3
"""Validate MuseScore's native VST3 four-note chord WAV at a coarse spectral level."""

from __future__ import annotations

import hashlib
import struct
from pathlib import Path

import numpy as np


WAV_PATH = Path(__file__).with_name("MuseScore-P03-four-note-chord-native.wav")
TARGETS_HZ = {
    "C4": 261.6256,
    "E4": 329.6276,
    "G4": 391.9954,
    "C5": 523.2511,
}


def read_float_wave(path: Path) -> tuple[bytes, int, int, np.ndarray]:
    data = path.read_bytes()
    position = 12
    fmt = None
    payload = None
    while position + 8 <= len(data):
        tag = data[position : position + 4]
        size = struct.unpack_from("<I", data, position + 4)[0]
        chunk = data[position + 8 : position + 8 + size]
        if tag == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", chunk, 0)
        elif tag == b"data":
            payload = chunk
        position += 8 + size + (size & 1)

    if fmt is None or payload is None:
        raise ValueError("missing WAVE fmt or data chunk")
    encoding, channels, sample_rate, _, _, bits = fmt
    if encoding != 3 or bits != 32:
        raise ValueError(f"expected 32-bit float WAVE, got encoding={encoding}, bits={bits}")
    samples = np.frombuffer(payload, dtype="<f4").reshape(-1, channels).astype(np.float64)
    return data, channels, sample_rate, samples


def main() -> None:
    data, channels, sample_rate, samples = read_float_wave(WAV_PATH)
    duration = len(samples) / sample_rate
    finite = bool(np.isfinite(samples).all())
    peak = float(np.max(np.abs(samples)))
    rms = float(np.sqrt(np.mean(samples * samples)))
    print(f"file={WAV_PATH} bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}")
    print(
        f"format=IEEE_FLOAT_32 channels={channels} sample_rate_hz={sample_rate} "
        f"frames={len(samples)} duration_s={duration:.6f}"
    )
    print(f"finite={finite} peak={peak:.6g} rms={rms:.6g}")
    if not finite:
        raise AssertionError("audio contains non-finite samples")

    mono = samples.mean(axis=1)
    detected: dict[str, float] = {}
    start_s, end_s = 0.10, 0.80
    segment = mono[int(start_s * sample_rate) : int(end_s * sample_rate)]
    window = np.hanning(len(segment))
    spectrum = np.abs(np.fft.rfft(segment * window))
    frequencies = np.fft.rfftfreq(len(segment), 1.0 / sample_rate)
    print(f"spectral_window_s={start_s:.2f}-{end_s:.2f}")
    for name, target_hz in TARGETS_HZ.items():
        band = (frequencies >= target_hz - 4.0) & (frequencies <= target_hz + 4.0)
        candidates = np.flatnonzero(band)
        index = candidates[np.argmax(spectrum[band])]
        detected[name] = float(frequencies[index])
        print(f"  {name}: peak_hz={frequencies[index]:.3f} magnitude={spectrum[index]:.6g}")

    if len(samples) != 154350 or channels != 2 or sample_rate != 44100:
        raise AssertionError("unexpected channel count, sample rate, or duration")
    for name, target_hz in TARGETS_HZ.items():
        if abs(detected[name] - target_hz) > 2.0:
            raise AssertionError(f"{name} peak outside coarse 2 Hz tolerance")


if __name__ == "__main__":
    main()
