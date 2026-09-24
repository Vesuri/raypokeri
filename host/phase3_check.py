#!/usr/bin/env python3
"""Check the I/O audit gate against existing Phase 2 scenario checkpoints.

This does NOT prove ROM/RAM relocation or native Line-A execution.
"""
import csv
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'host'))
from scenarios.check import COMMON
from phase3_audit import decode_hook


def main():
    # Synthetic instruction shapes exercise independent architectural cases.
    assert decode_hook(0x4a10) == ('test', 1, ('none', -1, -1), ('indirect', 0, -1), 2)
    assert decode_hook(0x10bc) == ('move', 1, ('immediate', -1, 2), ('indirect', 0, -1), 4)
    assert decode_hook(0x11e9)[-1] == 6  # d16(A1) to abs.w
    assert decode_hook(0x23fc)[-1] == 10 # immediate long to absolute long
    assert decode_hook(0x0810)[0] == 'bit_test'
    try:
        decode_hook(0x4e71)
    except ValueError:
        pass
    else:
        raise AssertionError('unsupported instruction accepted')
    table = 'tmp/phase3-audit/io-accesses.csv'
    subprocess.run([sys.executable, 'host/phase3_audit.py'], cwd=ROOT, check=True)
    for name, end, load, script in [
        ('setup', 25000, None, 'play'),
        ('attract', 40500, 'setup', 'play'),
        ('deal', 47500, 'attract', 'play'),
        ('win', 56000, 'deal', 'play'),
        ('double', 65500, 'win', 'play'),
        ('service', 39000, 'setup', 'service'),
    ]:
        prefix = f'tmp/phase3-gated-{name}'
        cmd = ['build/pokeri-host', *COMMON, '--ms', str(end), '--inputs', f'host/scenarios/{script}.inputs',
               '--out', prefix, '--io-table', table, '--save-state', prefix+'.state']
        if load:
            cmd += ['--load-state', f'tmp/phase3-gated-{load}.state']
        if name == 'double':
            cmd += ['--wav']
        subprocess.run(cmd, cwd=ROOT, check=True)
        for suffix in ['-ram.bin', '-coverage.bin', '-nvram.bin', '-devices.txt', '-final.ppm', '.state']:
            assert (ROOT / (prefix+suffix)).read_bytes() == (ROOT / ('tmp/scenario-'+name+suffix)).read_bytes(), (name,suffix)
    assert (ROOT / 'tmp/phase3-gated-double.wav').read_bytes() == (ROOT / 'tmp/scenario-double.wav').read_bytes(), 'gated WAV mismatch'
    # Remove the first actual access, not an arbitrary unused table row.
    with (ROOT / 'tmp/scenario-setup-trace.csv').open() as f:
        first = next(csv.DictReader(f))
    key = (int(first['pc'],16), int(first['address'],16), int(first['size']), first['direction'])
    with (ROOT / table).open() as f:
        rows = list(csv.DictReader(f))
    broken = ROOT / 'tmp/phase3-missing-access.csv'
    with broken.open('w') as f:
        writer = csv.DictWriter(f, ['pc','address','size','direction'], lineterminator='\n')
        writer.writeheader()
        writer.writerows(r for r in rows if (int(r['pc'],16),int(r['address'],16),int(r['size']),r['direction']) != key)
    failed = subprocess.run(['build/pokeri-host', *COMMON, '--ms', '10000', '--out', 'tmp/phase3-missing-access', '--io-table', str(broken)], cwd=ROOT, capture_output=True, text=True)
    assert failed.returncode == 2 and 'I/O access outside audited table' in failed.stdout, (failed.returncode,failed.stdout,failed.stderr)
    print('PASS: six gated scenarios equal baseline; missing descriptor stops before device access')


if __name__ == '__main__':
    main()
