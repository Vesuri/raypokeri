#!/usr/bin/env python3
"""Create the minimal deterministic Amiga LHA distribution (LH5)."""
import argparse
from pathlib import Path
import _amigaxdev  # noqa: F401
from amigaxdev import release
from amigaxdev.release import crc16  # noqa: F401  (check_release.py imports it from here)

from roms import CHIPS
ORIGINAL_HASHES = {v[1] for v in CHIPS.values()}
PREFIX = "RAYPokeri Install"
STAMP = release.dos_stamp(2026, 10, 11)
ICONS = Path(__file__).resolve().parent.parent / "release/icons"

def fresh_save_slots():
    # Authored empty slots, never extracted from a running game or its ROMs.
    return {"EmptyNVRAM": bytes(32768), "FreshAccounting": b"PKAF0001" + bytes(932)}

def installer_icon(game=False):
    # RAYPokeri.inf is distributed as is, renamed and configured by Installer, exactly
    # as RoF.inf in the reference package. No synthetic replacement artwork.
    return release.installer_icon(ICONS, "RAYPokeri", game=game)

def readme_icon():
    return release.template_icon(ICONS, "readme.info.b64")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    version = (root / "VERSION").read_text().strip()
    files = {
        "RAYPokeri": args.executable.read_bytes(),
        "RAYPokeri.slave": (root / "build/whdload/RAYPokeri.slave").read_bytes(),
        "RAYPokeri.inf": installer_icon(game=True),
        "Install": (root / "release/Install").read_bytes(),
        "Install.info": installer_icon(),
        "ReadMe.info": readme_icon(),
        "ReadMe": (root / "release/ReadMe").read_bytes(),
    }
    files.update(fresh_save_slots())
    release.refuse_originals(files, ORIGINAL_HASHES)
    archive = release.write(args.output_directory / f"RAYPokeri-{version}.lha",
                            release.archive(PREFIX, files, release.template_icon(ICONS, "drawer.info.b64"), STAMP))
    print(f"{archive}: {len(files)} drawer contents plus drawer icon, {archive.stat().st_size} bytes")

if __name__ == "__main__":
    main()
