#!/usr/bin/env python3
"""Extract only our linked exception-frame kernel for an independent host CPU test."""
from pathlib import Path
import argparse
import struct
import subprocess
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--elf',type=Path,default=root/'amiga/out/RAYPokeri.elf')
elf=parser.parse_args().elf
names={'nativeExceptionFrame','nativeExceptionFrameEnd'}
symbols=subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(elf)],text=True)
a={v[-1]:int(v[0],16) for line in symbols.splitlines() if (v:=line.split()) and v[-1] in names}
if not names.issubset(a):
    raise SystemExit('build the native candidate with EXCEPTION_FRAME_WORDS=1 first')
begin,end=a['nativeExceptionFrame'],a['nativeExceptionFrameEnd']
data=elf.read_bytes();h=struct.unpack_from('>HHIIIIIHHHHHH',data,16)
for i in range(h[11]):
    _,kind,flags,address,offset,size,*_=struct.unpack_from('>10I',data,h[5]+i*h[10])
    if kind==1 and flags&4 and address<=begin<end<=address+size:
        code=data[offset+begin-address:offset+end-address];break
else:raise AssertionError('exception frame kernel not found')
p=root/'tmp/native-exception-frame-code.bin';p.write_bytes(code)
subprocess.run([str(root/'build/native-exception-frame-test'),str(p)],check=True)
