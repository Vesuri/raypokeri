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
contiguously as `$00000–$2FFFF`.

DERIVED: `bsr.w` calls from the end of bank 2 (`$2E906`, `$2ECBA`, `$2F778`, …) land all over
`$30000–$3FFFF`, so `PARA200J` is mapped directly after them: the ROM is one contiguous
`$00000–$3FFFF` image.  (It is still possible those bytes are data that happens to decode as
calls; the harness boot confirms or kills this.)

## The memory map so far

The 68008 has 20 address lines (1 MB); the code masks addresses with `andi.l #$FFFFF`
(`$1E98`, `$29F6`, `$16140`), which fits.  Device bases are loaded as immediates
(`movea.l #$xxxxx,An`) and then used as `d16(An)`, so the base immediates are few, but the
access sites are not.

| Range | What | Evidence |
|---|---|---|
| `$00000–$3FFFF` | ROM: `77POK30/34/38` + `PARA200J` | DERIVED (above) |
| `$40000–$4xxxx` | work RAM.  Reset SSP `$40B00`, USP `$40700`; globals are `a6`-relative with `a6 = $48B00`, at negative offsets (`-$8000(a6)` = `$40B00` …) | MEASURED (`$2194–$21AE`) |
| `$C0000` | **MC68681 DUART**: init writes `$1A` to CRA (+2) and CRB (+10) = reset MR pointer / disable Rx+Tx, then MR1A=`$13`, MR2A=`$0F`, CSRA=`$DD`.  The base is stored at `-$8000(a6)`.  Its counter/timer is the likely level-2 IRQ source | INFERRED, strong (`$09C8`) |
| `$D0000–$D7FFF` | 32 KB scanned word by word, likely battery-backed RAM (the "cash memory") | INFERRED (`$270AA`) |
| `$E0000` | I/O port written with per-button/lamp codes at +4 | INFERRED (`$5B3A`) |
| `$F6000` | **HD63484 ACRTC**: word writes to the FIFO at +2 (`$4800`, `$55AA`, `$4800`, `$AA55` — a command + pattern), register select / status at +0 | INFERRED, strong (`$1D3C`, `$2E4E4`) |
| `$FB000` | device with base + 2 access; the AY-3-8912 is the prime candidate (address latch / data) | INFERRED (`$1B02`, `$EA8E`) |

The AY-3-8912 has not been positively located yet, and neither has the palette hardware
(the HD63484 has none of its own).

## Reset: the code relocates its own RAM (MEASURED, `$217E`)

```
217e  lea     $24EA(pc),a0
2182  moveq   #0,d7
2184  move.l  #0,(a0)          ; write to its own code location…
218a  tst.l   (a0)
218c  bne.s   $2194            ; …still nonzero → running from ROM, d7 = 0
218e  move.l  #$70000,d7       ; the write stuck → running from RAM, shift RAM by $70000
2194  movea.l #$40B00,a6 / adda.l d7,a6 / movea.l a6,sp   ; and USP, a6, RAM-test ranges likewise
```

The program was built to run **relocated from RAM** (a development setup), with every RAM
base derived from `d7`, globals `a6`-relative and code PC-relative (`bsr.w`, `lea d16(pc)`).
DERIVED: that makes RAM relocation mostly a matter of choosing `d7`.  ROM-absolute
references (vectors, jump tables, pointer tables) and device addresses are the remaining
relocation work.

Also at reset: warm-start magics `$AA552CE2` (at `-4(sp)`) and `$AA55E22C` (at
`-$787A(a6)`), a RAM test at `$11FA` that returns through `a4` (no stack yet), and a RAM-based
jump table called as `jsr -$6F4A(a6)` (MEASURED).

INFERRED: interrupt level 2 is the one device IRQ.  The handler at `$0A6E` saves d0–a5,
masks to IPL 7 and calls `$780E` (MEASURED).  The DUART timer is the most likely source.

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

### The pay tables — the "200 mk" in the name (MEASURED)

Five blocks of `$40` bytes, one per bet level, each holding eight big-endian words in hand order
from lowest to highest: two pairs, three of a kind, straight, flush, full house, four of a kind,
straight flush, five of a kind.  Unused words are `$FFFF`.

| Bet | Offset | 2P | 3K | St | Fl | FH | 4K | SF | 5K |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `$268` | 3 | 3 | 4 | 5 | 10 | 15 | 30 | 0 |
| 2 | `$2A8` | 6 | 6 | 8 | 10 | 20 | 30 | 60 | 0 |
| 3 | `$2E8` | 6 | 6 | 12 | 15 | 30 | 45 | 90 | 120 |
| 4 | `$328` | 8 | 8 | 16 | 20 | 40 | 60 | 120 | 160 |
| 5 | `$368` | 10 | 10 | 20 | 25 | 50 | 75 | 150 | 200 |

The bet-3 row matches the Finnish footage exactly (`docs/visual-reference.md`).  The top prize at
the top bet is **200 mk**, which is where the machine's "200 mk" name comes from.  The tables are
not linear in the bet: five of a kind pays nothing below bet 3, and bets 2 and 3 pay the same
for two pairs and three of a kind.  The words just before bet 1 (`$260`: 400, 0, 0, 8) look like
a header (8 = the number of hands?), and `$200–$25F` holds long-word triplets
(150/200/5, 200/300/7, 300/500/10, 500/3000/40, …) that are probably limits or coin/banknote
configuration.  Both are unidentified.

