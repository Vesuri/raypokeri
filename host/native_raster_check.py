#!/usr/bin/env python3
"""Assemble and CPU-test the standalone cached-raster kernel (no ROM bytes)."""
from pathlib import Path
import struct
import argparse
import subprocess
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--elf', type=Path)
args = parser.parse_args()
def run(*args):
    subprocess.run(args, cwd=root, check=True)
run('m68k-amiga-elf-as', '-m68000', 'src/platform/amiga/CachedRaster.s', '-o', 'tmp/cached-raster-kernel.o')
run('m68k-amiga-elf-ld', '-Ttext=0x1000', '-e', 'nativeCachedRasterComplete', 'tmp/cached-raster-kernel.o', '-o', 'tmp/cached-raster-kernel.elf')
b = (root/'tmp/cached-raster-kernel.elf').read_bytes()
assert b[:6] == b'\x7fELF\x01\x02'
h = struct.unpack_from('>HHIIIIIHHHHHH', b, 16)
sections = []
for i in range(h[11]):
    _, kind, flags, address, offset, size, *_ = struct.unpack_from('>10I', b, h[5]+i*h[10])
    if kind == 1 and flags & 4:
        assert address == 0x1000 and size < 0x800
        sections.append(b[offset:offset+size])
assert len(sections) == 1
(root/'tmp/cached-raster-kernel.bin').write_bytes(sections[0])
run('build/native-raster-test', 'tmp/cached-raster-kernel.bin')

if args.elf:
    b = args.elf.read_bytes()
    h = struct.unpack_from('>HHIIIIIHHHHHH', b, 16)
    segments = []
    for i in range(h[11]):
        _, kind, flags, address, offset, size, *_ = struct.unpack_from('>10I', b, h[5]+i*h[10])
        if kind == 1 and flags & 2:
            segments.append(struct.pack('>II', address, size)+b[offset:offset+size])
    (root/'tmp/raster-linked.bin').write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
    names = set('nativeFeedLoopCallModel nativeShortVideoWriteValue nativeRasterGrant nativeRasterGrantActive nativeRasterHits nativeFeedHeaderGrant nativeCachedVideoStatus nativeFeedInlineWord nativeFeedInlinePending nativeFeedInlineHigh nativeFeedInlineLength'.split())
    output = subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(args.elf)],text=True)
    addresses = {p[-1]:int(p[0],16) for line in output.splitlines() if (p:=line.split()) and p[-1] in names}
    assert addresses.keys() == names
    (root/'tmp/raster-linked.txt').write_text(''.join(f'{name} {address}\n' for name,address in addresses.items()))
    run('build/native-raster-test','tmp/raster-linked.bin','tmp/raster-linked.txt')
