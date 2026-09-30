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
from package_release import fresh_save_slots

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mode', choices=('smoke', 'boot', 'load', 'quit'), default='quit')
    p.add_argument('--whdload', type=Path, default=Path.home()/'.local/share/amiga/WHDLoad/C/WHDLoad')
    p.add_argument('--rom', type=Path)
    p.add_argument('--rtb', type=Path)
    p.add_argument('--smoke-size',type=int,choices=(4,32768),default=4,help='authored smoke payload size')
    p.add_argument('--smoke-preload-seed',action='store_true',help='seed an authored input so cold smoke PRELOAD is nonempty')
    p.add_argument('--smoke-data-dir',action='store_true',help='smoke slave declares data as its current directory')
    p.add_argument('--slave',type=Path,help='explicit diagnostic slave override; never changes installed slave')
    p.add_argument('--exe', type=Path, default=ROOT/'amiga/out/Pokeri')
    p.add_argument('--seconds', type=int, default=90, help='host safety ceiling')
    p.add_argument('--cpu', default='68020')
    p.add_argument('--mmu',action='store_true',help='enable the selected 030/040/060 MMU for compatibility tests')
    p.add_argument('--fast',type=int,default=8192,help='Fast RAM in KiB')
    p.add_argument('--seed-saves-from',type=Path,help='copy four existing save/backup images into the isolated fixture before PRELOAD')
    p.add_argument('--no-preload', action='store_true')
    p.add_argument('--write-cache',choices=('disabled','enabled'),default=None,
                   help='enabled uses release/WHDLoad default; disabled tests optional NOWRITECACHE')
    p.add_argument('--vbr',choices=('fixed','moved'),default=None,
                   help='moved uses release/WHDLoad default; fixed tests optional NOVBRMOVE')
    p.add_argument('--expect-replay-vbr-refusal',action='store_true',help='negative startup test: replay must refuse moved WHDLoad VBR')
    p.add_argument('--expect-trace-vbr-refusal',choices=('normal','no-short-hooks','generic-hooks','benchmark'),help='negative moved-VBR test for a trace-dependent build or research mode')
    p.add_argument('--expect-save-slot-refusal',choices=('missing','invalid'),help='negative startup test; do not create or overwrite saves')
    p.add_argument('--quit-key',type=int,help='diagnostic WHDLoad raw exit-key override (0..255)')
    p.add_argument('--no-resint',action='store_true',help='diagnostic: disable interrupts inside resload calls')
    p.add_argument('--file-log',action='store_true',help='enable WHDLoad FILELOG')
    p.add_argument('--write-delay',type=int,help='WHDLoad write delay in 1/50-second units')
    p.add_argument('--expect-startup-profile',action='store_true',help='validate STARTUP_PROFILE boundary timestamps in post-return Fast RAM')
    p.add_argument('--expect-cia-stress',action='store_true',help='validate diagnostic CIA delivery record in post-return Fast RAM (requires --capture-fast)')
    p.add_argument('--capture-fast',action='store_true',help='dump configured Fast RAM read-only to locate authored progress markers')
    p.add_argument('--debug-port',type=int,help='dedicated FS-UAE port; capture CPU state read-only on return/timeout')
    p.add_argument('--prepare-only',action='store_true',help='write isolated fixture without launching FS-UAE')
    p.add_argument('--repeat',type=int,default=1)
    p.add_argument('--standalone',choices=('data','current'),help='test AmigaDOS ROM lookup instead of WHDLoad')
    args = p.parse_args()
    # WHDLoad-only defaults must not make standalone mode reject itself.
    if args.write_cache is None:args.write_cache='disabled' if args.standalone else 'enabled'
    if args.vbr is None:args.vbr='fixed' if args.standalone else 'moved'
    if args.expect_replay_vbr_refusal and (args.mode!='quit' or args.vbr!='moved' or args.standalone or args.seed_saves_from):p.error('--expect-replay-vbr-refusal requires unseeded WHDLoad quit mode with moved VBR')
    if args.expect_trace_vbr_refusal and (args.mode!='quit' or args.vbr!='moved' or args.standalone or args.expect_replay_vbr_refusal or args.expect_save_slot_refusal):p.error('--expect-trace-vbr-refusal requires WHDLoad quit mode with moved VBR')
    if args.expect_save_slot_refusal and (args.mode!='quit' or args.standalone or args.expect_replay_vbr_refusal):p.error('--expect-save-slot-refusal requires WHDLoad quit mode')
    if args.smoke_preload_seed and args.mode!='smoke':p.error('--smoke-preload-seed requires smoke mode')
    if args.smoke_data_dir and args.mode!='smoke':p.error('--smoke-data-dir requires smoke mode')
    if args.seed_saves_from and args.mode!='quit':p.error('--seed-saves-from requires quit mode')
    if args.expect_startup_profile and (not args.capture_fast or args.mode!='quit'):p.error('--expect-startup-profile requires quit mode and --capture-fast')
    if args.expect_cia_stress and (not args.capture_fast or args.mode!='quit'):p.error('--expect-cia-stress requires quit mode and --capture-fast')
    if args.capture_fast and (args.debug_port is None or not args.fast):p.error('--capture-fast requires --debug-port and Fast RAM')
    if args.fast<0 or args.fast>8192:p.error('--fast must be 0..8192 KiB; larger Zorro II configurations are unsupported')
    if args.mmu and args.cpu not in ('68030','68040','68060'):p.error('--mmu requires 68030, 68040 or 68060')
    if args.repeat<1 or args.seconds<1:p.error('--repeat and --seconds must be positive')
    if args.quit_key is not None and not 0<=args.quit_key<=255:p.error('--quit-key must be 0..255')
    if args.write_delay is not None and args.write_delay<0:p.error('--write-delay must be nonnegative')
    if args.standalone and (args.write_cache!='disabled' or args.vbr!='fixed' or args.file_log or args.no_resint or args.write_delay is not None or args.quit_key is not None):
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
    cpu_args=['--uae_mmu_model='+args.cpu,'--uae_cpu_compatible=true'] if args.mmu else []
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
    if args.smoke_preload_seed:
        ((game/'data' if args.smoke_data_dir else game)/'authored-seed').write_bytes(b'Pokeri cache diagnostic input\n')
    shutil.copyfile(args.whdload, game/'WHDLoad')
    shutil.copyfile(args.slave or ROOT/'build/whdload'/slave, game/'Pokeri.slave')
    if args.mode != 'smoke' and not args.standalone:
        shutil.copyfile(args.rom, boot/'devs/Kickstarts'/args.rom.name)
        shutil.copyfile(args.rtb, boot/'devs/Kickstarts'/(args.rom.name+'.RTB'))
    if args.mode in ('load', 'quit'):
        shutil.copyfile(args.exe, game/'data/Pokeri')
    if args.mode in ('quit',):
        for chip in ('77POK30','77POK38','77POK34','PARA200J'):
            shutil.copyfile(ROOT/'rom'/chip,game/'data'/chip)
        (game/'data/native-live').write_bytes((96000000).to_bytes(4,'big'))
        if args.expect_replay_vbr_refusal:(game/'data/native-replay').touch()
        if args.expect_trace_vbr_refusal and args.expect_trace_vbr_refusal!='normal':
            (game/'data'/('native-'+args.expect_trace_vbr_refusal)).touch()
    if args.standalone:
        shutil.copyfile(args.exe,game/'Pokeri')
        (game/'native-live').write_bytes((96000000).to_bytes(4,'big'))
        if args.standalone=='current':
            for chip in ('77POK30','77POK38','77POK34','PARA200J'):
                (game/'data'/chip).rename(game/chip)
    saves=game if args.standalone else game/'data'
    if args.seed_saves_from:
        for name in ('nvram.bin','nvram.bak','accounting.bin','accounting.bak'):
            shutil.copyfile(args.seed_saves_from/name,saves/name)
    if args.mode=='quit' and not args.standalone and not args.expect_replay_vbr_refusal:
        for stem,template in (('nvram','EmptyNVRAM'),('accounting','FreshAccounting')):
            for suffix in ('bin','bak'):
                path=saves/(stem+'.'+suffix)
                if not path.exists():path.write_bytes(fresh_save_slots()[template])
        if args.expect_save_slot_refusal=='missing':(saves/'nvram.bak').unlink()
        if args.expect_save_slot_refusal=='invalid':(saves/'accounting.bak').write_bytes(bytes(940))
    slot_before={p.name:p.read_bytes() for p in saves.glob('*.b*') if p.name in ('nvram.bin','nvram.bak','accounting.bin','accounting.bak')}
    (boot/'s/WHDLoad.prefs').write_text('Expert\nReadDelay=0\n')
    options=[]
    if args.vbr=='fixed':options.append('NOVBRMOVE')
    if args.write_cache=='disabled':options.append('NOWRITECACHE')
    if not args.no_preload:options.append('PRELOAD')
    if args.file_log:options.append('FILELOG')
    if args.no_resint:options.append('NORESINT')
    if args.quit_key is not None:options.append('QUITKEY='+str(args.quit_key))
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
                '--window_width=720', '--window_height=568', '--state_dir='+str(base/'state')]+cpu_args+debug_args, stdout=log, stderr=log, env=dict(os.environ,SDL_AUDIODRIVER='dummy'))
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
                if args.expect_trace_vbr_refusal:
                    assert (boot/'failed').exists(), 'trace-dependent mode unexpectedly accepted moved VBR'
                    assert 'Selected service mode requires NOVBRMOVE' in report+output, report+output
                    after={p.name:p.read_bytes() for p in saves.glob('*.b*') if p.name in ('nvram.bin','nvram.bak','accounting.bin','accounting.bak')}
                    assert after==slot_before,'refused service mode changed saves'
                    print('PASS: trace-dependent mode refused before takeover without changing saves',flush=True)
                    continue
                if args.expect_replay_vbr_refusal:
                    assert (boot/'failed').exists(), 'diagnostic replay unexpectedly accepted moved VBR'
                    assert 'Diagnostic native-replay requires NOVBRMOVE' in report+output, report+output
                    assert not any((saves/name).exists() for name in ('nvram.bin','nvram.bak','accounting.bin','accounting.bak'))
                    print('PASS: moved-VBR replay refused clearly before creating saves',flush=True)
                    continue
                if args.expect_save_slot_refusal:
                    assert (boot/'failed').exists() and 'Save slots missing or invalid' in report+output,report+output
                    after={p.name:p.read_bytes() for p in saves.glob('*.b*') if p.name in ('nvram.bin','nvram.bak','accounting.bin','accounting.bak')}
                    assert after==slot_before,'refused startup changed saves'
                    print('PASS: invalid/missing slots refused without save mutations',flush=True)
                    continue
                assert (boot/'passed').exists(), report + output
                if args.mode == 'smoke':
                    assert ((game/'data' if args.smoke_data_dir else game)/'smoke-passed').read_bytes() == b'PASS'+b'Z'*(args.smoke_size-4)
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
                            + (f'dump binary memory {base}/fast-{attempt+1}.bin 0x200000 {0x200000+args.fast*1024:#x}\n' if args.capture_fast else '')
                            + 'detach\nquit\n')
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
                if args.capture_fast:
                    captured=base/f'fast-{attempt+1}.bin'
                    if captured.exists():
                        data=captured.read_bytes();start=0
                        while (at:=data.find(b'POK!STGE',start))>=0:
                            if at+24<=len(data):
                                values=[int.from_bytes(data[at+n:at+n+4],'big') for n in (8,12,16,20)]
                                print(f'Save marker at {0x200000+at:#x}: stage/phase/calls/size={values}',flush=True)
                            start=at+8
                # Preserve each attempt, including timeout/failure evidence.
                for name,location in [('result',boot/'result'),('register',game/'.whdl_register'),('filelog',game/'.whdl_log')]:
                    if location.exists():shutil.copyfile(location,base/f'{name}-{attempt+1}.txt')
                emu.terminate()
                try: emu.wait(timeout=5)
                except subprocess.TimeoutExpired: emu.kill(); emu.wait()
                if args.expect_startup_profile:
                    assert captured.exists(),'startup profile RAM capture missing'
                    records=[];start=0
                    while (at:=data.find(b'POK!BOOTTIME0001',start))>=0:
                        if at+44<=len(data):
                            values=tuple(int.from_bytes(data[at+n:at+n+4],'big') for n in range(16,44,4))
                            if values[-1]:records.append(values)
                        start=at+4
                    assert len(set(records))==1,f'startup profile missing or inconsistent: {records}'
                    t0,t1,t2,f0,f1,f2,mask=records[0]
                    assert mask==7,records
                    prep=(t1-t0)&0xffffff;init=(t2-t1)&0xffffff
                    frames=(f2-f1)&0xffffffff
                    assert 0<prep<30000 and 0<init<30000,records
                    assert abs(init-frames)<=1, f'TOD/PAL mismatch: {records}'
                    print(f'PASS: startup profile prep_ticks={prep} init_ticks={init} ready_ticks={prep+init} init_frames={frames} ticks={t0},{t1},{t2} frames={f0},{f1},{f2}',flush=True)
                if args.expect_cia_stress:
                    assert captured.exists(),'CIA stress RAM capture missing'
                    records=[];start=0
                    while (at:=data.find(b'POK!CIA!STRESS01',start))>=0:
                        if at+36<=len(data):
                            values=[int.from_bytes(data[at+n:at+n+4],'big') for n in range(16,36,4)]
                            if values[0]:records.append(tuple(values))
                        start=at+4
                    assert len(set(records))==1, f'CIA stress record missing or inconsistent: {records}'
                    requested,delivered,delayed,cancelled,pending=records[0]
                    assert requested>=100 and delivered+cancelled==requested and not pending and not delayed,records
                    assert cancelled<=1,records
                    print(f'PASS: CIA stress requested={requested} delivered={delivered} delayed={delayed} cancelled={cancelled} pending={pending}',flush=True)


if __name__ == "__main__":
    main()
