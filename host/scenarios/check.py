#!/usr/bin/env python3
"""ROM-dependent reference scenarios. All generated data stays in tmp/."""
import argparse
import hashlib
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
COMMON = ['--devices', '--serial-peer', '--system-hz', '100', '--input-hz', '50',
          '--watchdog-ms', '400', '--watchdog-reset-us', '50000', '--ay-clock', '1000000',
          '--palette-rom', '0', '--stall-instructions', '100000000']


def run(binary, name, end, script, load=None, save=True, wav=False):
    prefix = f'tmp/scenario-{name}'
    args = [binary, *COMMON, '--ms', str(end), '--inputs', script, '--out', prefix]
    if load:
        args += ['--load-state', load]
    if save:
        args += ['--save-state', prefix + '.state']
    if wav:
        args += ['--wav']
    subprocess.run(args, cwd=ROOT, check=True)
    return prefix


def digest(path):
    return hashlib.sha256((ROOT / path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default='build/pokeri-host')
    parser.add_argument('--verify', action='store_true', help='also compare uninterrupted and restored execution')
    args = parser.parse_args()
    play = 'host/scenarios/play.inputs'
    setup = run(args.binary, 'setup', 25000, play)
    attract = run(args.binary, 'attract', 40500, play, setup + '.state')
    deal = run(args.binary, 'deal', 47500, play, attract + '.state')
    win = run(args.binary, 'win', 56000, play, deal + '.state')
    double = run(args.binary, 'double', 65500, play, win + '.state', wav=True)
    run(args.binary, 'service', 39000, 'host/scenarios/service.inputs', setup + '.state')
    if args.verify:
        full = run(args.binary, 'uninterrupted', 65500, play, wav=True)
        for suffix in ['-ram.bin', '-nvram.bin', '-coverage.bin', '-context.txt', '-devices.txt', '-final.ppm', '.state']:
            if digest(full + suffix) != digest(double + suffix):
                raise SystemExit('FAIL: uninterrupted/restored mismatch: ' + suffix)
        # The resumed WAV begins at the snapshot time; compare its PCM to that
        # suffix of the uninterrupted stream, not its duration/header.
        import wave
        with wave.open(str(ROOT / (full + '.wav'))) as a, wave.open(str(ROOT / (double + '.wav'))) as b:
            count=b.getnframes();a.setpos(a.getnframes()-count)
            if a.readframes(count)!=b.readframes(count):
                raise SystemExit('FAIL: restored AY waveform differs')
        print('PASS: uninterrupted and restored CPU/RAM/devices/coverage/frame/full state/WAV match')
    print('Scenario captures: tmp/scenario-{attract,deal,win,double,service}-final.ppm')


if __name__ == '__main__':
    main()
