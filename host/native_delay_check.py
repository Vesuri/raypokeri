#!/usr/bin/env python3
"""Extract only our linked delay-state kernel for an independent host CPU test."""
from pathlib import Path
import struct
import subprocess
root=Path(__file__).resolve().parents[1]
elf=root/'amiga/out/RAYPokeri.elf'
names={'nativeDelayApply','nativeDelayApplyEnd'}
symbols=subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(elf)],text=True)
a={v[-1]:int(v[0],16) for line in symbols.splitlines() if (v:=line.split()) and v[-1] in names}
begin,end=a['nativeDelayApply'],a['nativeDelayApplyEnd']
data=elf.read_bytes();h=struct.unpack_from('>HHIIIIIHHHHHH',data,16)
for i in range(h[11]):
    _,kind,flags,address,offset,size,*_=struct.unpack_from('>10I',data,h[5]+i*h[10])
    if kind==1 and flags&4 and address<=begin<end<=address+size:
        code=data[offset+begin-address:offset+end-address];break
else:raise AssertionError('delay kernel not found')
p=root/'tmp/native-delay-code.bin';p.write_bytes(code)
subprocess.run([str(root/'build/native-delay-test'),str(p)],check=True)
