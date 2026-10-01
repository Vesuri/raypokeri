#!/usr/bin/env python3
"""Compare the linked T14 block with a separately assembled synthetic oracle."""
import argparse
import struct
import subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf',type=Path,default=Path('amiga/out/RAYPokeri.elf'))
a=p.parse_args()
subprocess.run(['m68k-amiga-elf-as','-m68000','host/native_sound_oracle.s','-o','tmp/sound-oracle.o'],check=True)
subprocess.run(['m68k-amiga-elf-ld','-Ttext=0x100000','-e','oracle_sound','tmp/sound-oracle.o','-o','tmp/sound-oracle.elf'],check=True)
names='nativeShortSoundWrite nativeShortIoGuard nativeShortAdmitted nativeShortDecline nativeShortControlPromote nativeShortNoControlDue nativeShortIoWriteValue nativeDiagnostic nativeShortPending pendingFrames seenFrames nativeShortNominal nativeInstructions nativeShortCalls nativeProfileEnabled nativeClockResumePc nativeLiveCounterMode'.split()
names += ['nativeSoundBoundary'+str(i) for i in range(10)]
names += ['oracle_sound','oracle_sound_end']
symbols={};segments=[]
for elf in [a.elf,Path('tmp/sound-oracle.elf')]:
    for line in subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(elf)],text=True).splitlines():
        v=line.split()
        if v and v[-1] in names:symbols[v[-1]]=int(v[0],16)
    data=elf.read_bytes();assert data[:6]==b'\x7fELF\x01\x02'
    h=struct.unpack_from('>HHIIIIIHHHHHH',data,16)
    for i in range(h[11]):
        _,kind,flags,address,offset,size,*_=struct.unpack_from('>10I',data,h[5]+i*h[10])
        if elf==a.elf and flags&2:assert address+size<0x100000,'native allocation overlaps fixture'
        if kind==1 and flags&4:segments.append(struct.pack('>II',address,size)+data[offset:offset+size])
assert set(symbols)==set(names),set(names)-set(symbols)
code=Path('tmp/native-sound-code.bin');meta=Path('tmp/native-sound-symbols.txt')
code.write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
meta.write_text(''.join(f'{k} {v}\n' for k,v in symbols.items()))
subprocess.run(['build/native-sound-test',str(code),str(meta)],check=True)
