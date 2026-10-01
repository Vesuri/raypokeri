#!/usr/bin/env python3
"""Compare the optional consumer-store/exit bridge with unmodified ROM instructions."""
from pathlib import Path
import argparse, struct, subprocess
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf',type=Path,default=root/'amiga/out/RAYPokeri.elf')
a=p.parse_args()
names='''nativeShortHandlerTail nativeHandlerTailStoreBoundary nativeHandlerTailPc nativeHandlerTailExit nativeShortHandlerExit nativeHandlerExitAddressBoundary nativeHandlerExitRestoreBoundary nativeShortControlGuard nativeShortControlRead nativeShortLengthDone nativeShortControlPromote nativeShortNoControlDue nativeShortDecline nativeVideoSelector nativeDiagnostic pendingFrames seenFrames nativeShortPending nativeInstructions nativeShortNominal nativeShortCalls nativeFeedInlineCount nativeFeedHeaderGrant nativeRegisters nativeProfileEnabled nativeClockResumePc nativeRamBegin nativeRamEnd nativeVirtualUsp nativeVirtualSsp'''.split()
lines=subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(a.elf)],text=True).splitlines()
s={v[-1]:int(v[0],16) for line in lines if (v:=line.split()) and v[-1] in names}
assert set(s)==set(names),set(names)-set(s)
for line in lines:
 v=line.split()
 if v and v[-1] in ('nativeLiveCounterMode','nativeRasterGrantActive'):
  s[v[-1]]=int(v[0],16)
b=a.elf.read_bytes();assert b[:6]==b'\x7fELF\x01\x02'
h=struct.unpack_from('>HHIIIIIHHHHHH',b,16);segments=[]
for i in range(h[11]):
 _,kind,flags,address,offset,size,*_=struct.unpack_from('>10I',b,h[5]+i*h[10])
 if kind==1 and flags&4:segments.append(struct.pack('>II',address,size)+b[offset:offset+size])
# Extract only at run time from the verified user chip; never embed these bytes.
import hashlib, sys
sys.path.insert(0,str(root/'tools'))
from roms import CHIPS
rom=(root/'rom/77POK30').read_bytes()
assert (len(rom),hashlib.sha256(rom).hexdigest())==CHIPS['77POK30'][:2]
original=rom[0x2e7e:0x2e8c]
segments.append(struct.pack('>II',0x100000,len(original))+original)
code=root/'tmp/handler-tail-code.bin';meta=root/'tmp/handler-tail-symbols.txt' 
code.write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
meta.write_text(''.join(f'{k} {v}\n' for k,v in s.items()))
subprocess.run([str(root/'build/native-handler-exit-test'),str(code),str(meta)],check=True)
