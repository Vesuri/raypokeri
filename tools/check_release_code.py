#!/usr/bin/env python3
"""Reject development code and symbols in the packaged Amiga executable."""
import argparse
from pathlib import Path
import re
import shutil
import struct
import subprocess


def check(elf, executable, objdump):
    objdump = shutil.which(objdump) or str(Path.home()/'.local/opt/bin'/objdump)
    symbols = subprocess.check_output([objdump, '-t', '-C', str(elf)], text=True)
    assert re.search(r'\*ABS\*.*\bPOKERI_RELEASE$', symbols, re.M), 'build with RELEASE=1'
    forbidden = ('nativeProfileBenchmark', 'nativeProfileSample', 'nativeShortReplayStart',
                 'nativeFeedReplayContinue', 'ReplayReader::', 'AmigaSurface::selfTest',
                 'AmigaSurface::cardBlitTest', 'AmigaScreen::compositionTest',
                 'nativeDoubleScenario', 'nativeWhdDebugMap',
                 'paulaWaveData', 'paulaWaveEntries')
    for line in symbols.splitlines():
        if '.text' in line or '.data' in line or '.rodata' in line or '.bss' in line:
            assert not any(name in line for name in forbidden), line
            assert not re.search(r'\bnative\w*Benchmark\w*$', line), line
    data = executable.read_bytes()
    assert not re.search(rb'native-(?:benchmark|replay|test-|clock-|measure|no-)', data), 'research marker in release'
    # Walk real HUNK records, not byte-pattern matches inside code/data.
    offset = 0
    def word():
        nonlocal offset
        value = struct.unpack_from('>I', data, offset)[0]
        offset += 4
        return value
    assert word() == 1011 and word() == 0, 'HUNK header'
    count, first, last = word(), word(), word()
    assert first == 0 and last + 1 == count
    for _ in range(count): word()
    while offset < len(data):
        kind = word() & 0x3fffffff
        if kind in (1001, 1002):
            size = word(); offset += size * 4
        elif kind == 1003: word()
        elif kind == 1004:
            while True:
                count = word()
                if not count: break
                word(); offset += count * 4
        else:
            assert kind == 1010, f'unexpected HUNK {kind}: symbols/debug must be stripped'
    assert offset == len(data), 'truncated HUNK'
    print(f'PASS: release-only code, no research markers or HUNK symbols/debug; {len(data)} bytes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--objdump', default='m68k-amiga-elf-objdump')
    args = parser.parse_args()
    check(args.elf, args.executable, args.objdump)
