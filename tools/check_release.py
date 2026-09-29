#!/usr/bin/env python3
"""Audit the exact minimal LHA release and its header and payload checksums."""
import argparse
import hashlib
import struct
import subprocess
from pathlib import Path
from package_release import ORIGINAL_HASHES, PREFIX, crc16
from installer_icon import installer_icon, readme_icon

REQUIRED = {"Pokeri", "Pokeri.slave", "Pokeri.inf", "Install", "Install.info", "ReadMe", "ReadMe.info"}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    raw = args.archive.read_bytes()
    pos = 0
    payloads = {}
    while pos < len(raw) and raw[pos]:
        size = raw[pos]
        header = raw[pos + 2:pos + 2 + size]
        assert len(header) == size and sum(header) & 255 == raw[pos + 1], "bad header checksum"
        assert header[:5] == b"-lh5-" and header[18] == 0, "expected level-zero LH5"
        packed, unpacked = struct.unpack_from("<II", header, 5)
        n = header[19]
        assert size == 22 + n
        name = header[20:20 + n].decode("ascii").replace("\\", "/")
        archive_name = name
        if name == PREFIX + '.info':
            name = '@drawer'
        else:
            assert name.startswith(PREFIX + "/"), "wrong installation drawer"
            name = name[len(PREFIX) + 1:]
        assert name in REQUIRED | {'@drawer'} and name not in payloads, "unexpected or duplicate file"
        pos += size + 2
        assert len(raw[pos:pos + packed]) == packed, "truncated payload"
        # Decode independently with Lhasa, without extracting paths to disk.
        data = subprocess.run(["lha", "pq", str(args.archive.resolve()), archive_name],
                              check=True, capture_output=True).stdout
        assert len(data) == unpacked and crc16(data) == struct.unpack_from("<H", header, 20 + n)[0], "bad payload CRC"
        assert hashlib.sha256(data).hexdigest() not in ORIGINAL_HASHES
        payloads[name] = data
        pos += packed
    assert raw[pos:] == b"\0" and set(payloads) == REQUIRED | {'@drawer'}, "wrong archive contents"
    for name in ("Pokeri", "Pokeri.slave"):
        assert payloads[name][:4] == b"\0\0\3\xf3", "not an Amiga HUNK executable"
    assert b'WHDLOADS' in payloads['Pokeri.slave'], 'missing WHDLoad slave header'
    for name, kind in (("Pokeri.inf", 4), ("ReadMe.info", 4), ("Install.info", 4), ('@drawer', 2)):
        assert payloads[name][:4] == b"\xe3\x10\0\1" and payloads[name][48] == kind
    root=Path(__file__).resolve().parent.parent
    assert payloads["Pokeri"] == (root/"amiga/out/Pokeri").read_bytes()
    assert payloads["Pokeri.slave"] == (root/"build/whdload/Pokeri.slave").read_bytes()
    for name in ("Install","ReadMe"):
        assert payloads[name] == (root/"release"/name).read_bytes()
    version=(root/'VERSION').read_text().strip().encode()
    for name in ('Pokeri','Pokeri.slave'):
        assert b'$VER: '+name.encode()+b' '+version+b' (29.09.2026)' in payloads[name]
    assert b"APPNAME=Pokeri\0" in payloads["Install.info"]
    assert payloads["Pokeri.inf"]==installer_icon(game=True)
    assert payloads["ReadMe.info"]==readme_icon()
    assert not any(n.lower().endswith((".rek",".cfg",".plr",".sss",".000",".dat",".ss")) for n in payloads)
    print(f"PASS: {len(payloads)} allowlisted LH5 members; independent decompression, CRCs, executables, scripts and icons match")
if __name__=="__main__": main()
