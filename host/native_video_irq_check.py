#!/usr/bin/env python3
"""Execute the linked opt-in video IRQ admission, with layout from debug types."""
from pathlib import Path
import argparse, re, struct, subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf',type=Path,required=True)
a=p.parse_args()
names='''nativeTryVideoIrq nativeVideoIrqTry nativeVideoIrqDecline nativeVideoIrqResume nativeShortPromote nativeClockPause nativeRegisters nativePhysicalSr nativePhysicalResume nativeClockEnabled nativeClockRunning nativeClockOverhead nativeClockCalibrating nativeClockMode nativeDiagnostic nativeSetupReady nativeStatus nativeInstructions nativeInterrupts nativeLastPc nativeCycles nativeShortPending nativeShortDrained nativeShortGuest nativeShortNominal nativeVirtualUsp nativeVirtualSsp nativeRomBegin nativeRomEnd nativeRamBegin nativeRamEnd nativeClockResumePc nativeExtendedFrame nativeCachedVideoStatus nativeShuffleNextPointer board rom romBase ramBase diagnostic liveTicks liveCycles liveStopCycles liveClock guestClockPhase startupFast clockDisplayCalibrated shuffleActive shuffleQueued shuffleQueue screen nativeCardCache compositionPending liveIrqActive uninterruptedPoll quitRequested pendingFrames seenFrames'''.split()
queries={n:'(unsigned)&'+n for n in names}
queries['timingActive']='(unsigned)&NativeTiming::active'
functions={'nativeTryVideoIrq','nativeVideoIrqTry','nativeVideoIrqDecline','nativeVideoIrqResume','nativeShortPromote','nativeClockPause'}
for name in names:
 if name not in functions:queries['size_'+name]='sizeof('+name+')'
queries['size_timingActive']='sizeof(NativeTiming::active)'
queries.update({
 'boardSize':'sizeof(*board)',
 'boardMemory':'(unsigned)&((decltype(board))0)->memory',
 'boardVideo':'(unsigned)&((decltype(board))0)->video',
 'boardFault':'(unsigned)&((decltype(board))0)->fault',
 'boardReset':'(unsigned)&((decltype(board))0)->resetRequested',
 'videoError':'(unsigned)&((decltype(board))0)->video.error',
 'videoStatus':'(unsigned)&((decltype(board))0)->video.status',
 'videoHold':'(unsigned)&((decltype(board))0)->video.presentationBusy',
 'videoEnable':'(unsigned)&((decltype(board))0)->video.control.values[3]',
 'videoPending':'(unsigned)&((decltype(board))0)->video.pendingCount',
 'videoRead':'(unsigned)&((decltype(board))0)->video.readFifo.n',
 'serialControl':'(unsigned)&((decltype(board))0)->serial[0].control',
 'serialRead':'(unsigned)&((decltype(board))0)->serial[0].receive.n',
 'screenActive':'(unsigned)&((decltype(&screen))0)->displaying',
 'screenPending':'(unsigned)&((decltype(&screen))0)->pending',
 'shuffleCount':'(unsigned)&((decltype(&shuffleQueue))0)->count',
 'cardHits':'(unsigned)&((decltype(nativeCardCache))0)->hits',
})
for side in range(2):
 for field in ('control','flags'):
  queries[f'pia{field}{side}']=f'(unsigned)&((decltype(board))0)->pia[0].{field}[{side}]'
for field in ('credit','debt','frame','discardedWall','limited','ratioSixteenths','windowFrames'):
 queries['clock_'+field]=f'(unsigned)&((decltype(&liveClock))0)->{field}'
args=['m68k-amiga-elf-gdb','-nx','-batch','-ex','file '+str(a.elf)]
for name,expr in queries.items():args+=['-ex',f'printf "META {name} %u\\n", {expr}']
r=subprocess.run(args,text=True,capture_output=True)
assert r.returncode==0,r.stdout+r.stderr
s={k:int(v) for k,v in re.findall(r'META (\w+) (\d+)',r.stdout)}
assert set(s)==set(queries),set(queries)-set(s)
symbols=subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(a.elf)],text=True)
for line in symbols.splitlines():
 v=line.split()
 if v and v[-1]=='nativeVideoIrqHits':
  s[v[-1]]=int(v[0],16)
  s['size_nativeVideoIrqHits']=int(v[-2],16)
data=a.elf.read_bytes();assert data[:6]==b'\x7fELF\x01\x02'
h=struct.unpack_from('>HHIIIIIHHHHHH',data,16);segments=[]
for i in range(h[11]):
 _,kind,flags,address,offset,size,*_=struct.unpack_from('>10I',data,h[5]+i*h[10])
 if kind==1 and flags&2:segments.append(struct.pack('>II',address,size)+data[offset:offset+size])
Path('tmp/video-irq-code.bin').write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
Path('tmp/video-irq-symbols.txt').write_text(''.join(f'{k} {v}\n' for k,v in s.items()))
subprocess.run(['build/native-video-irq-test','tmp/video-irq-code.bin','tmp/video-irq-symbols.txt'],check=True)