**The 100 mk version** (the user reports that 100 mk and 200 mk machines both existed): the pay
rules, including the top prize, live in this parameter chip, and only one set of five tables is
in it.  So a 100 mk machine most likely differs by its `PARA` chip (a `PARA100x`), not by a DIP
switch on the program ROMs (DERIVED from the above; not excluded until the harness shows no input
selects between tables).  The `MAKSIMIPANOS` (max bet) setting in the service menu can cap the
bet, which also caps the top prize, but the machine stays a 200 mk one.

INFERRED: `PARA200J` is a separately loaded module (the `$4AFC` tag suggests the main ROM
scans for it) holding the settings/service UI, not game logic.  The `J` suffix may be a
revision or a regional/legal variant.

## Open questions

1. The rest of the memory map: RAM extent (the code stores `$80000` and compares against
   `$7F000` at `-$787E(a6)` — memory sizing?), the AY-3-8912, the palette, and the
   input/lamp/hopper/coin-mech ports.  The harness's device-access trace answers this
   (`docs/bringup-plan.md`).
2. What exactly the level-2 and NMI handlers service.
3. The module format: how the main ROM finds `g200para` (a scan for `$4AFC`?) and whether
   `romgame` is itself such a module.
4. Which compiler produced the code.
5. Battery-backed RAM / NVRAM (the "cash memory", `KASSAMUISTI`) — its location and layout,
   because the game will refuse to run or will reset its books without a valid image.

## Host bring-up: first cold-reset trace (2026-09-24)

Phase 0 reproducible command: `make harness && build/pokeri-host --probe
--instructions 100000000`. Captures are `tmp/phase0-{trace.csv,coverage.bin,context.txt}`.
The zero-read probe is explicitly diagnostic, not a successful device model.
It stops after 22,710,087 instructions (20 million without a new PC), with 385
unique executed PCs and 13,386 device accesses. The time count is 204,738,390
Musashi 68000 cycles, not a measured 68008 cycle count.

- **MEASURED (execution):** reset passes the ROM-write probe with `d7=0`, tests work
  RAM and reaches `$1F2E`. The first device access is a byte write of 3 to `$FB002`
  at PC `$1F36`, instruction 1,769,571. No accesses to `$C0000`, `$D0000` or
  `$E0000` occur before the fatal startup loop.
- **MEASURED (execution and instruction inspection):** `$11B4` tests three four-byte
  register groups at `$FB014`, `$FB018`, `$FB01C`. Offsets 0/2 are read back after
  writes; setting bit 2 at offsets 1/3 changes their function. `$1A1C` initialises
  all six control registers. **INFERRED (strong):** these are three 6821-compatible
  PIAs, not a single AY register pair. The DDR/control selection and flag layout
  match Motorola's [MC6821 data sheet, figure 18](https://www.komponenten.es.aau.dk/fileadmin/komponenten/Data_Sheet/Microprossor/MC6821.pdf).
- **MEASURED:** `$C58` sends register numbers 0–13 and data through `$D58`, using
  `$FB014` as the data bus and bits 1/7 at `$FB016` as strobes. `$1FEE` writes test
  values `$55` and `$0A` and tries to read them back through that bus.
  **MEASURED (trace):** 60 register/data pairs pass through `$D58`: registers
  0–6 and 8–13 each receive 4 writes; register 7 receives 8. Values are 0/`$55`
  for register 0; 0/`$0A` for register 1; 0/`$FF` for register 7; zero for the
  others. These are software bus writes, not confirmation that an AY accepted them.
  **INFERRED:** the AY is behind the first PIA; its precise gating/wiring is not
  established. Direct AY mapping at `$FB000/+2` is not supported by this trace.
- **MEASURED:** `$2106` writes `$0E` to `$FB017`, reads/writes `$FB016`, then polls
  bit 6 of `$FB017` at `$2118` for up to 8,193 iterations. Zero reads exhaust it;
  `$212C` sets failure flags and `$1FE0` returns error code `$00060006`. Reset
  branches from `$227A` to the fatal display loop `$24FA`, repeating `$2526–$256E`.
  This is **not attract/idle**. **INFERRED:** this is a PIA CB2 edge flag check;
  the external signal source and frequency are unidentified. Setting the flag
  merely to pass boot would be a guessed success.
- **MEASURED (instruction inspection):** later startup tests also expect bit 6 at
  `$FB01F` (`$209E`) and bit 7 at `$FB015` (`$2132`) with bounded delays. These are
  not evidence of a DUART clock. No timer programming or level-2 interrupt was
  observed in this run; neither CPU clock nor IRQ rate can yet be estimated.
- **MEASURED:** the fatal loop reads `$F6000` twice as bytes and compares the pair
  with `$2323`. It sends **zero HD63484 commands**. No drawing was implemented.
- **MEASURED (instruction inspection):** `$227E` calls the second ROM-write probe
  `$25A4`; the ROM branch explicitly sets the module address to `$30000` at
  `$2292` (RAM branch uses `$BC000`). This strengthens the **DERIVED** PARA200J
  mapping, but the failing cold boot has not reached module loading or executed
  PARA200J. There is no runtime pay-table-selection evidence yet.

**Bring-up gate:** Phase 0 reaches and diagnoses the fatal startup loop. Phase 1
is blocked before the planned NVRAM/DUART sequence by the unidentified external
PIA signals. No flags, input values, ROM instructions or registers were forced
in order to advance past this gate. The remaining map entries above retain
only their earlier evidence, not runtime confirmation.
