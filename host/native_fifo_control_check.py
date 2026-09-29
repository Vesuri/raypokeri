#!/usr/bin/env python3
"""Check the linked FIFO-control triplet against synthetic original instructions."""
from pathlib import Path
import argparse
import struct
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf',type=Path,default=Path('amiga/out/Pokeri.elf'))
a=p.parse_args()
names='''nativeShortVideoGuard nativeShortAdmitted nativeShortFifoControl nativeFifoControlBoundary nativeShortControlPromote nativeShortNoControlDue nativeShortDecline nativeShortVideoWriteValue nativeVideoSelector nativeDiagnostic pendingFrames seenFrames nativeShortPending nativeInstructions nativeShortNominal nativeShortCalls nativeFeedInlineCount nativeFeedHeaderGrant nativeRegisters nativeProfileEnabled nativeClockResumePc'''.split()
symbols=subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(a.elf)],text=True)
s={v[-1]:int(v[0],16) for line in symbols.splitlines() if (v:=line.split()) and v[-1] in names}
assert set(s)==set(names),set(names)-set(s)
for line in symbols.splitlines():
 v=line.split()
 if v and v[-1]=="nativeFifoControlValue":s[v[-1]]=int(v[0],16)
d=a.elf.read_bytes();assert d[:6]==b'\x7fELF\x01\x02', 'expected big-endian ELF32'
h=struct.unpack_from('>HHIIIIIHHHHHH',d,16);segments=[]
for i in range(h[11]):
 _,kind,flags,address,offset,size,*_=struct.unpack_from('>10I',d,h[5]+i*h[10])
 if kind==1 and flags&4:segments.append(struct.pack('>II',address,size)+d[offset:offset+size])
code=Path('tmp/fifo-control-code.bin');meta=Path('tmp/fifo-control-symbols.txt')
code.write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
meta.write_text(''.join(f'{k} {v}\n' for k,v in s.items()))
subprocess.run(['build/native-fifo-control-test',str(code),str(meta)],check=True)
