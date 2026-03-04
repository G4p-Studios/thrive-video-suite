import wave, struct, math

SR = 44100
DUR = 0.15
NS = int(SR * DUR)
HALF = NS // 2
OUTDIR = r"C:\Users\alex\thrive-video-suite\resources\sounds"

def make(path, freq1, freq2):
    with wave.open(path, "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        for i in range(NS):
            t = i / SR
            freq = freq1 if i < HALF else freq2
            lt = i / HALF if i < HALF else (i - HALF) / HALF
            env = max(0.0, 1.0 - lt * 0.8)
            s = math.sin(2.0 * math.pi * freq * t) * env * 0.35
            w.writeframes(struct.pack("h", max(-32767, min(32767, int(s * 32767)))))

make(OUTDIR + r"\clip_added.wav", 523.25, 659.25)
make(OUTDIR + r"\clip_removed.wav", 659.25, 523.25)

import os
for f in ["clip_added.wav", "clip_removed.wav"]:
    p = os.path.join(OUTDIR, f)
    print(f"{f}: {os.path.getsize(p)} bytes")
