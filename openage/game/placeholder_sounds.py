# Copyright 2026-2026 the openage authors. See copying.md for legal info.

"""
Temporary placeholder sounds for FVD, made at game start.

Until real recordings exist, the game creates simple synthesised sounds for
every cue in a cache folder. Real files in assets/fvd/sound always win; the
engine only falls back to a placeholder for a cue that has no real file.
Remove this module (and its two callers) once every cue has a recording.
"""

from __future__ import annotations

import math
import os
import pathlib
import random
import struct
import wave

from ..log import info

RATE = 48000

# Bump to regenerate everyone's cached placeholders after changing a sound.
PLACEHOLDER_VERSION = 1


def placeholder_dir() -> pathlib.Path:
    """
    Writable per-user folder for the generated sounds.
    """
    from .. import default_dirs

    try:
        base = default_dirs.get_dir("cache_home")
    except ValueError:
        # Windows has no cache directory entry; use the data folder.
        base = default_dirs.get_dir("data_home")
    return pathlib.Path(base) / "openage" / f"fvd-placeholder-sounds-v{PLACEHOLDER_VERSION}"


def _write(path: pathlib.Path, samples: list[float]) -> None:
    """Write mono float samples (-1..1) as 16-bit WAV, normalised to 80 %."""
    path.parent.mkdir(parents=True, exist_ok=True)
    peak = max([1e-9, *(abs(s) for s in samples)])
    scale = 0.8 / peak
    frames = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * scale)) * 32767)) for s in samples)
    partial = path.with_suffix(".part")
    with wave.open(str(partial), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(frames)
    # Never leave a half-written file where the game would load it.
    os.replace(partial, path)


def _envelope(count: int, attack: float = 0.005, decay: float | None = None) -> list[float]:
    """Quick attack, then exponential (or linear) decay to silence."""
    rise = max(1, int(attack * RATE))
    out = []
    for i in range(count):
        if i < rise:
            out.append(i / rise)
        elif decay:
            out.append(math.exp(-(i - rise) / (decay * RATE)))
        else:
            out.append(1.0 - (i - rise) / max(1, count - rise))
    return out


def _tone(start: float, end: float, seconds: float, harmonics=(1.0,), decay=None) -> list[float]:
    count = int(seconds * RATE)
    env = _envelope(count, decay=decay)
    phase = 0.0
    out = []
    for i in range(count):
        freq = start + (end - start) * i / count
        phase += 2 * math.pi * freq / RATE
        value = sum(a * math.sin(phase * (k + 1)) for k, a in enumerate(harmonics))
        out.append(value * env[i])
    return out


def _noise(seconds: float, lowpass: float = 0.2, decay=None, seed: int = 1) -> list[float]:
    rng = random.Random(seed)
    count = int(seconds * RATE)
    env = _envelope(count, attack=0.002, decay=decay)
    level = 0.0
    out = []
    for i in range(count):
        level += lowpass * (rng.uniform(-1, 1) - level)
        out.append(level * env[i])
    return out


def _mix(*parts: list[float]) -> list[float]:
    count = max(len(p) for p in parts)
    return [sum(p[i] for p in parts if i < len(p)) for i in range(count)]


def _silence(seconds: float) -> list[float]:
    return [0.0] * int(seconds * RATE)


def _note(freq: float, seconds: float) -> list[float]:
    return _tone(freq, freq, seconds, harmonics=(1.0, 0.3, 0.1), decay=seconds / 3)


def _sequence(notes, step: float) -> list[float]:
    out: list[float] = []
    for freq in notes:
        out += _note(freq, step) if freq else _silence(step)
    return out


def _sounds():
    """(relative file name, generator) for every placeholder."""
    a, c, d, e, g = 220.0, 261.6, 293.7, 329.6, 392.0

    def match_music():
        pulse = _sequence([a / 2, None, a / 2, a / 2, d / 2, None, c / 2, None] * 4, 0.25)
        drone = _tone(a / 4, a / 4, len(pulse) / RATE, harmonics=(1, 0.5, 0.3), decay=1000)
        return _mix([s * 0.6 for s in pulse], [s * 0.25 for s in drone])

    def unleashed():
        horn = _tone(146.8, 146.8, 0.5, harmonics=(1, 0.6, 0.4, 0.25), decay=0.6) + _tone(
            110, 98, 1.1, harmonics=(1, 0.6, 0.4, 0.25), decay=0.5
        )
        return _mix(horn, _silence(0.4) + _noise(1.2, 0.05, decay=0.4, seed=7))

    return [
        ("hit.wav", lambda: _mix(_noise(0.12, 0.5, decay=0.025, seed=1), _tone(180, 120, 0.12, decay=0.03))),
        ("hit_2.wav", lambda: _mix(_noise(0.12, 0.4, decay=0.03, seed=2), _tone(220, 140, 0.12, decay=0.03))),
        (
            "death_horse.wav",
            lambda: _mix(
                _tone(520, 160, 0.6, harmonics=(1, 0.5, 0.25), decay=0.2),
                _noise(0.6, 0.1, decay=0.15, seed=3),
            ),
        ),
        (
            "death_builder.wav",
            lambda: _mix(
                _tone(300, 90, 0.5, harmonics=(1, 0.4), decay=0.15), _noise(0.3, 0.3, decay=0.05, seed=4)
            ),
        ),
        (
            "death_tower.wav",
            lambda: _mix(_noise(1.2, 0.04, decay=0.35, seed=5), _tone(70, 40, 1.2, decay=0.4)),
        ),
        (
            "foundation_placed.wav",
            lambda: _mix(_noise(0.08, 0.6, decay=0.01, seed=6), _tone(400, 400, 0.08, decay=0.015)),
        ),
        ("tower_built.wav", lambda: _note(523, 0.15) + _note(784, 0.35)),
        ("order.wav", lambda: _tone(880, 1100, 0.07, decay=0.02)),
        ("hunter_unleashed.wav", unleashed),
        (
            "flare.wav",
            lambda: _mix(
                _tone(1568, 1568, 0.6, harmonics=(1, 0.3, 0.15), decay=0.18),
                _silence(0.07) + _tone(2093, 2093, 0.55, harmonics=(1, 0.3), decay=0.16),
            ),
        ),
        ("music/lobby.wav", lambda: [s * 0.5 for s in _sequence([a, c, e, g, e, c] * 4, 0.35)]),
        ("music/match.wav", match_music),
    ]


def ensure_placeholder_sounds(target: pathlib.Path | None = None) -> pathlib.Path:
    """
    Create any missing placeholder sound in target (default placeholder_dir())
    and return the folder. Existing files are left alone, so this is quick
    after the first start.
    """
    if target is None:
        target = placeholder_dir()
    missing = [(name, make) for name, make in _sounds() if not (target / name).is_file()]
    if missing:
        info("generating %d placeholder sounds in %s", len(missing), target)
        for name, make in missing:
            _write(target / name, make())
    return target


def test() -> None:
    """Every cue is written once as valid WAV, and existing files are kept."""
    import tempfile

    from ..testing.testing import assert_value

    with tempfile.TemporaryDirectory() as tmp:
        target = pathlib.Path(tmp) / "sounds"
        ensure_placeholder_sounds(target)
        names = sorted(name for name, _ in _sounds())
        written = sorted(f.relative_to(target).as_posix() for f in target.rglob("*") if f.is_file())
        assert_value(written, names)
        with wave.open(str(target / "hit.wav"), "rb") as clip:
            assert_value(clip.getframerate(), RATE)
            assert_value(clip.getnframes() > 0, True)

        (target / "hit.wav").write_bytes(b"mine")
        (target / "order.wav").unlink()
        ensure_placeholder_sounds(target)
        assert_value((target / "hit.wav").read_bytes(), b"mine")
        assert_value((target / "order.wav").is_file(), True)
