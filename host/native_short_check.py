#!/usr/bin/env python3
"""Extract our assembled sentinel body for the host-only CPU differential test."""
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
elf = root / 'amiga/out/Pokeri.elf'
symbols = subprocess.check_output(['m68k-amiga-elf-objdump', '-t', str(elf)], text=True)
names = ('nativeShortSentinelRead', 'nativeShortDone', 'nativeShortSentinelGuard',
         'nativeShortAdmitted', 'nativeShortDecline', 'nativeRomBegin', 'nativeRomEnd',
         'nativeRamBegin', 'nativeRamEnd')
addresses = {line.split()[-1]: int(line.split()[0], 16) for line in symbols.splitlines()
             if line.split() and line.split()[-1] in names}
data = elf.read_bytes()
assert data[:6] == b'\x7fELF\x01\x02', 'expected big-endian ELF32'
header = struct.unpack_from('>HHIIIIIHHHHHH', data, 16)
section_offset, section_size, count = header[5], header[10], header[11]
def extract(first, last, filename):
    begin, end = addresses[first], addresses[last]
    for i in range(count):
        _, kind, flags, address, offset, size, *_ = struct.unpack_from('>10I', data, section_offset+i*section_size)
        if kind == 1 and flags & 4 and address <= begin < end <= address+size:
            code = data[offset+begin-address:offset+end-address]
            break
    else:
        raise AssertionError('sentinel code section missing')
    assert len(code) < 512
    path = root / 'tmp' / filename
    path.write_bytes(code)
    return str(path)

flags = extract('nativeShortSentinelRead', 'nativeShortDone', 'native-short-sentinel-code.bin')
guard = extract('nativeShortSentinelGuard', 'nativeShortAdmitted', 'native-short-guard-code.bin')
decline = addresses['nativeShortDecline'] - addresses['nativeShortSentinelGuard']
subprocess.run([str(root/'build/native-short-flags-test'), flags, guard, str(decline)] +
               [str(addresses[n]) for n in names[-4:]], check=True)
