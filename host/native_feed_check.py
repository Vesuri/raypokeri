#!/usr/bin/env python3
"""Run the opt-in assembled feed body against independent synthetic CPU code."""
from pathlib import Path
import struct
import subprocess
root = Path(__file__).resolve().parents[1]
elf = root/'amiga/out/Pokeri.elf'
symbols = subprocess.check_output(['m68k-amiga-elf-objdump', '-t', str(elf)], text=True)
names = '''nativeShortFeedRead nativeFeedBoundary0 nativeFeedBoundary1 nativeFeedSource
nativeShortLengthDone nativeShortControlPromote nativeShortVideoWriteValue
nativeFeedReplayContinue nativeShortReplayStart nativeShortVideoWrite
nativeCachedVideoStatus nativeDiagnostic nativeFeedTarget nativeFeedTests
nativeFeedBranches nativeFeedWrites nativeShortCalls nativeInstructions
nativeShortNominal nativeShortPending pendingFrames seenFrames
nativeRomBegin nativeRomEnd nativeRamBegin nativeRamEnd'''.split()
addresses = {v[-1]: int(v[0],16) for line in symbols.splitlines()
             if (v := line.split()) and v[-1] in names}
assert set(addresses) == set(names)
data = elf.read_bytes()
assert data[:6] == b'\x7fELF\x01\x02'
h = struct.unpack_from('>HHIIIIIHHHHHH', data, 16)
segments = []
for i in range(h[11]):
    _, kind, flags, address, offset, size, *_ = struct.unpack_from('>10I', data, h[5]+i*h[10])
    if kind == 1 and flags & 4:
        segments.append(struct.pack('>II',address,size)+data[offset:offset+size])
code = root/'tmp/native-feed-code.bin'
code.write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
meta = root/'tmp/native-feed-symbols.txt'
meta.write_text(''.join(f'{name} {address}\n' for name,address in addresses.items()))
subprocess.run([str(root/'build/native-feed-test'),str(code),str(meta)],check=True)
