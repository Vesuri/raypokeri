#!/usr/bin/env python3
"""Read-only WHDLoad Double timing via verified, relocated debug symbols.

Build SERVICE_REDIRECT=1 DOUBLE_SCENARIO=1 WHD_DEBUG_MAP=1, then freeze the
executable/ELF. No target memory/register writes or game-side timing probes.
Each invocation uses a fresh fixture and must receive the same saved-state seed
for a comparative series. Run from the repository root after sourcing env.sh.
"""
import argparse, json, os, re, selectors, socket, subprocess, sys, time
from pathlib import Path
sys.path.insert(0,str(Path('host').resolve()))
from whdload_symbols import resolve
from release_probe import prepare
from release_graphics_probe import prepare as prepare_graphics
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--vbr',choices=('fixed','moved'),required=True)
parser.add_argument('--debug-port',type=int,default=3187)
parser.add_argument('--exe',type=Path,required=True)
parser.add_argument('--elf',type=Path,required=True)
parser.add_argument('--slave',type=Path,required=True)
parser.add_argument('--rom',type=Path,required=True)
parser.add_argument('--rtb',type=Path,required=True)
parser.add_argument('--seed-saves-from',type=Path,required=True)
parser.add_argument('--seconds',type=int,default=900)
parser.add_argument('--graphics',action='store_true',help='read-only C++ command probes for one board second after Double; borrowed assembly completions are excluded')
options=parser.parse_args()
if not 1024<=options.debug_port<=65535 or options.seconds<1:parser.error('invalid port or time budget')
vbr=options.vbr;port=options.debug_port;exe=options.exe.resolve();elf=options.elf.resolve()
with socket.socket() as check:check.bind(('127.0.0.1',port))
args=['python3','tools/test_whdload.py','--prepare-only','--exe',str(exe),'--slave',str(options.slave),'--rom',str(options.rom),'--rtb',str(options.rtb),'--vbr',vbr,'--write-cache','enabled','--seed-saves-from',str(options.seed_saves_from)]
setup=subprocess.check_output(args,text=True);print(setup,flush=True)
base=Path(re.search(r'Fixture: (.+)',setup)[1]);boot=base/'boot';game=base/'game'
(game/'data/native-live').unlink();(game/'data/native-test-inputs').touch()
template=Path('amiga/release-double.gdb').read_text()
if options.graphics:template=prepare_graphics(template)
script=base/'double.gdb';script.write_text(prepare(elf,template.replace('break nativePlayReady', 'tbreak nativePlayReady', 1)))
before={p.name:p.read_bytes() for p in (game/'data').glob('*.bin')}
log=(base/'emulator-live.log').open('w');glog=(base/'gdb-out.log').open('w');raw=(base/'gdb-mi.log').open('w')
cmd=['fs-uae','--amiga_model=A1200','--cpu=68020','--uae_cpu_model=68020','--uae_cpu_24bit_addressing=false','--jit_compiler=0','--chip_memory=2048','--fast_memory=8192','--kickstart_file='+os.environ['KICKSTART'],'--hard_drive_0='+str(boot),'--hard_drive_0_priority=10','--hard_drive_1='+str(game),'--floppy_drive_0='+str(Path.home()/'Documents/Vette/tmp/Workbenchv2.04rev37.67Workbench.adf'),'--joystick_port_0=mouse','--joystick_port_1=nothing','--warp_mode=1','--fullscreen=0','--automatic_input_grab=0','--window_width=720','--window_height=568','--state_dir='+str(base/'state'),'--remote_debugger=20',f'--remote_debugger_port={port}','--remote_debugger_trigger=WHDLoad']
emu=subprocess.Popen(cmd,stdout=log,stderr=log,env=dict(os.environ,SDL_AUDIODRIVER='dummy'));gdb=None
try:
 deadline=time.monotonic()+20
 while time.monotonic()<deadline:
  if emu.poll() is not None:raise RuntimeError('emulator exited early')
  if subprocess.run(['lsof','-nP',f'-iTCP:{port}','-sTCP:LISTEN'],stdout=subprocess.DEVNULL).returncode==0:break
  time.sleep(.25)
 else:raise RuntimeError('debugger not listening')
 gdb=subprocess.Popen(['m68k-amiga-elf-gdb','-q','-nx','--interpreter=mi2'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,bufsize=0)
 selector=selectors.DefaultSelector();selector.register(gdb.stdout,selectors.EVENT_READ);pending=b'';token=0
 def line(timeout=30):
  global pending
  end=time.monotonic()+timeout
  while b'\n' not in pending:
   if not selector.select(max(0,end-time.monotonic())):raise TimeoutError('GDB response timeout')
   chunk=os.read(gdb.stdout.fileno(),65536)
   if not chunk:raise EOFError('GDB ended')
   pending+=chunk
  out,pending=pending.split(b'\n',1);out=out.decode(errors='replace');raw.write(out+'\n');raw.flush()
  if out.startswith(('~','&','@')):
   try:glog.write(json.loads(out[1:]));glog.flush()
   except json.JSONDecodeError:pass
  return out
 def command(cmd):
  global token
  token+=1;gdb.stdin.write(f'{token}{cmd}\n'.encode());gdb.stdin.flush()
  while True:
   out=line()
   if out.startswith(f'{token}^'):
    if out.startswith(f'{token}^error'):raise RuntimeError(out)
    return out
 def console(cmd):return command('-interpreter-exec console '+json.dumps(cmd))
 command('-gdb-set pagination off');command('-gdb-set confirm off');command('-gdb-set target-async on');command('-gdb-set remotetimeout 30')
 command(f'-target-select remote 127.0.0.1:{port}')
 found=None
 for attempt in range(40):
  console('handle SIGTRAP nostop noprint pass');command('-exec-continue');time.sleep(.15)
  console('handle SIGTRAP stop print pass');command('-exec-interrupt')
  while not line().startswith('*stopped'):pass
  dump=base/'live-ram.bin';console(f'dump binary memory {dump} 0x200000 0xa00000')
  found=resolve(elf.read_bytes(),dump.read_bytes(),0x200000)
  if found:break
 if not found:raise RuntimeError('no loaded game map within probe budget')
 print('LIVE sections',found,flush=True)
 console('symbol-file')
 console(f'add-symbol-file "{elf}" {found[".text"]:#x}'+''.join(f' -s {s} {a:#x}' for s,a in found.items() if s!='.text'))
 # The stub advertises signal-bearing steps but does not handle them.
 # Restore normal breakpoint handling after the bootstrap interrupt.
 console('handle SIGTRAP stop print nopass')
 ready=command('-data-evaluate-expression nativeSetupReady');print('Before capture',ready,flush=True)
 assert 'value="0"' in ready,'attached after Ready; invalid full comparison'
 # The existing command file verifies instructions, sets read-only probes,
 # runs the scenario, and exits GDB after nativeReturned.
 token+=1;gdb.stdin.write(f'{token}-interpreter-exec console {json.dumps("source "+str(script))}\n'.encode());gdb.stdin.flush()
 deadline=time.monotonic()+options.seconds
 while time.monotonic()<deadline:
  try:line(timeout=60)
  except EOFError:break
  except TimeoutError:
   if gdb.poll() is not None:break
   print('Capture still running',flush=True)
 else:raise TimeoutError('scenario capture budget exceeded')
 gdb.wait(timeout=10);glog.flush()
 subprocess.run(['python3','host/release_timing.py',str(base/'gdb-out.log'),'--scenario','double'],check=True)
 deadline=time.monotonic()+30
 while not (boot/'passed').exists() and time.monotonic()<deadline:time.sleep(.1)
 assert (boot/'passed').exists(),'WHDLoad did not return successfully'
 for name,value in before.items():assert (game/'data'/name.replace('.bin','.bak')).read_bytes()==value,name
 print('PASS measured Double and save/backup return',base,flush=True)
finally:
 for process in (gdb,emu):
  if process is not None and process.poll() is None:
   process.terminate()
   try:process.wait(timeout=10)
   except subprocess.TimeoutExpired:process.kill();process.wait()
 for f in (log,glog,raw):f.close()
