#!/usr/bin/env python3
"""Report measured native dispatcher call distributions, never ROM bytes."""
import argparse
import csv
from collections import Counter
from pathlib import Path
import re
import struct

ROUTINES = ['clock pause', 'guest-cycle charge', 'prepared hook executor',
            'generic hook executor', 'virtual RTE', 'virtual USP transfer',
            'virtual SR read', 'virtual SR logic', 'board reset',
            'push virtual exception', 'guard check', 'Board::irq',
            'advanceClock / Board::tick', 'liveInputs', 'coldSetupStep',
            'screen presentation', 'publish video status', 'clock calibration']


def words(path):
    raw = Path(path).read_bytes()
    if len(raw) % 4:
        raise ValueError(f'partial word: {path}')
    return list(struct.unpack('>' + 'I' * (len(raw) // 4), raw))


def counters(prefix, stride=4):
    result = {k: words(f'{prefix}-{k}.bin') for k in ('hooks', 'kinds', 'routines')}
    short = words(f'{prefix}-short.bin')
    if len(short) % stride:
        raise ValueError('partial short descriptor')
    result['short'] = short[3::stride]
    traps = Path(f'{prefix}-traps.bin')
    result['traps'] = words(traps)[3::stride] if traps.exists() else [0]*16
    return result


def table(text, name):
    match = re.search(r'\b' + name + r'\[\]\s*=\s*\{(.*?)\n\};', text, re.S)
    if not match:
        raise ValueError(f'missing table {name}')
    return match[1]


def metadata(path):
    text = Path(path).read_text()
    pattern = (r'\{(0x[0-9a-f]+),(\d+),(\d+),Operation::(\w+),'
               r'\{Ea::(\w+),(-?\d+),(-?\d+)\},\{Ea::(\w+),(-?\d+),(-?\d+)\}\}')
    hooks = re.findall(pattern, table(text, 'hooks'))
    accesses = re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+),(\d+),(true|false)\}', table(text, 'accesses'))
    ranges = re.findall(r'\{(\d+),(\d+),(\d+)\}', table(text, 'hookMetadata'))
    if not hooks or len(hooks) != len(ranges):
        raise ValueError('inconsistent hook metadata')
    labels = []
    for h, (first, last, _) in zip(hooks, ranges):
        pc, _, size, op, src, sr, _, dst, dr, _ = h
        ports = sorted({int(a[1], 16) for a in accesses[int(first):int(last)]})
        device = '/'.join(f'${a:05X}' for a in ports) or 'ROM/RAM'
        operand = lambda kind, reg: kind + (f'({reg})' if int(reg) >= 0 else '')
        labels.append((int(pc, 16), f'{op}.{int(size)*8} {operand(src,sr)} → {operand(dst,dr)} [{device}]'))
    operations = {int(r['pc'],16):r['operation'] for r in csv.DictReader(
        (Path(__file__).resolve().parents[1]/'host/tables/cpu-control-hooks.csv').open())}
    for pc in re.findall(r'0x[0-9a-f]+',table(text,'controls')):
        labels.append((int(pc,16),'virtual '+operations[int(pc,16)]))
    return labels


def report(name, data, labels, seconds=None, limit=18):
    kinds, hooks, short, routines = (data[k] for k in ('kinds', 'hooks', 'short', 'routines'))
    if len(hooks) != 4096 or len(kinds) != 48 or len(routines) != len(ROUTINES) or len(short) != len(labels):
        raise ValueError('counter layout does not match reporter')
    full = sum(kinds)
    short_total = sum(short) + sum(data['traps'])
    entries = full - kinds[11] + short_total
    if sum(hooks) != kinds[10]:
        raise ValueError('full Line-A counters do not reconcile')
    print(f'\n## {name}\n\n{entries:,} entries: {full:,} full C dispatches and {short_total:,} short assembly accesses.')
    if kinds[11]:
        print(f'{kinds[11]:,} dispatcher calls promote an already-counted short access; these are not extra entries.')
    if seconds:
        print(f'{entries / seconds:,.1f} entries per board-second.')
    print('\n### Entry types\n\n| Entry | Count | Share |\n|---|---:|---:|')
    entry_names = {0: 'CPU fault', 9: 'trace / scheduler', 10: 'full Line-A', 11: 'short scheduling/replay promotion'}
    for kind, n in sorted(enumerate(kinds), key=lambda row: -row[1]):
        if n and kind != 11:
            label = entry_names.get(kind, f'TRAP #{kind-32}' if kind >= 32 else f'kind {kind}')
            print(f'| {label} | {n:,} | {100*n/entries:.2f}% |')
    print(f'| short assembly | {short_total:,} | {100*short_total/entries:.2f}% |')
    for number, n in enumerate(data['traps']):
        if n:
            print(f'| ↳ short TRAP #{number} | {n:,} | {100*n/entries:.2f}% |')
    families, sites = Counter(), []
    for i, (pc, label) in enumerate(labels):
        for path, n in [('C', hooks[i]), ('ASM', short[i])]:
            if n:
                families[f'{path}: {label}'] += n
                sites.append((n, pc, path, label))
    for index, label in [(0xffc, 'virtual CPU control'), (0xffd, 'RESET'), (0xffe, 'RAM relocation setup')]:
        if hooks[index]:
            families[label] += hooks[index]
    if sum(families.values()) != kinds[10] + sum(short):
        raise ValueError('unclassified Line-A entries')
    print('\n### Hook families (share of all entries)\n\n| Handler / operand form | Calls | Share |\n|---|---:|---:|')
    shown = families.most_common(limit)
    for label, n in shown:
        print(f'| {label} | {n:,} | {100*n/entries:.2f}% |')
    other = sum(families.values()) - sum(n for _, n in shown)
    if other:
        print(f'| Other hook forms | {other:,} | {100*other/entries:.2f}% |')
    print('\n### Direct dispatcher work\n\nCounts overlap: one dispatch calls several helpers. They are not time percentages.')
    print('\n| Routine or control operation | Calls | Calls / full dispatch |\n|---|---:|---:|')
    for label, n in sorted(zip(ROUTINES, routines), key=lambda row: -row[1]):
        if n:
            print(f'| {label} | {n:,} | {n/max(1,full):.3f} |')
    print('\n### Most frequent original access sites\n\n| PC | Path | Calls | Operation |\n|---|---|---:|---|')
    for n, pc, path, label in sorted(sites, reverse=True)[:limit]:
        print(f'| ${pc:05X} | {path} | {n:,} | {label} |')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--tables', default='amiga/generated/NativeTables.h')
    p.add_argument('--ready', required=True, help='startup snapshot prefix')
    p.add_argument('--end', required=True, help='completed run snapshot prefix')
    p.add_argument('--play-seconds', type=float, required=True)
    p.add_argument('--limit', type=int, default=18)
    p.add_argument('--descriptor-bytes',type=int,choices=(16,32),default=32,help='use 16 for historical captures')
    args = p.parse_args()
    if args.play_seconds <= 0 or args.limit < 1:
        p.error('play seconds and row limit must be positive')
    labels = metadata(args.tables)
    ready, end = counters(args.ready,args.descriptor_bytes//4), counters(args.end,args.descriptor_bytes//4)
    # Historical captures predate directly indexed CPU-control descriptors.
    labels = labels[:len(ready["short"])]
    if any(len(ready[k]) != len(end[k]) for k in ready):
        p.error('snapshot counter layouts differ')
    play = {k: [b-a for a,b in zip(ready[k],end[k])] for k in ready}
    if any(n < 0 for values in play.values() for n in values):
        p.error('counters reversed or wrapped')
    report('Startup through operator setup', ready, labels, limit=args.limit)
    report('Gameplay after ready', play, labels, args.play_seconds, args.limit)


if __name__ == '__main__':
    main()
