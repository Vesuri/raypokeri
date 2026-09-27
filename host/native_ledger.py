#!/usr/bin/env python3
"""Summarize a TIME_LEDGER native capture (reserved CIA timer, E-clock units).

Reads the big-endian ledger snapshots dumped by amiga/ledger.gdb. Scopes
are inclusive; children are reported under their enclosing kind so exclusive
time can be derived without double counting. Guest time is the measured
original-program interval ledger (8 MHz board-cycle units of wall time).
"""
import argparse
import re
import struct

KINDS = ['Service', 'BoardTick', 'Present', 'Guard', 'AyTick', 'AyVbi', 'BlitWait',
         'VideoBus', 'ShortCall', 'Command', 'Backpressure', 'HookExec', 'Prologue']
TOP = len(KINDS)
NAMES = [0, "ORG", "WPR", "RPR", 0, 0, "WPTN", "RPTN", 0, "DRD", "DWT", "DMOD", 0, 0, 0, 0,
         0, "RD", "WT", "MOD", 0, 0, "CLR", "SCLR", "CPY", "CPY", "CPY", "CPY", "SCPY", "SCPY", "SCPY", "SCPY",
         "AMOVE", "RMOVE", "ALINE", "RLINE", "ARCT", "RRCT", "APLL", "RPLL", "APLG", "RPLG", "CRCL", "ELPS",
         "AARC", "RARC", "AEARC", "REARC", "AFRCT", "RFRCT", "PAINT", "DOT", "PTN", "PTN", "PTN", "PTN",
         "AGCPY", "AGCPY", "AGCPY", "AGCPY", "RGCPY", "RGCPY", "RGCPY", "RGCPY"]
ETICK = 1 / 709379.0  # PAL E-clock seconds per tick
LEDGER_WORDS = 6 + 2 * (TOP + 1) * TOP + 3 * 64
MARKS = {0: 'ready', 3: 'deal down', 5: 'hold', 11: 'draw down', 13: 'double down',
         15: 'big down', 17: 'lamp panel', 19: 'coin', 21: 'door open', 23: 'door close', 25: 'finish'}


def ledger(words):
    head = dict(zip(['clock', 'cycles', 'guest', 'hooked', 'short', 'dispatches'], words[:6]))
    n = (TOP + 1) * TOP
    ticks = [words[6 + p * TOP:6 + (p + 1) * TOP] for p in range(TOP + 1)]
    calls = [words[6 + n + p * TOP:6 + n + (p + 1) * TOP] for p in range(TOP + 1)]
    base = 6 + 2 * n
    head.update(ticks=ticks, calls=calls, opTicks=words[base:base + 64],
                opCalls=words[base + 64:base + 128], opMax=words[base + 128:base + 192])
    return head


def read_ledgers(path):
    data = open(path, 'rb').read()
    size = LEDGER_WORDS * 4
    return [ledger(struct.unpack('>%dI' % LEDGER_WORDS, data[i:i + size]))
            for i in range(0, len(data) - size + 1, size)]


def delta(a, b):
    """b - a for all counters; TOD is 24-bit."""
    out = {k: (b[k] - a[k]) & 0xffffffff for k in ['cycles', 'guest', 'hooked', 'short', 'dispatches']}
    out['clock'] = (b['clock'] - a['clock']) & 0xffffffff
    out['ticks'] = [[(y - x) & 0xffffffff for x, y in zip(pa, pb)] for pa, pb in zip(a['ticks'], b['ticks'])]
    out['calls'] = [[(y - x) & 0xffffffff for x, y in zip(pa, pb)] for pa, pb in zip(a['calls'], b['calls'])]
    out['opTicks'] = [(y - x) & 0xffffffff for x, y in zip(a['opTicks'], b['opTicks'])]
    out['opCalls'] = [(y - x) & 0xffffffff for x, y in zip(a['opCalls'], b['opCalls'])]
    out['opMax'] = b['opMax']
    return out


