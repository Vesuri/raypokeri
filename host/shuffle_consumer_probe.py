#!/usr/bin/env python3
"""Compare integrated consumer pacing with the legacy producer reference.

Headless only; ROM-derived captures stay in tmp/. The --wrap fixture alone builds
a copy of the host entry point with a synthetic empty-ring starting position.
"""
from pathlib import Path
import argparse
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = 'tmp/shuffle-consumer-experiment'
OBJECTS = ('m68kcpu m68kops m68kdasm softfloat board hd63484 hd63484drawing '
           'cardbackcache videooutput display serialpeer boardstate cpustate '
           'ayaudio wavoutput window').split()


def replace(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError('host entry point changed; review prototype insertion: ' + old[:80])
    return source.replace(old, new)


def run(command, log):
    with (ROOT / log).open('w') as f:
        subprocess.run(command, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wrap', action='store_true', help='synthetically reposition an empty command ring to exercise wrap')
    args = parser.parse_args()
    (ROOT / 'tmp').mkdir(exist_ok=True)
    run(['make', 'harness'], BASE + '-build.log')
    ready = BASE + '-ready'
    run(['build/pokeri-host', '--devices', '--skip-hardware-tests', '--serial-peer',
         '--system-hz', '100', '--input-hz', '50', '--watchdog-ms', '400',
         '--watchdog-reset-us', '50000', '--ay-clock', '1000000', '--auto-setup',
         '--ms', '10000', '--out', ready, '--save-state', ready + '.state'], ready + '.log')
    (ROOT / (BASE + '.inputs')).write_text('11000 packet 3 0\n12000 1 0 0xfe\n12200 1 0 0xff\n')
    for kind in ("consumer", "producer"):
        source = (ROOT / "host/main.cpp").read_text()
        if args.wrap:
            source = replace(source, '    Window window;if(windowRequested)window.open(cycles);', r'''
    auto get=[&](unsigned a){return (uint32_t(memory[a])<<24)|(uint32_t(memory[a+1])<<16)|(uint32_t(memory[a+2])<<8)|memory[a+3];};
    if(get(0x41326)!=get(0x4132a))throw std::runtime_error("wrap fixture requires an empty ring");
    uint32_t start=get(0x413be)+96;
    for(unsigned a:{0x41326u,0x4132au})for(unsigned i=0;i<4;++i)memory[a+i]=uint8_t(start>>(24-8*i));
    Window window;if(windowRequested)window.open(cycles);''')
        stem = BASE + '-' + kind
        (ROOT / (stem + '.cpp')).write_text(source)
        run(['clang++', '-std=c++11', '-O2', '-g', '-Ihost', '-Ihost/musashi', '-Ibuild',
             '-c', stem + '.cpp', '-o', stem + '.o'], stem + '-build.log')
        run(['clang++', stem + '.o'] + ['build/' + obj + '.o' for obj in OBJECTS] +
            ['-o', stem], stem + '-link.log')
        run([stem, '--devices', '--skip-hardware-tests', '--load-state', ready + '.state',
             '--inputs', BASE + '.inputs', '--shuffle-producer-vblank' if kind == 'producer' else '--shuffle-vblank', '--shuffle-frames',
             '--stall-instructions', '0', '--ms', '13200', '--video-catalog', stem + '.catalog',
             '--out', stem], stem + '.log')
    logs = {kind: (ROOT / (BASE + '-' + kind + '.log')).read_text() for kind in ('producer', 'consumer')}
    consumer_events = (ROOT / (BASE + '-consumer-events.txt')).read_text()
    producer_events = (ROOT / (BASE + '-producer-events.txt')).read_text()
    steps = [tuple(map(int, m)) for m in re.findall(r'shuffle boundary=(\d+) cycles=(\d+) irqs=(\d+)', consumer_events)]
    assert [n for n, _, _ in steps] == list(range(1, 31)), 'lost or duplicated shuffle step'
    for n, _, _ in steps:
        assert (ROOT / (BASE + '-consumer-shuffle-' + str(n) + '.ppm')).read_bytes() == (
            ROOT / (BASE + '-producer-shuffle-' + str(n) + '.ppm')).read_bytes(), f'frame {n} differs'
    commands = {}
    for kind in logs:
        commands[kind] = [(int(x.split()[1]), x.split()[4:]) for x in
                          (ROOT / (BASE + '-' + kind + '.catalog')).read_text().splitlines() if x.startswith('V ')]
    last_producer = int(re.findall(r'shuffle boundary=30 cycles=(\d+)', producer_events)[0])
    a = [w for cycle, w in commands['producer'] if cycle <= last_producer]
    b = [w for cycle, w in commands['consumer'] if cycle <= steps[-1][1]]
    assert a == b, 'original command stream changed through the final shuffle frame'
    events = consumer_events
    assert 'watchdog CPU reset' not in events, 'watchdog reset'
    writes = [int(c) for c in re.findall(r'^AY .* cycle=(\d+)$', events, re.M)]
    during = sum(steps[0][1] <= c <= steps[-1][1] for c in writes)
    assert during > 0, 'sound sequence still held until shuffle completion'
    rows = [tuple(int(x, 16) for x in m) for m in re.findall(
        r'shuffle marker producer=(\w+) consumer=(\w+) begin=(\w+) end=(\w+)', consumer_events)]
    occupancy = max((p-c) % (end-begin) for p, c, begin, end in rows)
    wrapped = any(y[0] < x[0] for x, y in zip(rows, rows[1:]))
    if args.wrap:
        assert wrapped, 'fixture did not exercise a shuffle ring wrap'
    print(f'PASS: 30 byte-identical frames, unchanged {len(a)} commands, {during} AY writes during shuffle, no watchdog reset')
    print(f'Max observed producer-boundary occupancy: {occupancy}/{rows[0][3]-rows[0][2]} bytes; wrap={wrapped}')



if __name__ == '__main__':
    main()
