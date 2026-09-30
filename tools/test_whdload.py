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
import socket
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
    p.add_argument('--write-cache',choices=('disabled','enabled'),default='disabled',
                   help='disabled preserves release NOWRITECACHE; enabled tests WHDLoad default')
    p.add_argument('--vbr',choices=('fixed','moved'),default='fixed',
                   help='fixed preserves release NOVBRMOVE; moved tests WHDLoad default')
    p.add_argument('--file-log',action='store_true',help='enable WHDLoad FILELOG')
    p.add_argument('--write-delay',type=int,help='WHDLoad write delay in 1/50-second units')
    p.add_argument('--debug-port',type=int,help='dedicated FS-UAE port; capture CPU state read-only on return/timeout')
    p.add_argument('--prepare-only',action='store_true',help='write isolated fixture without launching FS-UAE')
    p.add_argument('--repeat',type=int,default=1)
    p.add_argument('--standalone',choices=('data','current'),help='test AmigaDOS ROM lookup instead of WHDLoad')
    args = p.parse_args()
    if args.repeat<1 or args.seconds<1:p.error('--repeat and --seconds must be positive')
    if args.write_delay is not None and args.write_delay<0:p.error('--write-delay must be nonnegative')
    if args.standalone and (args.write_cache!='disabled' or args.vbr!='fixed' or args.file_log or args.write_delay is not None):
        p.error('WHDLoad option experiments cannot be combined with --standalone')
    if args.standalone and args.mode!='quit':p.error('--standalone requires quit mode')
    if not args.standalone and args.mode != 'smoke' and (not args.rom or not args.rtb):
        p.error('--rom and --rtb are required except for smoke mode')
    if not args.standalone and args.mode!='smoke':
        if not args.rom.is_file() or args.rom.stat().st_size!=524288:
            p.error('--rom must be a complete 512 KiB Kickstart image, not an installer-test placeholder')
        if not args.rtb.is_file() or not args.rtb.stat().st_size:
            p.error('--rtb must be a nonempty relocation file')
    debug_args=[]
    if args.debug_port is not None:
        if not 1024<=args.debug_port<=65535:p.error('--debug-port must be 1024..65535')
        # Never reclaim another emulator's port.
        with socket.socket() as probe:
            try:probe.bind(('127.0.0.1',args.debug_port))
            except OSError:p.error('--debug-port is already in use')
        debug_args=['--remote_debugger=20','--remote_debugger_port='+str(args.debug_port),
                    '--remote_debugger_trigger='+('Pokeri' if args.standalone else 'WHDLoad')]
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
    options=[]
    if args.vbr=='fixed':options.append('NOVBRMOVE')
    if args.write_cache=='disabled':options.append('NOWRITECACHE')
    if not args.no_preload:options.append('PRELOAD')
    if args.file_log:options.append('FILELOG')
    if args.write_delay is not None:options.append('WRITEDELAY='+str(args.write_delay))
    options+=['SPLASHDELAY=0','NOREQ']
    command='Pokeri' if args.standalone else 'WHDLoad Pokeri.slave '+' '.join(options)
    (base/'command.txt').write_text(command+'\n')
    print('Command:',command,flush=True)
    (boot/'s/startup-sequence').write_text(
        'DF0:C/Assign C: DF0:C\nDF0:C/Assign LIBS: DF0:Libs\n'
        'DF0:C/Assign DEVS: DH0:devs\nStack 16384\nFailAt 999\n'
        f'CD DH1:\n{command} >DH0:result\n'
        'If WARN\nEcho failed >DH0:failed\nElse\nEcho passed >DH0:passed\nEndIf\n')
    if args.prepare_only:return
    for attempt in range(args.repeat):
        for name in ('passed','failed','result'):
            (boot/name).unlink(missing_ok=True)
        before={name:(saves/name).read_bytes() for name in ('nvram.bin','accounting.bin') if (saves/name).exists()}
        with (base/f'emulator-{attempt+1}.log').open('w') as log:
            emu = subprocess.Popen(['fs-uae', '--amiga_model=A1200', '--cpu='+args.cpu,
                '--uae_cpu_model='+args.cpu, '--uae_cpu_24bit_addressing=false',
                '--jit_compiler=0', '--chip_memory=2048', '--fast_memory='+str(args.fast),
                '--kickstart_file='+os.environ['KICKSTART'],
                '--hard_drive_0='+str(boot), '--hard_drive_0_priority=10', '--hard_drive_1='+str(game),
                '--floppy_drive_0='+str(Path.home()/'Documents/Vette/tmp/Workbenchv2.04rev37.67Workbench.adf'),
                '--joystick_port_0=mouse', '--joystick_port_1=nothing', '--warp_mode=1', '--fullscreen=0',
                '--window_width=720', '--window_height=568', '--state_dir='+str(base/'state')]+debug_args, stdout=log, stderr=log, env=dict(os.environ,SDL_AUDIODRIVER='dummy'))
            debugger=None;debug_log=None
            try:
                if args.debug_port is not None:
                    ready=time.monotonic()+20
                    while time.monotonic()<ready:
                        if emu.poll() is not None:raise RuntimeError('FS-UAE exited before debugger startup')
                        if subprocess.run(['lsof','-nP','-iTCP:'+str(args.debug_port),'-sTCP:LISTEN'],stdout=subprocess.DEVNULL).returncode==0:break
                        time.sleep(.25)
                    else:raise RuntimeError('FS-UAE debugger did not listen')
                    debug_log=(base/f'cpu-{attempt+1}.log').open('w')
                    debugger=subprocess.Popen(['m68k-amiga-elf-gdb','-q','-nx',
                        '-ex','set pagination off','-ex','set confirm off',
                        '-ex','set target-async on','-ex','set remotetimeout 10',
                        '-ex',f'target remote 127.0.0.1:{args.debug_port}',
                        '-ex','handle SIGTRAP nostop noprint pass','-ex','continue &'],
                        stdin=subprocess.PIPE,stdout=debug_log,stderr=debug_log,text=True)
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
                try:
                    if debugger is not None and debugger.poll() is None:
                        # CLI async mode lets us restore trap stopping before the
                        # remote interrupt; FS-UAE reports that interrupt as TRAP.
                        debugger.stdin.write('handle SIGTRAP stop print pass\ninterrupt\n')
                        debugger.stdin.flush()
                        time.sleep(1)
                        debugger.stdin.write('info registers\nx/24i $pc\nx/64wx $sp\nx/16wx 0\n'
                            f'dump binary memory {base}/cpu-window-{attempt+1}.bin $pc-128 $pc+512\n'
                            f'dump binary memory {base}/a1-window-{attempt+1}.bin $a1 $a1+256\n'
                            'x/64wx $a1\nx/8wx $a4+0x1588\nx/hx 0xdff002\nx/hx 0xdff01c\n'
                            'detach\nquit\n')
                        debugger.stdin.flush()
                        try:debugger.wait(timeout=20)
                        except subprocess.TimeoutExpired:debugger.kill();debugger.wait()
                except (BrokenPipeError,OSError) as error:
                    print('CPU capture failed:',error,flush=True)
                finally:
                    if debugger is not None and debugger.poll() is None:
                        debugger.kill();debugger.wait()
                    if debug_log is not None:debug_log.close()
                if args.debug_port is not None and not (base/f'cpu-window-{attempt+1}.bin').exists():
                    print('CPU snapshot missing; inspect debugger log before diagnosing the wait',flush=True)
                # Preserve each attempt, including timeout/failure evidence.
                for name,location in [('result',boot/'result'),('register',game/'.whdl_register'),('filelog',game/'.whdl_log')]:
                    if location.exists():shutil.copyfile(location,base/f'{name}-{attempt+1}.txt')
                emu.terminate()
                try: emu.wait(timeout=5)
                except subprocess.TimeoutExpired: emu.kill(); emu.wait()


if __name__ == "__main__":
    main()
