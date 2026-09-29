#!/usr/bin/env python3
"""Summarize read-only release-timing.gdb observations from a normal binary."""
import argparse
from pathlib import Path
import re


def read_rows(text):
    rows = []
    for line in text.splitlines():
        match = re.match(r'RELEASE (\w+) (.*)', line)
        if match:
            fields = {key: int(value, 16 if key == 'error' else 10)
                      for key, value in re.findall(r'(\w+)=([0-9a-f]+)', match[2])}
            rows.append(dict(kind=match[1], **fields))
    return rows


def elapsed(row):
    # Frame count is serviced VBI; beam sampling has 64 us resolution.
    # End has no beam sample, so a session total has up to one frame uncertainty.
    return row['frame'] * .020 + row.get('beam', 0) * .000064


def interval(first, last):
    return (last['cycle'] - first['cycle']) / 8_000_000, elapsed(last) - elapsed(first)


def report(rows):
    ready = next(r for r in rows if r['kind'] == 'ready')
    end = next(r for r in rows if r['kind'] == 'end')
    if any(end[k] != expected for k, expected in
           [('status', 4), ('error', 0), ('resets', 0), ('vectors', 1), ('inputs', 24)]):
        raise ValueError('scenario did not complete cleanly with all 24 inputs')
    board, wall = interval(ready, end)
    print(f'Ready to finish: {board:.3f} board s / {wall:.3f} PAL s; ratio {board/wall:.4f}')
    print('Session endpoint uncertainty: up to 20 ms; internal checkpoints: roughly 64 us.')
    keys = {r['index']: r for r in rows if r['kind'] == 'key'}
    for label, first, last in [('deal', 3, 5), ('hold', 5, 11), ('draw', 11, 13),
                               ('double-input interval', 13, 15), ('choice interval', 15, 17),
                               ('service door', 21, 23)]:
        if first in keys and last in keys:
            board, wall = interval(keys[first], keys[last])
            print(f'{label}: {board:.3f} board s / {wall:.3f} PAL s; ratio {board/wall:.4f}')
    double = sum(r['kind'] == 'double_accepted' for r in rows)
    print(f'Observed accepted Double callbacks: {double}; requested Double-ready flag: '
          f'{keys.get(13, {}).get("double_ready", "unavailable")}')
    if not double:
        print('No confirmed Double workload: do not claim Double performance coverage.')
    print('\nCompleted cached backs (begin -> hit; final publication is separate):')
    starts = {}
    for r in rows:
        if r['kind'] == 'card_begin':
            starts[r['card']] = r
        elif r['kind'] == 'card_hit' and r['card'] in starts:
            board, wall = interval(starts.pop(r['card']), r)
            print(f'  card {r["card"]}: {wall*1000:.3f} ms PAL, {board*1000:.3f} ms board')
    audio = [r for r in rows if r['kind'] == 'ay']
    gaps = []
    for first, last in zip(audio, audio[1:]):
        board, wall = interval(first, last)
        gaps.append((wall-board, wall, board, first, last))
    print('\nLargest excess between consecutive AY writes (not an audible-duration claim):')
    for excess, wall, board, first, last in sorted(gaps, key=lambda g:g[0], reverse=True)[:5]:
        print(f'  cycle {first["cycle"]}->{last["cycle"]}: {wall*1000:.3f} ms PAL, '
              f'{board*1000:.3f} ms board, excess {excess*1000:.3f} ms; '
              f'R{first["reg"]}={first["value"]} -> R{last["reg"]}={last["value"]}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    report(read_rows(args.log.read_text()))
