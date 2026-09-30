#!/usr/bin/env python3
"""Verify user ROMs and produce whole video-handler boundary fixtures locally."""
import hashlib
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from roms import CHIPS
image = bytearray()
for name in ('77POK30', '77POK38', '77POK34', 'PARA200J'):
    data = (root / 'rom' / name).read_bytes()
    if (len(data), hashlib.sha256(data).hexdigest()) != CHIPS[name][:2]:
        raise SystemExit(f'ROM verification failed: {name}')
    image.extend(data)
(root / 'tmp').mkdir(exist_ok=True)
rom = root / 'tmp/video-handler-reference-rom.bin'
rom.write_bytes(image)
subprocess.run([str(root / 'build/native-video-handler-reference'), str(rom),
                str(root / 'tmp/video-handler-boundaries.jsonl')], check=True)
