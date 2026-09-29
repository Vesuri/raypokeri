#!/usr/bin/env python3
"""CPU-check the linked opt-in cache batch using synthetic state only."""
from pathlib import Path
import argparse, struct, subprocess
p=argparse.ArgumentParser();p.add_argument('--elf',type=Path,required=True);a=p.parse_args()
b=a.elf.read_bytes();h=struct.unpack_from('>HHIIIIIHHHHHH',b,16);segments=[]
for i in range(h[11]):
    _,kind,flags,address,offset,size,*_=struct.unpack_from('>10I',b,h[5]+i*h[10])
    if kind==1 and flags&2:segments.append(struct.pack('>II',address,size)+b[offset:offset+size])
Path('tmp/native-batch-code.bin').write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
names=set('nativeBatch nativeBatchFinish nativeFeedAcceptWord nativeShortVideoWriteValue nativeFeedInlineCount nativeFeedHeaderGrant nativeRasterGrantActive nativeShortNoControlDue nativeShortControlPromote nativeShortReturn nativeShortPromote nativeRegisterFeedStore nativeDiagnostic nativeClockResumePc _ZN6pokeri11CachedBatch11materializeEv'.split())
output=subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(a.elf)],text=True)
addresses={v[-1]:int(v[0],16) for line in output.splitlines() if (v:=line.split()) and v[-1] in names}
assert addresses.keys()==names
Path('tmp/native-batch-symbols.txt').write_text(''.join(f'{k} {v}\n' for k,v in addresses.items()))
subprocess.run(['build/native-batch-test','tmp/native-batch-code.bin','tmp/native-batch-symbols.txt'],check=True)
