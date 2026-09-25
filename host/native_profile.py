#!/usr/bin/env python3
"""Attribute opt-in native VBI samples; input captures stay in tmp/."""
import argparse
import bisect
import collections
from pathlib import Path
import re
import struct
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--samples', required=True)
    p.add_argument('--elf', required=True)
    p.add_argument('--log', required=True)
    p.add_argument('--cycle-start', type=int, default=0)
    p.add_argument('--cycle-end', type=int, default=2**32)
    p.add_argument('--limit', type=int, default=20)
    a = p.parse_args()
    log = Path(a.log).read_text()
    m = re.search(r'BASE rom=([0-9a-f]+) ram=([0-9a-f]+) dispatch=([0-9a-f]+)', log)
    if not m:
        p.error('capture log needs BASE rom=... ram=... dispatch=...')
    rom, ram, dispatch = (int(x, 16) for x in m.groups())
    table = subprocess.check_output(['m68k-amiga-elf-objdump', '-t', '-C', a.elf], text=True)
    symbols = []
    text_end = 0
    for line in table.splitlines():
        found = re.match(r'^([0-9a-f]+)\s+.*?\s\.text\s+([0-9a-f]+)\s+(.+)$', line)
        if found and found[3] != '.text':
            address, size = int(found[1], 16), int(found[2], 16)
            symbols.append((address, found[3]))
            text_end = max(text_end, address+max(size, 2))
    symbols.sort()
    link_dispatch = next(address for address, name in symbols if name == 'nativeDispatch')
    relocation = dispatch - link_dispatch
    addresses = [address for address, _ in symbols]
    raw = Path(a.samples).read_bytes()
    if len(raw) % 12:
        p.error('partial sample record')
    counts = collections.Counter()
    contexts = collections.Counter()
    total = 0
    for pc, cycle, context in struct.iter_unpack('>III', raw):
        if not a.cycle_start <= cycle < a.cycle_end:
            continue
        total += 1
        contexts[context] += 1
        if rom <= pc < rom + 0x40000:
            name = f'guest ROM ${((pc-rom)//256)*256:05x}..'
        elif ram <= pc < ram + 0x40000:
            name = f'guest RAM ${0x40000+((pc-ram)//256)*256:05x}..'
        elif addresses[0] <= pc-relocation < text_end:
            name = symbols[bisect.bisect_right(addresses, pc-relocation)-1][1]
        else:
            name = f'OS/other ${pc//4096*4096:08x}..'
        counts[name] += 1
    print(f'{total} VBI samples; approximately {total/50:.2f} PAL seconds in selected cycle interval')
    print('Percentages describe sampled PCs, not inclusive function costs; masked and higher-priority IRQ work is under-sampled.')
    for name, count in counts.most_common(a.limit):
        print(f'{count:7d} {100*count/max(total,1):6.2f}% {name}')
    print('Scope context counts (nested context only, never sum with PC rows):', dict(sorted(contexts.items())))


if __name__ == '__main__':
    main()
