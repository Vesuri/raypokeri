#!/usr/bin/env python3
"""ROM-dependent shuffle wait and mid-wait snapshot check; captures stay in tmp/."""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = 'tmp/shuffle-check'
COMMON = ['build/pokeri-host', '--devices', '--skip-hardware-tests']


def run(name, args):
    prefix = BASE + '-' + name
    with (ROOT / (prefix + '.log')).open('w') as log:
        subprocess.run(COMMON + args + ['--out', prefix, '--save-state', prefix + '.state'],
                       cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    text = (ROOT / (prefix + '.log')).read_text()
    if 'budget:' not in text:
        raise AssertionError(text)
    return prefix


def main():
    (ROOT / 'tmp').mkdir(exist_ok=True)
    # Keep the normal cabinet watchdog enabled throughout the paced shuffle.
    ready = run('ready', ['--serial-peer', '--system-hz', '100', '--input-hz', '50',
                         '--watchdog-ms', '400', '--watchdog-reset-us', '50000', '--ay-clock', '1000000',
                         '--auto-setup', '--ms', '10000'])
    inputs = BASE + '.inputs'
    (ROOT / inputs).write_text('11000 packet 3 0\n12000 1 0 0xfe\n12200 1 0 0xff\n')
    start = ['--load-state', ready + '.state', '--inputs', inputs, '--shuffle-vblank']
    full = run('full', start + ['--ms', '16000'])
    split = run('split', start + ['--ms', '12500'])
    resumed = run('resumed', ['--load-state', split + '.state', '--shuffle-vblank', '--ms', '16000'])
    assert (ROOT / (full + '.state')).read_bytes() == (ROOT / (resumed + '.state')).read_bytes(), 'mid-wait continuation differs'
    events = (ROOT / (full + '-events.txt')).read_text()
    steps = [tuple(map(int, m)) for m in re.findall(r'shuffle boundary=(\d+) cycles=(\d+) irqs=(\d+)', events)]
    assert [s[0] for s in steps] == list(range(1, 31)), 'expected two passes of 15 visible steps'
    for before, after in zip(steps, steps[1:]):
        assert 144000 <= after[1] - before[1] <= 176000, 'step cadence outside 20 ms +/- 2 ms'
        assert after[2] > before[2], 'interrupt delivery stopped during wait'
    assert 'watchdog CPU reset' not in events
    split_events = (ROOT / (split + '-events.txt')).read_text()
    assert 0 < len(re.findall(r'shuffle boundary=', split_events)) < 30, 'snapshot did not split the shuffle'
    print('PASS: 30 VBlank steps, 20 ms cadence, live IRQ delivery, exact full-state continuation through a mid-shuffle snapshot with normal watchdog enabled')


if __name__ == '__main__':
    main()
