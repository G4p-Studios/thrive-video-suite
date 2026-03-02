#!/usr/bin/env python3
"""Generate placeholder WAV audio cue files for Thrive Video Suite.

Each file is a short sine-wave tone at a different frequency so the
cues are distinguishable during development.  Replace these with
professionally designed sounds before release.
"""

import struct
import math
import os

SAMPLE_RATE = 44100
BITS = 16
CHANNELS = 1

def make_wav(filename: str, freq: float, duration_ms: int, volume: float = 0.5):
    """Write a mono 16-bit PCM WAV file with a sine tone."""
    num_samples = int(SAMPLE_RATE * duration_ms / 1000)
    max_amp = int(32767 * volume)

    samples = bytearray()
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        val = int(max_amp * math.sin(2 * math.pi * freq * t))
        samples += struct.pack('<h', val)

    data_size = len(samples)
    file_size = 36 + data_size

    with open(filename, 'wb') as f:
        # RIFF header
        f.write(b'RIFF')
        f.write(struct.pack('<I', file_size))
        f.write(b'WAVE')
        # fmt chunk
        f.write(b'fmt ')
        f.write(struct.pack('<I', 16))             # chunk size
        f.write(struct.pack('<H', 1))              # PCM
        f.write(struct.pack('<H', CHANNELS))
        f.write(struct.pack('<I', SAMPLE_RATE))
        byte_rate = SAMPLE_RATE * CHANNELS * BITS // 8
        f.write(struct.pack('<I', byte_rate))
        block_align = CHANNELS * BITS // 8
        f.write(struct.pack('<H', block_align))
        f.write(struct.pack('<H', BITS))
        # data chunk
        f.write(b'data')
        f.write(struct.pack('<I', data_size))
        f.write(samples)

    print(f'  {filename}  ({duration_ms} ms @ {freq} Hz, {data_size} bytes)')


def main():
    out_dir = os.path.join(os.path.dirname(__file__), 'sounds')
    os.makedirs(out_dir, exist_ok=True)

    cues = {
        'clip_start.wav':      (880,  80),   # A5 – short high ping
        'clip_end.wav':        (660,  80),   # E5 – slightly lower ping
        'gap.wav':             (330, 120),   # E4 – hollow mid tone
        'track_boundary.wav':  (220, 150),   # A3 – low thud
        'selection.wav':       (1047, 60),   # C6 – quick tick
        'error.wav':           (200, 200),   # low buzz
    }

    print('Generating placeholder audio cues:')
    for name, (freq, dur) in cues.items():
        make_wav(os.path.join(out_dir, name), freq, dur)

    print('Done.')


if __name__ == '__main__':
    main()