def report(title, d, read_cost):
    """read_cost: E-ticks per ledger timestamp. Each scope's own interval
    holds about one timestamp; every enclosing scope also holds both of a
    nested scope's timestamps. Corrected values remove that observer cost."""
    wall = d['clock'] * ETICK
    board = d['cycles'] / 8e6
    if wall <= 0:
        return
    t, n = d['ticks'], d['calls']
    total_calls = [sum(n[p][k] for p in range(TOP + 1) if p != k) for k in range(TOP)]
    memo = {}
    def descendants(k):
        # Nested scope calls beneath all calls of kind k, attributing a
        # child's own descendants in proportion to its calls under k.
        if k not in memo:
            memo[k] = 0.0
            memo[k] = sum(n[k][j] * (1 + descendants(j) / max(1, total_calls[j]))
                          for j in range(TOP) if j != k)
        return memo[k]
    def corrected(child, parent):
        share = n[parent][child] / max(1, total_calls[child])
        raw = t[parent][child]
        return max(0.0, raw - read_cost * (n[parent][child] + 2 * descendants(child) * share)) * ETICK
    inc = [sum(corrected(k, p) for p in range(TOP + 1) if p != k) for k in range(TOP)]
    by = corrected
    guest = d['guest'] / 8e6
    observer = 2 * read_cost * sum(total_calls) * ETICK
    print(f"\n== {title}: wall {wall:.2f} s, board {board:.2f} s (board/wall {board / wall:.3f}); "
          f"dispatches {d['dispatches']}, short calls {d['short']}")
    def row(name, seconds, note=''):
        print(f"  {name:40s} {seconds:8.2f} s {100 * seconds / wall:6.1f}%  {note}")
    row('guest (measured original code)', guest)
    row('full dispatch (C, inclusive)', inc[0], f"{total_calls[0]} calls, {1e6 * inc[0] / max(1, total_calls[0]):.0f} us each")
    row('  presentation (inclusive)', by(2, 0), f"{n[0][2]} calls")
    row('    blitter wait inside presentation', by(6, 2))
    row('    backpressure inside presentation', by(10, 2))
    row('  prepared hook execution', by(11, 0), f"{n[0][11]} calls")
    row('  board tick (inclusive)', by(1, 0))
    row('  video bus in full dispatch', by(7, 0))
    row('  guard', by(3, 0))
    row('masked C prologue of full dispatch', inc[12], f"{total_calls[12]} calls, {1e6 * inc[12] / max(1, total_calls[12]):.0f} us each")
    row('short-path C calls (inclusive)', inc[8], f"{total_calls[8]} calls, {1e6 * inc[8] / max(1, total_calls[8]):.0f} us each")
    row('commands, inclusive (all paths)', inc[9], f"{sum(d['opCalls'])} completed of {total_calls[9]} FIFO writes")
    row('  blitter wait inside commands', by(6, 9))
    row('  backpressure inside commands', by(10, 9))
    row('blitter wait elsewhere', inc[6] - by(6, 9) - by(6, 2))
    top = sum(corrected(k, TOP) for k in range(TOP))
    row('observer (ledger timestamps)', observer)
    residual = wall - guest - top - observer
    # Hook entries: every full dispatch plus every completed short hook
    # (promotions count twice, so this is a slight overestimate).
    entries = total_calls[0] + d['short']
    row('residual (asm hooks, exceptions, IRQs)', residual,
        f"~{1e6 * residual / max(1, entries):.0f} us per hook entry, IRQs included")
    ops = sorted(range(64), key=lambda g: -d['opTicks'][g])
    per_command_desc = descendants(9) / max(1, total_calls[9])
    print("  per command group (inclusive, includes its blitter waits):")
    for g in ops:
        if not d['opCalls'][g]:
            continue
        sec = max(0.0, d['opTicks'][g] - read_cost * d['opCalls'][g] * (1 + 2 * per_command_desc)) * ETICK
        print(f"    {NAMES[g] or '?':6s}{g:3d} {d['opCalls'][g]:6d} calls {sec:8.2f} s {100 * sec / wall:5.1f}%"
              f"  mean {1e3 * sec / d['opCalls'][g]:7.3f} ms  run-max {1e3 * d['opMax'][g] * ETICK:7.1f} ms")


def frames(path, start, end):
    data = open(path, 'rb').read()
    recs = [struct.unpack('>8I', data[i:i + 32]) for i in range(0, len(data) - 31, 32)]
    print("\nVBI  wall-ms board-ms guest-ms service-ms command-ms present-ms blitwait-ms short-ms")
    prev = None
    for i, r in enumerate(recs):
        if prev and start <= r[1] <= end:
            dw = ((r[0] - prev[0]) & 0xffffffff) * ETICK * 1e3
            vals = [(r[k] - prev[k]) & 0xffffffff for k in range(1, 8)]
            print(f"{i:5d} {dw:7.1f} {vals[0] / 8e3:8.1f} {vals[1] / 8e3:8.1f} " +
                  ' '.join(f"{v * ETICK * 1e3:10.1f}" for v in vals[2:]))
        prev = r


def slow(path, start, end):
    data = open(path, 'rb').read()
    print("\nSlow commands (>= 2 ms): board-ms  ms  words")
    for i in range(0, len(data) - 27, 28):
        clock, ticks, cycles = struct.unpack('>3I', data[i:i + 12])
        words = struct.unpack('>8H', data[i + 12:i + 28])
        if start <= cycles <= end:
            n = NAMES[words[0] >> 10] or '?'
            print(f"  {cycles / 8e3:9.1f} {1e3 * ticks * ETICK:7.2f}  {n:6s} " + ' '.join(f"{w:04x}" for w in words))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--marks', default='tmp/ledger-marks.bin')
    ap.add_argument('--frames', default='tmp/ledger-frames.bin')
    ap.add_argument('--frame-window', nargs=2, type=int, metavar=('CYCLE0', 'CYCLE1'))
    ap.add_argument('--slow', default='tmp/ledger-slow.bin')
    ap.add_argument('--slow-window', nargs=2, type=int, metavar=('CYCLE0', 'CYCLE1'))
    ap.add_argument('--log', required=True, help='gdb-out.log with the LEDGER readcost line')
    args = ap.parse_args()
    costs = [int(m.group(1)) for m in re.finditer(r'LEDGER readcost=(\d+)', open(args.log).read())]
    if not costs:
        raise SystemExit('no LEDGER readcost line in ' + args.log)
    args.read_cost = costs[-1] / 256.0
    marks = read_ledgers(args.marks)
    points = [i for i in sorted(MARKS) if marks[i]['clock']]
    for a, b in zip(points, points[1:]):
        report(f"{MARKS[a]} -> {MARKS[b]}", delta(marks[a], marks[b]), args.read_cost)
    report('ready -> finish', delta(marks[0], marks[25]), args.read_cost)
    if args.frame_window:
        frames(args.frames, *args.frame_window)
    if args.slow_window:
        slow(args.slow, *args.slow_window)


if __name__ == '__main__':
    main()
