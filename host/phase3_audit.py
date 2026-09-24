#!/usr/bin/env python3
"""Audit covered 68000 operands and observed I/O; emit descriptors, never ROM bytes.

Uses the pinned ROM set plus the Phase 2 scenario captures in tmp/. The resulting
CSV files are research metadata, not yet a complete relocation recipe.
"""
import argparse
import csv
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from roms import CHIPS


def ea(mode, reg, offset, size):
    names = ['data', 'address', 'indirect', 'postincrement', 'predecrement', 'displacement', 'indexed']
    if mode < 7:
        return (names[mode], reg, offset if mode >= 5 else -1), offset + (2 if mode >= 5 else 0)
    if reg > 4:
        raise ValueError('reserved effective address')
    kind = ['absolute_word', 'absolute_long', 'pc_displacement', 'pc_indexed', 'immediate'][reg]
    return (kind, -1, offset), offset + (4 if reg == 1 or (reg == 4 and size == 4) else 2)


def decode_hook(word):
    """Structural 68000 decoder for the observed I/O instruction families."""
    group = word >> 12
    size = {1: 1, 2: 4, 3: 2}.get(group)
    if size:
        source, end = ea((word >> 3) & 7, word & 7, 2, size)
        dest, end = ea((word >> 6) & 7, (word >> 9) & 7, end, size)
        return 'move', size, source, dest, end
    size_code = (word >> 6) & 3
    if size_code < 3 and (word & 0xff00) in (0, 0x0200):
        size = 1 << size_code
        dest, end = ea((word >> 3) & 7, word & 7, 2 + (4 if size == 4 else 2), size)
        return ('or' if word & 0xff00 == 0 else 'and'), size, ('immediate', -1, 2), dest, end
    if size_code < 3 and (word & 0xff00) in (0x4200, 0x4a00, 0x0c00):
        size = 1 << size_code
        immediate = (word & 0xff00) == 0x0c00
        operand, end = ea((word >> 3) & 7, word & 7, 2 + ((4 if size == 4 else 2) if immediate else 0), size)
        name = {0x4200: 'clear', 0x4a00: 'test', 0x0c00: 'compare'}[word & 0xff00]
        return name, size, ('immediate', -1, 2) if immediate else ('none', -1, -1), operand, end
    if group == 0xb and ((word >> 6) & 7) < 3:
        size = 1 << size_code
        source, end = ea((word >> 3) & 7, word & 7, 2, size)
        return 'compare', size, source, ('data', (word >> 9) & 7, -1), end
    if (word & 0xffc0) == 0x0800 or (word & 0xf1c0) == 0x0100:
        immediate = (word & 0xffc0) == 0x0800
        dest, end = ea((word >> 3) & 7, word & 7, 4 if immediate else 2, 1)
        return 'bit_test', 1, ('immediate', -1, 2) if immediate else ('data', (word >> 9) & 7, -1), dest, end
    raise ValueError('unsupported I/O instruction family')


def write_csv(path, header, rows):
    with path.open('w', newline='') as f:
        out = csv.writer(f, lineterminator='\n')
        out.writerow(header)
        out.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'tmp/phase3-audit')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    image = bytearray()
    for name in ['77POK30', '77POK38', '77POK34', 'PARA200J']:
        data = (ROOT / 'rom' / name).read_bytes()
        size, digest, _ = CHIPS[name]
        if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
            raise SystemExit('ROM mismatch: ' + name)
        image.extend(data)
    union = bytearray(0x20000)
    accesses = set()
    for scenario in ['setup', 'attract', 'deal', 'win', 'double', 'service']:
        prefix = ROOT / ('tmp/scenario-' + scenario)
        data = Path(str(prefix) + '-coverage.bin').read_bytes()
        if len(data) != len(union):
            raise SystemExit('invalid scenario coverage: ' + scenario)
        for i, value in enumerate(data):
            union[i] |= value
        with Path(str(prefix) + '-trace.csv').open() as f:
            for row in csv.DictReader(f):
                accesses.add((int(row['pc'], 16), int(row['address'], 16), int(row['size']), row['direction']))
    coverage = ROOT / 'tmp/phase3-union-coverage.bin'
    coverage.write_bytes(union)
    subprocess.run([str(ROOT / 'build/pokeri-host'), '--code-map', str(coverage), '--out', 'tmp/phase3-union'], cwd=ROOT, check=True)
    with (ROOT / 'tmp/phase3-union-code.csv').open() as f:
        lengths = {int(r['pc'], 16): int(r['length']) for r in csv.DictReader(f)}
    hooks = []
    for pc in sorted({a[0] for a in accesses}):
        if pc not in lengths:
            raise SystemExit(f'I/O site outside covered ROM: {pc:x}')
        try:
            op, size, source, dest, length = decode_hook(int.from_bytes(image[pc:pc+2], 'big'))
        except ValueError as e:
            raise SystemExit(f'{pc:06x}: {e}')
        if length != lengths[pc]:
            raise SystemExit(f'length disagreement at {pc:06x}: {length} != {lengths[pc]}')
        if any(s != size for p, a, s, d in accesses if p == pc):
            raise SystemExit(f'access size disagreement at {pc:06x}')
        hooks.append([f'{pc:06x}', length, op, size, *source, *dest])
    write_csv(args.out / 'io-sites.csv', ['pc', 'length', 'operation', 'size', 'source_ea', 'source_register', 'source_extension', 'dest_ea', 'dest_register', 'dest_extension'], hooks)
    write_csv(args.out / 'io-accesses.csv', ['pc', 'address', 'size', 'direction'], [(f'{p:06x}', f'{a:06x}', s, d) for p, a, s, d in sorted(accesses)])
    # A conservative aligned-long candidate list: no assertion that immediate
    # integers are pointers. Keep values local; only offsets enter metadata.
    candidates = []
    for pc, length in sorted(lengths.items()):
        for offset in range(2, length - 3, 2):
            value = int.from_bytes(image[pc+offset:pc+offset+4], 'big')
            if 0 < value < 0x40000:
                candidates.append([f'{pc:06x}', offset, 'unclassified_rom_range_long'])
    write_csv(args.out / 'relocation-candidates.csv', ['pc', 'operand_offset', 'classification'], candidates)
    covered = sum(v.bit_count() for v in union)
    short = sum(r[1] == 2 for r in hooks)
    print(f'Covered PCs: {covered}; ROM PCs: {len(lengths)}; I/O sites: {len(hooks)} ({short} two-byte); access descriptors: {len(accesses)}; unresolved operand candidates: {len(candidates)}')


if __name__ == '__main__':
    main()
