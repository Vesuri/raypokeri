#!/usr/bin/env python3
"""Resolve an opt-in WHDLoad debug map from a read-only Fast RAM capture."""
import argparse
from pathlib import Path
import struct

MAGIC = b'POK!DEBUGMAP0001'
ANCHORS = (('nativePlayReady', '.text'), ('pokeriVersionString', '.rodata'),
           ('pokeriWhdLoad', '.data'), ('pendingFrames', '.bss'))


def layout(elf):
    assert elf[:6] == b'\x7fELF\x01\x02', 'expected big-endian ELF32'
    h = struct.unpack_from('>HHIIIIIHHHHHH', elf, 16)
    sections = [struct.unpack_from('>10I', elf, h[5]+i*h[10]) for i in range(h[11])]
    def string(section, offset):
        start = section[4]+offset
        return elf[start:elf.index(b'\0', start)].decode()
    names = {string(sections[h[12]], s[0]):s for s in sections}
    symbols = {}
    for table in sections:
        if table[1] != 2:
            continue
        for at in range(table[4], table[4]+table[5], table[9]):
            name, value, size, info, other, index = struct.unpack_from('>IIIBBH', elf, at)
            if index < len(sections):
                symbols[string(sections[table[6]], name)] = (value, sections[index])
    return names, symbols


def resolve(elf, ram, ram_base):
    sections, symbols = layout(elf)
    for name, section in ANCHORS:
        assert symbols[name][1] == sections[section], f'anchor section changed: {name}'
    maps = []
    start = 0
    while (at := ram.find(MAGIC, start)) >= 0:
        start = at+4
        if at % 4 or at+36 > len(ram):
            continue
        pointers = struct.unpack_from('>5I', ram, at+16)
        if pointers[4] != ram_base+at:
            continue  # unrelocated file cache or a relocated copy at another address
        bases = {}
        for pointer, (name, section) in zip(pointers, ANCHORS):
            value, s = symbols[name]
            base = pointer-(value-s[3])
            if base < ram_base or base+s[5] > ram_base+len(ram) or base % s[8]:
                break
            bases[section] = base
        if len(bases) != len(ANCHORS):
            continue
        value, s = symbols['nativeWhdDebugMap']
        if s != sections['.data'] or bases['.data']+value-s[3] != pointers[4]:
            continue
        # Verify the leaf Ready marker and the exact version string in memory.
        okay = True
        for index, length in ((0, 2), (1, 32)):
            name, section = ANCHORS[index]
            value, s = symbols[name]
            expected = elf[s[4]+value-s[3]:s[4]+value-s[3]+length]
            offset = pointers[index]-ram_base
            okay &= ram[offset:offset+length] == expected
        if okay:
            maps.append(bases)
    if not maps:
        return None
    if len(maps) != 1:
        raise ValueError('multiple verified live debug maps')
    return maps[0]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf', type=Path, required=True)
    p.add_argument('--ram', type=Path, required=True)
    p.add_argument('--base', type=lambda x:int(x,0), default=0x200000)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    bases = resolve(a.elf.read_bytes(), a.ram.read_bytes(), a.base)
    if bases is None:
        raise SystemExit(2)
    path = str(a.elf.resolve())
    if any(c in path for c in '\n\r"\\'):
        raise ValueError('unsupported GDB path quoting')
    command = f'add-symbol-file "{path}" {bases[".text"]:#x}'
    command += ''.join(f' -s {s} {base:#x}' for s, base in bases.items() if s != '.text')
    a.out.write_text('symbol-file\n'+command+'\n')
    print('WHD debug sections:', bases)


if __name__ == '__main__':
    main()
