#!/usr/bin/env python3
"""Compare the linked T13 joined handler with separately assembled synthetic oracles."""
import argparse
import struct
import subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf',type=Path,default=Path('amiga/out/Pokeri.elf'))
a=p.parse_args()
objects=[]
for name in ['native_handler_setup_oracle','native_handler_joined_oracle']:
    subprocess.run(['m68k-amiga-elf-as','-m68000',f'host/{name}.s','-o',f'tmp/{name}.o'],check=True);objects.append(f'tmp/{name}.o')
subprocess.run(['m68k-amiga-elf-ld','-Ttext=0x100000','-e','oracle_setup',*objects,'-o','tmp/handler-joined-oracle.elf'],check=True)
names='''nativeLiveCounterMode nativeShortHandlerJoinedSetup nativeHandlerJoinedSelectBoundary nativeHandlerJoinedQueue
nativeHandlerJoinedDelivery nativeVideoIrqGuestResume nativeJoinedVector nativeJoinedA0 nativeJoinedEntry
nativeShortVideoGuard nativeShortStatusGuard nativeShortHandlerTail nativeShortControlPromote nativeShortNoControlDue
nativeDiagnostic nativeShortPending pendingFrames seenFrames nativeShortNominal nativeInstructions nativeShortCalls
nativeProfileEnabled nativeClockResumePc nativeRamBegin nativeRamEnd nativeHandlerFeed nativeHandlerEmpty nativeFeedTarget
nativeFeedInlineCount nativeFeedHeaderGrant nativeRasterGrantActive nativeVideoSelector nativeShuffleNextPointer
oracle_setup oracle_setup_feed oracle_setup_empty oracle_setup_exit oracle_delivery oracle_delivery_end'''.split()
symbols={};segments=[]
for elf in [a.elf,Path('tmp/handler-joined-oracle.elf')]:
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
code=Path('tmp/native-handler-joined-code.bin');meta=Path('tmp/native-handler-joined-symbols.txt')
code.write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
meta.write_text(''.join(f'{k} {v}\n' for k,v in symbols.items()))
subprocess.run(['build/native-handler-joined-test',str(code),str(meta)],check=True)
