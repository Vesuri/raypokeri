#!/usr/bin/env python3
"""Prepare/run an isolated human-operated Help or Escape persistence check.

Source amiga/env.sh. Preparation copies ROMs/saves into tmp; normal drives are
untouched. A human must observe a credit increase before pressing the quit key.
A successful file check alone is not proof that the requested physical key was used.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('nvram.bin', 'nvram.bak', 'accounting.bin', 'accounting.bak')


def validate(key, before, after):
    if key == 'help':
        if after != before:
            raise ValueError('emergency exit changed save or backup images')
        return
    for name in ('nvram.bin', 'accounting.bin'):
        if after[name.replace('.bin', '.bak')] != before[name]:
            raise ValueError('backup differs from preceding save: ' + name)
    data = after['accounting.bin']
    if len(after['nvram.bin']) != 32768 or len(data) != 940 or data[:8] != b'PKAC0001':
        raise ValueError('invalid saved image size or accounting header')
    if zlib.crc32(data[:-4]) != int.from_bytes(data[-4:], 'big'):
        raise ValueError('invalid accounting checksum')
    if all(after[name] == before[name] for name in ('nvram.bin', 'accounting.bin')):
        raise ValueError('no saved progress changed; add a coin before quitting')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--fixture', type=Path, help='run an already prepared fixture')
    p.add_argument('--key', choices=('help', 'esc'))
    for name in ('exe', 'slave', 'rom', 'rtb', 'seed-saves-from'):
        p.add_argument('--' + name, type=Path)
    p.add_argument('--seconds', type=int, default=300)
    args = p.parse_args()
    os.chdir(ROOT)
    if not args.fixture:
        if not args.key or any(getattr(args, n) is None for n in ('exe', 'slave', 'rom', 'rtb', 'seed_saves_from')):
            p.error('preparation requires --key, --exe, --slave, --rom, --rtb and --seed-saves-from')
        command = ['python3', 'tools/test_whdload.py', '--prepare-only', '--vbr', 'moved', '--write-cache', 'enabled']
        for name in ('exe', 'slave', 'rom', 'rtb', 'seed-saves-from'):
            command += ['--' + name, str(getattr(args, name.replace('-', '_')).resolve())]
        output = subprocess.check_output(command, text=True)
        print(output, end='')
        fixture = Path(re.search(r'^Fixture: (.+)$', output, re.M)[1])
        (fixture/'game/data/native-live').unlink()
        config = dict(key=args.key, kickstart=os.environ['KICKSTART'])
        (fixture/'keyboard-check.json').write_text(json.dumps(config, indent=2) + '\n')
        print(f'Prepared only. Run: python3 tools/test_whdload_keys.py --fixture {fixture}')
        return
    fixture = args.fixture.resolve()
    config = json.loads((fixture/'keyboard-check.json').read_text())
    key = config['key']
    if key not in ('help', 'esc') or args.seconds < 1:
        p.error('invalid key or time budget')
    boot, game = fixture/'boot', fixture/'game'
    if (boot/'passed').exists() or (boot/'failed').exists():
        p.error('fixture already used; prepare a fresh one')
    saves = game/'data'
    before = {n: (saves/n).read_bytes() for n in NAMES}
    command = ['fs-uae', '--amiga_model=A1200', '--cpu=68020', '--uae_cpu_model=68020',
               '--uae_cpu_24bit_addressing=false', '--jit_compiler=0', '--chip_memory=2048',
               '--fast_memory=8192', '--kickstart_file='+config['kickstart'],
               '--hard_drive_0='+str(boot), '--hard_drive_0_priority=10', '--hard_drive_1='+str(game),
               '--floppy_drive_0='+str(Path.home()/'Documents/Vette/tmp/Workbenchv2.04rev37.67Workbench.adf'),
               '--joystick_port_0=mouse', '--joystick_port_1=nothing', '--warp_mode=0',
               '--fullscreen=0', '--automatic_input_grab=0', '--window_width=720', '--window_height=568',
               '--state_dir='+str(fixture/'state')]
    print(f'Wait for poker, press Enter once, confirm credits increase, then press {key.upper()}.', flush=True)
    print('Debug audio is muted. The test closes its own emulator after return.', flush=True)
    with (fixture/'keyboard-emulator.log').open('w') as log:
        emu = subprocess.Popen(command, stdout=log, stderr=log, env=dict(os.environ, SDL_AUDIODRIVER='dummy'))
        try:
            deadline = time.monotonic() + args.seconds
            while time.monotonic() < deadline and emu.poll() is None:
                if (boot/'failed').exists():
                    raise RuntimeError('WHDLoad returned failure: '+(boot/'result').read_text(errors='replace'))
                if (boot/'passed').exists():
                    break
                time.sleep(.1)
            if not (boot/'passed').exists():
                raise RuntimeError('no successful WHDLoad return within test budget')
            validate(key, before, {n: (saves/n).read_bytes() for n in NAMES})
            print(f'PASS {key.upper()} file/return checks. Human key/credit observation still required.', flush=True)
        finally:
            if emu.poll() is None:
                emu.terminate()
                try:
                    emu.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    emu.kill()
                    emu.wait()


if __name__ == '__main__':
    main()
