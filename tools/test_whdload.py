#!/usr/bin/env python3
"""Run isolated WHDLoad tests with all fixtures under tmp/.

Source amiga/env.sh first. ROMs and original data are local inputs, never shipped.
The production slave and game run with a finite diagnostic cycle budget.
Repeated runs verify both save files and their previous-image backups.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mode', choices=('smoke', 'boot', 'load', 'quit'), default='quit')
    p.add_argument('--whdload', type=Path, default=Path.home()/'.local/share/amiga/WHDLoad/C/WHDLoad')
    p.add_argument('--rom', type=Path)
    p.add_argument('--rtb', type=Path)
    p.add_argument('--exe', type=Path, default=ROOT/'amiga/out/Pokeri')
    p.add_argument('--seconds', type=int, default=90, help='host safety ceiling')
    p.add_argument('--cpu', default='68020')
    p.add_argument('--fast',type=int,default=8192,help='Fast RAM in KiB')
    p.add_argument('--no-preload', action='store_true')
    p.add_argument('--repeat',type=int,default=1)
    p.add_argument('--standalone',choices=('data','current'),help='test AmigaDOS ROM lookup instead of WHDLoad')
    args = p.parse_args()
    if args.standalone and args.mode!='quit':p.error('--standalone requires quit mode')
    if not args.standalone and args.mode != 'smoke' and (not args.rom or not args.rtb):
        p.error('--rom and --rtb are required except for smoke mode')
    slave = {'smoke':'Smoke.slave', 'boot':'BootTest.slave', 'load':'LoadTest.slave','quit':'Pokeri.slave'}.get(args.mode, 'Pokeri.slave')
    base = Path(tempfile.mkdtemp(prefix='whdload-test-', dir=ROOT/'tmp'))
    print('Fixture:', base, flush=True)
    boot, game = base/'boot', base/'game'
    for d in (boot/'s', boot/'devs/Kickstarts', game/'data', base/'state'):
        d.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.whdload, game/'WHDLoad')
    shutil.copyfile(ROOT/'build/whdload'/slave, game/'Pokeri.slave')
    if args.mode != 'smoke' and not args.standalone:
        shutil.copyfile(args.rom, boot/'devs/Kickstarts'/args.rom.name)
        shutil.copyfile(args.rtb, boot/'devs/Kickstarts'/(args.rom.name+'.RTB'))
    if args.mode in ('load', 'quit'):
        shutil.copyfile(args.exe, game/'data/Pokeri')
    if args.mode in ('quit',):
        for chip in ('77POK30','77POK38','77POK34','PARA200J'):
            shutil.copyfile(ROOT/'rom'/chip,game/'data'/chip)
        (game/'data/native-live').write_bytes((96000000).to_bytes(4,'big'))
    if args.standalone:
        shutil.copyfile(args.exe,game/'Pokeri')
        (game/'native-live').write_bytes((96000000).to_bytes(4,'big'))
        if args.standalone=='current':
            for chip in ('77POK30','77POK38','77POK34','PARA200J'):
                (game/'data'/chip).rename(game/chip)
    saves=game if args.standalone else game/'data'
    (boot/'s/WHDLoad.prefs').write_text('Expert\nReadDelay=0\n')
    preload = '' if args.no_preload else 'PRELOAD '
    command='Pokeri' if args.standalone else f'WHDLoad Pokeri.slave NOVBRMOVE NOWRITECACHE {preload}SPLASHDELAY=0 NOREQ'
    (boot/'s/startup-sequence').write_text(
        'DF0:C/Assign C: DF0:C\nDF0:C/Assign LIBS: DF0:Libs\n'
        'DF0:C/Assign DEVS: DH0:devs\nStack 16384\nFailAt 999\n'
        f'CD DH1:\n{command} >DH0:result\n'
        'If WARN\nEcho failed >DH0:failed\nElse\nEcho passed >DH0:passed\nEndIf\n')
    for attempt in range(args.repeat):
        for name in ('passed','failed','result'):
            (boot/name).unlink(missing_ok=True)
        before={name:(saves/name).read_bytes() for name in ('nvram.bin','accounting.bin') if (saves/name).exists()}
        with (base/'emulator.log').open('w') as log:
            emu = subprocess.Popen(['fs-uae', '--amiga_model=A1200', '--cpu='+args.cpu,
                '--uae_cpu_model='+args.cpu, '--uae_cpu_24bit_addressing=false',
                '--jit_compiler=0', '--chip_memory=2048', '--fast_memory='+str(args.fast),
                '--kickstart_file='+os.environ['KICKSTART'],
                '--hard_drive_0='+str(boot), '--hard_drive_0_priority=10', '--hard_drive_1='+str(game),
                '--floppy_drive_0='+str(Path.home()/'Documents/Vette/tmp/Workbenchv2.04rev37.67Workbench.adf'),
                '--joystick_port_0=mouse', '--joystick_port_1=nothing', '--warp_mode=1', '--fullscreen=0',
                '--window_width=720', '--window_height=568', '--state_dir='+str(base/'state')], stdout=log, stderr=log, env=dict(os.environ,SDL_AUDIODRIVER='dummy'))
            try:
                deadline = time.monotonic()+args.seconds
                while time.monotonic()<deadline and not any((boot/n).exists() for n in ('passed','failed')):
                    if emu.poll() is not None:
                        raise RuntimeError('FS-UAE exited unexpectedly')
                    time.sleep(.25)
                output = (boot/'result').read_text(errors='replace') if (boot/'result').exists() else ''
                report = (game/'.whdl_register').read_text(encoding='latin1') if (game/'.whdl_register').exists() else ''
                assert (boot/'passed').exists() or (boot/'failed').exists(), f'WHDLoad did not return: {base}\n{output}'
                assert (boot/'passed').exists(), report + output
                if args.mode == 'smoke':
                    assert (game/'smoke-passed').read_bytes() == b'PASS'
                print(f'PASS: {args.mode} slave returned normally',flush=True)
                if args.mode=='quit':
                    assert (saves/'nvram.bin').stat().st_size==32768
                    assert (saves/'accounting.bin').exists()
                    for name,old in before.items():
                        assert (saves/name.replace('.bin','.bak')).read_bytes()==old
                    print('PASS: both save files written; previous saves backed up on repeat',flush=True)
            finally:
                emu.terminate()
                try: emu.wait(timeout=5)
                except subprocess.TimeoutExpired: emu.kill(); emu.wait()
    

if __name__ == "__main__":
    main()
