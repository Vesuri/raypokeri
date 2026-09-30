#!/usr/bin/env python3
"""Summarize read-only release-timing.gdb observations from a normal binary.

--scenario double reads amiga/release-double.gdb logs of the DOUBLE_SCENARIO=1
build: a variable number of keyboard rounds ending in an accepted Double.
"""
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


def report(rows, scenario='normal24', double_line=None):
    ready = next(r for r in rows if r['kind'] == 'ready')
    end = next(r for r in rows if r['kind'] == 'end')
    expected = [('status', 4), ('error', 0), ('resets', 0), ('vectors', 1)]
    if scenario == 'normal24':
        expected.append(('inputs', 24))
    if any(end[k] != value for k, value in expected):
        raise ValueError(f'{scenario} scenario did not complete cleanly')
    if scenario == 'double':
        if not double_line or double_line.get('done') != 1 or double_line.get('failed') != 0:
            raise ValueError('Double scenario did not report done=1 failed=0')
        print(f'Double scenario: accepted in round {double_line["rounds"]}; {end["inputs"]} key transitions')
    board, wall = interval(ready, end)
    print(f'Ready to finish: {board:.3f} board s / {wall:.3f} PAL s; ratio {board/wall:.4f}')
    print('Session endpoint uncertainty: up to 20 ms; internal checkpoints: roughly 64 us.')
    keys = {r['index']: r for r in rows if r['kind'] == 'key'} if scenario == 'normal24' else {}
    for label, first, last in [('deal', 3, 5), ('hold', 5, 11), ('draw', 11, 13),
                               ('double-input interval', 13, 15), ('choice interval', 15, 17),
                               ('service door', 21, 23)]:
        if first in keys and last in keys:
            board, wall = interval(keys[first], keys[last])
            print(f'{label}: {board:.3f} board s / {wall:.3f} PAL s; ratio {board/wall:.4f}')
    double = sum(r['kind'] == 'double_accepted' for r in rows)
    ready_flag = keys.get(13, {}).get('double_ready', 'unavailable') if scenario == 'normal24' else \
        next((r['double_ready'] for r in rows if r['kind'] == 'key' and r['code'] in (0x22,0x55) and r['down']), 'unavailable')
    print(f'Observed accepted Double callbacks: {double}; requested Double-ready flag: {ready_flag}')
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
    # One original sound update writes several registers at the same board cycle.
    batches = []
    for r in audio:
        if batches and batches[-1][-1]['cycle'] == r['cycle']:
            batches[-1].append(r)
        else:
            batches.append([r])
    if len(batches) > 1:
        spans = sorted(elapsed(b[-1]) - elapsed(b[0]) for b in batches)
        late = [interval(a[0], b[0]) for a, b in zip(batches, batches[1:])]
        excess = [wall - board for board, wall in late]
        print(f'\nAY write batches: {len(batches)}; median application span '
              f'{spans[len(spans)//2]*1000:.1f} ms PAL (first to last write)')
        print('Batch-to-batch lateness (PAL minus board): ' + ', '.join(
            f'>{t} ms {sum(e*1000 > t for e in excess)}' for t in (20, 50, 100)) +
            f' of {len(excess)}; largest ' + ', '.join(f'{e*1000:.1f}' for e in sorted(excess, reverse=True)[:3]) + ' ms')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--scenario', choices=('normal24', 'double'), default='normal24')
    args = parser.parse_args()
    text = args.log.read_text(errors='replace')
    match = re.search(r'^DOUBLE (.*)$', text, re.M)
    double_line = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', match[1])} if match else None
    report(read_rows(text), args.scenario, double_line)
