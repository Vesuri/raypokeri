#!/usr/bin/env python3
"""Synthetic trace: selected Line-A calls, nested IRQ exclusion and unchanged totals."""
import csv
from pathlib import Path
import struct
import subprocess
import tempfile
from native_trace import reducer, card_counter_sites

word = lambda *values: struct.pack('<' + 'I'*len(values), *values)

def rows(path):
    with path.open() as f:
        return list(csv.DictReader(f, delimiter='\t'))

with tempfile.TemporaryDirectory(prefix='pokeri-trace-') as directory:
    root = Path(directory)
    config = root/'config'
    config.write_text('kind lineA\nkind vbi\nentry 100 0\nentry 300 1\ncall 110 112\nrte 120\nrte 304\nguest 1000\ntext 1000\nfull 200\n')
    records = [
        (0x1c3e, 0x8000, 1), (0x100, 0x7ffa, 2), (0x110, 0x7ffa, 3),
        (0x200, 0x7ff6, 5), (0x202, 0x7ff6, 7),
        (0x300, 0x7ff0, 11), (0x302, 0x7ff0, 13), (0x304, 0x7ff0, 17),
        (0x204, 0x7ff6, 11), (0x112, 0x7ffa, 17), (0x120, 0x7ffa, 19),
        (0x1c40, 0x8000, 1), (0x100, 0x7ffa, 23), (0x110, 0x7ffa, 29),
        (0x200, 0x7ff6, 31), (0x112, 0x7ffa, 37), (0x120, 0x7ffa, 41),
        (0x1c42, 0x8000, 1),
    ]
    data = b''
    for pc, sp, cycles in records:
        regs = [0]*17
        regs[15] = sp
        data += word(pc, 0xffffffff-cycles) + word(*regs)
    trace = root/'trace.bin'
    trace.write_bytes(word(1, 0) + bytes(16) + word(0, 0, 0, 0, 0) +
                     word(520) + bytes(520) + word(0, 0, 0, 0, 0) +
                     word(sum(r[2] for r in records), 0, len(data)//4) + data + word(0, 0))
    for name, args in [('all', []), ('tick', ['--site', '0xc3e']), ('absent', ['--site', '0x100'])]:
        out = root/name
        out.mkdir()
        subprocess.run([str(reducer()), str(config), str(out), str(trace)] + args, check=True)
    for filename in ('fields.tsv', 'native.tsv', 'edges.tsv', 'kind_pc.tsv', 'kick.tsv', 'guest.tsv', 'sites.tsv'):
        assert (root/'all'/filename).read_bytes() == (root/'tick'/filename).read_bytes(), filename
    summary = dict(csv.reader((root/'tick/summary.tsv').open(), delimiter='\t'))
    assert int(summary['selected_cycles']) == 64
    counters = {int(r['pc'], 16): r for r in rows(root/'tick/site-native.tsv')}
    assert sum(int(r['self']) for r in counters.values()) == 64
    assert int(counters[0x200]['calls']) == 1
    assert int(counters[0x200]['incl']) == 23
    assert all(pc not in counters for pc in (0x300, 0x302, 0x304))
    assert not rows(root/'absent/site-native.tsv')
print('PASS selected-site trace attribution: exact costs/calls, nested IRQ exclusion, other-site exclusion, unchanged global tables')

# Relocated counter instructions and changing member offsets are independent.
fixture = """
 100: 598f           subq.l #4,sp
 102: 48e7 3f3e      movem.l d2-d7/a2-a6,-(sp)
 106: 246f 0034      movea.l 52(sp),a2
 180: 52aa 000e      addq.l #1,14(a2)
 1a0: 52aa 0016      addq.l #1,22(a2)
 1d0: 52aa 0012      addq.l #1,18(a2)
"""
assert card_counter_sites(fixture, {'starts': 14, 'hits': 18}) == {'starts': 0x180, 'hits': 0x1d0}
assert card_counter_sites(fixture, {'starts': 22, 'hits': 18})['starts'] == 0x1a0
for bad in (fixture.replace('0034', '0038'), fixture + ' 200: 52aa 000e addq.l #1,14(a2)\n',
            fixture.replace('52aa 0012', '52aa 001a'), fixture + ' 220: 2440 movea.l d0,a2\n'):
    try:
        card_counter_sites(bad, {'starts': 14, 'hits': 18})
    except ValueError:
        pass
    else:
        raise AssertionError('ambiguous or unproved card counter accepted')
print('PASS card counters: field offsets, relocated instructions, ABI proof and fail-closed ambiguity checks')

framed = fixture.replace('598f           subq.l #4,sp', '4e55 ffe0      link.w a5,#-32').replace('246f 0034      movea.l 52(sp),a2', '246d 0008      movea.l 8(a5),a2')
assert card_counter_sites(framed, {'guardMisses': 14}) == {'guardMisses': 0x180}
print('PASS frame-pointer ABI card guard counter')

other_register = framed.replace('246d', '266d').replace('52aa', '52ab').replace('a2', 'a3')
assert card_counter_sites(other_register, {'guardMisses': 14}) == {'guardMisses': 0x180}
print('PASS relocated preserved counter register')
