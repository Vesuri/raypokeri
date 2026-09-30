#!/usr/bin/env python3
"""Compare aligned AY batches in two read-only release-timing captures.

An identical sound-write sequence does not prove identical intervening graphics.
This is attribution evidence, not a whole-game performance gate.
"""
import argparse
import difflib
import statistics
from pathlib import Path
from release_timing import elapsed, interval, read_rows


def batches(rows):
    result = []
    for row in rows:
        if row['kind'] != 'ay':
            continue
        if result and result[-1][-1]['cycle'] == row['cycle']:
            result[-1].append(row)
        else:
            result.append([row])
    return result


def compare(first, second):
    a, b = batches(first), batches(second)
    signature = lambda batch: tuple((r['reg'], r['value']) for r in batch)
    matcher = difflib.SequenceMatcher(None, list(map(signature, a)),
                                      list(map(signature, b)), autojunk=False)
    pairs = [(i + k, j + k) for i, j, n in matcher.get_matching_blocks()
             for k in range(n)]
    span = lambda batch: elapsed(batch[-1]) - elapsed(batch[0])
    deltas = [(span(b[j]) - span(a[i])) * 1000 for i, j in pairs]
    adjacent = set(pairs)
    gaps = []
    for i, j in pairs:
        if (i + 1, j + 1) not in adjacent:
            continue
        ac = a[i + 1][0]['cycle'] - a[i][0]['cycle']
        bc = b[j + 1][0]['cycle'] - b[j][0]['cycle']
        if ac != bc:
            continue
        gaps.append((interval(b[j][0], b[j + 1][0])[1] -
                     interval(a[i][0], a[i + 1][0])[1]) * 1000)
    return {'first_batches': len(a), 'second_batches': len(b),
            'span_delta_ms': deltas, 'equal_board_gap_delta_ms': gaps}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('first', type=Path)
    parser.add_argument('second', type=Path)
    args = parser.parse_args()
    result = compare(read_rows(args.first.read_text()), read_rows(args.second.read_text()))
    print(f'Batches: {result["first_batches"]} first / {result["second_batches"]} second')
    for label, key in [('Identical aligned batch spans', 'span_delta_ms'),
                       ('Adjacent matches with equal board intervals', 'equal_board_gap_delta_ms')]:
        values = result[key]
        if not values:
            print(f'{label}: no matches')
            continue
        print(f'{label}: {len(values)}; second minus first median '
              f'{statistics.median(values):.3f} ms, range '
              f'{min(values):.3f}..{max(values):.3f} ms')
    print('Matching audio does not establish matching graphics; repeated signatures can be ambiguous.')


if __name__ == '__main__':
    main()
