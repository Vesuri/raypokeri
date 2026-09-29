#!/usr/bin/env python3
"""Synthetic trace: selected Line-A calls, nested IRQ exclusion and unchanged totals."""
import csv
from pathlib import Path
import struct
import subprocess
import tempfile
from native_trace import reducer

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
