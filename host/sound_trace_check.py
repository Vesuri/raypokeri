#!/usr/bin/env python3
"""Compare original sound-call arguments and live Paula writes in CPU captures.

Checks complete calls within each capture; explicitly reports boundary-clipped
calls. This is live stream evidence, complementary to the linked CPU oracle.
"""
import argparse
from pathlib import Path
import re
import subprocess
from native_trace import ROOT, load_elf, text_word

MASKS = [255, 15, 255, 15, 255, 15, 31, 255, 31, 31, 31, 255, 255, 15, 255, 255]

def check(events):
    pending = None
    seen = False
    consumed = False
    matched = leading = 0
    expected_hash = actual_hash = 0
    for event in events.splitlines():
        parts = event.split()
        if parts[0] == 'G':
            if pending is not None:
                raise ValueError('sound call did not return before next entry')
            reg, strobe, value = map(int, parts[1:])
            if (strobe & 0x82) != 2 or reg >= 15:
                raise ValueError(f'unexpected sound-call arguments: {event}')
            pending = (reg, value & MASKS[reg])
            consumed = False
            seen = True
        elif parts[0] == 'P':
            actual = tuple(map(int, parts[1:]))
            if pending is None:
                if seen or leading:
                    raise ValueError('unexpected backend write without a guest call')
                leading += 1
                continue
            if consumed or actual != pending:
                raise ValueError(f'backend {actual} differs from expected {pending}, duplicate={consumed}')
            consumed = True
            for value in pending:
                expected_hash = ((expected_hash * 33) ^ value) & 0xffffffff
            for value in actual:
                actual_hash = ((actual_hash * 33) ^ value) & 0xffffffff
            matched += 1
        elif parts[0] == 'R':
            if pending is not None and not consumed:
                raise ValueError('guest returned without delivering its sound write')
            pending = None
        else:
            raise ValueError(f'unknown event: {event}')
    return matched, leading, int(pending is not None and not consumed), expected_hash, actual_hash


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    args = parser.parse_args()
    elf = args.run / 'Pokeri.elf'
    symbols, _, _, _ = load_elf(elf)
    entries = [a for a, _, name in symbols if name == 'PaulaAy::write']
    if len(entries) != 1:
        raise ValueError('PaulaAy::write is not unique')
    entry = entries[0]
    # Prove D1 and D2 hold index/value at entry+16, independently of symbol offsets.
    prologue = [0x48e7, 0x3800, 0x206f, 0x0010, 0x222f, 0x0014, 0x242f, 0x0018]
    if [text_word(elf, entry + 2*i) for i in range(8)] != prologue:
        raise ValueError('unrecognized Paula write ABI/prologue')
    base = re.search(r'TRACE base rom=([0-9a-f]+)', (args.run / 'gdb-out.log').read_text())
    if base is None:
        raise ValueError('missing ROM base')
    binary = ROOT / 'tmp/sound_trace_reduce'
    subprocess.run(['c++', '-O2', '-std=c++11', str(ROOT / 'host/sound_trace_reduce.cpp'), '-o', str(binary)], check=True)
    traces = sorted(args.run.glob('trace-*.bin'))
    if not traces:
        raise ValueError('no captures')
    total = 0
    for trace in traces:
        events = subprocess.check_output([str(binary), base[1], 'd58', 'd84', f'{entry+16:x}', str(trace)], text=True)
        count, leading, trailing, expected, actual = check(events)
        total += count
        print(f'{trace.name}: matched={count} clipped_before={leading} clipped_after={trailing} expected={expected:08x} actual={actual:08x}', flush=True)
    if not total:
        raise ValueError('no matched sound writes')
    print(f'PASS: {total} ordered live AY writes match original call arguments')

if __name__ == '__main__':
    main()
