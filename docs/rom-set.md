# The Pokeri ROM set

The only primary source.  There is no schematic, manual or MAME driver (MAME 0.289 has none
matching these chip names), so everything below is read out of the bytes.  Each claim is
tagged **MEASURED** (read directly from the dump), **DERIVED** (follows from measured facts)
or **INFERRED** (plausible reading, not yet proven).  Upgrade the tag when you prove one;
delete the claim when you disprove it.

The chips live in `rom/` (git-ignored), unpacked and verified by `make roms`
(`tools/roms.py`, which holds the checksums).  Never commit them or anything derived from
them byte for byte.

## Chips

| Chip | Size | SHA-256 | Role |
|---|---|---|---|
| `77POK30` | 64 KB | `2841c239…9decd` | 68008 program, bank 0 — holds the reset vectors |
| `77POK34` | 64 KB | `3facfb79…c0a7` | 68008 program, bank 1 |
| `77POK38` | 64 KB | `fd87d156…ec8e` | 68008 program, bank 2 |
| `PARA200J` | 64 KB | `ae1b91f8…244b` | parameter / service-menu module |

None of the four has `$FF` padding at the end: all 64 KB of each chip is in use (MEASURED).
The 68008 has an 8-bit data bus, so each chip is a plain linear byte image with no
even/odd interleave (DERIVED; confirmed by 77POK30 decoding as a sane vector table).

## 77POK30 — the vector table (MEASURED)

| Vector | Offset | Value | Note |
|---|---|---|---|
| Reset SSP | `$000` | `$00040F00` | RAM reaches at least `$40F00` |
| Reset PC | `$004` | `$00000450` | points at `bra.w $217E` |
| Bus / address error | `$008`/`$00C` | `$0B9A` / `$0BAA` | `$0BAA` is the shared "unexpected exception" handler (also illegal, div0, CHK, TRAPV, privilege, trace, line A/F, spurious) |
| Level 2 autovector | `$068` | `$0A6E` | the only hardware IRQ level wired besides NMI |
| Level 7 (NMI) | `$07C` | `$0EE0` | |
| TRAP #0–#14 | `$080–$0B8` | `$29D0`, `$0DB8`, `$0DD6`, `$141E`, … `$12C14` | an OS/runtime reached by TRAPs; #4, #6 unused |
| TRAP #15 | `$0BC` | `$29D0` | same handler as TRAP #0 |

DERIVED: the TRAP #14 target `$12C14` lies in bank 1, so the three program chips are mapped
contiguously as `$00000–$2FFFF`.  Where `PARA200J` and the video/sound/IO devices sit is
unknown.

INFERRED: interrupt level 2 is the one device IRQ.  The HD63484 is the most likely source
(its INT output, on e.g. vertical blank or command end), but that needs to be read from the
handler at `$0A6E`.

## 77POK30 — the "romgame" module (MEASURED)

At `$0448` is the NUL-terminated name `romgame`, and at `$0450` (the reset PC) a `bra.w`.
Immediately after, at `$0454`, is a second table in the same shape as the vector table
(`$00040B00`, `$00000050`, `$079A`, `$07AA`, …).  INFERRED: a module/ROM header followed by
a table the startup code copies or installs, maybe a RAM vector copy with a different stack.

Strings `" of: "`, `", time: "`, `"Possible caller: "` suggest a crash/diagnostic reporter.
The code is full of `link a5` / `unlk a5; rts` (`4E55` / `4E5D 4E75`) frames: compiled C
with an a5 frame pointer (INFERRED — identify the compiler before trusting any calling
convention).

## PARA200J — the parameter module (MEASURED)

Starts with `$4AFC` — the `ILLEGAL` opcode used as a magic word, exactly like an Amiga
`RT_MATCHWORD` resident tag — followed by header words and the name `g200para` at `$48`.
The rest carries the operator/service menus in Finnish, written in 7-bit Finnish ASCII
(`[` = Ä, `\` = Ö, `]` = Å — a display font will need these glyphs):

- settings: `HOPPERIN 1 RAJA` (hopper limits), `KOLIKON n ARVO` (coin values),
  `SETELIN n ARVO` (banknote values), `PELIN MINIMIHINTA`, `MAKSIMIPANOS` (min price, max bet)
- `TESTI` menu: fault-code statistics, machine info, refunds, coin-sensor test, display
  test, lamp test, switches and sound (`KYTKIMET JA ÄÄNI`), payout test, money channels,
  hopper contents, time setting, counter reset, game features, cash-memory reset, pricing

INFERRED: `PARA200J` is a separately loaded module (the `$4AFC` tag suggests the main ROM
scans for it) holding the settings/service UI, not game logic.  The `J` suffix may be a
revision or a regional/legal variant.

## Open questions

1. Full memory map: RAM extent (≥ `$40F00`), where `PARA200J` maps, HD63484 and AY-3-8912
   and input/lamp/hopper/coin-mech ports.  Find them from absolute-address accesses in a
   Ghidra pass (a DumpHwAccesses-style script, as Rescue on Fractalus used).
2. What exactly the level-2 and NMI handlers service.
3. The module format: how the main ROM finds `g200para` (a scan for `$4AFC`?) and whether
   `romgame` is itself such a module.
4. Which compiler produced the code.
5. Battery-backed RAM / NVRAM (the "cash memory", `KASSAMUISTI`) — its location and layout,
   because the game will refuse to run or will reset its books without a valid image.
