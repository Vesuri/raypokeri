#!/usr/bin/env python3
"""Validate relocated and rejected debugger mappings against a linked test ELF."""
import argparse
from pathlib import Path
import struct
from whdload_symbols import ANCHORS, layout, resolve

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf',type=Path,required=True)
a=p.parse_args();elf=a.elf.read_bytes();sections,symbols=layout(elf)
base=0x200000
for shift in (0,0x8000,0x90000):
    ram=bytearray(0x800000)
    addresses=dict(zip(('.text','.rodata','.data','.bss'),(0x210000+shift,0x330000+shift,0x450000+shift,0x550000+shift)))
    for name,address in addresses.items():
        s=sections[name]
        if s[1]!=8:ram[address-base:address-base+s[5]]=elf[s[4]:s[4]+s[5]]
    at=addresses['.data']+symbols['nativeWhdDebugMap'][0]-sections['.data'][3]-base
    pointers=[addresses[section]+symbols[name][0]-sections[section][3] for name,section in ANCHORS]
    struct.pack_into('>5I',ram,at+16,*pointers,at+base)
    assert resolve(elf,ram,base)==addresses
    for offset in (at,at+32,pointers[0]-base,pointers[1]-base):
        bad=ram.copy();bad[offset]^=1
        assert resolve(elf,bad,base) is None
    # Raw file-cache descriptors must not be mistaken for relocated load images.
    bad=ram.copy();struct.pack_into('>I',bad,at+32,symbols['nativeWhdDebugMap'][0])
    assert resolve(elf,bad,base) is None
    # A second valid data hunk/map must be rejected, even with matching code.
    other=0x800000
    s=sections['.data'];start=addresses['.data']-base
    ram[other-base:other-base+s[5]]=ram[start:start+s[5]]
    second=other+symbols['nativeWhdDebugMap'][0]-s[3]-base
    struct.pack_into('>I',ram,second+24,other+symbols['pokeriWhdLoad'][0]-s[3])
    struct.pack_into('>I',ram,second+32,second+base)
    try:resolve(elf,ram,base)
    except ValueError:pass
    else:raise AssertionError('ambiguous mapping accepted')
print('PASS: three independent section layouts, corrupt signature/self/code/version, raw cache rejection and duplicate-map refusal')
