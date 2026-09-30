#!/usr/bin/env python3
"""Reproduce the bounded W2 opcode inventory and observer-equivalence check."""
import csv
import json
import re
import subprocess
from pathlib import Path
from opcode_audit import summarize
from scenarios.check import COMMON, ROOT


def main():
    prefix = ROOT / 'tmp/w2-opcode'
    accounting = ROOT / 'tmp/w2-opcode-accounting.bin'
    accounting.unlink(missing_ok=True)
    profiles = [
        ('cold', ['--skip-hardware-tests', '--auto-setup', '--accounting-ram', 'tmp/w2-opcode-accounting.bin', '--ms', '1000']),
        ('warm', ['--skip-hardware-tests', '--auto-setup', '--accounting-ram', 'tmp/w2-opcode-accounting.bin', '--ms', '1000']),
        ('play', ['--inputs', 'host/scenarios/play.inputs', '--ms', '65500']),
        ('service', ['--inputs', 'host/scenarios/service.inputs', '--ms', '39000']),
    ]
    captures = []
    for name, extra in profiles + [('control', profiles[2][1])]:
        stem = str(prefix) + '-' + name
        command = ['build/pokeri-host', *COMMON, *extra, '--out', f'tmp/w2-opcode-{name}']
        if name != 'control':
            relative = f'tmp/w2-opcode-{name}.csv'
            command += ['--opcode-audit', relative]
            captures.append(ROOT / relative)
        with open(stem + '.log', 'w') as log:
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        context = Path(stem + '-context.txt').read_text()
        events = Path(stem + '-events.txt').read_text()
        resets = re.findall(r'watchdog CPU reset instruction=\d+ cycles=(\d+)\ninstructions=\d+ cycles=\d+ pc=([0-9a-f]+)', events)
        expected_resets = 1 if name in ('play', 'service', 'control') else 0
        # Hardware-test runs deliberately wait for the documented boot reset.
        if (not context.startswith('budget\n') or len(resets) != expected_resets
                or events.count('watchdog CPU reset') != len(resets)
                or any(int(pc, 16) not in (0x20dc, 0x20e0, 0x20e2)
                       or int(cycle) >= 144000000 for cycle, pc in resets)):
            raise RuntimeError(f'{name}: failed scenario or unexpected reset')
        if name != 'control':
            observed = summarize([captures[-1]])['instruction_entries']
            if observed != int(re.search(r'instructions=(\d+)', context)[1]):
                raise RuntimeError(f'{name}: incomplete entry capture')
        print('Completed', name, flush=True)
    with captures[2].open(newline='') as source:
        pcs = {int(row['pc'], 16) for row in csv.DictReader(source)}
    if not {0x18176, 0x18380} <= pcs:
        raise RuntimeError('play did not reach Double and its choice callback')
    for suffix in ['-ram.bin', '-vram.bin', '-indices.bin', '-cpu-state.bin',
                   '-board-state.bin', '-coverage.bin', '-events.txt', '-context.txt']:
        if Path(str(prefix) + '-play' + suffix).read_bytes() != Path(str(prefix) + '-control' + suffix).read_bytes():
            raise RuntimeError('observer changed play result: ' + suffix)
    result = summarize(captures)
    Path(str(prefix) + '-report.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    print('PASS: opcode observer preserves full play result and event stream')


if __name__ == '__main__':
    main()
