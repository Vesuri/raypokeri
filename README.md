# Pokeri — Amiga

An Amiga port of *Pokeri*, the Finnish RAY (Raha-automaattiyhdistys) video poker machine.

The original board is a Motorola 68008 with a Hitachi HD63484 ACRTC graphics processor and a
General Instrument AY-3-8912 sound chip.  The port runs the original 68008 program on the
Amiga's 68000, and provides video and sound through Amiga bitplanes/blitter and Paula.

Initial Amiga release packaging is in `release/` and `whdload/`. The playable
native build has planar/blitter graphics, Paula audio and persistent accounting.
Performance work remains open; see [remaining work](docs/remaining-work.md).
The [release ReadMe](release/ReadMe) describes installation, controls and saves.

## Requirements

- Host reference: clang/clang++, GNU Make; SDL2 for the playable window.
- Initial WHDLoad release: PAL 68020+, 1 MB Chip and 4 MB expansion memory
  reserved by the slave, plus Kickstart and host overhead. A1200 with 2 MB Chip
  and 8 MB Fast RAM is the test configuration. NoVBRMove and NoWriteCache
  are required (set by the installer).
- Supply the four original ROM files, WHDLoad, Installer 43 and a supported
  Kickstart 3.1 image/RTB pair. None of those third-party files are packaged.

## Original data

This source repository ships **no** original ROM dumps.  You must supply your own dump of the four
64 KB EPROMs.  The host preparation tools verify checksums; the Amiga loader checks sizes and
audited patch sites:

| Chip | Size | SHA-256 |
|---|---|---|
| `77POK30` | 65,536 | `2841c2393d469c744eb5e575b08f1e4f13320e73fd205eb27d2ea2cf4b59decd` |
| `77POK34` | 65,536 | `3facfb79dfd6942a197bc6f9456712cb1a0de92e0035a589711988148f07c0a7` |
| `77POK38` | 65,536 | `fd87d156b71753d7ba03f548bf12bc3fee1d16477d1e89a9e9fe36521808ec8e` |
| `PARA200J` | 65,536 | `ae1b91f898d8d69a36fde41bff1139c94c8b9cb93e77b4c697ddc43a362d244b` |

```sh
make roms SRC=/path/to/four-rom-files   # verify + copy into rom/ (git-ignored)
```

Supply ROM dumps you have permission to use. This repository does not distribute
the ROM files or point to a download.

## Building

For the playable host version, follow [host/README.md](host/README.md).

The Amiga cross-build uses the BartmanAbyss `vscode-amiga-debug` toolchain installed in `~/.local`
(see `src/platform/amiga/framework/GCC-PORT.md`):

```sh
cd amiga
. ./env.sh      # the toolchain on PATH (same shell as the build)
make            # -> out/Pokeri
./run.sh        # play in FS-UAE (Esc or left mouse button saves and quits)
./debug.sh      # source-level debugging through FS-UAE's gdb stub
```

Normal launches require the ROMs, not a diagnostic replay. The Amiga loader
searches `data/`, the current drawer and legacy `rom/` in that order. Saves are
relative to the current drawer. For native research/replay see the
[native diagnostic procedure](docs/phase4-preflight.md).

Build the initial release with `make release`; the output is
`dist/Pokeri-0.1.lha`. The installer accepts one drawer containing the four chips.
It preserves existing ROMs and saves when updating with Keep.

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
