#!/usr/bin/env python3
"""Verify the user-supplied Pokeri EPROM set and unpack it to rom/ (git-ignored).

    python3 tools/roms.py [SOURCE]      # SOURCE = .zip or a directory; default tmp/pokeri-rom.zip
    python3 tools/roms.py --check       # verify an already-unpacked rom/

The repository ships NO original data.  This tool only reads the user's own dump,
checks every chip against the SHA-256 recorded below, and writes the chips to
rom/<NAME> for the rest of the tooling to consume.  Case-insensitive on names
(MAME-style sets are usually lowercase).  Exits non-zero on any mismatch.

The ROLE column is the current best reading (docs/rom-set.md), not a measured fact.
"""
import hashlib
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEST = REPO / "rom"
DEFAULT_SOURCE = REPO / "tmp" / "pokeri-rom.zip"

# name: (size, sha256, role)
CHIPS = {
    "77POK30":  (65536, "2841c2393d469c744eb5e575b08f1e4f13320e73fd205eb27d2ea2cf4b59decd",
                 "68008 program, mapped at $00000, holds the reset vectors"),
    # The upper 32 KB is zero-filled, which is harmless: mapped at $28000-$2FFFF it lies beyond
    # the main module's end ($276FD).  ⚠ The address order is 30, 38, 34 (docs/rom-set.md).
    "77POK34":  (65536, "3facfb79dfd6942a197bc6f9456712cb1a0de92e0035a589711988148f07c0a7",
                 "68008 program, mapped at $20000"),
    "77POK38":  (65536, "fd87d156b71753d7ba03f548bf12bc3fee1d16477d1e89a9e9fe36521808ec8e",
                 "68008 program, mapped at $10000"),
    "PARA200J": (65536, "ae1b91f898d8d69a36fde41bff1139c94c8b9cb93e77b4c697ddc43a362d244b",
                 "parameter/settings module ($4AFC header, Finnish texts)"),
}


def read_source(src: Path) -> dict:
    """Return {UPPERCASE_NAME: bytes} for every file in a zip or directory."""
    out = {}
    if src.is_dir():
        for p in src.iterdir():
            if p.is_file():
                out[p.name.upper()] = p.read_bytes()
    elif zipfile.is_zipfile(src):
        with zipfile.ZipFile(src) as z:
            for info in z.infolist():
                if not info.is_dir():
                    out[Path(info.filename).name.upper()] = z.read(info)
    else:
        sys.exit(f"roms: not a zip or directory: {src}")
    return out


def verify(files: dict) -> bool:
    ok = True
    for name, (size, sha, role) in CHIPS.items():
        data = files.get(name)
        if data is None:
            print(f"  MISSING  {name:9s} {role}")
            ok = False
        elif len(data) != size or hashlib.sha256(data).hexdigest() != sha:
            print(f"  BAD      {name:9s} size={len(data)} sha256={hashlib.sha256(data).hexdigest()}")
            ok = False
        else:
            print(f"  OK       {name:9s} {role}")
    return ok


def main() -> None:
    args = sys.argv[1:]
    if args == ["--check"]:
        ok = verify(read_source(DEST)) if DEST.is_dir() else False
        sys.exit(0 if ok else 1)
    src = Path(args[0]) if args else DEFAULT_SOURCE
    if not src.exists():
        sys.exit(f"roms: source not found: {src}  (supply your own dump; see README.md)")
    print(f"roms: verifying {src}")
    files = read_source(src)
    if not verify(files):
        sys.exit("roms: set does not match — nothing written")
    DEST.mkdir(exist_ok=True)
    for name in CHIPS:
        (DEST / name).write_bytes(files[name])
    print(f"roms: wrote {len(CHIPS)} chips to {DEST.relative_to(REPO)}/")


if __name__ == "__main__":
    main()
