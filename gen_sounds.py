import wave, struct, math, os

sr = 44100
dur = 0.15
ns = int(sr * dur)
half = ns // 2
base = r"C:\Users\alex\thrive-video-suite\resources\sounds"

# clip_added.wav - rising C5 -> E5
path = os.path.join(base, "clip_added.wav")
with wave.open(path, "w") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(sr)
    for i in range(ns):
        t = i / sr
        freq = 523.25 if i < half else 659.25
        lt = i / half if i < half else (i - half) / half
        env = max(0.0, 1.0 - lt * 0.8)
        s = math.sin(2 * math.pi * freq * t) * env * 0.35
        w.writeframes(struct.pack("h", int(s * 32767)))
print(f"Created {path} ({os.path.getsize(path)} bytes)")

# clip_removed.wav - falling E5 -> C5
path2 = os.path.join(base, "clip_removed.wav")
with wave.open(path2, "w") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(sr)
    for i in range(ns):
        t = i / sr
        freq = 659.25 if i < half else 523.25
        lt = i / half if i < half else (i - half) / half
        env = max(0.0, 1.0 - lt * 0.8)
        s = math.sin(2 * math.pi * freq * t) * env * 0.35
        w.writeframes(struct.pack("h", int(s * 32767)))
print(f"Created {path2} ({os.path.getsize(path2)} bytes)")
