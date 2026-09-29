#!/usr/bin/env python3
"""Load the linked product fixture into the independent CPU oracle."""
from pathlib import Path
import struct
import subprocess
root=Path(__file__).resolve().parents[1]
b=(root/'build/native-product-fixture.elf').read_bytes()
assert b[:6]==b'\x7fELF\x01\x02'
h=struct.unpack_from('>HHIIIIIHHHHHH',b,16)
assert h[0]==2 and h[1]==4
parts=[]
for i in range(h[11]):
    s=struct.unpack_from('>10I',b,h[5]+i*h[10])
    if s[1]==1 and s[2]&2:
        parts.append(struct.pack('>II',s[3],s[5])+b[s[4]:s[4]+s[5]])
p=root/'build/native-product.bin'
p.write_bytes(struct.pack('>II',h[3],len(parts))+b''.join(parts))
subprocess.run([str(root/'build/native-product-test'),str(p)],check=True)
