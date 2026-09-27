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
log=Path(a.log).read_text()
layout=re.search(r'VRAM layout=(\d+) words=(\d+)',log)
interleaved=bool(layout and int(layout.group(1)))
expected_words=262200 if interleaved else 262144
assert len(raw)==expected_words*2, 'incomplete native VRAM capture or missing layout metadata'
if layout:assert int(layout.group(2))==expected_words,'VRAM layout/size mismatch'
planes=struct.unpack('>'+str(expected_words)+'H',raw)
def at(q,p):return (q//38)*152+p*38+q%38 if interleaved else p*65536+q
packed=bytearray()
for address in range(0x40000):
    word=0
    for x in range(4):
        bit=15-((address&3)*4+x)
        for plane in range(4):word|=((planes[at(address>>2,plane)]>>bit)&1)<<(x*4+plane)
    packed.extend(word.to_bytes(2,'big'))
reference=read(a.host,'-vram.bin')
assert packed==reference, f'VRAM differs in {sum(x!=y for x,y in zip(packed,reference))} bytes'
screen=read(a.native,'-screen.bin')
# Retain historical 576-pixel captures as evidence; new output pads each
# 608-pixel plane row to 640 so AGA fetch pointers stay aligned.
formats={81504:(576,36),90560:(608,40)}
assert len(screen) in formats,'incomplete screen capture'
width,plane_words=formats[len(screen)];row_words=plane_words*4
words=struct.unpack('>'+str(len(screen)//2)+'H',screen)
indices=bytes(sum(((words[y*row_words+plane*plane_words+x//16]>>(15-(x&15)))&1)<<plane for plane in range(4)) for y in range(283) for x in range(width))
reference_indices=read(a.host,'-indices.bin')
assert len(reference_indices)==width*292,'host/native display geometry differs'
expected=reference_indices[5*width:288*width]
assert indices==expected,f'frame differs in {sum(x!=y for x,y in zip(indices,expected))} pixels'
masks=[255,15,255,15,255,15,31,255,31,31,31,255,255,15,255,255]
count=0;hash=5381
for reg,value in re.findall(r'AY register=(\d+) value=([0-9a-f]+)',Path(a.host+'-events.txt').read_text()):
    reg=int(reg);value=int(value,16)&masks[reg]
    hash=(((hash*33)^reg)*33)^value;hash&=0xffffffff;count+=1
match=re.search(r'AY writes=(\d+) hash=(\d+)',Path(a.log).read_text())
assert match and tuple(map(int,match.groups()))==(count,hash),'AY register stream differs'
print(f'PASS: all 524288 VRAM bytes, {len(indices)} cropped pixels and {count} AY writes match')
