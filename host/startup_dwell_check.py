#!/usr/bin/env python3
"""Compare original-code diagnostic returns; captures stay under tmp/. No SDL."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', default='build/pokeri-host')
    args = parser.parse_args()
    common = [args.exe, '--devices', '--serial-peer', '--system-hz', '100',
              '--input-hz', '50', '--watchdog-ms', '400', '--watchdog-reset-us', '50000',
              '--ay-clock', '1000000', '--palette-rom', '0', '--stall-instructions', '0',
              '--skip-hardware-tests', '--auto-setup', '--instructions', '10000000']
    for name, bases in [('physical', None), ('relocated', (0x512300, 0x684680, 0x923400))]:
        extra = []
        if bases:
            for option, value in zip(('--rom-base', '--ram-base', '--device-base'), bases):
                extra += [option, hex(value)]
            extra += ['--io-table', 'host/tables/io-accesses.csv']
        extra += ['--break-pc', '0x1392']
        runs = []
        for mode in ('original', 'short'):
            prefix = 'tmp/dwell-check-' + name + '-' + mode
            command = common + extra + ['--out', prefix]
            if mode == 'original':
                command.append('--diagnostic-display-delays')
            with (ROOT / (prefix + '.log')).open('w') as log:
                result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
            assert result.returncode == 2 and 'breakpoint:' in (ROOT / (prefix + '.log')).read_text(), prefix
            context = (ROOT / (prefix + '-context.txt')).read_text().splitlines()
            counts = re.fullmatch(r'instructions=(\d+) cycles=(\d+) pc=([0-9a-f]+) sr=([0-9a-f]+)', context[1])
            assert counts, prefix
            registers = (counts[3], counts[4], context[2:6])
            ram = (ROOT / (prefix + '-ram.bin')).read_bytes()
            vram = (ROOT / (prefix + '-vram.bin')).read_bytes()
            devices = (ROOT / (prefix + '-devices.txt')).read_text().splitlines()
            # External clock edge counts intentionally differ. CPU IRQs remain
            # masked throughout this routine; all AY and video state must match.
            assert devices[0].startswith('IRQs=0 ')
            runs.append((registers, ram, vram, devices[1:], int(counts[1]), int(counts[2])))
        old, new = runs
        assert old[:4] == new[:4], name + ': CPU, full RAM, VRAM or device state differs'
        assert len(new[1]) == 262144 and len(new[2]) == 524288
        assert old[4] - new[4] == 953784
        assert old[5] - new[5] == 14068314
        print('PASS', name, ': complete CPU/RAM/VRAM/AY/video equality; 953784 diagnostic instructions removed', flush=True)


if __name__ == '__main__':
    main()
