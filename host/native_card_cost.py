#!/usr/bin/env python3
"""Report diagnostic card endpoints, including scopes open at both boundaries.

Scope columns are raw inclusive times: they overlap and must not be added.
Only wall time has timestamp-reader calibration removed; guest time uses the
existing native guest-cycle accounting. Captures remain local under tmp/.
"""
import argparse
from pathlib import Path
import re
import struct
from native_ledger import KINDS

STRIDE = 9 + len(KINDS)


def pairs(rows):
    start = None
    for row in rows:
        if row[0] == 3:
            start = row
        elif row[0] == 6:
            if start is not None:
                yield start, row
            start = None
        else:
            raise ValueError('unknown card endpoint type')


def difference(a, b):
    return (b - a) & 0xffffffff


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--boundaries', type=Path)
    p.add_argument('--log', type=Path)
    p.add_argument('--events', type=Path, help='optional AY/card event stream to identify cache hits')
    p.add_argument('--self-test', action='store_true')
    a = p.parse_args()
    if a.self_test:
        assert list(pairs([(3,), (3, 1), (6, 2), (6, 3)])) == [((3, 1), (6, 2))]
        assert difference(0xfffffff0, 16) == 32
        try:
            list(pairs([(4,)]))
        except ValueError:
            pass
        else:
            raise AssertionError('unknown endpoint accepted')
        print('PASS: replaced/incomplete card starts, unknown records and wrapped counters')
        return
    if not a.boundaries or not a.log:
        p.error('--boundaries and --log are required')
    log = a.log.read_text()
    count = re.search(r'CARD COST count=(\d+) dropped=(\d+)', log)
    calibration = re.search(r'LEDGER readcost=(\d+)', log)
    if not count or int(count[2]) or not calibration:
        p.error('missing/overflowing capture or missing timer calibration')
    raw = a.boundaries.read_bytes()
    if len(raw) != int(count[1]) * STRIDE * 4:
        p.error('card endpoint size/count mismatch')
    rows = list(struct.iter_unpack('>' + 'I' * STRIDE, raw))
    events = list(struct.iter_unpack('>6I', a.events.read_bytes())) if a.events else None
    if events is not None:
        dropped = re.search(r'EVENTS count=(\d+) dropped=(\d+)', log)
        if dropped:
            if int(dropped[2]) or int(dropped[1]) != len(events):
                p.error('incomplete event stream')
        elif len(events) >= 16384:
            # Legacy ledger.gdb omits the counters. eventCount only increments
            # to EventCapacity and never resets during a run; a shorter dump
            # cannot have overflowed. A full buffer needs explicit drop data.
            p.error('full legacy event buffer needs an explicit drop count')
    rate = 709.379  # PAL E-clock ticks per millisecond
    columns = ['Service', 'ShortCall', 'Command', 'Present', 'BlitWait', 'HookExec', 'Prologue']
    print('Scope times are raw/inclusive; do not sum them. Guest ms are measured guest cycles/8000.')
    print('| Start cycle | Cached | Wall ms | Wall less clock-read cost | Guest ms | ' + ' | '.join(columns) + ' | Start capture cost ms |')
    print('|---:' * (6 + len(columns)) + '|')
    for first, last in pairs(rows):
        wall = difference(first[1], last[1])
        reads = difference(first[3], last[3])
        corrected = (wall - reads * int(calibration[1]) / 256) / rate
        costs = [difference(first[9 + KINDS.index(k)], last[9 + KINDS.index(k)]) / rate for k in columns]
        values = [wall / rate, corrected, difference(first[4], last[4]) / 8000] + costs + [first[8] / rate]
        hit = str(int(any(e[2] == 4 and difference(first[1], e[0]) <= wall for e in events))) if events is not None else '?'
        print('| ' + str(first[2]) + ' | ' + hit + ' | ' + ' | '.join(f'{v:.3f}' for v in values) + ' |')
    if rows:
        print(f'Maximum endpoint capture cost: {max(r[8] for r in rows)/rate:.3f} ms; not removed from individual scope columns.')


if __name__ == '__main__':
    main()
