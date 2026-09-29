#!/usr/bin/env python3
"""Run the assembled, relocation-free memory fill in the CPU oracle."""
from pathlib import Path
import argparse
import struct
import subprocess
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--elf', type=Path, help='also verify the linked wrapper is exactly the tested code')
args = parser.parse_args()
b = (root / 'build/native-memset.o').read_bytes()
assert b[:6] == b'\x7fELF\x01\x02'
h = struct.unpack_from('>HHIIIIIHHHHHH', b, 16)
assert h[0] == 1 and h[1] == 4  # relocatable m68k object
sections = [struct.unpack_from('>10I', b, h[5] + i*h[10]) for i in range(h[11])]
strings = sections[h[12]]
names = b[strings[4]:strings[4]+strings[5]]
selected = [(i, s) for i, s in enumerate(sections)
            if names[s[0]:].split(b'\0')[0] == b'.text.pokeriMemset']
assert len(selected) == 1
index, s = selected[0]
assert s[1] == 1 and s[2] & 4 and s[4]+s[5] <= len(b)
assert not any(r[1] in (4, 9) and r[7] == index and r[5] for r in sections)
code = root / 'build/native-memset.bin'
body = b[s[4]:s[4]+s[5]]
code.write_bytes(body)
if args.elf:
    linked = args.elf.read_bytes()
    eh = struct.unpack_from('>HHIIIIIHHHHHH', linked, 16)
    sh = [struct.unpack_from('>10I', linked, eh[5] + i*eh[10]) for i in range(eh[11])]
    symbols = {}
    for table in sh:
        if table[1] != 2:
            continue
        names_section = sh[table[6]]
        names = linked[names_section[4]:names_section[4]+names_section[5]]
        for offset in range(table[4], table[4]+table[5], table[9]):
            name, value, size, info, other, section = struct.unpack_from('>IIIBBH', linked, offset)
            symbols[names[name:].split(b'\0')[0]] = (value, section)
    address, section = symbols[b'pokeriMemset']
    assert symbols[b'__wrap_memset'] == (address, section)
    assert b'memset' not in symbols, 'unwrapped bytewise implementation remains linked'
    offset = sh[section][4] + address - sh[section][3]
    assert linked[offset:offset+len(body)] == body
    print('PASS linked wrapper is exactly the tested assembly; old bytewise memset is absent', flush=True)
subprocess.run([str(root / 'build/native-memset-test'), str(code)], check=True)
