#!/usr/bin/env python3
"""Compare ignored native planar captures with a paired host reference."""
import argparse
from pathlib import Path
import re
import struct
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--native',default='tmp/phase5-boot')
p.add_argument('--host',default='tmp/native-comparison')
p.add_argument('--log',default='amiga/.run/phase5-planar/gdb-out.log')
a=p.parse_args()
def read(prefix,suffix):return Path(prefix+suffix).read_bytes()
raw=read(a.native,'-vram.bin')
assert len(raw)==0x80000, 'incomplete native VRAM capture'
planes=struct.unpack('>262144H',raw)
packed=bytearray()
for address in range(0x40000):
    word=0
    for x in range(4):
        bit=15-((address&3)*4+x)
        for plane in range(4):word|=((planes[plane*65536+(address>>2)]>>bit)&1)<<(x*4+plane)
    packed.extend(word.to_bytes(2,'big'))
reference=read(a.host,'-vram.bin')
assert packed==reference, f'VRAM differs in {sum(x!=y for x,y in zip(packed,reference))} bytes'
screen=read(a.native,'-screen.bin')
assert len(screen)==81504,'incomplete screen capture'
words=struct.unpack('>40752H',screen)
indices=bytes(sum(((words[y*144+plane*36+x//16]>>(15-(x&15)))&1)<<plane for plane in range(4)) for y in range(283) for x in range(576))
expected=read(a.host,'-indices.bin')[5*576:288*576]
assert indices==expected,f'frame differs in {sum(x!=y for x,y in zip(indices,expected))} pixels'
masks=[255,15,255,15,255,15,31,255,31,31,31,255,255,15,255,255]
count=0;hash=5381
for reg,value in re.findall(r'AY register=(\d+) value=([0-9a-f]+)',Path(a.host+'-events.txt').read_text()):
    reg=int(reg);value=int(value,16)&masks[reg]
    hash=(((hash*33)^reg)*33)^value;hash&=0xffffffff;count+=1
match=re.search(r'AY writes=(\d+) hash=(\d+)',Path(a.log).read_text())
assert match and tuple(map(int,match.groups()))==(count,hash),'AY register stream differs'
print(f'PASS: all 524288 VRAM bytes, 163008 cropped pixels and {count} AY writes match')
