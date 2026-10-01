# RAY Pokeri — Amiga

An unofficial Amiga port of **Pokeri**, the 1986 Finnish video poker machine by
RAY (Raha-automaattiyhdistys). Bet, deal, hold and draw, then double your winnings
by guessing low or high.

The port runs the original 68008 program on the Amiga with Amiga implementations
of the board's devices: the Hitachi HD63484 graphics processor uses bitplanes and
the blitter, and the AY-3-8912 sound chip plays on Paula. Credits and accounting
are saved between sessions.

## Requirements and installation

- PAL Amiga with a 68020 or better; **AGA recommended**.
- WHDLoad 17+ and a supported Kickstart 3.1 image with its matching RTB file.
- The slave reserves 864 KB Chip RAM and 1472 KB other RAM, including its
  Kickstart image. Allow extra memory for the host system and PRELOAD.
- Installer V43+.

The release is `RAYPokeri-0.90.lha`. Open its **RAYPokeri Install** drawer and run
**Install**. Select the destination and the drawer containing the four ROM files
below. Start the installed **RAYPokeri** icon.

| ROM | Size | SHA-256 |
| --- | --- | --- |
| `77POK30` | 65,536 | `2841c2393d469c744eb5e575b08f1e4f13320e73fd205eb27d2ea2cf4b59decd` |
| `77POK38` | 65,536 | `fd87d156b71753d7ba03f548bf12bc3fee1d16477d1e89a9e9fe36521808ec8e` |
| `77POK34` | 65,536 | `3facfb79dfd6942a197bc6f9456712cb1a0de92e0035a589711988148f07c0a7` |
| `PARA200J` | 65,536 | `ae1b91f898d8d69a36fde41bff1139c94c8b9cb93e77b4c697ddc43a362d244b` |

No original ROMs, Kickstart image or WHDLoad binary is distributed. See
[the release ReadMe](release/ReadMe) for the supported Kickstart filenames.

## Controls

| Key | Action |
| --- | --- |
| Enter | Insert coin |
| F9 | Bet |
| Space | Deal / draw |
| F1–F5 | Hold cards 1–5 |
| F10 | Collect winnings |
| F6 | Double |
| F7 / F8 | Low / high |
| Escape or left mouse button | Quit and save |
| Help | Immediate WHDLoad quit; changes are not saved |

## Building

The game uses the `m68k-amiga-elf-gcc` toolchain (see
[GCC-PORT.md](src/platform/amiga/framework/GCC-PORT.md)) and vasm for the slave;
release packaging needs Python 3 and an LH5-capable LHa encoder.

```sh
make roms SRC=/path/to/four-rom-files   # verify and copy into rom/ (git-ignored)
cd amiga && . ./env.sh && make           # -> out/RAYPokeri
./run.sh                                 # play in FS-UAE
cd .. && make release                    # -> dist/RAYPokeri-0.90.lha
```

A playable host reference with SDL is described in [host/README.md](host/README.md).
Developer documentation: [architecture](docs/architecture.md),
[testing](docs/testing.md), [performance](docs/performance.md),
[release](docs/release.md) and [remaining work](docs/remaining-work.md).

## Credits

Pokeri and its original program belong to their respective copyright holders.
This is an unofficial fan port, not affiliated with or endorsed by them.
Amiga port by Vesuri.
