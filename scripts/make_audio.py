#!/usr/bin/env python3
"""Rebuild Scarlet Mesa's three original compositions. Requires Python + NumPy.

All notes, synthesis, and arrangements below were composed for this project.
No samples, soundfonts, existing Touhou melodies, or external music are used.
Each file is exactly 32 bars; delay tails wrap into the beginning for looping.
"""
from pathlib import Path
import argparse
import wave
import numpy as np

RATE = 22050
TAU = 2 * np.pi

# Hand-composed scale-degree melodies; each row is a bar of eighth notes.
# Degrees above 6 continue into the next octave, -1 is the leading note below.
MELODIES = [
    [
        [7, 4, 5, 7, 9, 8, 7, 4], [5, 7, 8, 5, 4, 2, 4, 5],
        [3, 5, 7, 8, 7, 5, 3, 2], [4, 6, 8, 7, 6, 4, 2, 6],
        [7, 9, 11, 9, 8, 7, 5, 4], [5, 8, 10, 8, 7, 5, 4, 2],
        [3, 5, 8, 7, 5, 3, 2, 1], [2, 4, 6, 8, 7, 6, 8, 6],
    ],
    [
        [7, 8, 4, 7, 10, 9, 7, 5], [6, 8, 9, 6, 8, 4, 3, 6],
        [5, 7, 10, 9, 7, 5, 8, 7], [4, 6, 8, 11, 10, 8, 6, 4],
        [10, 9, 7, 8, 10, 12, 10, 9], [8, 6, 4, 6, 9, 8, 6, 3],
        [5, 8, 10, 12, 11, 10, 8, 7], [6, 4, 2, 6, 8, 9, 8, 6],
    ],
    [
        [7, 6, 7, 11, 10, 8, 7, 6], [5, 7, 10, 8, 5, 4, 5, 8],
        [9, 8, 5, 9, 12, 11, 9, 8], [6, 4, 6, 8, 10, 8, 6, 4],
        [7, 11, 14, 13, 11, 10, 8, 7], [8, 10, 12, 10, 8, 5, 7, 8],
        [9, 12, 11, 9, 8, 7, 5, 4], [6, 8, 11, 10, 8, 6, 4, 6],
    ],
]
TRACKS = [
    ("Golden Gate Overclock", 144, 69, [0, 5, 3, 4, 0, 5, 3, 4]),
    ("Gradient Descent at Midnight", 154, 74, [0, 6, 5, 4, 0, 6, 5, 4]),
    ("Boundary of a Finite Oracle", 164, 76, [0, 5, 1, 4, 0, 5, 1, 4]),
]
SCALE = [0, 2, 3, 5, 7, 8, 11]  # Harmonic minor, including its tense leading tone.


def pitch(root, degree):
    octave, index = divmod(degree, 7)
    return root + SCALE[index] + 12 * octave


