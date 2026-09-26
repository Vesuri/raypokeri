#!/usr/bin/env python3
"""Verify and cycle-count the linked Paula DMA server, with no ROM data."""
from pathlib import Path
import struct
import subprocess
root = Path(__file__).resolve().parents[1]
elf = root / 'amiga/out/Pokeri.elf'
symbols = subprocess.check_output(['m68k-amiga-elf-objdump', '-t', str(elf)], text=True)
names = {'pokeriPaulaStream', 'pokeriPaulaStreamEnd'}
addresses = {v[-1]: int(v[0], 16) for line in symbols.splitlines()
             if (v := line.split()) and v[-1] in names}
start, end = addresses['pokeriPaulaStream'], addresses['pokeriPaulaStreamEnd']
data = elf.read_bytes()
h = struct.unpack_from('>HHIIIIIHHHHHH', data, 16)
for i in range(h[11]):
    _, kind, flags, address, offset, size, *_ = struct.unpack_from('>10I', data, h[5] + i * h[10])
    if kind == 1 and flags & 4 and address <= start < end <= address + size:
        path = root / 'tmp/paula-stream-code.bin'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data[offset + start - address:offset + end - address])
        subprocess.run([str(root / 'build/paula-stream-test'), str(path)], check=True)
        break
else:
    raise RuntimeError('linked stream server missing')
