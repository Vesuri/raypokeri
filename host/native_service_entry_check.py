#!/usr/bin/env python3
"""Exercise linked W3 wrappers and descriptor with explicit clock test doubles."""
import argparse
from pathlib import Path
import struct
import subprocess
root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf', type=Path, required=True)
a = p.parse_args()
b = a.elf.read_bytes()
assert b[:6] == b"\x7fELF\x01\x02"
h = struct.unpack_from('>HHIIIIIHHHHHH', b, 16)
sections = [struct.unpack_from('>10I', b, h[5]+i*h[10]) for i in range(h[11])]
allocated = [s for s in sections if s[2]&2]
end = max(s[3]+s[5] for s in allocated)
assert end < 0x100000
image = bytearray(end)
for s in allocated:
    if s[1] != 8:
        image[s[3]:s[3]+s[5]] = b[s[4]:s[4]+s[5]]
names = {'nativeClockEnter','nativeClockPauseInterrupt','nativeClockCalibrating',
 'nativeServiceRedirectEnabled','nativeServiceRedirectState','nativeServiceOpcode',
 'nativeProfileEnabled','nativeRegisters','nativeServiceDescriptor','nativeSave',
 'nativeFault','nativeLineA','nativeDiagnostic',
 'nativeShortCount','nativeShortStatus','nativeClockEnabled',
 'nativeServiceRequest','nativeServiceRequestPending','nativeExit','nativeStatus','nativeError'}
for n in (2,3,4,6):
    names.update((f'nativeLevel{n}',f'nativeOldLevel{n}'))
listing = subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(a.elf)],text=True)
symbols = {v[-1]:int(v[0],16) for line in listing.splitlines()
           if (v:=line.split()) and v[-1] in names}
assert symbols.keys() == names, names-symbols.keys()
img, syms = root/'build/service-entry-image.bin', root/'build/service-entry-symbols.txt'
img.write_bytes(image)
syms.write_text(''.join(f'{name} {value:x}\n' for name,value in sorted(symbols.items())))
subprocess.run([str(root/'build/native-service-entry-test'),str(img),str(syms)],check=True)