def compose(index, output):
    name, bpm, root, progression = TRACKS[index]
    beat = 60 / bpm
    total = round(32 * 4 * beat * RATE)
    mix = np.zeros((total, 2), dtype=np.float64)
    rng = np.random.default_rng(0x5CA12 + index)

    def add(sound, position, pan=0.0, volume=1.0):
        begin = round(position * beat * RATE)
        indices = (begin + np.arange(len(sound))) % total
        # Equal-power pan. All events are shorter than the loop.
        mix[indices, 0] += sound * volume * np.sqrt((1 - pan) / 2)
        mix[indices, 1] += sound * volume * np.sqrt((1 + pan) / 2)

    def note(midi, position, duration, instrument, volume, pan=0):
        seconds = duration * beat
        t = np.arange(round((seconds + 0.065) * RATE)) / RATE
        hz = 440 * 2 ** ((midi - 69) / 12)
        vibrato = 0.003 * np.sin(TAU * 5.5 * t) * np.minimum(1, t / 0.2)
        phase = TAU * hz * t + vibrato * 8
        attack = np.minimum(1, t / 0.006)
        release = np.clip((seconds + 0.06 - t) / 0.06, 0, 1)
        if instrument == "lead":
            sound = (np.sin(phase) + 0.32 * np.sin(phase * 2)
                     + 0.16 * np.sin(phase * 3) + 0.09 * np.sin(phase * 5))
            envelope = attack * release * (0.76 + 0.24 * np.exp(-t * 18))
        elif instrument == "bell":
            sound = np.sin(phase + np.sin(phase * 2) * np.exp(-t * 10) * 1.6)
            envelope = attack * release * np.exp(-t * 5.5)
        elif instrument == "bass":
            sound = np.sin(phase) + 0.22 * np.sin(phase * 2) + 0.10 * np.sin(phase * 3)
            envelope = attack * release * np.exp(-t * 1.5)
        else:
            sound = (np.sin(phase) + np.sin(phase * 1.003)) * 0.5
            envelope = np.minimum(1, t / 0.06) * release
        add(sound * envelope, position, pan, volume)

    def drum(kind, position, strength=1):
        duration = {"kick": 0.24, "snare": 0.17, "hat": 0.048, "crash": 0.60}[kind]
        t = np.arange(round(RATE * duration)) / RATE
        n = rng.uniform(-1, 1, len(t))
        if kind == "kick":
            phase = TAU * (47 * t + 105 * (1 - np.exp(-t * 37)) / 37)
            sound = np.sin(phase) * np.exp(-t * 20) + n * np.exp(-t * 160) * 0.12
            volume, pan = 0.30, 0
        elif kind == "snare":
            sound = (n * 0.8 + np.sin(TAU * 185 * t) * 0.30) * np.exp(-t * 23)
            volume, pan = 0.18, 0.12
        elif kind == "hat":
            high = n - np.roll(n, 1)
            sound = high * np.exp(-t * 80)
            volume, pan = 0.038, -0.30
        else:
            sound = (n - np.roll(n, 1)) * np.exp(-t * 7)
            volume, pan = 0.07, 0.25
        # A two-millisecond onset keeps every percussion event click-free.
        add(sound * np.minimum(1, t / 0.002), position, pan, volume * strength)

    for bar in range(32):
        section, phrase = divmod(bar, 8)
        chord = progression[phrase]
        chord_notes = [pitch(root - 24, chord + degree) for degree in [0, 2, 4]]
        # Chorus answers the first phrase an octave higher, bridge creates space.
        for tone, chord_note in enumerate(chord_notes):
            note(chord_note + 12, bar * 4, 3.93, "pad", 0.036, (tone - 1) * 0.65)
        for half in range(8):
            note(chord_notes[0] - 12 + (12 if half % 4 == 3 else 0),
                 bar * 4 + half / 2, 0.40, "bass", 0.17)
        for sixteenth in range(16):
            arpeggio = [0, 1, 2, 1, 0, 2, 1, 2][sixteenth % 8]
            note(chord_notes[arpeggio] + (24 if sixteenth % 8 >= 4 else 12),
                 bar * 4 + sixteenth / 4, 0.20, "bell", 0.052,
                 -0.48 if sixteenth % 2 == 0 else 0.48)
        melody = MELODIES[index][phrase]
        if section == 2:
            # The bridge moves in longer notes, then climbs into the final chorus.
            bridge = [melody[0] - 7, melody[2] - 7, melody[4] - 3, melody[6]]
            for step, degree in enumerate(bridge):
                note(pitch(root, degree), bar * 4 + step, 0.86, "lead", 0.17, -0.12)
        else:
            for eighth, degree in enumerate(melody):
                # Long downbeat and two sixteenth-note pickups give the hook a pulse.
                if eighth == 1 and phrase % 2 == 0:
                    continue
                length = 0.91 if eighth == 0 and phrase % 2 == 0 else 0.41
                if section == 3 and eighth >= 6:
                    degree += 7
                note(pitch(root, degree), bar * 4 + eighth / 2, length,
                     "lead", 0.18, -0.08)
                if section in (1, 3) and eighth in (2, 5):
                    note(pitch(root, degree - 2), bar * 4 + eighth / 2,
                         length, "bell", 0.058, 0.35)
        for pulse in range(4):
            drum("kick", bar * 4 + pulse, 0.85 if pulse % 2 else 1)
            if pulse % 2:
                drum("snare", bar * 4 + pulse)
            drum("hat", bar * 4 + pulse, 0.55)
            drum("hat", bar * 4 + pulse + 0.5)
        if phrase in (3, 7):
            drum("snare", bar * 4 + 3.5, 0.65)
            drum("snare", bar * 4 + 3.75, 0.45)
        if phrase == 0:
            drum("crash", bar * 4)

    # Circular ping-pong echoes preserve the exact musical loop boundary.
    delay = round(0.75 * beat * RATE)
    mix += np.roll(mix[:, ::-1], delay, axis=0) * 0.18
    mix += np.roll(mix[:, ::-1], delay * 2, axis=0) * 0.065
    mix = np.tanh(mix * 1.20)
    mix *= 0.88 / np.max(np.abs(mix))
    pcm = (mix * 32767).astype("<i2")
    path = output / f"stage{index + 1}.wav"
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(pcm.tobytes())
    print(f"{path.name}: {name} | {bpm} BPM | {total / RATE:.2f}s | original composition")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "assets")
    arguments = parser.parse_args()
    arguments.output.mkdir(parents=True, exist_ok=True)
    for track in range(3):
        compose(track, arguments.output)
