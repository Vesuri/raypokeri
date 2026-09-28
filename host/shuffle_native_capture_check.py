#!/usr/bin/env python3
"""Compare each displayed native shuffle frame with the headless reference."""
from pathlib import Path
import argparse
import struct
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--native',required=True,help='prefix before -frame-N.bin')
p.add_argument('--host',default='tmp/shuffle-consumer-experiment-consumer',help='prefix before -shuffle-N.ppm')
a=p.parse_args()
palette=[0x000000,0x0000aa,0x00aa00,0x00aaaa,0xaa0000,0xaa00aa,0xaa5500,0xaaaaaa,0x555555,0x5555ff,0x55ff55,0x55ffff,0xff5555,0xff55ff,0xffff55,0xffffff]
lookup={v.to_bytes(3,'big'):i for i,v in enumerate(palette)}
for n in range(1,31):
    raw=Path(f'{a.native}-frame-{n}.bin').read_bytes()
    assert len(raw)==90560,'incomplete 608x283 interleaved screen'
    words=struct.unpack('>45280H',raw)
    actual=bytes(sum(((words[y*160+plane*40+x//16]>>(15-(x&15)))&1)<<plane for plane in range(4)) for y in range(283) for x in range(608))
    ppm=Path(f'{a.host}-shuffle-{n}.ppm').read_bytes()
    header,pixels=ppm.split(b'\n255\n',1)
    assert b'Placeholder palette' in header and header.endswith(b'608 292'),'reference format changed'
    expected=bytes(lookup[pixels[i:i+3]] for i in range(5*608*3,288*608*3,3))
    if actual!=expected:
        points=[(i%608,i//608) for i,(x,y) in enumerate(zip(actual,expected)) if x!=y]
        raise AssertionError(f'frame {n}: {len(points)} differing pixels, bounds {min(x for x,y in points)},{min(y for x,y in points)} .. {max(x for x,y in points)},{max(y for x,y in points)}')
print('PASS: all 30 retired native shuffle frames match all 172064 reference pixels each')
