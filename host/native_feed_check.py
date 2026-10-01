#!/usr/bin/env python3
"""Run the opt-in assembled feed body against independent synthetic CPU code."""
from pathlib import Path
import argparse
import struct
import subprocess
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--elf', type=Path, default=root/'amiga/out/RAYPokeri.elf')
elf = parser.parse_args().elf
symbols = subprocess.check_output(['m68k-amiga-elf-objdump', '-t', str(elf)], text=True)
names = '''nativeInlineBoundaryMode nativeRegisterBoundary0 nativeRegisterBoundary1 nativeRegisterBoundary2 nativeFeedCounterMode nativeShortFeedRead nativeFeedBoundary0 nativeFeedBoundary1 nativeFeedSource
nativeShortLengthDone nativeShortControlPromote nativeShortVideoWriteValue
nativeFeedReplayContinue nativeShortReplayStart nativeShortVideoWrite
nativeFeedHeaderGrant nativeFeedHeaderWords nativeFeedInlineLength nativeFeedFormats
nativeFeedInlineCount nativeFeedInlineWord nativeFeedInlinePending nativeFeedInlineHigh nativeFeedInlineWords
nativeShuffleNextPointer nativeCachedVideoStatus nativeDiagnostic nativeFeedTarget nativeFeedTests
nativeFeedBranches nativeFeedWrites nativeShortCalls nativeInstructions
nativeShortNominal nativeShortPending pendingFrames seenFrames
nativeRomBegin nativeRomEnd nativeRamBegin nativeRamEnd
nativeFeedLoopValueReady nativeFeedLoopCallModel nativeShortFeedLoopWrite nativeFeedLoopAfterWrite nativeFeedBoundary nativeClockResumePc
nativeFeedLoopWords nativeFeedLoopTurns nativeFeedLoopSaved nativeShortNoControlDue'''.split()
addresses = {v[-1]: int(v[0],16) for line in symbols.splitlines()
             if (v := line.split()) and v[-1] in names}
assert set(addresses) == set(names)
for line in symbols.splitlines():
    v=line.split()
    if v and v[-1] in ('nativeBatch','nativeBatchFinish','nativeLiveCounterMode'):
        addresses[v[-1]]=int(v[0],16)
addresses.setdefault('nativeLiveCounterMode',1)
# Older frozen reference executables predate the optional joined-boundary flag.
addresses['nativeJoinedBoundaryMode'] = next((int(v[0],16) for line in symbols.splitlines()
    if (v:=line.split()) and v[-1]=='nativeJoinedBoundaryMode'),0)
data = elf.read_bytes()
assert data[:6] == b'\x7fELF\x01\x02'
h = struct.unpack_from('>HHIIIIIHHHHHH', data, 16)
segments = []
for i in range(h[11]):
    _, kind, flags, address, offset, size, *_ = struct.unpack_from('>10I', data, h[5]+i*h[10])
    if flags & 2:
        assert address+size <= 0x100000, "native allocation overlaps synthetic feed fixtures"
    if kind == 1 and flags & 4:
        segments.append(struct.pack('>II',address,size)+data[offset:offset+size])
code = root/'tmp/native-feed-code.bin'
code.write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
meta = root/'tmp/native-feed-symbols.txt'
meta.write_text(''.join(f'{name} {address}\n' for name,address in addresses.items()))
subprocess.run([str(root/'build/native-feed-test'),str(code),str(meta)],check=True)

# Independently assembled synthetic loop, with word branches and another field
# offset. No original instruction bytes are extracted for the oracle.
subprocess.run(['m68k-amiga-elf-as','-m68000','host/native_feed_loop_oracle.s','-o','tmp/feed-loop-oracle.o'],cwd=root,check=True)
subprocess.run(['m68k-amiga-elf-ld','-Ttext=0x100000','-e','oracle_status','tmp/feed-loop-oracle.o','-o','tmp/feed-loop-oracle.elf'],cwd=root,check=True)
oracle=root/'tmp/feed-loop-oracle.elf'
symbols=subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(oracle)],text=True)
addresses.update({v[-1]:int(v[0],16) for line in symbols.splitlines() if (v:=line.split()) and v[-1].startswith('oracle_')})
data=oracle.read_bytes();h=struct.unpack_from('>HHIIIIIHHHHHH',data,16)
for i in range(h[11]):
    _,kind,flags,address,offset,size,*_=struct.unpack_from('>10I',data,h[5]+i*h[10])
    if kind==1 and flags&4:segments.append(struct.pack('>II',address,size)+data[offset:offset+size])
code.write_bytes(struct.pack('>I',len(segments))+b''.join(segments))
meta.write_text(''.join(f'{name} {address}\n' for name,address in addresses.items()))
subprocess.run([str(root/'build/native-feed-loop-test'),str(code),str(meta)],check=True)
