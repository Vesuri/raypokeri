# Pokeri — Amiga

An Amiga port of *Pokeri*, the Finnish RAY (Raha-automaattiyhdistys) video poker machine.

The original board is a Motorola 68008 with a Hitachi HD63484 ACRTC graphics processor and a
General Instrument AY-3-8912 sound chip.  The port runs the original 68008 program on the
Amiga's 68000, and provides video and sound through Amiga bitplanes/blitter and Paula.

> **Status (2026-09-25):** the host SDL version is playable with live audio.
> Native Amiga diagnostic boot passes a complete RAM comparison against the host.
> Native planar video matches the host pixel for pixel, and replay boot switches
> to live Paula audio and controls, but the extended live test resets. Phase 5
> remains in progress; the live timing fix is pending.
> See the [Amiga instructions](docs/phase5-amiga.md), [bring-up plan](docs/bringup-plan.md),
> [host instructions](host/README.md) and [ROM findings](docs/rom-set.md).

## Requirements

- Host reference: clang/clang++, GNU Make; SDL2 for the playable window.
- Native diagnostic target: A500+, 68000, Kickstart 3.1, PAL; validated in
  FS-UAE with 1 MB Chip and 8 MB Fast RAM. Final release requirements are unsettled.

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

For the playable host version, follow [host/README.md](host/README.md).

The Amiga cross-build uses the BartmanAbyss `vscode-amiga-debug` toolchain installed in `~/.local`
(see `src/platform/amiga/framework/GCC-PORT.md`):

```sh
cd amiga
. ./env.sh      # the toolchain on PATH (same shell as the build)
make            # -> out/Pokeri
./run.sh        # run the prepared diagnostic in FS-UAE (left mouse button quits)
./debug.sh      # source-level debugging through FS-UAE's gdb stub
```

The native build requires verified ROMs and a local `replay.bin` staged beside
the executable. Follow the [native diagnostic procedure](docs/phase4-preflight.md)
before running it. Use the `native-live` marker described in the [Amiga instructions](docs/phase5-amiga.md)
to continue into live play with display and Paula audio.

## Layout

| Path | What |
|---|---|
| `host/` | Musashi reference, playable SDL frontend and validation tools |
| `src/board/`, `src/native/` | shared device models, native hook semantics and replay support |
| `src/platform/amiga/framework/` | the dA JoRMaS framework hardware classes (template subset) |
| `src/platform/amiga/` | the Amiga application (`Pokeri`) and runtime |
| `amiga/` | build + run/debug/probe scripts |
| `docs/` | research findings and design notes |
| `ghidra_scripts/`, `disasm/symbols.csv` | disassembly tooling; the curated names |
| `tools/` | host-side tools (ROM verification, …) |

### Play on the host

Build with `make harness SDL=1`, then run `build/pokeri-host-sdl`. It prepares a
fresh game automatically, enables live audio, and plays until you quit. No
frames, WAVs or diagnostic captures are written unless requested. Space deals
and draws, 1–5 hold cards, C inserts a coin, and Escape quits. See
[host options and controls](host/README.md).
