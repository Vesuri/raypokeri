# Pokeri — Amiga

An Amiga port of *Pokeri*, the Finnish RAY (Raha-automaattiyhdistys) video poker machine.

The original board is a Motorola 68008 with a Hitachi HD63484 ACRTC graphics processor and a
General Instrument AY-3-8912 sound chip.  The port runs the original 68008 program on the
Amiga's 68000, and reimplements the video and sound hardware on Amiga bitplanes/blitter and
Paula.

> **Research stage.**  Only the ROM dumps are available — no schematics, manual or reference
> emulator.  Nothing is playable yet.  Findings so far: [docs/rom-set.md](docs/rom-set.md).

## Requirements

- An Amiga (target: A500+, Kickstart 3.1, PAL)

## Original data

This project ships **no** original game data.  You must supply your own dump of the four
64 KB EPROMs.  The tools check each chip against its checksum and refuse anything else:

| Chip | Size | SHA-256 |
|---|---|---|
| `77POK30` | 65,536 | `2841c2393d469c744eb5e575b08f1e4f13320e73fd205eb27d2ea2cf4b59decd` |
| `77POK34` | 65,536 | `3facfb79dfd6942a197bc6f9456712cb1a0de92e0035a589711988148f07c0a7` |
| `77POK38` | 65,536 | `fd87d156b71753d7ba03f548bf12bc3fee1d16477d1e89a9e9fe36521808ec8e` |
| `PARA200J` | 65,536 | `ae1b91f898d8d69a36fde41bff1139c94c8b9cb93e77b4c697ddc43a362d244b` |

```sh
make roms SRC=/path/to/pokeri-rom.zip    # verify + unpack into rom/ (git-ignored)
```

The dump is legally yours to use only if you own the original hardware.  This repository does not
distribute the ROMs or point to any download.

## Building

The Amiga cross-build uses the BartmanAbyss `vscode-amiga-debug` toolchain installed in `~/.local`
(see `src/platform/amiga/framework/GCC-PORT.md`):

```sh
cd amiga
. ./env.sh      # the toolchain on PATH (same shell as the build)
make            # -> out/Pokeri
./run.sh        # boot it in FS-UAE (left mouse button quits)
./debug.sh      # source-level debugging through FS-UAE's gdb stub
```

## Layout

| Path | What |
|---|---|
| `src/platform/amiga/framework/` | the dA JoRMaS framework hardware classes (template subset) |
| `src/platform/amiga/` | the Amiga application (`Pokeri`) and runtime |
| `amiga/` | build + run/debug/probe scripts |
| `docs/` | research findings and design notes |
| `ghidra_scripts/`, `disasm/symbols.csv` | disassembly tooling; the curated names |
| `tools/` | host-side tools (ROM verification, …) |
