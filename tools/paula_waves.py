#!/usr/bin/env python3
"""Build local-only Paula oscillator loops from the verified sound data directory.

No envelopes, volumes, durations or game decisions are baked into these loops.
The original sequencer still selects registers; this only replaces AY oscillators.
"""
import hashlib
import math
from pathlib import Path
import sys
from roms import CHIPS

ROOT = Path(__file__).resolve().parent.parent
MASKS = (255, 15, 255, 15, 255, 15, 31, 255, 31, 31, 31, 255, 255, 15)


def catalog(data):
    groups, cursor = [], 0x82c
    for _ in range(3):
        group = []
        while True:
            if cursor + 4 > 0x89c:
                raise ValueError('sound directory exceeds verified bounds')
            address = int.from_bytes(data[cursor:cursor + 4], 'big')
            cursor += 4
            if not address:
                break
            if not 0x1f70 <= address < 0x2800:
                raise ValueError('invalid sound pointer')
            group.append(address)
        groups.append(group)
    records = {}
    for start in sorted(set(sum(groups, []))):
        pos = start
        for _ in range(1024):
            if pos + 16 > 0x2800:
                raise ValueError('sound sequence exceeds verified bounds')
            records[pos] = bytes(a & b for a, b in zip(data[pos:pos + 14], MASKS))
            pos += 15
            control = data[pos]
            if control in (0, 1):
                break
            if control == 2:
                pos += 2
                if not data[pos]:
                    break
        else:
            raise ValueError('unterminated sound sequence')
    keys = set()
    for r in records.values():
        for c in range(3):
            if not (r[8+c] & 31) or (r[7] & (8 << c)):
                continue
            tone = 0 if r[7] & (1 << c) else max(1, r[2*c] | r[2*c+1] << 8)
            keys.add((tone, max(1, r[6])))
    return groups, records, sorted(keys)


def waveform(tone, noise):
    # Decimate six AY clock/8 steps per sample (48 us). PAL Paula PER=170
    # gives 20,864 Hz, 0.148% above the 20,833 Hz ideal. Full tone cycles at
    # loop boundaries; noise has a finite, deliberately long 0.2s+ prefix.
    quantum = math.lcm(tone // math.gcd(tone, 3), 2) if tone else 2
    length = ((4096 + quantum - 1) // quantum) * quantum
    state, tc, nc, high = 1, 0, 0, False
    signal = []
    for _ in range(length * 6):
        signal.append(1 if (state & 1) and (not tone or high) else -1)
        tc += 1
        if tone and tc == tone:
            tc, high = 0, not high
        nc += 1
        if nc == noise * 2:
            nc = 0
            state = (state >> 1) | (((state ^ (state >> 3)) & 1) << 16)
    # Offline low-pass before 6:1 decimation. A box average alone aliases
    # high AY tones into audible false pitches (e.g. tone period 4).
    cutoff = 9000 / 125000
    kernel = []
    for k in range(63):
        x = k - 31
        sinc = 2 * cutoff if not x else math.sin(2 * math.pi * cutoff * x) / (math.pi * x)
        kernel.append(sinc * (0.54 - 0.46 * math.cos(2 * math.pi * k / 62)))
    scale = 127 / sum(kernel)
    kernel = [v * scale for v in kernel]
    samples = bytearray()
    for i in range(length):
        value = sum(weight * signal[(i * 6 + k - 31) % len(signal)] for k, weight in enumerate(kernel))
        samples.append(max(-127, min(127, round(value))) & 255)
    return bytes(samples)


def main():
    data = (ROOT / 'rom/PARA200J').read_bytes()
    if hashlib.sha256(data).hexdigest() != CHIPS['PARA200J'][1]:
        raise ValueError('unrecognized parameter ROM; refusing guessed sound coverage')
    groups, records, keys = catalog(data)
    raw, entries = bytearray(), []
    for tone, noise in keys:
        wave = waveform(tone, noise)
        entries.append((tone, noise, len(raw), len(wave)))
        raw.extend(wave)
    out = ROOT / 'amiga/generated/PaulaWaves.h'
    out.parent.mkdir(parents=True, exist_ok=True)
    text = '// ROM-derived generated audio. LOCAL ONLY. Do not commit.\n'
    text += 'struct PaulaWaveEntry { unsigned short tone,noise; unsigned offset,length; };\n'
    text += 'static const PaulaWaveEntry paulaWaveEntries[] = {\n'
    text += ''.join('    {%d,%d,%d,%d},\n' % e for e in entries) + '};\n'
    text += 'static const unsigned char paulaWaveData[] = {\n'
    text += ''.join('    ' + ','.join(str(v) for v in raw[p:p+32]) + ',\n' for p in range(0, len(raw), 32)) + '};\n'
    out.write_text(text)
    print('Paula: groups=%s records=%d noise/mixed loops=%d bytes=%d (no runtime synthesis)' %
          ([len(g) for g in groups], len(records), len(keys), len(raw)))
    if '--manifest' in sys.argv:
        p = ROOT / 'tmp/paula-wave-records.csv'
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('offset,registers\n' + ''.join('%x,%s\n' % (a, r.hex()) for a, r in sorted(records.items())))


if __name__ == '__main__':
    main()
