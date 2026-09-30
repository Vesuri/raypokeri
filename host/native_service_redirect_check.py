#!/usr/bin/env python3
"""Extract relocation-free production redirect code and run the CPU matrix."""
from pathlib import Path
import struct
import subprocess
root = Path(__file__).resolve().parents[1]
b = (root / "build/native-service-redirect.o").read_bytes()
h = struct.unpack_from(">HHIIIIIHHHHHH", b, 16)
assert b[:6] == b"\x7fELF\x01\x02" and h[:2] == (1, 4)
sections = [struct.unpack_from(">10I", b, h[5]+i*h[10]) for i in range(h[11])]
strings = sections[h[12]]
names = b[strings[4]:strings[4]+strings[5]]
selected = [(i, s) for i, s in enumerate(sections)
            if names[s[0]:].split(b"\0")[0] == b".text.nativeServiceRedirect"]
assert len(selected) == 1
index, s = selected[0]
assert s[1] == 1 and s[2] & 4
assert not any(r[1] in (4, 9) and r[7] == index and r[5] for r in sections)
symbols = subprocess.check_output(["m68k-amiga-elf-objdump", "-t", str(root/"build/native-service-redirect.o")], text=True)
addresses = {v[-1]:int(v[0],16) for line in symbols.splitlines()
             if (v:=line.split()) and v[-1] in ("nativeServiceRedirect", "nativeServiceConsume")}
assert addresses["nativeServiceRedirect"] == 0
code = root/"build/native-service-redirect.bin"
code.write_bytes(b[s[4]:s[4]+s[5]])
subprocess.run([str(root/"build/native-service-redirect-test"),str(code),str(addresses["nativeServiceConsume"])],check=True)
