#!/usr/bin/env python3
"""Exercise the linked opt-in startup delay assembly; no ROM bytes are used."""
from pathlib import Path
import argparse, struct, subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--elf',type=Path,required=True);a=p.parse_args()
names='nativeStartupDelayGuard nativeStartupDelayCompleted nativeTryStartupDelay nativeShortDecline nativeShortControlPromote nativeSave nativeRomBegin nativeDiagnostic nativeRegisters nativeClockResumePc nativeClockRunning pendingFrames seenFrames nativeClockEnabled nativeClockPause irqStartupFast irqDiagnostic irqQuit irqLiveActive irqLiveTicks irqGuestPhase _ZL9haveEvent _ZL5board nativeShortPending nativeClockCalibrating nativeFeedInlineCount nativeFeedHeaderGrant nativeRasterGrantActive nativeStartupDelayShortHits nativeIdleCalls nativeIdleInstructions nativeIdleCycles'.split()
out=subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(a.elf)],text=True)
s={v[-1]:int(v[0],16) for line in out.splitlines() if (v:=line.split()) and v[-1] in names}
assert set(s)==set(names),set(names)-set(s)
# Read the compiler's layout instead of guessing a native Board offset.
import re
layout=subprocess.check_output(['m68k-amiga-elf-gdb','-nx','-batch','-ex','file '+str(a.elf),'-ex',
 'printf "FAULT %u\\n", (unsigned)&((decltype(board))0)->fault'],text=True,stderr=subprocess.STDOUT)
s['offset_fault']=int(re.search(r'FAULT (\d+)',layout)[1])
b=a.elf.read_bytes();h=struct.unpack_from('>HHIIIIIHHHHHH',b,16);segments=[]
for i in range(h[11]):
 _,kind,flags,address,offset,size,*_=struct.unpack_from('>10I',b,h[5]+i*h[10])
 if kind==1 and flags&2:segments.append(struct.pack('>II',address,size)+b[offset:offset+size])
Path('tmp/startup-delay-code.bin').write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
Path('tmp/startup-delay-symbols.txt').write_text(''.join(f'{k} {v}\n' for k,v in s.items()))
subprocess.run(['build/native-startup-delay-test','tmp/startup-delay-code.bin','tmp/startup-delay-symbols.txt'],check=True)
