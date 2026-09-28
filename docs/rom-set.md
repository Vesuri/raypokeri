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

**MEASURED: the program chips are mapped in the order `77POK30`, `77POK38`, `77POK34`**, not
in name order:

| Range | Chip | Contents |
|---|---|---|
| `$00000–$0FFFF` | `77POK30` | vectors, startup, main-module header at `$400` |
| `$10000–$1FFFF` | `77POK38` | code (the TRAP #14 target `$12C14` is here) |
| `$20000–$2FFFF` | `77POK34` | code up to the module end `$276FD`; `$28000–$2FFFF` is zero-filled free space |
| `$30000–$3FFFF` | `PARA200J` | parameter module (`$359A` bytes used); upper 32 KB zero-filled |

Evidence: the ROM's own module-integrity routine (`$100E`, run unmodified in Unicorn over the
concatenated chips) returns exactly the value startup expects (`$800FE3`, `cmpi.l` at `$10C0`)
for this order, and `$7EE0F4` (fail) for name order 30/34/38.  `$800FE3` is also what the
parameter module yields, so it is a CRC residue that every intact module hits (DERIVED).
Independently, `bsr`/`jsr` targets inside the module land on a `link`/`movem` function entry
79–99% of the time in every 32 KB region, and none points past `$276FD`.  In name order, all 98
targets in the zero region landed on `$0000`, which is what first suggested (wrongly) a
truncated dump; see the section on the second gate below.

`PARA200J` at `$30000`: MEASURED that startup loads the module address `$30000` at `$2292`, and
the parameter module's own checksum is valid there.  (An earlier argument from `bsr` targets
beyond `$30000` was an artifact of the wrong chip order and is withdrawn.)

## The memory map so far

The 68008 has 20 address lines (1 MB); the code masks addresses with `andi.l #$FFFFF`
(`$1E98`, `$29F6`, `$26140`), which fits.  Device bases are loaded as immediates
(`movea.l #$xxxxx,An`) and then used as `d16(An)`, so the base immediates are few, but the
access sites are not.

| Range | What | Evidence |
|---|---|---|
| `$00000–$3FFFF` | ROM: `77POK30`, `77POK38`, `77POK34`, `PARA200J` (64 KB each, in that order) | MEASURED (above) |
| `$40000–$4xxxx` | work RAM.  Reset SSP `$40B00`, USP `$40700`; globals are `a6`-relative with `a6 = $48B00`, at negative offsets (`-$8000(a6)` = `$40B00` …) | MEASURED (`$2194–$21AE`) |
| `$C0000` | **MC68681 DUART**: init writes `$1A` to CRA (+2) and CRB (+10), MR1A=`$13`, MR2A=`$0F`, CSRA=`$DD`. Base stored at `-$8000(a6)`. Level 2 handles serial TX/RX; timer supplies serial clocks | INFERRED identity; MEASURED register setup and handler (`$09C8`, `$780E`); not reached in boot |
| `$D0000–$D7FFF` | 32 KB scanned word by word, likely battery-backed RAM (the "cash memory") | INFERRED (`$170AA`) |
| `$E0000` | **Palette RAMDAC** (G171/Bt47x layout: +0 address, +2 RGB data, +4 pixel mask); programmed only on the ≥ 1M-word video configuration | INFERRED, strong (`$5CAC`; see "HD63484 bring-up") |
| `$F6000` | **HD63484 ACRTC**: word writes to the FIFO at +2 (`$4800`, `$55AA`, `$4800`, `$AA55` — a command + pattern), register select / status at +0 | INFERRED, strong (`$1D3C`, `$1E4E4`) |
| `$FB002/3`, `$FB006/7`, `$FB00A/B` | Three serial control/data pairs, consistent with 6850 ACIAs | MEASURED accesses; INFERRED chip identity |
| `$FB014–$FB01F` | Three four-register PIA groups; first exposes sound/output bus, tick and input-scan flags | MEASURED accesses/self-tests; INFERRED 6821 identity |
| via `$FB014/$FB016` | AY address/data bus with PB bits 1/7 as strobes | MEASURED original sound-test readback; INFERRED AY chip identity |

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

MEASURED: level-2 handler `$0A6E` saves d0–a5, masks to IPL 7 and calls `$780E`,
which services serial TX/RX. Additional device vectors `$40–$47` include the PIA
system tick and HD63484 FIFO handlers (see continued bring-up research below).
The earlier inference that the DUART timer supplies the game tick is superseded.

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

### Continued code research after the first gate

The user has no additional board material and requested continuation from code.

- **MEASURED:** ROM vectors beyond the autovector/TRAP area were missing from the
  earlier analysis: vector `$40` (`$100`) -> `$2E26` services HD63484 FIFO; `$43`
  (`$10C`) -> `$C06` services the PIA `$FB017` bit-6 flag and increments the
  software tick at `-$7D42(a6)`; `$45` -> `$19B8`, `$46` -> `$DF4`, `$47` ->
  `$199A`. **DERIVED:** the program uses device-supplied interrupt vectors too;
  the earlier claim that level-2 DUART is the only periodic IRQ is unsupported.
- **MEASURED:** level-2 body `$780E` examines DUART ISR bits 0/1 (serial transmit /
  receive), not bit 3 (counter ready). Init writes ACR=`$60`, CTUR/CTLR=`$000D`,
  CSRA/CSRB=`$DD`. **DERIVED:** the timer supplies serial clocks; its output is
  not established as the game tick. With oscillator X1, timer output is X1/(2*13)
  and serial baud X1/(2*13*16), approximately 8861.5 baud if X1=3.6864 MHz.
  Neither that oscillator nor baud is measured. Source: [MC68681 user manual](https://www.nxp.com/docs/en/user-guide/MC68681UM.pdf).
- **INFERRED:** device vectors `$40–$47` likely share level 5. The 20-address-line
  48-pin MC68008 offers interrupt levels 2/5/7, level 2 is serial and level 7 has
  a separate handler. The physical priority encoder remains unidentified.
  Source: [Motorola MC68008 data sheet, section 4.1.5](https://islandlabs.eu/_media/mc68008.pdf).
- **INFERRED (strong):** `$FB002/3`, `$FB006/7`, `$FB00A/B` are 6850-compatible
  serial interfaces: control writes 3/`$17` reset; `$95`/`$15` select normal
  operation; routines `$16A2/$16E2` poll TX-ready bit 1, RX-ready bit 0 and IRQ
  bit 7, transferring data at base+1. This is separate from the DUART.

### Second gate: main-ROM integrity failure (independently reproduced)

- **MEASURED:** implementing the PIA DDR selection plus peripheral `RESET` makes
  all three `$11B4` register tests pass. The AY bus uses PA (`$FB014`) for address
  and data: a falling PB bit 1 latches an address when PB bit 7 is low, or writes
  data when bit 7 is high; PB bits 7/1 both high select reading. The unchanged
  `$1FEE` test reads back `$55` from register 0 and `$0A` from register 1 on its
  first attempt. **DERIVED:** this bus wiring is sufficient for the ROM's sound
  self-test. AY identity remains **INFERRED** from the 14-register usage/masks.
- **MEASURED (experiment, not physical timing):** explicit external-source settings
  of 100 Hz system tick, 50 Hz input scan and 400 ms watchdog, using the placeholder
  8 MHz Musashi clock, pass the three bounded PIA flag tests and reach `$107A`.
  These values are **INFERRED hypotheses only**, not a measured board clock or
  proof of steady-state behavior. They are disabled by default and never returned
  as hard-coded ready flags. `RESET` and port strobes reset the watchdog age;
  free-running sources set ordinary PIA edge latches independently of CPU polling.
- **MEASURED:** main module header is at `$00400`, length `$272FE`; its checksum
  range is `$00400–$276FD`. The original subroutine at `$100E` returns `$7EE0F4`.
  Startup at `$10C0` expects `$800FE3`, retries once, then branches at `$10DC` to
  `$11A2`, producing fatal error `$003F004F`. **This fails before PARA validation.**
- **MEASURED, independently reproduced:** Unicorn 2.1.4 (M68000 CPU) executing the
  same unmodified subroutine over the same four chip files returns `$7EE0F4` for
  the main range and `$800FE3` for PARA200J (`$30000`, header length `$359A`). The
  independent experiment sets only the subroutine's documented argument registers
  and a scratch stack; it does not translate the checksum to C/Python, patch ROM,
  or bypass boot. Local script/results: `tmp/check_crc_cpu.py` / `tmp/crc-unicorn.txt`.
- **MEASURED:** all four chip SHA-256 values still match `tools/roms.py`. Thus file
  identity is verified, but it does not establish that the supplied main module
  passes the program's own integrity check. **DERIVED:** the mismatch is not
  specific to the board models or Musashi. Its origin (bad/modified/mismatched
  dump, intentional original checksum defect, or an unrecognised mapping detail)
  is not established. No ROM byte or checksum condition has been changed.
- **DERIVED, stronger than earlier:** main startup explicitly selects `$30000`
  and the parameter module's own checksum is valid there. Full cold-boot execution
  of PARA200J remains unobserved because the main-module check fails first.

Reproduce the second gate:

```
build/pokeri-host --devices --system-hz 100 --input-hz 50 --watchdog-ms 400 \
  --instructions 20000000 --out tmp/pia-module
```

The run stops with `$003F004F` after 12,991,560 instructions. No HD63484 command,
DUART access, NVRAM access, or hardware interrupt has occurred. AY bus usage is
30 data writes: register 7 four times; each of registers 0–6 and 8–13 twice.
The module integrity gate is independent of the still-unconfirmed periodic-signal
frequencies. Phase 1 is **not complete**; neither idle nor pay-table selection has
been observed. Remaining device implementations are deferred until this gate is
resolved, rather than manufacturing a passing checksum or ready flag.

### Cause of the second gate: wrong chip order, not a bad dump (resolved 2026-09-24)

- **MEASURED:** with the program chips concatenated as `77POK30`, `77POK38`, `77POK34`, the
  unmodified `$100E` routine returns `$800FE3` for the main module.  That is the value the
  startup code compares against, so the integrity check passes.  All four dumps are complete and
  correct.  Name order (30/34/38), the order the harness and `make program-image` used, returns
  `$7EE0F4`, giving the `$003F004F` failure.
- **MEASURED:** the upper 32 KB of `77POK34` and of `PARA200J` are all `$00`.  With the correct
  order both lie outside their module's checksummed range (`$28000–$2FFFF` is beyond the main
  module end `$276FD`; PARA uses `$359A` bytes), so this is unused space, zero-filled by the
  dumper or when the images were built.  Both chips' contents would fit a 32 KB part, which
  matches the 27C256s in the board photo (`docs/hardware.md`).
- **Withdrawn:** an earlier conclusion in this section (commit `08fa0b8`) that `77POK34` was
  missing its upper 32 KB.  It rested on a call-target statistic computed over the wrong chip
  order.  Any PC between `$10000` and `$2FFFF` that was read off the name-order image must be
  remapped: name-order `$1xxxx` (77POK34) is really `$2xxxx`, and name-order `$2xxxx` (77POK38)
  is really `$1xxxx`.  The addresses in this file have been corrected; captures in `tmp/` taken
  before the fix are in the old layout.
- Why the socket numbers don't follow address order (IC30 → `$00000`, IC38 → `$10000`, IC34 →
  `$20000`) is unknown; the address-decoding PALs define it.

## HD63484 bring-up (2026-09-24)

`src/board/Hd63484.{h,cpp}`: the portable model, with synthetic checks in `make harness-check`.

- **MEASURED (ROM code + passing ROM self-tests):** 8-bit host bus.  `$F6000` (A1 = 0) is the
  address register on write and the status register on read; `$F6002` (A1 = 1) is data.  Words
  go high byte first, so a 68008 `move.w` to `$F6002` is one FIFO word.  Registers below `$80`
  are byte-addressed through AR (e.g. AR = 3 is the low byte of CCR, read and then written back
  around critical sections at `$1D88`/`$1E4E0`).  From `$80` up, AR advances one byte per data
  access: the display set-up at `$2B18` streams 26 bytes after a single AR write.  The idle
  status is `$23` (WFE | WFR | CED), which is exactly what the fatal-error loop compares against.
- **MEASURED:** the read/write pointer is set with `WPR $0C` (display-select bits 15–14,
  address bits 19–12 in the low byte) and `WPR $0D` (address bits 11–0 in bits 15–4), built at
  `$1E10`.  The first video-RAM test (`$1D3C`: `WT $55AA`/`$AA55` to adjacent words, `RD` back)
  passes with the model.
- **MEASURED: the ROM probes the installed video memory** at `$5BC8`.  It writes `$3456` at word
  `$30000` and `$BCDE` at word `$B0000`, reads both back, and returns 1 if they differ.  Only
  then (`$2E12` → `$5CAC`) does it program the device at `$E0000`.  So there are two supported
  video configurations:
  - **≥ 1M words (2 MB), no aliasing:** it programs `$E0000` and stops there in the harness,
    because that device isn't modelled.
  - **256K words (512 KB, the size kasinohai.com gives), so `$B0000` aliases `$30000`:** it
    skips `$E0000` and **runs on into a steady loop**: 318 M instructions, 39,478 interrupts,
    and the full drawing-command mix (AMOVE/RMOVE, lines, polylines, CRCL, ELPS, arcs, RFRCT,
    PAINT, DOT, PTN patterns, AGCPY copies).  Consistent with the attract/idle loop, but not
    yet seen, because drawing isn't implemented.
  **Our target is the 512 KB variant** (user, 2026-09-24; also the article's figure), now the
  harness default.  `--video-kwords 1024` selects the 2 MB path.  The ROM's `PCB5002/5003/5501/
  5502` strings suggest the program knows several board variants.
- **INFERRED (strong): `$E0000` is a VGA-style palette RAMDAC** (INMOS G171 / Brooktree
  Bt47x layout): `+0` write address, `+2` colour data (three bytes per entry), `+4` pixel read
  mask.  `$5CAC` sets the mask to `$CF`, the address to 0, and streams a 64-colour RGB table
  (`$5D76–$5E35`) four times, filling 256 entries.  The mask writes at `$5B3A` (`$EF` etc.) are
  then palette effects, not lamps as first guessed above.
- **DERIVED (with the Application Note below): 4 bits per pixel, 16 colours.**  CCR bits
  10–8 are GBM (graphic bit mode), and the ROM writes CCR's high byte as `$02`, so GBM = `010`
  = 4 bpp.  This matches the article's "16-colour palette".  CCR's low byte is the per-source
  interrupt enable, bit for bit with the status register; the ROM leaves it at `$80`, so only
  **command error** can interrupt.  Status bit 0 is WFE (write FIFO empty) and bit 1 is WFR
  (write FIFO ready); the ROM's `btst #0` waits are "FIFO empty" waits before bursts.
- **Checked against the HD63484 User's Manual** (Hitachi, November 1984; local copy
  `ref/manuals/hd63484-acrtc-users-manual.pdf`, OCR text in `tmp/hd63484-um.txt`, both
  git-ignored).  Confirmed: every command's word count (command table and per-command `Wn`);
  high byte first on an 8-bit host; AR auto-increments by 1 for `$80–$FF` and not below; the RWP
  layout (PR0C bits 15–14 display number, bits 7–0 address 19–12; PR0D bits 15–4 address 11–0);
  and MOD's MM codes (replace/OR/AND/EOR).  Fixed: MOD now honours the MASK register (PR04),
  as the manual requires (§6.5.1).
- **MEASURED: the ROM sends `WPTN`'s count in words, contrary to the manual.**  The manual says
  n is in bytes on an 8-bit host (p. 181).  But `WPTN $1800, n=16` and `WPTN $1802, n=14` each
  fill the 16-word pattern RAM exactly, and a valid command follows n words later; treating n
  as bytes misaligns the stream into an invalid command word.  Unresolved: this suggests the
  board presents 16-bit transfers to the FIFO (a byte-pairing latch), yet the ROM also uses
  byte-level register access (AR = 3, CCR's low byte), which fits true 8-bit mode.  The model
  follows the ROM (`Hd63484::wptnCountsBytes = false`).
- **MEASURED (with the manual's display-number table): the four logical screens' memory.**  The
  RWP set-up at `$1E10` assigns display numbers by word address: upper screen (DN 00) below
  `$2300`, lower screen (DN 10) `$2300–$4AFF`, window (DN 11) `$4B00–$AFFF`, base screen
  (DN 01) from `$B000`.  Step 2 of the Phase 2 plan decodes the matching start-address and
  memory-width registers.
- The steady run is unchanged: 318 M instructions, 39,478 interrupts, CCR low `$81` (command-error
  and write-FIFO-empty interrupts enabled; the FIFO feeder at `$2E54` runs from the latter).
- **Not yet modelled:** drawing (Phase 2), the raster/timing registers read back live, and the
  `$47000` byte write at `$13EC`, which lands in the harness's generous RAM although the board
  has only 16 KB (`$40000–$43FFF`).  It may be a latch, or RAM decoded with mirrors.

Reproduce the steady run:

```
build/pokeri-host --devices --system-hz 100 --input-hz 50 --watchdog-ms 400 \
  --stall-instructions 300000000 --instructions 600000000 --out tmp/hd-256b
```

## Display format — Phase 2 step 2 (2026-09-24)

**Presentation correction (user-approved, 2026-09-27): SDL now presents the full
608 × 292 programmed row; Amiga presents 608 × 283 after the existing vertical
crop.** **MEASURED:** the ROM draws the VOITOT box through x=587 and its right
gray margin at x=588–607 in the stored upper screen. The former 576-pixel
viewport cut off the box and margin; these pixels were not wrapped to the left.
Local evidence: `tmp/header-full-row.png`, reconstructed from canonical replay
VRAM, and the paired `tmp/display608-*-reference-*` captures.

**DERIVED (original register interpretation):** HDR gives a nominal 576 × 292
frame at 4 bits per pixel (16 colour indices). Its backing row is **608 pixels /
152 words / 304 bytes**. The full scan, including blanking, is equivalent to
768 × 304 pixel periods. The new viewport exposes the complete stored row for
this ROM mode without modifying the HD63484 timing registers or memory stride.
It is a port presentation policy, not proof that the physical CRT displayed
608 pixels; pixel aspect ratio and physical overscan remain unestablished.

### Evidence and mode

**MEASURED:** reconstructed every display-register write in the existing long
`tmp/um-fix2-trace.csv` capture (318,832,104 instructions), and cross-checked a
fresh 20,000,000-instruction run in `tmp/display-step2-*`. All display registers
match. The timing, screen, start-address and zoom registers are programmed once
and are unchanged throughout the long run. CCR interrupt-enable changes do not
change the display format. The ROM routine at `$2AF6` programs timing at
`$2B18–$2B82`, screen RAM at `$2B86–$2C40`, and mode at `$2C46–$2C7C`.
Local analysis products: `tmp/display-register-writes.txt`,
`tmp/display-decoded.txt`, `tmp/display-init-disasm.txt` and `tmp/decode_display.py`.

Sources for the **DERIVED** interpretations below: Hitachi *HD63484 ACRTC User's
Manual*, November 1984, §§2.2.1, 5.5–5.9 (printed pages 17–23, 64–111;
`tmp/hd63484-um.txt`); *ACRTC Application Note*, April 1986, the worked display
calculation on printed pp. 97–98 and the interleaved memory-width calculation on
p. 102 (`tmp/hd63484.txt`). Printed page numbers differ from PDF page indices.
The Application Note explicitly divides horizontal memory cycles by two in
interleaved mode when calculating the number of words displayed.

| Register | MEASURED value | DERIVED interpretation |
|---|---|---|
| CCR `$02–03` | high byte `$02`; low byte changes | GBM bits 10–8 = `010`: **4 bpp**, four pixels per 16-bit word. Colour depth comes from CCR, not OMR |
| OMR `$04–05` | `$CD28` after setup | Master, running; display-priority access; no window smooth scroll; cursor skew 3 memory cycles, DISP skew 1; dynamic RAM refresh; GAI = **4 words per display fetch**; **interleaved access**; **non-interlaced** raster |
| DCR `$06–07` | `$FF3F` | Upper, base, lower and window all enabled and displayed. DSP=1: DISP1 carries horizontal enable and DISP2 vertical enable. ATR=`$3F` is an external video attribute, not a palette value |
| ZFR `$EA` | `$00` | Base screen horizontal and vertical zoom both ×1 |

One memory cycle is **two 2CLK periods**. Interleaved access uses two memory
cycles per display fetch: one display access and one drawing access. Thus:

- 4 words/fetch × 4 pixels/word = **16 pixels/fetch**.
- 16 pixels/fetch ÷ 2 memory cycles/fetch = **8 pixels/memory cycle**.
- HDR width = `$47 + 1` = 72 memory cycles = **576 nominal pixels**, or 144 words.
- MWR = `$098` = 152 words = **608 pixels**; eight words / 32 pixels of each
  backing row lie beyond that nominal width; the approved port viewport now includes them.

Do not multiply the 72-cycle width directly by 16: that would incorrectly give
1152 pixels by counting the interleaved drawing slots as display fetches.

### Screen layout and memory

**MEASURED:** all four MWRs are `$0098`, all RARs are `$1000`, all SAR high words
and start-dot offsets are zero. **DERIVED:** all screens are **graphics** (MWR
CHR=0); RAR's first/last character rasters (0/16) do not multiply their height.
SAR combines its low 16 address bits with SAH's low nibble to form a **word
address**. It does not use the WPR read/write pointer's shifted encoding.

Coordinates below are relative to the top-left of the visible 576 × 292 frame.
Ranges are inclusive. Sizes and positions are **DERIVED** from measured registers.

| Screen / DN | Register block | Start word (byte offset) | Visible rectangle | Stride |
|---|---|---|---|---|
| Upper / 0 | `$C0–C7` | `$00000` (`$00000`) | x=0–575, y=0–39; **576 × 40** | 152 words |
| Base / 1 | `$C8–CF` | `$0B000` (`$16000`) | x=0–575, y=40–261; **576 × 222** | 152 words |
| Lower / 2 | `$D0–D7` | `$02300` (`$04600`) | x=0–575, y=262–291; **576 × 30** | 152 words |
| Window / 3 | `$D8–DF` | `$04B00` (`$09600`) | nominal x=0–87, y=44–143; **88 × 100 nominal, 96 × 100 with final fetch** | 152 words |

These starts agree with the independently observed RWP display-number selection
at `$1E10`. The split heights are SP0=`$28` (40), SP1=`$DE` (222), SP2=`$1E`
(30); their sum is 292. Note that SSW's register order is **base, upper, lower**,
not display-number order.

For later composition, each screen starts its own row count at its SAR. With
zero SDA and no zoom, its source word is `SAR + local_y * 152 + floor(local_x/4)`.
The installed 512 KB memory mask applies to the resulting word address. The
window replaces the corresponding base-screen area in interleaved mode; this is
not the ACRTC's superimposed-access mode. A colour-index-zero transparency rule
is not implied by these settings.

Window offsets before interleaved fetch adjustment: HWS=HDS=`$09`, so nominal x=0; VWS=`$32`, VDS=`$06`, so y=50−6=44.
The width follows HWW+1 = 11 cycles and the height follows VWW = 100 rasters.
**Unresolved manual conflict:** §5.6 (p. 74) requires even horizontal widths in
interleaved/superimposed access. The background's 72 cycles obey this; the window's
11 do not. The initial reference preserved the nominal **88-pixel** width pending
last-fetch/right-edge comparison. That comparison exposed a truncated card;
see the 2026-09-26 correction below (96 displayed pixels). This does not
make the 576 × 292 output size ambiguous.

### Interleaved window alignment (2026-09-25)

**MEASURED (host ROM trace)**: during the deal animation HWS advances through
`$0F`, `$1B`, `$27`, `$33`, with HDS `$09` and HWW `$0A` (11 cycles).
The moving card is the DN 3 overlay, not a mispositioned AGCPY destination.
The old composition put it 16 logical pixels left of the stationary cards,
matching the user's 32-pixel displacement at 2× SDL scale.

**DERIVED (reference implementation)**: MAME `hd63484_device::draw_graphics_line`
adds two memory cycles to the window start for an odd interleaved window width.
At 8 pixels/cycle this is +16 pixels: the first card's HWS `$0F` therefore
starts the overlay at x=64 rather than x=48. The initial fix applied this delay to host composition and native
planar presentation, preserving the nominal 88-pixel width and source
coordinates. The width was subsequently corrected below. Even widths retain their existing position. This
resolves the observed alignment, but is not a measurement of an original PCB;
the manual's even-width restriction and right-edge fidelity qualification remain.
Synthetic tests cover odd/even widths and left clipping. Local frame captures
and traces are under `tmp/card-offset-*` and are not distributable assets.

### Moving-window right edge investigation (2026-09-26)

**MEASURED (native captures):** twelve completed moving-card frames from the
production A1200 build match host `compose()` pixel for pixel over 576 × 283,
using the captured planar VRAM and display registers. Both renderers clip the
same rightmost eight logical pixels (sixteen at 2× scale). With HWR `$0F0A`,
HDS `$09`, SAR3 `$04B00`, the window begins at x=64 and ends at x=151; the
card artwork starts at x=72 and its right border lies beyond that boundary.
Thus this is a shared window-display interpretation, not a native blitter
mask/copy error. Local captures: `tmp/card-edge-before-00` through `-11`.
**MEASURED (source artwork):** the DN3 row has eight leading padding pixels
followed by an 88-pixel card, through local x=95. The original-machine footage
at 1.5 seconds shows a complete moving card, including its right border.

**INFERRED (adopted display model):** retain the entire last display fetch when
an interleaved window has an odd number of memory cycles. Round its displayed
width up to a pair of memory cycles, preserving the existing start delay and
source coordinates. For HWW `$0A`, six 16-pixel fetches display 96 pixels,
restoring x=152–159 without moving the card's left edge. Even widths are
unchanged. Both renderers use the same geometry helper. This resolves the
observed clipping; the exact chip/external-shifter behavior remains inferred
because the manual specifies even widths and no PCB signal capture is available.

**MEASURED (validation):** after the correction, twelve newly captured A1200
moving-card frames (`tmp/card-edge-after-*`) match corrected host composition
with zero differences across all 576 × 283 pixels. Visual inspection confirms
complete right borders while the cards move; background cards remain aligned.
Synthetic regressions cover the ROM's window geometry using generated artwork,
its last half-fetch, unchanged even widths, and left/right screen clipping.
Harness and planar/backend test suites and the native no-software-mul/div audit
pass. These checks establish the renderer correction, not measured PCB timing.

### Timing and auxiliary registers

The values are **MEASURED**; counts and timing consequences are **DERIVED**.

| Register | Value | Decode |
|---|---|---|
| HSR `$82–83` | `$5F06` | Horizontal total HC+1 = **96 memory cycles**; HSYNC low = **6 cycles** |
| HDR `$84–85` | `$0947` | From HSYNC rising edge, display start HDS+1 = **10 cycles**; active width HDW+1 = **72 cycles** |
| VSR `$86–87` | `$0130` | Non-interlaced frame total = **304 rasters**, with no +1 |
| VDR `$88–89` | `$0605` | From VSYNC rising edge, display start VDS+1 = **7 rasters**; VSYNC low = **5 rasters** |
| SSW `$8A–8F` | base `$00DE`, upper `$0028`, lower `$001E` | **222 + 40 + 30 = 292 active rasters**, with no +1 on any split height |
| BCR `$90–91` | `$FFFF` | External blink attributes; does not alter frame dimensions |
| HWR `$92–93` | `$090A` | Window start **10 cycles** after HSYNC rises; nominal width **11 cycles** |
| VWR `$94–97` | `$0032`, `$0064` | Window start **51 rasters** after VSYNC rises; height **100 rasters** |
| GCR `$98–9D` | `$2825`, `$001A`, `$0029` | Cursor X start/end **37/40 memory cycles** from HSYNC falling; Y start/end **26/41 rasters** from VSYNC rising. These are cursor outputs, not screen dimensions |
| CDR `$E8–E9` | `$B83F` | Graphic cursor mode; OMR separately gives cursor skew = 3 cycles. External cursor/video combination remains outside this decode |

Before DISP skew, the horizontal layout is **6 sync + 10 back porch + 72 active
+ 8 remaining cycles = 96**. The display-address active interval begins at cycle
16; DISP skew delays the enable signals by one memory cycle (8 pixel periods).
That signal delay should not become an extra column in a cropped frame. The
vertical arithmetic is **5 sync + 7 back porch + 292 active = 304**, leaving no
additional front-porch raster under the manual's programmed-count definitions.

Let **F** be the video controller's **2CLK input frequency**, not the CPU clock:

- Memory-cycle frequency = `F / 2`.
- Pixel rate = `4 * F` (8 pixels per memory cycle).
- Horizontal frequency = `F / (2 * 96) = F / 192`.
- Frame frequency = `F / (192 * 304) = F / 58368`.

**Unidentified:** the actual video oscillator and external serializer wiring.
For illustration only, a 3 MHz 2CLK would give 12 MHz pixels, 15.625 kHz lines
and **51.398 Hz frames**. Exact 50 Hz would instead require 2CLK=2.9184 MHz and
15.2 kHz lines. Neither is a measured clock estimate, and the configured 100 Hz
PIA signal must not be used to declare a 50 Hz video rate. Registers establish
the pixel dimensions and timing ratios, not an oscillator frequency.

Step 2 is complete for frame allocation and nominal screen composition. Drawing,
frame output, palette selection and resolution/aspect conversion for Amiga remain
in their subsequent plan steps.

### Drawing command modes — Phase 2 step 3

**MEASURED** (complete `tmp/um-fix2-trace.csv`, reconstructed FIFO words): the
512 KB boot uses WPTN `$1800/$1802`, CLR `$5800`, AMOVE/RMOVE `$8000/$8400`,
RLINE `$8C00`, APLL/RPLL `$9800/$9C00`, CRCL `$A900`, ELPS `$AD00`, RARC
`$B500`, REARC `$BC00/$BD00`, RFRCT `$C400/$C401` (79 replace, 2 OR), PAINT
`$C800`, DOT `$CC00`, PTN `$D000/$D008` (70 opaque, 551 transparent-zero),
and AGCPY `$E000/$E300` (20 positive/positive, 5 negative/negative destination
scans). Every drawing command has AREA=0; the programmed area bounds do not
clip these operations. Pattern controls are either PP=PS=`$0000`, PE=`$0070`
(8×1), or PP=PS=`$2000`, PE=`$F0F0` (16×14); both have unit zoom. Every PAINT
is followed by AMOVE before another drawing command. These are observations of
this boot scenario, not a guarantee about gameplay paths.

**DERIVED** (User's Manual §§5.10, 6.7–6.8 and ORG): drawing addresses are
`origin_word - y * MW[DN] + floor((x + origin_dot / bpp) / (16 / bpp))`.
The leftmost logical pixel uses the least significant bits; color registers are
sampled at the same physical dot position. MASK applies only to DMOD/MOD/SCLR/
SCPY, not pixel drawing or CLR. ORG resets CP to (0,0). Line/polyline/arc final
endpoints are excluded; rectangles/copies include both rectangle corners and
leave CP one scanline beyond the destination. CRCL/ELPS restore CP to center.

**DERIVED — implementation semantics:** `src/board/Hd63484Drawing.cpp` implements
these commands as synchronous operations on the existing VRAM. Pattern RAM
supports a selected subrectangle, PP start offset and zoom; line patterns advance
across polyline vertices. The boot's opaque and transparent-zero PTN modes, replace
and OR drawing, and both AGCPY destination scan directions are implemented. AND,
XOR, transparent-one patterns and inverse-edge PAINT share the tested primitives.
Copies read/write in scan order, including overlap; their CP advances according
to **destination** direction, independently of the signs of the source dimensions.
Unsupported area modes, direct-color patterns, conditional drawing modes, scan
directions and commands set CER and a diagnostic error. Reserved opcode bits are
also rejected. CER now remains set until abort; RPR clears ARD as specified in
User's Manual p. 61. The model updates CP and physical DP for movement/drawing.

**INFERRED — raster fidelity, not a silicon measurement:** circles/ellipses/arcs
use a midpoint ellipse outline, ordered clockwise/counterclockwise with each
pixel written once. The manual specifies the conic and endpoint semantics but
not sufficient internal rounding details to claim pixel-exact hardware equality.
Pattern scanning begins at Pr05 for each command; intermediate pattern state is
local to the drawing operation. Conic pixel ties and readback of intermediate
pattern state need hardware or visual confirmation. MAME's current implementation
was consulted as a semantic cross-check
([source](https://github.com/mamedev/mame/blob/master/src/devices/video/hd63484.cpp)),
not imported. In particular, its ELPS code reads a fourth parameter despite the
three-parameter command format; this implementation uses the documented third
parameter dX (User's Manual pp. 250–252).

**INFERRED — PAINT reference model:** bounded scanline filling treats EDG and both
color-register values as boundaries (User's Manual pp. 271–276), with patterns
anchored at the starting CP. Four pending seeds are supported. If the model needs
more, it stops explicitly: the chip's stack-overflow triples `(X,Y,pattern point)`
and suspend/resume on a full read FIFO are not yet implemented. Stack traversal
and the final CP (model: last painted pixel) are not claimed hardware-exact. The
observed boot neither overflows this guard nor relies on the final CP: all 107
PAINT commands are followed by AMOVE. Unbounded painting or excessive dimensions
also stop at a documented diagnostic work limit, never a fabricated success.

**MEASURED — verification:** every command in the boot histogram has a synthetic
physical-memory/pointer test, including signed coordinates, nonzero origin dot,
color-word alignment, negative rectangle/copy directions, endpoint exclusion,
pattern selection/zoom/transparency, overlap, concave PAINT and error guards.
`make harness-check` and the drawing suite under AddressSanitizer/UndefinedBehaviorSanitizer
pass. The original-ROM long run (`tmp/drawing-step3-*`, the same hypothetical
100 Hz/50 Hz external signals and 400 ms watchdog as Phase 1) ends at the same
no-new-PC watchdog: **318,832,104 instructions, 2,946,130,996 cycles, PC `$023FA`**.
CPU context, executed-PC coverage and NVRAM match `tmp/um-fix2-*`; all 3,114
previously unexecuted commands execute, with no new unknown access/error. The
existing 16,409 read-FIFO underflows are unchanged. This establishes boot-path
compatibility, not that the loop is attract mode or that the raster matches the
physical machine. Frame composition and palette remain the next step.

## Phase 2 frame/input bring-up (2026-09-24)

**MEASURED:** the first composed frame (`tmp/phase2-frames-final.ppm`, placeholder
palette) has the deck and zero credit/win boxes but no pay table or active game.
The long-run PC `$023FA` is the error-indicator delay, not attract. The error is
04: the watchdog self-test at `$209E` sees its warning flag, then waits at `$20DC`
for a hardware reset. Without a reset it reaches `$2100` and logs error 04 through
`$12A2`. On reset, `$21B0` checks the retained stack marker and `$21BA` checks the
countdown against `$7F000`; a reset after enough countdown iterations continues initialization without error.
A 1 ms delay fails this lower-bound check; 50 ms passes and reaches `$02442`
without the startup-error latch. The actual circuit delay remains unmeasured.

**DERIVED:** the watchdog must reset the CPU, preserving RAM; a PIA edge alone
cannot pass this test. **INFERRED timing:** experimental `--watchdog-reset-us 50000`
requests CPU reset 50 ms after the existing `--watchdog-ms` warning edge. This is
an explicit timing hypothesis, not a patched result or measured circuit delay.

**MEASURED:** physical switch scan `$0E92` combines PIA1 PA, PIA1 PB and
PIA2 PA (addresses `$FB018/$FB01A/$FB01C`) into a 24-bit input, masked with
`$FF7F3C`. `$0EB4` maps physical bits to logical input IDs using a ROM table.
The callable input dispatcher is `$0AD74`; it chooses the parameter module's
edge/mode routing table and invokes registered callbacks. PIA1 PB bit 6 selects
the boot mode flag at A6−`$770C` (low sets it, high clears it). PIA2 PA bit 2
routes to `$0FF1A`; its falling edge reaches `$0FF70` and clears A6−`$78CE`
at `$100F4`. Its physical label is not established yet. Switch scripts manipulate
only these external input pins; no internal flags or credits are forced.

**MEASURED:** ACIA0 transmission stopped after its first byte because the model
never exposed its TX interrupt. The exact `$199A` vector-$47 handler addresses
the serial block based at `$FB002`, tests status bit 7 and dispatches TX-ready to
`$170A`, which sends the remaining packet bytes and disables TX IRQ after a
high-bit terminator. The board model now routes this source and reports its IRQ
status. No peer replies are fabricated. PIA2 PA bit 3 low invokes `$135D0`, which
sets the attention state through `$108E8`; holding that pin high avoids this
particular attention trigger. PIA2 PA bit 2 is associated with cash collection:
its falling-edge experiment displayed the ROM's “TYHJENNYKSEN HYVÄKSYNTÄ JA
LASKINTEN NÄYTTÖ 'JAKO' KYTKIMELLÄ” prompt. It is not a generic boot-ready input.

**DERIVED — ACIA0 link transport:** `$1720` receives packets of at most four bytes;
bit 7 marks the checksum terminator, `(~sum(payload)) | $80`. `$E328` dispatches
link states. `$30` requests transmission, `$00/$40` are alternating sequence
acknowledgments, and `$50` ends a transfer. Application headers use bits 0–5;
bit 6 alternates. The model's explicit `--serial-peer` is a diagnostic transport
peer, not a claim to emulate the missing coin/meter firmware. It acknowledges
valid packets but generates no application result; application input is scripted.
Its 1 ms byte interval is INFERRED test pacing, not measured serial baud timing.

**MEASURED:** closing PIA1 PB6 sends application command 9 with parameter 1.
Replying at the transport layer completes that transfer but does not clear the
startup wait. Incoming application command 1 with two zero payload bytes invokes
`$1247C` and clears A6−`$78CE` through `$1298E`. Command `$31` dispatches to `$0AEE`,
which unpacks peripheral status bits. Bit 8 becomes the input-enable flag checked
by `$9990`. Incoming command 3 invokes `$991E`, the first coin accounting path;
its denomination is selected from the parameter/accounting data, not supplied
as an arbitrary credit amount. These are protocol experiments, not observations
of a physical peripheral. No ROM instruction or internal state was patched.

**MEASURED:** opening PIA1 PB6 reaches AGCPY `$EC00` (S=1, DSD=100),
previously unseen in boot: source and destination scan vertically, advancing CP
past the last destination column. **DERIVED:** implemented from User's Manual
AGCPY tables C37-1/C37-2, retaining sequential reads/writes for overlap.

**MEASURED:** peripheral status with an empty accounting state triggers error
`P3 84` via `$C7EA/$D03C`. The program checks its reserve against the parameter
module's minimum; this is not a serial checksum error. Refilling must go through
original input/accounting code, not an injected work-RAM balance.

**MEASURED — playable input path:** PIA1 PA0 falling deals/draws; PA2 cycles
stake. PB5/PB1/PB0/PA7/PA6 hold cards 1/2/3/4/5. Cabinet PB6 low displays
“OVI AUKI”. With that door open, PA1 (Collect) reaches `$122FC/$BB7E/$10A58`
and enters refill mode (A6−`$78D2`=1). Subsequent command-3 coin events increase
the reserve through the ROM's accounting routines. Merely inserting coins with
the door open supplies test credits instead; closing afterward without a real
refill is not valid reserve initialization. A 100-event refill followed by door
close and status exchange leaves reserve 101 after the next coin, no attention
flag, and a real played hand. The inferred denomination for command 3 is 1 mk.

**MEASURED — service:** PB2 rising while the cabinet is open enters/cycles TESTI.
TESTI 5 is NÄYTTÖ TESTI; Deal selects its circle/color-patch/alignment pattern.
TESTI 7 is KYTKIMET JA ÄÄNI (switches and sound). These run through physical
inputs and original callbacks. No game flag, balance, card or outcome is injected.

**MEASURED — frame comparison:** 576×292 composition gives the same overall
bar/deck/pay-table/five-card layout as the Finnish footage. The 2 MB RAMDAC table
at `$5D76`, read at runtime with `--palette-rom 0`, gives red backs, blue boxes,
yellow coin icons and white cards. **INFERRED candidate only:** the 512 KB board's
palette is not yet measured; the candidate's gray bars and text colors differ
from the filmed CRT. Native pixels also expose a striped card-center texture;
its fidelity needs checking before calling the visual reference complete.

### Phase 2 reference scenarios and limits

**MEASURED:** `make harness-scenarios` reaches attract (40.5 s), a dealt hand
(47.5 s), a 10 mk win (56 s), a successful Big double to 20 mk (65.5 s), and
TESTI 5's display pattern (39 s on the service branch). The winning test holds
the two nines and draws through the original program. Inputs select a repeatable
execution; no cards, balance, game state or code are patched. The double-up
screen reads “VOITIT 20 — TUPLAATKO”. PA5 invokes Double, PA3 selects Big;
PA4 as Small is DERIVED from the paired input callbacks.

**MEASURED:** the 65.5 s run completes 65,876,069 instructions, 524,000,002
model cycles and 25,125 acknowledged interrupts, with no unmapped accesses or
unexecuted HD63484 commands. It still reports 16,409 read-FIFO underflows, the
known permissive FIFO behavior described above; this is not silicon-exact FIFO
validation. AY registers 0–6 and 8–13 each receive 138 writes, R7 276, and
R14/R15 none. R13 is rewritten even when its value does not change; each write
must restart the envelope.

**MEASURED:** uninterrupted execution and checkpoint replay have byte-identical
full snapshots, CPU context, RAM, NVRAM, coverage, device summaries and final
frame. The resumed WAV's PCM is exactly the corresponding uninterrupted suffix.
Snapshots include pending CPU/device/input/serial state and tone/noise/envelope
phases. They exclude ROM bytes, file handles and callbacks. All snapshots,
images, audio and traces remain in ignored `tmp/`.

**MEASURED — retention experiment:** loading the initialized full main RAM and
starting a fresh CPU reset preserves reserve 101 (at `$44000`), coin reserve 101
(at `$4400C`) and credits 46 (at `$44074`), without the initial attention error.
Accounting references live in ordinary work RAM, including `$43E60/$44000`;
the separately mapped `$D0000` NVRAM remains untouched. **INFERRED:**
`--retained-ram` retains all `$40000–$7FFFF` as a research option. This does not
establish the physical RAM size, address mirroring, or battery-backed subrange.

**DERIVED — AY digital model:** tone dividers, 17-bit noise, mixer gating and all
16 envelope shapes follow the AY timing model cross-checked with the primary
[MAME AY implementation](https://raw.githubusercontent.com/mamedev/mame/master/src/devices/sound/ay8910.cpp)
(BSD-3-Clause; independent implementation here). An approximate logarithmic DAC
and DC filter feed 44.1 kHz mono PCM through `Tone`. **INFERRED:** the 1 MHz AY
profile gives bands near 520/780/1047 Hz; footage includes bands near
527/787/1047 Hz, but sequences and room noise differ. This is a plausibility
check, not an oscillator measurement. The by-ear comparison is still pending:
the agent environment cannot listen to audio. Analog levels and amplifier
response are unmeasured.

**MEASURED — validation:** synthetic harness tests pass, including all envelope
shapes and device-state continuation. Address/undefined-behavior sanitizers pass
the serial/audio/state suite. A truncated host snapshot is rejected. SDL dummy-
display and headless runs from the same checkpoint produce identical RAM,
coverage, device summaries and frame bytes.

The host now supplies an optional SDL window, physical input keys, WAV capture,
portable frame composition and deterministic scenarios. The research profile
uses a diagnostic serial transport peer, not a complete coin/meter peripheral.
It models RX with a lossless queue rather than cycle-accurate overrun/baud
behavior. Physical palette/clock calibration, striped card-center texture,
nominal odd-width window edge, curve pixels and exact PAINT/FIFO behavior remain
fidelity questions. No Phase 3 relocation or Amiga implementation was started.

## Phase 3 access/relocation audit (2026-09-24; initial audit)

**MEASURED:** the union of Phase 2 setup/attract/deal/win/double/service coverage
contains 17,911 executed PCs: 17,804 in ROM and 107 in RAM, spanning
`$41B8C–$42030`. The RAM instructions are runtime jump stubs; a ROM-only coverage
scan cannot establish relocation completeness. The module loader at `$107A`
copies initialized data and applies its own ROM-relative and RAM-relative
pointer lists at `$116A` and `$1182`. Preserve this original loader.

**MEASURED:** 388 distinct instruction sites produce 420 observed
PC/address/size/direction combinations. `host/tables/io-sites.csv` records
operation families, effective-address forms, extension offsets and lengths;
`io-accesses.csv` records the observed accesses. Neither contains original
opcodes, immediate data, graphics, or ROM bytes. The generator verifies all four
ROM SHA-256 hashes and checks decoded lengths against Musashi. The runtime gate
checks the image fingerprint; `make roms-check` supplies SHA-256 verification.
These are **audit descriptors, not completed relocation or native hook tables**.

**MEASURED:** nine hardware-access instructions are only two bytes long:
`$11B8/$11C6/$11CC/$14D8/$14E0/$1528/$1530/$16DA/$1718`. This disproves the
Phase 4 plan's claim that every access occupies at least four bytes. A two-byte
Line-A replacement still fits; its handler must use the recorded original
length when resuming and must not overwrite the following instruction.

**MEASURED:** the strict `--io-table` host gate leaves all six scenario states
identical to the Phase 2 baseline. It checks PC, address, transfer size and
direction before invoking any device. This demonstrates the observed access
inventory only: no ROM/RAM address relocation or Line-A execution is claimed.
The static aligned-long scan produces 1,271 **unclassified candidates**, including
ordinary integers; these must not be automatically treated as pointers.

**DERIVED — architectural decision at the initial audit (resolved below):** the main module's original
checksum at `$100E` covers `$00400–$276FD` (length `$272FE`); parameter validation
covers `$30000–$33599` (length `$359A`). Relocation operands and Line-A opcode
patches inside these ranges alter the checked bytes. The agreed plan does not
specify how to reconcile them with the runtime check. Proposed approach:
verify the unmodified ROM hashes, then recalculate checksum data for the patched
module while preserving the original check. Alternative: explicitly replace
runtime validation after loader verification. At this point neither was implemented. The continuation below records the
user-authorized bypass and completed relocation tests.

### Phase 3 relocated execution, continued

**DECISION (user):** temporarily bypass runtime module checksum validation,
after verifying the unmodified ROM image. Header/entry checks and the original
runtime data/relocation loader remain active. The two bypass sites are `$10AE`
and `$110C`; the checksum algorithm is not translated into host code.

**MEASURED:** the first strict relocated run reaches `$08E4`, which dereferences
unrelocated accounting RAM `$44004`. `$0868/$0878/$0888` construct the three
accounting pointers with low constants plus `$20000` addends at
`$0872/$0882/$0892`; these do not use the reset D7 mechanism. Their addends
therefore need the work-RAM relocation delta. The faulting old address was
unmapped, not aliased back to RAM.

**MEASURED:** the next miss is `$1197C` reading `$43FF9`, whose pointer is the
immediate at `$11704`. The covered absolute write at `$13EC` targets `$47000`
(operand `$13F0`). Both are explicit RAM operands independent of D7.

**MEASURED:** `$616A` dereferences `$000004` with A2=0 while searching an empty
scheduler list; `$6170/$6186` also read this null sentinel before checking list
bounds. This deliberately observes ROM reset-vector data on the original
board, not a missing pointer initialization. **DERIVED:** these access sites
need explicit low-vector read hooks, preserving A2 and condition codes. The
host executes the original comparison/test with a temporarily rebased A2 and
restores A2 after that instruction; the old vector page remains unmapped.

**MEASURED:** complete bypass-reference traces add two null-vector sites,
`$61CA` and `$61E2`, both through A0. The latter compares a timer against the
value at original vector offset 8; replacing that scalar with a relocated code
pointer changes list ordering. Therefore the five low-vector hooks read a
separate immutable 32-byte **original vector-data shadow**, created from the
verified ROM at runtime, rather than the CPU's relocated vector table. Only the
listed site/offset/width combinations can read it. No old-address alias exists.

**MEASURED:** opening the door at 18 s exposes the remaining accounting-structure
pointer `$43E60`, initialized by the operand at `$1170C`; its first failing write
is `$11DA6` to `$43EF6`. It also needs a RAM-delta fixup.

**MEASURED — two placements:** both `$100000/$200000/$300000` and
`$512300/$684680/$923400` (ROM/RAM/device-guard bases) complete all scenario
milestones with the checksum bypass. ROM placement must preserve 256-byte
alignment: `$1096E` clears the low byte to recover its parameter-module base.
RAM and guard bases in the second test deliberately change low address bits.
The reference runs with the identical bypass at the original addresses.

**MEASURED — checksum-bypass scenario:** omitting the checksum loop changes
boot timing and the deterministic deal. `relocation-play.inputs` retains the
operator/refill sequence and uses later hold/draw events to produce a real
10 mk win and a successful Big double to 20 mk; no RAM, cards or outcome is
injected. Phase 2's original-input/validation reference remains available.

**MEASURED — residual stack data:** complete RAM comparison exposes surviving
halves of overwritten saved pointers at a small set of stack bytes. The audit
records each byte's last writer (PC, logical write address/size/value and
instruction count), across checkpoints. Musashi writes MOVEM.L predecrement
registers as low/high 16-bit bus operations; the diagnostic joins that measured
pair, without changing CPU execution. A fragment is accepted only when its
complete original write differs by the same ROM/RAM/device placement delta in
both runs, its writer identity agrees, and its surviving byte matches that
write. No stack range or unexplained byte is excluded from comparison.

**MEASURED — ROM writes:** the covered bypass profile writes to its read-only
image at three sites: `$2184` → `$24EA` (long), `$25AA` → `$25C2` (long), and
`$2358` → `$257B` (byte). The first two are ROM/RAM probes; the third reaches
ROM through the caller's retained A0. These writes must remain ignored by the
native compatibility layer; they must not mutate the loaded program image.
The observed destinations and widths are in `host/tables/rom-write-hooks.csv`.

**MEASURED — Phase 3 verification:** setup, attract, deal, win, double-up and
service match the original-address bypass reference at both independent
placements. Complete CPU/device state, every RAM byte (including proven pointer
fragments), coverage, frames, audio and device transactions agree after the
recorded placement deltas. The final uninterrupted relocated run matches the
checkpoint chain byte for byte, including the full snapshot and PCM suffix.
Removing required ROM/RAM/device fixups, low-vector/D7/RESET hooks or I/O records
stops execution; corrupt input ROMs, incompatible snapshots and invalid
placements are rejected. Synthetic checks also pass under address/undefined
behavior sanitizers. The union is 17,739 PCs (17,632 ROM, 107 RAM), with 91
operand fixups and all 29 covered absolute-long operands accounted for. This
measures the covered host contract, not native Amiga hook execution or unknown
firmware paths; see `phase3-relocation.md` and `host/tables/coverage.json`.

## Phase 4 hook preflight

**MEASURED (reference implementation):** Musashi's `m68k_op_clr_8_ai` and
related CLR handlers emit only a write callback, with no destination read.
The native hook therefore uses the same write-only Board transaction as the
Phase 3 access tables. This is a property of the pinned reference, not evidence
about the physical 68008 bus or read side effects on the original board.

**MEASURED (FS-UAE 68000 native harness, not the physical RAY board):** entering
Exec Supervisor after replacing its privilege vector faults in Amiga ROM before
any game instruction. Installing owned vectors from inside the supervisor
entry resolves this. With single-step tracing enabled, TRAP #3 additionally
produces a supervisor trace whose PC is the native TRAP entry; that trace must
return to the pending TRAP frame without incrementing the game instruction
count. Native execution then passes the early 87,899-instruction boundary with
all 262,144 RAM bytes identical to Musashi at the same allocation addresses.

**MEASURED (native live-pacing diagnostic):** scanning the whole 512 KB guard
on each VBI causes repeated watchdog resets during the initial RAM-clear code
around `$2222`. Sampled stops repeatedly land in the guard scan; after about
79.5 million virtual cycles there are only 501 native service/trace boundaries,
no virtual IRQs, and another reset entry. The plan's full-per-frame guard check
is not a viable live pacing policy in this implementation. The user approved bounded incremental live checks on 2026-09-25: 1 KB
per serviced frame, with a complete sweep every 512 serviced frames. Diagnostic
full-guard checks and exit checks remain unchanged.

**MEASURED (FS-UAE 68000 native harness, 2026-09-25):** the complete checksum-bypass
attract replay reaches original PC `$2442`, SR `$2000` after 40,477,629 original
instructions, 324,000,006 reference cycles and 21,267 virtual IRQs. All 262,144
work-RAM bytes match a Musashi rerun at the actual native placements (ROM
`$4C1100`, RAM `$278A54`, device guard `$501194`), with no masked bytes or pointer
normalization. The guard remains intact and owned vectors are restored. This
is a native execution/shared-model result under the approved diagnostic schedule;
it does not validate physical-board timing or the still-blocked live VBI pacing.

**MEASURED (incremental live guard, 2026-09-25):** replacing full per-frame guard
scans with the approved 1 KB portions does not by itself fix live boot. At
113,600,000 virtual cycles the live run has only 715 service/trace boundaries
and zero virtual IRQs, with the last guest PC at `$221A`. Two debugger samples
stop in `Ay38912::tick`/`clockStep`. The synchronous reference synthesizer
executes 2,500 AY divider steps per 20 ms virtual frame. **INFERRED:** its
service cost leaves another VBI pending on return, starving the game. The
earlier guard samples identified one costly operation, not the sole cause.
The bounded run exits at 400,000,000 virtual cycles with 2,503 service/trace
boundaries, zero virtual IRQs and original PC `$21D8`; status 4, null native
error, intact full guard and restored owned vectors verify its cleanup. Five
debugger samples stop in AY synthesis. Live timing remains unvalidated; no
sound state or watchdog behavior is bypassed.

**DECISION (user, 2026-09-25):** accept Phase 4 as complete under its verified
native diagnostic/full-RAM scope. Defer live VBI-paced boot validation to
Phase 5 with the Amiga audio/video backends. The live starvation findings above
remain unresolved evidence; this scope change does not establish live boot or
physical-board timing.

## Phase 5 native timing and display findings

**DERIVED (Commodore Hardware Reference Manual, table 3-13):** standard PAL
non-interlaced video has 283 visible lines after vertical blank ends at `$1D`.
The board's 292-line logical frame therefore needs cropping or vertical scaling
for this Amiga mode. Source: local ADCD 2.1
`REFERENCE/ROM_KERNEL_MANUALS/HARDWARE/HARD_3`, node `3-4-2`.

**MEASURED (native live run with Paula backend):** removing reference PCM
synthesis reaches the watchdog timing test but fails at original `$20E8`.
D1 is 106,290, above the `$10000` bound tested at `$20CC`. The warning edge
arrived too early relative to the slowed polling loop. `$20E8` is the test's
RESET/retry target; it was outside the covered RESET-hook catalog and therefore
stops loudly as a native privilege exception. Owned vectors restore correctly.
Evidence: `tmp/phase5-live-audio.log`. **DERIVED:** wall-clock device timing and
native MMIO-hook overhead are incompatible with this self-test's reference
iteration bounds; choosing a live boot schedule needs an explicit decision.

**DECISION (user, 2026-09-25):** native boot uses the validated instruction/event
replay schedule, then switches to live VBI timing at idle. The replay supplies
no CPU or RAM results. The 292-row image is cropped by five rows above and four
below for 576×283 PAL output. AY sound uses Paula loops and bounded envelope/noise
updates rather than per-sample synthesis. Native HD63484 storage is authoritative
bitplanes; only CPU-visible word reads/writes reconstruct the packed bus format.

**MEASURED (FS-UAE native planar boot):** at 40,477,629 original instructions,
324,000,006 reference cycles, 21,267 virtual IRQs and original PC `$2442`, all
262,144 work-RAM bytes match Musashi at native placements ROM `$2C5900`, RAM
`$27D23C`, guard `$305974`. All 524,288 reconstructed VRAM bytes and all 163,008
pixels of the cropped frame match the packed host reference. The 840 masked AY
writes have the same ordered stream hash (1961304243). This validates logical
pixel indices and register delivery, not physical palette or analogue audio.
Evidence: `tmp/phase5-planar-comparison.log`, `tmp/phase5-boot-*.bin`,
`amiga/.run/phase5-planar/gdb-out.log`; checker `host/planar_capture_check.py`.

**MEASURED (FS-UAE live continuation):** the same run reaches 400,000,006 cycles,
22,134 total virtual IRQs and original PC `$C06` after 9.5 seconds of live time.
The exit guard is intact, owned vectors restore, native error is null, and no
watchdog reboot occurs. The planar backend reports 11 fills, 307 block copies
and 366 presented frames across boot and live continuation. IRQ count includes
all virtual sources, not just the 100 Hz system signal.

**MEASURED (emulator configuration audit, 2026-09-25):** the successful
9.5-second continuation above ran with `uae_cpu_speed=max` and CPU,
CPU-memory and blitter cycle accuracy disabled, in addition to warp mode.
These explicit settings appear in `amiga/.run/phase5-planar/fsuae-dbg.log`.
The later failing `phase5-final` run has warp mode but none of those acceleration
overrides. **DERIVED:** the initial live pass is not evidence for A500-speed
scheduling; emulator configuration is a confounding difference, so the failure
must not be attributed to the input/wrap integration alone. Its exact logical
RAM/VRAM/frame/AY comparisons remain valid at the paired replay boundary.
The current baseline and optimized comparisons use the default A500+ CPU and
chipset settings, with only warp playback enabled.

**MEASURED (FS-UAE Agnus self-test):** native fills and disjoint copies match an
independent packed reference for replace/OR/AND/XOR, pixel offsets 0/1/4/15,
partial edge words and a 608-pixel stride. The original program then completes
the early 87,899-instruction diagnostic with no native error and restored vectors.
Evidence: `tmp/phase5-selftest.log`.

**MEASURED (FS-UAE platform resource checks):** with all four Paula channels
reserved through `audio.device`, INTENA is `$602C` (all AUD interrupt-enable bits
clear), DMA is `$03CF`, and audio requests may remain latched without generating
an interrupt storm. The short live run exits with status 4, null native error,
restored vectors and a 32,768-byte NVRAM file. A separate eight-byte NVRAM fixture
stops with `invalid NVRAM size` before native vectors are installed and leaves
the file unchanged. Evidence: `tmp/phase5-audio-irqs.log` and
`tmp/phase5-invalid-native.log`.

**MEASURED (extended Phase 5 native run):** a second full boot matches all RAM,
VRAM, cropped frame pixels and AY writes at native ROM `$2C6E00`, RAM `$27E77C`,
guard `$306EB4`. The subsequent combined input/wrap test does **not** pass live
play: it advances to 640,000,006 total virtual cycles and 8,000 system edges,
but adds only 44 virtual IRQs and no HD63484 commands, ending in the RAM test at
original `$1224`, SR `$2704`. The low counter crosses its forced wrap and ends
at 315,934,464. All 22 input events were submitted; this does not establish that
the game processed them. Guard and owned-vector checks pass and the bounded
exit reports status 4/null error, exposing that these checks alone do not detect
live reset loops. Evidence: `tmp/phase5-final-native.log`,
`tmp/phase5-final-comparison.log`. **INFERRED:** watchdog resets have returned
the game to startup. The exact first expiry context and the effects of wrap,
input/resource integration and catch-up timing have not yet been isolated.
Native reset counters and an opt-in first-expiry stop are now available.

**MEASURED (Amiga NVRAM file path):** all 32,768 bytes of a deterministic fixture
survive loading, clean-exit saving and backup unchanged. This validates the
mapped device's persistence, not work-RAM credit or book retention.


**MEASURED (FS-UAE queued Agnus validation):** the bounded framework queue
submits 2,180 blits, queues 2,050 and takes 1,338 backpressure iterations in the
mask/minterm and saturation tests. Four real BLIT interrupts drain the explicit
interrupt test. All 262,144 game RAM bytes then match Musashi at 87,899 original
instructions, 800,002 reference cycles and zero virtual IRQs, with native ROM
`$2C7800`, RAM `$27F194` and guard `$3078CC`. Owned CPU vectors restore; the queue
is empty, the old BLIT handler is restored, and final INTENA is `$602C`.
Evidence: `amiga/.run/phase5-queue-early/gdb-out.log`,
`tmp/phase5-queue-early-comparison.log`. This is an early execution/graphics
ordering gate, not a full-boot or live-performance result.

**DECISION (user, 2026-09-25):** measure the first watchdog expiry and service
costs before choosing the live clock policy. Excluding service time is a valid
option, but first strive for real-time scheduling through efficient Paula
updates and asynchronous blits using the framework queue. VBI timing remains
unchanged during this investigation.


**MEASURED (short native timing probe, not idle):** after the early 87,899-
instruction replay, a 0.2-second live continuation has 20 board ticks, ten
incremental guard checks, eleven presentation checks, no drawing commands and
no virtual IRQs. The E-clock is 709,379 Hz; paired reads cost at least 91 ticks.
Raw inclusive mean durations are 1,306.95 ticks for board tick, 1,725.2 for guard,
1,319.64 for presentation, 170 for AY tick and 288.6 for AY VBI. Subtracting the
paired-read floor gives rough estimates of 1.71/2.30/1.73/0.11/0.28 ms respectively.
Instrumentation adds additional overhead, so these are not exact production
costs or evidence for the idle reset's cause. Status 4, zero live watchdog resets,
intact guard and restored vectors pass. Evidence:
`amiga/.run/phase5-timing-early/gdb-out.log`.


**MEASURED (native housekeeping optimization, short probe):** at the same early
87,899-instruction boundary followed by 0.2 seconds live, word-at-a-time guard
scanning and write-driven display invalidation reduce raw mean guard cost from
1,725.2 to 771.8 E-clock ticks, and presentation checks from 1,319.64 to 275.36.
With the 91-tick paired-read floor subtracted, these are approximately 0.96 ms
and 0.26 ms. Short-operand integer multiplication paths reduce the board-tick
mean from 1,306.95 to 1,207.45 ticks (approximately 1.57 ms after subtraction).
AY costs remain approximately unchanged. These inclusive instrumented startup
measurements do not establish idle performance or the watchdog failure's cause.
The native guard self-test detects corruption at offsets 0, 1020, `$3FFFC`,
`$40000` and `$7FFFC`, and verifies that the incremental check stays within its
1 KB range. Status 4, no native error and vector restoration pass. Evidence:
`amiga/.run/phase5-housekeeping-early/gdb-out.log`.

**MEASURED (unthrottled emulator control):** repeating the short probe with
FS-UAE's warp mode preserves all 262,144 RAM bytes against Musashi at the replay
boundary, and closely reproduces the emulated E-clock measurements (guard
771.8 ticks; presentation 275.64). This validates using unthrottled playback
for the ongoing long diagnostic comparisons; it does not accelerate the
emulated 68000 or change the board clock policy. Evidence:
`amiga/.run/phase5-warp-probe/gdb-out.log`,
`tmp/phase5-warp-probe-comparison.log`.


**MEASURED (queue ordering stress gate):** the saturation test now shifts each
of 512 tall fills down one row, preserving a distinct result from every
submission. All 6,136 reconstructed words match, detecting dropped or reordered
entries that a final full overwrite could hide. The real BLIT handler drains
the subsequent interrupt test (five interrupts); total submissions are 2,180,
with 2,050 queued and 1,510 backpressure iterations. The early 87,899-instruction
boot boundary still matches all 262,144 RAM bytes, and the short continuation
exits with no error, intact guard and restored vectors. Evidence:
`amiga/.run/phase5-queue-order/gdb-out.log`,
`tmp/phase5-queue-order-comparison.log`. Full boot/live validation remains open.


**DERIVED (watchdog service path, ROM inspection):** the native handoff PC
`$2442` is the main-loop decrement/branch delay. The later conditional paths
write PIA `$FB01E` at `$246A` or `$2472`; `$2458` can skip those writes based
on a RAM flag. The level-5 system handler `$0C06` instead acknowledges the tick
through `$0C40` and calls the output/sound update at `$0C58`. Consequently,
continued system IRQ delivery alone does not demonstrate main-loop progress
or watchdog service. Pending-tick catch-up starving the main loop remains an
inference until the first-expiry context is captured; the RAM skip flag is
another condition to inspect then.


**DERIVED (native blitter Boolean ABI defect):** the SAS/C assembly helper
`_isBlitterBusy__13AmigaHardwareFv` returns an SNE byte (`$00`/`$FF`). Its GCC
wrapper declared that output as `bool`, allowing `blitterIdle()` to invert it
with XOR 1: `$FF` became nonzero `$FE`, falsely reporting idle. This is visible
in the linked 68000 instructions. Inlined queue branches could still work,
which explains why the queue stress test missed the out-of-line screen-flip
check. The optimized full run logged Copper/blitter conflicts during drawing;
both full comparison emulators subsequently quit before boot completion, with
no first-watchdog capture. The ABI defect must be fixed independently of the
still-unresolved live clock question. Evidence: saved optimized ELF and
`amiga/.run/phase5-optimized-warp/gdb-out.log`.


**MEASURED (Boolean ABI regression controls):** an isolated build with the old
wrapper fails the new busy-with-empty-queue check at seven original instructions
with `planar blitter self-test failed`, and restores owned vectors. The clean
corrected build passes that check, queue ordering and interrupt draining, then
matches every RAM byte at the 87,899-instruction boundary and exits its short
live continuation without error. The test preserves raw return bytes so compiler
Boolean inversions cannot cancel the defect inside the assertion. Evidence:
`amiga/.run/phase5-bool-negative2/gdb-out.log`,
`amiga/.run/phase5-bool-clean/gdb-out.log`,
`tmp/phase5-bool-clean-comparison.log`. The corrected full replay/live test is
running separately; this early result does not establish its outcome.


**DERIVED (shared queue flag ABI):** the assembly queue consumer also stored
an SNE byte directly into the C++ `bool hasQueuedBlits`. It now normalizes to
0/1 before publishing the flag; the stress test checks the raw byte after every
submission. Existing inlined branch tests often tolerate `$FF`, but that is
not a valid GCC Boolean representation and must not be relied on.


**MEASURED (normalized shared flag gate):** all 512 saturation submissions
preserve the shared flag's 0/1 representation, all retained fill rows and the
busy/idle checks pass, and all 262,144 early boot RAM bytes still match Musashi.
The short live continuation exits with status 4, no error and restored vectors.
Evidence: `amiga/.run/phase5-queue-flag/gdb-out.log`,
`tmp/phase5-queue-flag-comparison.log`.


## First live watchdog expiry: default-speed controls (2026-09-25)

**MEASURED:** both silent FS-UAE controls complete the same replay boundary:
40,477,629 original instructions, 324,000,006 cycles, 21,267 IRQs, PC `$2442`.
At their own allocation addresses, each matches all 262,144 RAM bytes, all
524,288 packed VRAM bytes, all 163,008 cropped pixels and the ordered 840-write
AY hash 1961304243. The baseline uses ROM/RAM/guard `$2C6F00`/`$27E834`/`$306F6C`;
the queued/housekeeping/busy-return-corrected build uses
`$2C8500`/`$27FDFC`/`$308534`. The latter full run precedes the separate shared
queue-flag normalization, which has its own early native regression gate above.
Neither uses forced counter wrap or scripted live inputs. CPU/chipset settings
are the default A500+ profile; warp accelerates playback only. Host audio is
silent via SDL's dummy driver, with emulated Paula active.

**MEASURED:** both stop at the first watchdog expiry after 3,600,000 live cycles
(450 ms), with watchdog age 3,602,830 and one pending timer tick. The unprofiled
baseline adds 45 IRQs and 166 native service entries, stopping after RTE at
original PC `$0D9C`. The corrected, profiled build adds 44 IRQs and 180 service
entries, stopping at `$0EB0`. Both have virtual SR `$0000`, and main-loop D6 has
changed only from 7399 to 7398. Live service-entry counts are not original
instruction counts: per-instruction tracing is off after the handoff. The full
guard remains intact and owned vectors restore. The deliberate error is
`live watchdog expired`; this is a failed live gate, not a successful exit.

**DERIVED (ROM inspection):** `$0D86` saves registers and invokes an original
callback in virtual user mode: `$0D98` deliberately clears S/IPL, `$0D9C` calls
A0, then TRAP 5 returns control. Therefore SR `$0000` at expiry is expected,
not evidence of a privilege-state defect. At the baseline stop A0 resolves to
original `$0E226`; the corrected stop is inside the known physical-switch read
helper `$0E92`. **INFERRED:** repeated timer delivery is starving progress of
these callbacks and the supervising main loop, preventing its watchdog strobe.
The endpoint alone does not prove the exact cycle cost of every hook.

**MEASURED:** no HD63484 command count changes during either live interval;
fills/copies remain 11/307, and the profiled run records zero blitter waits.
Its observed elapsed time is 320,840 E-clock ticks at 709,379 Hz (452.28 ms).
Raw inclusive measurements: board tick 53,586 ticks over 45 calls (75.54 ms),
guard 17,054 over 22 (24.04 ms), presentation 5,037 over 22 (7.10 ms), AY tick
7,561 over 45 (10.66 ms), AY VBI 8,969 over 23 (12.64 ms). AY tick is nested in
board tick, so these totals must not be added. Paired reads cost at least 91
ticks and are included. Only three of 180 native services were sampled (3,793
ticks total); this is too sparse and potentially aliased to estimate total
service utilization reliably. The unprofiled baseline's matching expiry shows
that the profiler is not necessary for the failure. Drawing and AY synthesis
are not supported as its dominant cause by this interval.

**MEASURED:** the corrected full run logs no Copper/blitter conflict warnings,
unlike the prior queued build with the noncanonical busy return. Evidence:
`amiga/.run/phase5-baseline-silent/gdb-out.log`,
`amiga/.run/phase5-corrected-live/gdb-out.log`,
`tmp/phase5-baseline-comparison.log`, `tmp/phase5-corrected-comparison.log`.
The next decision is whether to exclude native service time from the board
clock or retain wall-clock scheduling and further optimize the CPU/hook path.

### Clean standalone startup (2026-09-25)

**MEASURED:** the previous SDL setup replayed 50 command-3 events with the cabinet
open before entering refill mode, plus one event after closing it. These produced
51 player credits and bet 5. They were test inputs, not the ROM's empty-machine
initial state. Removing those events while retaining the 100-event operator
refill yields `$44074` = 0 player credits, `$4400C` = 100 payout reserve, displayed
winnings 0 and bet 1. The reserve is required by the existing minimum-reserve
check; zero player credit does not mean an empty coin hopper. A clean-start cache
now preserves the original executed initialization rather than patching self-tests
or accounting RAM. It is build-specific, local, ignored and never updated by play.

**MEASURED (host startup timing):** an instrumented cold SDL launch completed ROM
verification/core initialization by 0.001 s, SDL renderer creation at 0.522 s,
and first ROM display output at 2.950 s. The old window path never presented
before that ROM frame. The new path clears/presents immediately (0.542 s in this
run). These are local wall times, not board clocks or a claim to reproduce the
user's full five-second delay. Cold initialization/refill completed at 16.512 s;
a dummy-backend cached startup took 0.133 s with an identical full snapshot.

### Native startup cleanup (2026-09-25)

**MEASURED (FS-UAE startup probe):** direct startup with no `replay.bin` reaches
`nativeRun` with status 1, diagnostic=false, live=true, replaySize=0 and a null
replay pointer. Packed video storage is also null/zero-sized; it was already
excluded by the freestanding constructor (the earlier conversational claim of
an allocated 2 MB packed buffer was incorrect). Execution proceeds into original
ROM code without a preparation error. This is not a boot-to-idle or timing pass.

Native SHA-256 has been removed. Host-side ROM verification remains; native
startup compares only the exact words about to be patched and chip file sizes.
These locally generated comparison constants stay in ignored generated/.
An aligned Board allocation now holds both relocated ROM and RAM, saving the
separate 256 KB ROM allocation/copy and avoiding a redundant ROM pre-clear.
Twelve original vector bytes preserve the five audited low-vector reads.
Normal launches no longer allocate/load the 3.75 MB replay; diagnostics opt in.

**MEASURED (early replay regression):** 87,899 native instructions / 800,002 cycles
complete at PC `$1222`, with vectors restored and no native error. All 262,144 RAM
bytes match Musashi at the new adjacent ROM/RAM placements. The reference's
low-vector redirect is now active only inside its audited hook, so ordinary RAM
reads at ROM+`$40000` remain RAM reads. Full boot has not been rerun for this change.

### Direct-boot blank-screen capture (2026-09-25)

**MEASURED:** the direct-start capture at `tmp/direct-blank-driver.log` found
normal mode still executing the native planar/blitter regression suite from the
`$2194` relocation hook. That test now runs only in explicit replay diagnostics.
The subsequent 35-second probe (`tmp/direct-blank-after-driver.log`) nevertheless
records 57 watchdog resets, PC `$1222` in the original RAM test, 1,299 VBIs and
207,840,000 board cycles. All 256 HD63484 control bytes and all AY write counts
remain zero; no display frame was composed. The blank screen/silence therefore
precedes game video/audio initialization, rather than demonstrating a failure
of a rendered frame or Paula output. Removing the test reduces startup work but
does not cure the reset loop. The board-clock policy remains pending.

### Native guest-time investigation (2026-09-25, in progress)

**MEASURED:** counting E-clock intervals between C service calls, while also
excluding Amiga VBI handling, passes the initial ROM RAM test but reaches the
watchdog retry RESET at `$20E8`. This previously uncovered RESET is now named
and hooked; it resets peripherals through the existing model. The captured D1
was 109,502 at retry, above the test's `$10000` upper bound, consistent with a
warning arriving too early relative to original loop progress. C-boundary
sampling still includes trap entry/exit overhead for every hardware poll.

**MEASURED (abandoned clock probes):** PAL beam sampling included variable
hardware-access latency, and C-boundary `ReadEClock` sampling of the unchanged
watchdog polling loop measured 1,470–3,570 CPU clocks. Neither is a usable
per-instruction reference after subtracting a guessed fixed overhead.

**DERIVED (implementation under test):** reserve an available CIA timer through
`AddICRVector`, disable only that timer's interrupt with `AbleICR`, and start/stop
its E-clock counter across native resume/exception boundaries. A separate,
authored user-mode NOP/Line-A test calibrates boundary cost; it does not modify
or execute game logic. Original hooked instruction cycle costs come from the
pinned Musashi 68000 timing table during host table generation, never a linked
native emulator. The CIA resource API requires ownership before touching timer
registers (ADCD 2.1 `TEXT_AUTODOCS/CIA.DOC`, `AddICRVector`); hardware timer
start/stop and force-load semantics are in `HARDWARE/HARD_F`, F-2-3.

**MEASURED (continuing):** the reserved timer's initial run obtained CIA-A timer
A. With VBI and level-6 work excluded, 256 consecutive polls still measured
460–2,990 clocks (calibrated boundary cost 502), indicating another source of
interference. The next probe also excludes CIA-A level-2 service. All wrappers
chain the saved OS handlers and restore their vectors on exit. No direct boot
or 50 FPS claim follows from these calibration probes.

**MEASURED (short boundaries, deferred bitplane DMA):** reserving CIA-A timer A
and stopping it before register saves gives a stable startup measurement when
bitplane DMA is left off until the first real frame. All 256 consecutive ROM
watchdog polls measured 140 CPU clocks; the independent NOP calibration measured
110, giving 106 boundary clocks and exactly 34 clocks for the unhooked loop.
The original BTST's nominal 16 clocks are charged separately. Capture:
`tmp/deferred-display-calibration-driver.log`. Earlier display-enabled probes
varied with launch conditions; one passed the watchdog test and another reached
error `$94`. They are not repeatable boot validation.

**MEASURED (progress capture, not idle):**
`tmp/guest-irq-boundary-idle-driver.log` ran to 39,360,000 board cycles, with one
expected watchdog reset at `$20DC`, 57 composed frames and no native fault.
It stopped in the FIFO feeder at `$2E5E`, not the main loop. Decoding its native
bitplanes shows the card-back graphic and top/bottom bands. The earlier `$13E6`
capture was the watchdog strobe routine, not the video-RAM test. Main-loop and
play validation remain pending.

**DERIVED (native implementation):** level-2/3/6 entry wrappers pause guest time
and chain the original Amiga handlers with the intact exception frame. BLIT
interrupts now also pause the clock and arm a return trace. The resource-owned
CIA-A timer is stopped before the wrapper's register saves. Original vectors
are restored on exit. Normal startup does no replay loading; it uses the same
authored door/Collect/reserve/status inputs as SDL's clean start, leaving the
original ROM to initialize accounting and player credits.

**MEASURED (regressions):** the early diagnostic replay still matches every one
of the 262,144 RAM bytes at 87,899 instructions / 800,002 cycles, with vectors
restored (`tmp/guest-clock-ram-check.log`). Host device, drawing and 2,560 native
hook cases pass. All 65,536 packed video words at all four nibble positions
agree with independent planar pixel reads. The faster Paula period generator
matches the old integer formula for all 4,096 entries. These checks do not
replace the remaining full native boot/play gate.

**MEASURED/DERIVED (coverage):** failed timing probes also reached the already-named PIA CA1 test at `$2132`; its RESET was absent from the native RESET catalog. It and the statically confirmed `$212A` failure-return RESET now use the existing peripheral-reset model. This does not suppress the ROM error code or mark either test as passed.

### A1200 bring-up probe (2026-09-25)

**MEASURED:** the same 68000-target binary, with native 68020 physical-frame
handling, completes the early replay at 87,899 instructions / 800,002 cycles.
All 262,144 RAM bytes match Musashi and owned vectors restore. Evidence:
`tmp/a1200-early-driver.log`, `tmp/a1200-early-comparison.log`.

**MEASURED:** direct A1200 boot rejects the watchdog timing check (error $94)
and later reaches the CA1 timing failure RESET at $2164, which is not yet in
the native RESET catalog. The generic CPU exception stop is therefore expected,
not evidence of a broken 68020 exception frame. In a separate probe the 256
watchdog polls measure 40–50 nominal clock units against boundary overhead 56;
most intervals are clamped to zero. Explicit cycle-accuracy options give similar
40–50-unit samples and overhead 46. The CIA timing estimate is not portable to
this faster execution path as currently implemented. Evidence:
`tmp/a1200-direct-driver.log`, `tmp/a1200-clock-driver.log`,
`tmp/a1200-real-clock-driver.log`. No A1200 boot-to-idle or 50 FPS claim follows.

**MEASURED (full replay regression):** the concurrent A500-profile diagnostic
completes 40,477,629 original instructions, 324,000,006 cycles and 21,267 IRQs at
PC $2442. Every one of the 262,144 RAM bytes matches Musashi at ROM/RAM/guard
$243300/$283300/$2CBA1C; it exits with status 4, no native error and restored
vectors. This validates the guest-clock changes with live guest timing disabled,
not direct boot or A1200 performance. It precedes the 68020 frame changes, which
have their separate early gate above. Evidence: `tmp/current-native-comparison.log`
and `amiga/.run/full-guest-regression/gdb-out.log`.

**DERIVED (short-loop clock accounting):** consecutive polls at $20BE, $2118
and $214A with their counter decremented once identify fixed unhooked paths of
34, 10 and 26 nominal 68000 cycles respectively. The original BTST is charged
separately. Native timing can count these intervals without relying on sub-CIA
tick measurements; it neither changes the instructions nor supplies test results.
The observed $2164 RESET now uses the existing peripheral-reset model, preserving
the failure/retry behavior. Direct boot validation is pending.

**MEASURED/DERIVED (68020 return-trace stack leak):** corrected poll timing
passes startup far enough to produce 60 frames and one expected watchdog reset,
but the 150-second run corrupts native service state. The 68020 trace frame is
format 2 (12 bytes), whereas Line-A/TRAP use format 0 (8 bytes). The common
entry removed eight bytes for both, leaking four per slow trace. Native entry
now consumes the frame's format; virtual 68000 game frames remain unchanged.
Capture: `tmp/a1200-loop-clock-driver.log`. This is not a gameplay pass.

**MEASURED (ECS compatibility):** with A500+, the current binary reports AGA
false, physical exception frames of six bytes, and no private vector table.
Its Copper list retains FMODE=0, DIW $1D91/$38B1 and DDF $44/$CC. At 87,899
instructions / 800,002 cycles every RAM byte matches Musashi and vectors restore.
Evidence: `tmp/a500-current-driver.log`, `tmp/a500-current-comparison.log`.

**MEASURED (A1200 boundary isolation):** temporarily copying the complete CPU
vector table to Fast RAM and selecting it through VBR reduces display-enabled
NOP boundary samples from 40–220 nominal units to 30–40. The 360-second capture
reaches 27.93 seconds of guest time, refill graphics, one expected watchdog reset
and no native fault; service SP remains inside its allocation. The OS VBR is
restored on exit. This remains progress rather than a completed play test.
Evidence: `tmp/a1200-fast-vbr-driver.log`.

**DERIVED (slow startup readback):** $10F2C calculates a graphics-memory
checksum through $10FA0. Each byte iteration writes RD at $10FC0, polls RFR at
$10FC6 and reads a FIFO byte at $10FCC; $11030 drains the FIFO. Those three
hardware instructions each enter a native hook. Repeated live samples in this
routine explain a major startup bottleneck independent of blitter drawing.
The checksum remains executed; common MOVE/BTST hook forms are being optimized.
Research disassembly is ignored at `tmp/native-slow-loop.txt`.

**MEASURED (checksum FIFO depth, host reference):** the host reaches the
checksum exit at `$10FD8` after 4,997,823 instructions and 55,172,422 cycles
(6.90 s at 8 MHz). This is the same 78,204 RDs that the native profile
completed by 2.62 board-seconds. The loop runs 76,152 times, reading one FIFO
byte per RD word. The saved state then holds 38,076 read-FIFO words with
`readLow` set (16,409 underflows in total). `$11030` drains them with 76,152
`TST.B`/`BTST` iterations at `$1103C/$11040`, i.e. two bytes per word. The model
sets RFF at eight words but neither bounds the FIFO nor suspends RD. The ROM
reads the data port only at `$F6002` (14 byte and four word sites, never a
byte at `$F6003`). Whether one byte read consumes one RD result on the real
board is open; see the model question in `docs/native-performance-plan.md`.
Evidence: `tmp/review-fifo-12000-*`, `tmp/review-play.replay`.

**MEASURED (live boot / first deal):** direct native execution now completes
cold setup at 40.5 virtual seconds, takes the authored coin and Deal inputs,
then stops at a previously unsupported display-window alignment during the
first deal (42.76 seconds). The window moves in eight-pixel steps; its source
and destination need not share a sixteen-pixel word boundary. The native
composer now shifts planar words for those windows; aligned regions retain
queued blits. A synthetic pixel oracle covers all 256 source/destination
alignment combinations, masks, blanking and source-storage boundaries.
Evidence: `tmp/a1200-final-play-driver.log`. Completion of a hand is still open.

**MEASURED (chipset identification):** the A1200 probe reports graphics.library
ChipRevBits0=$13, Lisa ID=$00F8 and VPOSR=$A300. Requiring the public AA flags
alone incorrectly selected the ECS path. Wide fetches now require either both
public Alice/Lisa flags or matching physical Alice/Lisa IDs; CPU type does not
select them. The resulting A1200 Copper list uses FMODE=3 and DDF $38/$B8.
The ECS path keeps FMODE=0 and DDF $44/$CC. Evidence:
`tmp/aga-id-probe-driver.log`; emulator ID behavior cross-checked against
Amiberry's `custom.cpp` VPOSR/DENISEID implementation.

**DERIVED (late VBI / vertical jump):** native exception entry left physical
IPL=7 throughout C++ device services. This delays VERTB, Paula updates and the
BLIT queue consumer. Restarting the Copper from a delayed VBI reloads bitplane
pointers partway through the visible display, consistent with the reported
one-frame vertical jump. Services now enable Amiga IRQs after saving guest
state and pausing its clock, then mask them again before restoring that state.
Supervisor-mode Amiga interrupts chain to Exec without executing guest code.
The display also defers swaps outside scanlines 0..7, retaining both buffer
ownership and the current Copper pointer until a safe blanking interval.
Visual confirmation of the jump fix remains pending.

**MEASURED (interrupt-enabled service regression):** all 262,144 RAM bytes
still match the early ECS replay at 87,899 instructions / 800,002 cycles;
owned vectors restore and the six-byte 68000 frame path remains selected.
Evidence: `tmp/service-replay-comparison.log`. This early gate is not a full
boot/gameplay verification.

**MEASURED (complete native live sequences):** both direct A1200 runs complete
76.5 seconds of guest time (612,000,000 cycles), including cold setup, coin,
Deal, three Hold keys, Draw, another coin, lamp panel and service-door toggles.
Both exit with status 4, null native error and restored vectors. Only the one
expected startup watchdog reset occurs. Ready accounting is zero player credits
and 100 reserve coins; final accounting is one player credit and 102 reserve
coins. Planar captures show the dealt hand and replacement cards. The two runs
produce identical command counts and AY register-write counts. One run uses
ECS fetches on A1200; the other positively identifies AGA and uses FMODE=3.
This is functional validation, not a 50 FPS or real-hardware display/audio claim.
Evidence: `tmp/service-play-driver.log`, `tmp/aga-play-driver.log`,
`tmp/service-play-frame-6.png`, `tmp/service-play-frame-9.png`.

**MEASURED (swap guard):** the AGA sequence publishes 365 frames and records
520 VBI visits with a pending frame outside scanlines 0..7. Those visits leave
Copper selection and buffer ownership unchanged. A pending blit can also delay
a swap, so this counter is not a count of frames that previously jumped.

**MEASURED (costs, before final copy acceleration):** with Amiga service IRQs
enabled, the ECS-fetch A1200 run records 81,748 profiled VBI calls, approximately
1,635 seconds of PAL machine time. Video bus services take 676,900,761 E-clock
ticks at 709,379 Hz (954 seconds, approximately 58%); the longest takes 9.14
seconds. Paula VBI and tick work together account for approximately 10.3 seconds
(under 1%). These inclusive instrumented categories include measurement overhead;
service samples must not be summed with their nested categories. The startup
executes 78,204 RD commands. Graphics service cost and repeated exception-hook
bookkeeping dominate; wide fetches alone do not establish real-time play.
The AGA run has 72,252 profiled VBI calls and 568,864,026 video-service ticks.

**DERIVED (copy acceleration):** the old native overlap guard compared the
rectangles' enclosing linear address spans, incorrectly declining side-by-side
cards whose actual pixel intervals are disjoint. It now checks sorted row
intervals without division. Genuine overlaps retain the shared sequential
pixel algorithm. An independent pixel-occupancy oracle covers 10,000 cases;
the native blitter self-test includes side-by-side copies with all logical
operations and partial-word masks. The complete live sequences above precede
this optimization; its own native regression is recorded separately.

**MEASURED (final copy regression):** the ECS diagnostic executes the expanded
real-blitter self-test successfully (`videoSurface.tested=true`), including
side-by-side partial-word copies, masked queue wrap/backpressure, and actual
BLIT-interrupt draining. It records 2,245 submissions and 1,303 backpressure
waits, then reaches 876,360 instructions / 8,000,002 cycles at $125A. All 262,144
RAM bytes match Musashi; vectors restore and FMODE remains zero. Evidence:
`tmp/blitter-final-driver.log`, `tmp/blitter-final-comparison.log`.


**MEASURED (native drawing bottlenecks, 2026-09-25):** the optional command
profile in `tmp/command-profile-driver.log` identifies 41 RFRCT commands taking
27,623,138 E-clock ticks (about 39 seconds), one CLR taking 5,299,536 ticks
(7.47 seconds), 152 PTN commands taking 2,383,368 ticks, and 59 PAINT commands
taking 727,519 ticks. These inclusive service timings use 709,379 Hz; they
are software cost, not ACRTC hardware timings.

**DERIVED (solid fills):** the ROM programs a solid active pattern window while
other pattern RAM holds artwork. Testing all 16 pattern words incorrectly
prevented native fill acceleration. The initial CLR covers 153 packed words
per row with a 152-word pitch and 1,231 rows; its overlapping rows form one
continuous replace interval. Masked, bounded blits preserve that result.

**MEASURED (patterns):** the 40.5-second trace contains 1,752 PTN commands, all
15×14 pixels (`SZ=$0D0E`), with unzoomed pattern rows 2..15 selected by
PR5/6=`$2000`, PR7=`$F0F0`. These are suitable for planar mask/colour tiles;
pattern uploads still occur at runtime. Local evidence:
`tmp/current-drawing-commands.json`. Cached expansion retains physical colour
word phase and reverses logical Y into display row order. The ECS diagnostic
checks the real blitter against synthetic independent pixel expectations,
including cache eviction and transparent logical drawing, then matches all
262,144 work-RAM bytes against Musashi (`tmp/cache-regression-comparison.log`).

**MEASURED (cached native live run):** the unprofiled A1200 test completes
612,000,000 virtual cycles and all 24 coin/deal/hold/draw/later-input transitions,
with 376 displayed frames, no native error and restored vectors. The ready
state has zero credits; the final state has credits 1, reserve 102. Only the
expected cold-start watchdog reset occurs. PAL VBI count falls from 51,429 in
the preceding active-window-fill build to 41,787 with tall clears and cached
PTN tiles (about 19%); this remains far slower than real time. Local evidence:
`amiga/.run/window-fast/gdb-out.log`, `amiga/.run/cache-live/gdb-out.log`.


**MEASURED (current startup cost, 2026-09-25):** an isolated A1200 direct-boot
profile of commit `167f486` stops at 20,960,000 virtual cycles (2.62 seconds),
592,990 native dispatches, and 16,277 profiled PAL VBI calls (approximately
325.54 seconds). The live `nativeInstructions` counter counts dispatches, not
all original instructions. Video-bus services consume 53,670,057 E-clock ticks
(75.66 seconds), inclusive of command execution. Completed drawing commands
(groups 32..63 plus CLR) total 12,364,154 ticks (17.43 seconds). The 78,204 RD
commands alone consume 11,916,549 ticks (16.80 seconds). Paula tick plus VBI
consume 1,205,361 ticks (1.70 seconds). These categories overlap: command costs
are inside video-bus costs. The service counter samples only one dispatch in
64 and must not be presented as an exact total. Measurements include observer
cost; the run is a bounded startup sample, not a full-hand or first-pixel test.
Evidence: `amiga/.run/current-cost/gdb-out.log`, `tmp/current-cost-driver.log`.

**DERIVED (startup slowdown explanation):** the severe startup slowdown cannot
be attributed to drawing/Paula alone. Every hooked bus instruction enters a
full register-save/clock-stop/C++ instruction-and-address-dispatch/scheduling/
register-restore path; startup performs hundreds of thousands of these. Normal
boot also unconditionally schedules cold operator setup through 40,500 virtual
milliseconds, including after loading NVRAM. Pausing virtual time during service
work stretches that schedule. The actual time split between exception wrappers,
dispatch bookkeeping and unhooked CPU execution still needs finer measurement.
Unaligned/disallowed copy shapes additionally fall back to generic per-pixel
planar reads/writes; only 2 of the 19 copy commands in this sample reach the
blitter copy path. This sample does not classify each rejection's reason.


**DERIVED (FIFO investigation, 2026-09-25):** Hitachi User's Manual printed
pp. 60 and 64 specifies AR-controlled byte selection for control registers,
AR=0 FIFO entry, and high-then-low FIFO byte order in 8-bit mode; printed pp.
62–64 specifies a 16-byte read FIFO. Printed p. 49 disallows byte transfers
in 16-bit bus mode. Thus the current unbounded queue is not a faithful physical
FIFO, while the manual alone does not establish the board's external glue.

**MEASURED (isolated host hypotheses):** a host-only experiment which stops
when an RD result would exceed 16 resident bytes stops at 4,299,773 instructions,
48,546,270 cycles, PC $10FC6. A separate word-latch hypothesis (even data-port
read consumes a word and returns its high byte; odd read returns its latched
low byte) reaches the 40.5-second scenario endpoint at 40,616,002 instructions,
324,000,002 cycles, PC $2442. These are experiments, not a selected production
model or hardware confirmation. Files: `tmp/fifo-investigation/word-latch.log`,
`tmp/fifo-investigation/bounded.log`; production FIFO/reference remain unchanged.


**MEASURED (FIFO hypothesis qualification):** the word-latch hypothesis with
an enforced 16-byte capacity also completes the 69.5-second host scenario
(71,802,316 instructions / 556,000,002 cycles). Its 40.5-second ready image
matches all 168,192 palette indices from the existing reference. Nevertheless,
the original checksum code computes $002B7E21 with that hypothesis and $00111672
with current sequencing, while comparing against $0093D9D6 at $3097C. Neither
matches. Thus boot/image compatibility does not establish the correct interface.
Production semantics remain unchanged. Local evidence: `tmp/fifo-investigation/`
`latch-bounded.log`, `latch-checksum-context.txt`, `original-checksum-context.txt`
and `latch-compare-context.txt`.

**MEASURED (low-overhead native profile):** the replacement VBI sampler and
whole-batch ablations confirm distinct bottlenecks. Startup PC samples are
33.50% nativeDispatch, 16.61% Bus::access and 10.45% executeHook. Gameplay samples
are dominated by per-pixel drawing/address calculation. The read-only status
batch costs approximately 356.61 microseconds for C dispatch after subtracting
its context-reset control, excluding exception entry. Active sampling preserves
all 262,144 RAM bytes in the early ECS replay. See `docs/native-profile.md` for
observer comparison, phase boundaries, measurement limitations and local logs.

**MEASURED (shifted planar copies):** the independent native ECS blitter oracle
passes all 256 source/destination alignments, eight boundary widths and four
logical operations, including shifted display composition. A subsequent A1200
cold/play run submits 808 surface copies, 576 shifted, and 1,337 display-region
blits. Unsupported scan directions still use the shared ordered fallback.
This validates the backend against the current shared model, not physical ACRTC
bus/FIFO behavior. See `docs/phase5-amiga.md` for timing qualifications.


**MEASURED/DERIVED (clock-accounting limit):** corrected units alone, and an
initial option-C K=2 run, each reset the watchdog repeatedly and eventually
stop in an original CPU exception. K=1 completes the same cold/play scenario.
This is not evidence for lengthening the watchdog: adjacent patched accesses
can accumulate positive measured intervals despite executing no original
instructions between them. A resume-PC equality check proves such an interval
contains only hook/exception overhead, so it must contribute zero guest cycles;
its emulated instruction still receives its nominal reference charge. The live
short handler and full handler now apply this rule; two subsequent K=2
cold/play runs complete with only the expected startup reset.
Local failed experiments: `amiga/.run/clock-units/gdb-out.log` and
`amiga/.run/clock-c2/gdb-out.log`.


**MEASURED/DERIVED (throughput calibration):** the host checksum-to-drain
interval is 2,681,828 cycles. The final native K=2 experiment charges 1,831,430
nominal hook cycles and 489,788 measured guest cycles in that interval, implying
a residual throughput ratio of 1.736. A functional K=2 hand is therefore not
proof of preserving the reference per-phase instruction budget. The production
request is K=1.5, with runtime CPU calibration allowed only to reduce it.
These are estimates against the shared model, not board oscillator measurements.
The exact status snapshot/assembly path passes all 262,144 RAM bytes in the
8-second ECS replay. Evidence: `amiga/.run/clock-final/gdb-out.log`,
`tmp/clock-pair-checksum-end.log`, `tmp/clock-pair-drain-end.log`,
`tmp/status-cache-replay-comparison.log`.


## Normal-game hardware-test bypass (user decision, 2026-09-25)

**DECISION:** the user explicitly requested skipping/passing all coin-op
hardware tests during normal startup, then prioritizing gameplay performance.
This supersedes the earlier prohibition on bypassing those ROM diagnostics.
It does not authorize fabricating game/accounting state. The original program
still initializes devices, loads modules/graphics, and runs gameplay.

**DERIVED (instruction inspection):** RAM test $11FA already clears its range
through $127E before entering destructive pattern tests. Redirecting the LEA
operand at $121E from $1222 to the success return at $127A retains that original
clear and skips pattern writing/readback. The return is through A4, not RTS.
$1F2E is the isolated PIA/AY/edge/watchdog test wrapper; returning D0=0 with
V clear passes the caller's $227A check. Normal I/O init at $1A1C still runs.

**DERIVED:** $5B9C is a video-memory alias probe, returning zero for the
configured 512 KB path. $16E1C is the video-RAM pattern/external-board test,
called through RAM stub $41C40 at $2372; D0=1 is success. $10F2C checks graphics
readback and drains its read FIFO, called through $41C22 at $23AA; D0=1 is
success. These prologues can return their successful values immediately.
Display register setup $2AF6 and original graphics-loading calls remain.
The model's unresolved FIFO byte behavior is unchanged by this boot policy.
Patch metadata contains addresses/operations only; guards are generated locally.
**DERIVED:** $25A4 probes whether code storage is writable and returns zero
for ROM. The normal-game policy returns that known result directly, preserving
A0 and the original result flags. This also removes the same repeated hardware
probe during play (16,320 calls in the earlier 60-second profile).
Validation results are in `docs/startup-policy.md`.

**MEASURED (fast cold boot):** bypassing the resetting watchdog test leaves
A0=0 at the already-known ROM marker write $2358. Its byte write of $C3
therefore targets $00091 rather than $0257B. The ordinary host ROM map ignores
both. Relocated/native execution now explicitly admits only that exact extra
PC/address/size/value combination; it does not map low Amiga memory.

**DERIVED (state-driven operator setup):** the shared controller observes the
main-loop output sites $2472/$246A, door-mode byte A6−$770C, pending peripheral
attention A6−$78CE, input-enable A6−$78DE, and refill mode A6−$78D2. It sends
only external pin changes and existing protocol packets. Each refill coin waits
for transport completion and the reserve increment at $4400C; no fixed seconds
are inserted between actions. Completion additionally requires a closed door,
exited refill, cleared attention, enabled input, zero player credits ($44074),
and reserve 100. A stalled stage fails loudly instead of claiming readiness.
These are software observations, not board timings; validation is recorded below.

**MEASURED/DERIVED (rapid refill correction):** native rapid refill exposed a
partial outgoing application packet overlapping a new peer request. Peer-side
idle and the reserve increment alone do not prove that the ROM has finished
its link work. The serial structure is at A6−$76E8 ($41418), confirmed by
$19A8. Its state byte +$16 returns to $61 at $E46C/$E6BA; +$1C0 is outstanding
transmit data (tested at $E71E), and +$1C4 is pending receive work (drained at
$E4F4). The controller now waits for that idle state, both counts zero, and no
TX interrupt enable, in addition to empty peer queues. It also waits for a
fresh main-loop pass after close-door work, rather than snapshotting during a
callback whose flags have already changed. No serial result is forced.

**MEASURED (final fast-start policy, 2026-09-26):** SDL reaches the verified
zero-credit ready display in 8.12 board-seconds (2.90 host seconds; cached 0.11).
The A1200 live capture reaches ready with credits 0/reserve 100 at 11.87
board-seconds, about 136 PAL seconds; subsequent scripted play has no resets,
no device/native error, intact guard and restored vectors. Gameplay remains
slow: 60 board-seconds takes 199.26 sampled PAL seconds. ECS replay with the
same boot patches matches every RAM byte at 7,008,979 instructions / 64,000,002
cycles / 7,831 IRQs. See `docs/startup-policy.md` for reproducible scope and logs.


**MEASURED (live cabinet-message overlap, 2026-09-26):** a K=37/16 native
run stopped with the peer assembling `$71,$50,$AF` (`serial transmit checksum`)
after the second service-door action; a K=1.5 counter-instrumented run failed
at the first door action. The latter rules out blaming the higher clock cap
alone. Normal coin/door keys had enqueued protocol requests without the outgoing
ROM-idle checks already used by rapid refill. **INFERRED:** a new request can
interleave control acknowledgement `$50,$AF` with an unfinished application
packet. Native and SDL key frontends now retain those requests until the same
observed idle predicate holds, and issue one application packet at a time.
This changes external input pacing, not ACIA/FIFO semantics or ROM results.

**MEASURED (live pacing regression):** both native schedules complete through
600,000,000 board cycles after this change, all 24 key transitions delivered,
external input queues empty, no native/device error, zero watchdog resets and
restored vectors. Evidence: `amiga/.run/cabinet-live/gdb-out.log` and
`amiga/.run/cabinet-cap37/gdb-out.log`. These two schedules support the fix;
they do not establish physical peripheral timing or justify raising the default K.


**MEASURED (release-check distinction):** the native post-service final image
shows `P2 87`, despite zero harness errors and watchdog resets. This is a ROM
attention state and remains an open gameplay/service gate; counting key edges
and restored vectors does not establish correct service-mode exit.
**DERIVED:** `$14DEA` is a two-stage timer callback. It sets A6−$78CA, schedules
itself using parameter 1000, and on the later visit reports `$87` at `$14E48`
when A6−$770D, −$78D2 and −$78D0 are clear, then calls the known attention
entry `$108E8`. Its physical peripheral meaning and triggering live action are
not yet established. Added this callback to both entry-point catalogs at discovery.


**MEASURED (P2 87 reproduced on host, 2026-09-26):** a ready-state Musashi
run with the native play/door action sequence and immediate door status packets
reaches `$14E48` at 505,282,180 cycles (63.16 board seconds), with D1 becoming
`$87`. Thus this failure is not unique to native timing or planar drawing.
The callback's outgoing packet structure at A6−$77DE contains `$09,$01,$80`:
command 9 / parameter 1, followed by the structure terminator. Evidence remains
local in `tmp/door-play-*`; response sequencing is still being investigated.

**MEASURED (door-reply ordering):** delaying only the same status replies by
300 ms in the host reproduction avoids `$14E48` through 80 board seconds;
immediate replies reach it at 63.16 seconds. No clock/model/ROM changes are used.
**DERIVED:** live door replies need the same completed main-loop/new-door-mode
boundary used by cold setup. The shared native/SDL input controller now queues
the door edge, waits for that observed boundary and transport idle, then sends
the existing status packets. It inserts no fixed delay and preserves rapid
successive door edges as separate transitions. Native validation through 85 board seconds completes all 24 key transitions,
returns to the normal poker screen with one credit, and has no reset/device
error (`amiga/.run/door-ack-live/gdb-out.log`). ECS replay remains exact.


**DERIVED (normal-loop throughput bound):** after the first entry, `$244A`
loads D6.W with `$1D00`; the `$2442/$2444` SUBQ.W/BNE loop consumes that count
before the next main-loop I/O at `$246A` or `$2472`. Under the existing Musashi
68000 reference timing, those 7,424 iterations cost 103,934 cycles. Counting
completed normal-loop passes therefore gives a lower bound on reference CPU
work, without replacing or accelerating an original instruction. Subtract one
pass at each measurement interval to exclude its potentially partial first
iteration; other executed code only increases actual reference work. Opt-in
native measurements now record these counts alongside measured guest/nominal
charges at the ready and input-action boundaries. This is a calibration input,
not approval to assume a new clock ratio from CPU microbenchmarks alone.

**MEASURED (gameplay throughput, K=1.5):** native ready/input-boundary snapshots
(`tmp/play-floor-measure.bin`) give conservative reference/guest lower bounds
5.305–5.431 across ready/coin, deal, hold, draw, double, big, panel/coin and door
workloads. Subtracting all nominal hook charges and one complete delay loop per
interval yields these bounds. A host histogram over seconds 10–70 records
31,323,718 delay iterations and 4,219 normal-loop output passes, consistent with
7,424 iterations/pass plus partial interval endpoints. The 12.5%-margin minimum
is 4.642. This supersedes the skipped startup-drain loop as the relevant
*gameplay* calibration constraint; boot calibration remains separate.


**DERIVED (display invalidation):** `$2E74/$2EB6` write CCR low byte `$80/$81`
to disable/enable WFR interrupts. Those changes do not affect the display, as
already recorded under display-format evidence. The native frontend had marked
every CCR write as requiring a full composition. It now ignores CCR-low changes
and CCR-high changes outside GBM bits 2:0 for display invalidation; unchanged
register values also leave the image clean. All device writes still execute and
all other control-register changes conservatively invalidate the display.


**MEASURED (corrected native throughput observer):** count the `$246A/$2472`
main-loop output only when its Line-A instruction executes, or in its completed
short body. A trace at the same PC is before the instruction and must not count
another pass. With that correction, the same ready/play/service scenario gives
a conservative reference/guest lower bound 5.207, or 4.556 with 12.5% margin
(`tmp/pia-count-live.bin`). Earlier loop-counter bounds are superseded by this
measurement; physical-board timing and unexercised hand-specific paths remain
unproven.


**DERIVED (bounded command-feed sequence):** `$2E58` tests status bit 1 at
`(A0)`; `$2E5C` branches to `$2E7E` if clear; otherwise `$2E5E` writes one word
from `(A1)+` to `2(A0)` and continues at `$2E62`. This does not include the
surrounding command-buffer loop. The original source is consumed only on the
ready branch. **MEASURED (reference CPU timing):** independent synthetic 68000
instructions cost 12 cycles for the status test; 8/10 for the not-taken/taken
short branch; and 16 for the postincrement word write. The opt-in native fusion
experiment preserves these charges and stops at any intermediate replay event.


**MEASURED (native implementation, not board calibration):** the approved bounded
feed experiment matches the host reference on both ECS/68000 and AGA/68020 at
7,008,979 instructions / 64,000,002 cycles / 7,831 IRQs, including all RAM, VRAM,
cropped pixels and 30 AY writes. Each replay combines 30,200 command-feed writes;
intermediate scheduled events still stop at their original instruction boundary.
The physical RD byte/FIFO question is unaffected. Evidence:
`tmp/feed-replay-ecs-comparison.log`, `tmp/feed-replay-aga-comparison.log`.


**MEASURED (software regression, not hardware calibration):** default combined
command feeding, shared byte-write inlining, reused pixel addresses and queued
solid PAINT rows retain the established boot replay's complete RAM/video/frame/
AY results. The PAINT replay uses 891 fills versus 815 before, with identical
pixels and drawing positions. `tmp/paint-spans-replay-comparison.log` compares
the native output with the unchanged pre-optimization graphics reference.


**MEASURED (software regression, not hardware calibration):** bounded reuse of
exact curve outlines preserves all RAM, VRAM, cropped pixels and AY writes in
the established AGA and ECS boot replays (`tmp/curve-cache-replay-comparison.log`,
`tmp/curve-outline-replay-ecs-comparison.log`). The
live scenario reuses 146 of 148 post-ready outlines. This changes only rendering
cost; no claim about the physical ACRTC rasterizer or FIFO behavior is added.


**DERIVED (renderer optimization):** a one-point pattern window always selects
its programmed row/column regardless of zoom/count wrapping. **MEASURED
(software regression):** direct selection passes 2,592 packed/planar contour
checks and exact AGA/ECS replay RAM/VRAM/frame/AY comparisons
(`tmp/single-pattern-replay-comparison.log`,
`tmp/single-pattern-replay-ecs-comparison.log`). General pattern windows retain
the existing arithmetic; this adds no new physical-chip claim.

### Paula noise fidelity investigation (2026-09-26)

**MEASURED (implementation audit):** native noise previously played uniformly
random signed bytes, refreshed 64 bytes per active VBI, instead of the AY's
one-bit output. It also rounded the divider to `57*N` Paula ticks and added
noise to tone. The shared reference uses the 17-bit recurrence
`(s >> 1) | (((s ^ (s >> 3)) & 1) << 16)` from seed 1 and AND-gates noise
with tone per voice.

**MEASURED (host trace):** `tmp/review-play-events.txt` and
`tmp/bypass-play-events.txt` contain combined tone/noise sounds. The former
includes sustained states with three combined voices, so a single Paula
volume-modulation pair cannot implement the game's complete mixer. These
are traced register states, not a by-ear identification of particular effects.

**DERIVED (offline sound coverage):** `$118CE–$1196E` installs three
null-terminated sound-pointer groups from parameter-body A4 + `$7DA`.
A4 is module + `$52`, so PARA200J's directory starts at offset `$82C`.
`$E058` copies fourteen literal AY register bytes, followed by one delay
byte; the next byte is a continuation record unless it is 0 (end), 1
(restart), or 2 (repeat count then continuation/end). `$E19E` selects these
sequences; `$DF8E/$DFEE` cycle the first two groups. Delay variation affects
scheduling, not oscillator parameters. This is data decoding, not replacement
of the sequencer: the original code continues to execute on both CPUs.

**MEASURED (table enumeration):** the three pointer groups contain 5, 4 and
16 entries (21 distinct starts), covering 125 register records and 37 distinct
active noise/mixed-tone parameter pairs. This is broader than the gameplay
trace. User approved offline waveform generation on 2026-09-26; envelopes,
volume and timing remain live. Table/sound bytes and generated audio stay in
ignored build directories.

**MEASURED (reference checks):** `make harness-paula-check` executes the original
`$E058` routine for all 125 records with only its scheduling callback returning
immediately, and verifies the fourteen resulting register bytes. All agree
with the data catalog. All 162,588 generated samples agree within one 8-bit
quantization step with an independently stepped AY reference and the specified
63-tap, 9 kHz low-pass filter. Full tone cycles close at each loop boundary.

**DERIVED (native implementation):** 37 loops in 162,588 Chip RAM bytes replace
the additive shared-noise approximation. Each AY channel retains its own live
volume/envelope. A 68000 assembly audio server queues resident 256-byte slices
without mixing or copying. Finite loops and restart phase are approximations;
Paula period 170 is 0.148% faster than the 48-microsecond source sampling period.
Source waveform construction and filtering occur only on the host during build.
The manual's audio DMA reload/interrupt mechanism is described in the ADCD
Hardware Reference Manual §5-3-1; level-4 time is excluded by the same native
clock wrapper used for the other service interrupts.

**MEASURED (native/assembly validation):** the filtered-bank A1200/PAL run
completes all 24 scripted input transitions at 480,000,000 board cycles, with
no native/audio error, no live watchdog reset and restored native vectors.
It services 754 / 1,005 / 1,328 audio section interrupts. The previous-audio
control ends at 5,605 PAL frames and the new run at 5,956, but their different
live outcomes produce 525 versus 2,115 AY writes; this pair is not an isolated
audio-overhead benchmark and does not establish a performance improvement.
Both use 1 MB Chip RAM and muted debug output. Local logs:
`tmp/paula-final-run.log`, `tmp/paula-control-run.log`.

The actual linked DMA server passes 4,608 synthetic Musashi 68000 cases over
all even lengths 2–1024, full/tail/wrap positions and three IRQ bits, including
callee-saved registers, stack balance, pointer/length writes and double
acknowledgment. Core execution costs 254–282 cycles per section; this excludes
Exec, exception/trace entry and Chip RAM contention. Tests:
`make harness-paula-stream-check` after the native build with toolchain on PATH.
At period 170, full 256-byte sections request about 81.5 interrupts/s per active
noise voice. Silent voices disable these interrupts. A shorter final section
adds a small number of requests per loop. There is no runtime PCM generation.

**MEASURED (final exit check):** the final cached-lookup build reaches
160,000,000 board cycles with 351 / 350 / 351 audio interrupts, no audio/native
error and restored native vectors. Before resource release, audio is inactive,
all three audio servers have been uninstalled, and INTENA's four audio bits are
clear (`tmp/paula-exit-run.log`). Normal launcher audio remains enabled; all
of these debugger runs use the existing dummy host audio driver.

### Repeating planar fill experiment — not retained (2026-09-27)

**MEASURED (host command trace):** the existing play trace includes RFRCT and
PAINT with pattern bounds PR6=$0000 / PR7=$0070: one row, eight unzoomed bits.
**DERIVED:** a one-row pattern whose horizontal period divides sixteen can be
converted into one repeating opacity word and four colour words. The Amiga
blitter applies these from constant A/B registers with first/last masks and
C/D DMA. Transparent pixels and all four supported ROPs are preserved; other
geometries retain the scalar path. No device timing/ROM changes.

**MEASURED (paired synthetic A1200 batches):** eight 128x96 patterned rectangles
cost 13,029,032 E-ticks with this backend disabled and 16,927 with it enabled;
eight 48x24 patterned PAINT regions cost 3,275,467 versus 1,962,169 ticks.
Frequency 709,379 Hz; timing includes queued completion and PAINT border setup.
This is an isolated operation comparison, not a whole-game performance claim.
Evidence: `amiga/.run/pattern-fill-benchmark/gdb-out.log`. Host packed/planar
comparisons pass. The subsequent live run used this fast path zero times;
its total duration was essentially unchanged. The implementation was therefore
removed rather than using its synthetic speedup to justify additional code.
The final optimization targets per-pixel pattern selection and planar writes.


**DERIVED (retained graphics optimization):** a one-row, unzoomed-X pattern
whose width divides sixteen can select a pre-rotated repeating word using the
original phase modulo sixteen. This changes only host/native implementation
work, not HD63484 state or point order. Background-only screen pixels remain
valid across window-register changes, independently in each display buffer;
VRAM/base changes invalidate both.

**MEASURED (final validation):** 96 full/incremental composition pairs match on
AGA and ECS; twelve native moving-card frames match the independent host
compositor with zero differing pixels. Final replay matches all RAM, VRAM,
cropped pixels and AY writes (`tmp/graphics-final-comparison.log`). Controlled
DOT/circle reductions are 11.38%/21.37%; active-DMA composition reductions are
79.90% A1200 / 74.75% ECS. The live first-deal sample improves from 8.12 to 6.96
PAL seconds but uses a different hand. Full-game real-time performance remains
unmet; clock settings and FIFO semantics have not changed.


### Word-parallel planar PAINT (2026-09-27)

**DERIVED (implementation equivalence):** at four bits per pixel, comparing
four native plane words with the expanded PR0/PR1/edge colours computes sixteen
PAINT eligibility results together. The nibble phase includes ORG's dot offset.
Masking visited spans and retaining scalar seed/run order preserves the four-seed
overflow behavior. Masked short-span writes and existing wide-span blits leave
patterns, ROPs, final CP/DP and diagnostic work-limit failures unchanged.

**MEASURED:** packed/planar synthetic comparisons and full AGA/ECS replay agree
byte for byte; the replay has 7,008,979 instructions, 64,000,002 cycles, 7,831
IRQs and 30 AY writes. The live 24-input A1200 ledger completes without a
watchdog reset/error; deal PAINT mean falls from 8.026 to 3.414 ms. Remaining
polygon and flipped-copy costs keep the real-time gate open. See
[native-burst-plan.md](native-burst-plan.md#execution-a1-word-parallel-paint-2026-09-27)
for measurements and local evidence. This changes no hardware-model semantics.


**MEASURED (planar line follow-up):** the card's 13-point RPLL outline falls
from 42.402 to 11.677 ms with uniform-colour word-mask batches; all RPLL commands
in the deal average 6.541 ms versus 16.840 ms after the PAINT change alone.
**DERIVED:** batching the same Bresenham masks preserves the excluded endpoint,
phase, ROPs and XOR parity even under VRAM aliasing. Full AGA/ECS replay still
matches RAM/VRAM/frame/AY; the 24-input live ledger has no reset/error. No new
hardware behavior is inferred. See the A3 execution record in native-burst-plan.md.


**DERIVED (flipped-copy implementation):** for `$E300` with positive source
axes, destination `(x-i,y-j)` reads source `(sx+i,sy+j)`, a 180-degree rotation.
Non-overlapping rectangles can reorder their plane/row work and reverse bits in
whole planar words; overlap must retain sequential reads and writes. The new
path keeps coordinate/VRAM wrapping and negative source axes on the scalar
fallback, and changes no visible HD63484 registers or command timing contract.
**MEASURED:** matched 17×17 copies fall from 53.202 to 2.986 ms after the line
stage, with exact ECS/AGA replay and all 24 live inputs passing without reset or
error. The initial performance table's copy dimensions are corrected: its slow
records include both 17×17 and 11×11 copies. Details: native-burst-plan.md, A4.

**MEASURED (2026-09-27, A2 implementation equivalence).** Lazy planar curve
masks and bounded small-curve integer construction retain complete RAM, VRAM,
cropped frame and AY equality in both ECS and AGA native replay. Deal circles
average 1.591 ms (previous 4.541), ellipses 0.780 ms (previous 3.025). These are
implementation measurements, not new physical-chip findings. See A2 in
[native-burst-plan.md](native-burst-plan.md).

**MEASURED (2026-09-27, A5 equivalence):** inline command storage, short WPR/move
handlers and small empty-queue CPU fills retain exact native ECS/AGA RAM, VRAM,
frame and AY results. Snapshot word encoding, partial command continuation and
CCR abort of a spilled polygon are covered by synthetic tests. No FIFO hardware
semantics changed. Performance and the rejected eager-drain policy are recorded
in `native-burst-plan.md`, A5.

**MEASURED (2026-09-27, A6 equivalence):** direct CPU plane access preserves
queued-fill ordering and all native ECS/AGA replay outputs. It changes no
chip behavior, status flags, work counts or command ordering. The access view
is discarded before operations that may enqueue DMA; synthetic tests cover
that boundary and surfaces without this optional capability. See A6 in
`native-burst-plan.md`.

**DERIVED (Amiga presentation, Hardware Reference Manual section 2-5).**
COP1LC reloads at vertical blank without a COPJMP1 strobe. Native presentation
now publishes completed frames ahead of that edge and releases the old buffer
on the following VBI. This is a host display policy, not an ACRTC finding.

### Command-ring loop boundaries (2026-09-27)

**DERIVED (instruction research).** The ACRTC interrupt feeder at `$2E54`
compares consumer A1 against producer snapshot D1, tests WFR at `$2E58`, and
writes one postincremented word at `$2E5E`. `$2E62` compares A1 with ring-end
D0; the loop wraps from the A6-relative ring-start field when needed. Fields
relative to A6 are producer `-$77DA`, consumer `-$77D6`, ring start `-$7742`
and ring end `-$773E`. D0/D1 stay unchanged in this loop. The exit at `$2E7E`
stores A1. `$2E70` is the empty-ring control-register sequence; `$2E8C` is
the status-bit-7 branch. These boundaries are recorded for the authorized
whole-feed-loop experiment; no new hook is installed by this finding.

**MEASURED (native loop fusion).** Guarded assembly across the `$2E54–$2E6E`
ring tail preserves full ECS/AGA replay equality at the established 7,008,979
instruction boundary. The live non-I/O tail charges exact nominal cycles;
WFR and scheduling checks remain per word. The independent synthetic CPU
oracle covers ring wrap, backpressure and intermediate CCR/PC states. This
optimization supplies no new evidence about physical FIFO depth or RD byte
consumption; those model questions remain unchanged. See native-burst-plan.md.

**DERIVED (delay-loop boundaries, existing instruction research).** `$2442`
decrements D6 as a word; `$2444` branches back while nonzero, and `$2446` is
the continuation. Starting from D6.w=N (zero means 65,536 iterations), the
complete loop costs `14*N-2` original 68000 cycles. Partial execution may stop
at either instruction boundary. The final real subtraction determines all
five CCR bits; a zero initial count wraps rather than skipping the loop.
These boundaries are recorded for the authorized C1 experiment.


### Deferred-command ordering experiment (2026-09-27)

**MEASURED (native opt-in D1, 112M-cycle cold-boot/deal sample):** 9,508
completed commands were queued and executed, but none reached the idle worker.
Of 3,487 drains, 3,486 were control writes and one was a status read. Treating
all control writes as drawing barriers is too conservative for experimentation.

**DERIVED:** the already-audited `$2E70` empty-ring tail writes `$80` to CCR low
at `$2E74`, enabling CER IRQs; `$2EB6` also changes that IRQ-enable byte. These
writes do not alter display geometry, drawing parameters or the queued commands.
The refined experiment keeps them immediate without draining. It still drains
before display/format/abort changes and before explicit status/data reads.
Native cached tests may retain only WFE/WFR while deferring; CER and other status
tests must use the synchronizing shared read. This is an optional scheduling
experiment, not a new claim about physical ACRTC command latency.

**MEASURED (refined 112M-cycle sample):** avoiding CCR-low drains while routing
non-WFE/WFR tests through explicit shared reads still executes zero commands at
idle. All 3,487 drains now occur at status reads. **DERIVED:** the next FIFO IRQ's
CER test at `$2E30` forces the ordered prefix to finish before main-loop idle.
Under the plan's explicit observable-status barrier rule, the proposed idle
worker gets no graphics work in this sample. The experiment is reverted;
normal synchronous status/error behavior is unchanged.

### Card-back command identification and audio scheduling (2026-09-27)

**MEASURED (host trace and isolated reconstruction).** The 79-command / 260-word
sequence previously called a “card face” is the red lattice-and-club **card back**.
FIFO words through instruction 39,936,560 in `tmp/perf-hand-trace.csv`, following
the preceding command at 39,606,428, reproduce that artwork through the shared
HD63484 renderer. Local diagnostic `tmp/card-back-probe.{cpp,words,ppm,png}`
contains the extracted data and is not distributable. Counts: 10 WPR, 11 AMOVE,
21 RMOVE, 17 RFRCT, four CRCL, four ELPS, three RPLL and nine PAINT. The first
sequence spans command completion cycles 330,414,968–330,457,364. Twenty-six
matching mnemonic sequences occur in this captured hand at differing origins.
The four radius-seven circles and paints form rounded corners; the polylines,
ellipses and remaining fills construct the lattice, inset and central motif.
Later AGCPY commands copy rectangles; moving-card display also changes the DN3
window position, as recorded in the window investigations above.

**DERIVED (current code).** FIFO command completion synchronously calls drawing
from Hd63484::push/execute before nativeShortVideoWriteValue returns to guest
scheduling. Paula DMA and real Amiga interrupts can continue during this work,
but original sound-sequencer execution and AY envelope advancement depend on
board time. PaulaAy::vbi applies the latest saved registers/envelope; it cannot
apply future sound-stop writes that the guest has not executed. Board-time
stalls therefore stretch audible events even when Paula interrupts are timely.
This explains a mechanism consistent with the user's report; it is not a new
by-ear measurement or a capture of the reported particular sound.

**DERIVED (Hitachi manual, printed pp. 61–62; local `tmp/hd63484-um.txt`).** SR
reads report internal flags. CER (bit 7) reports a detected undefined command or
invalid parameter, while CED (bit 5) separately reports command end/availability;
WFR/WFE report the write FIFO. The documented CER read does not itself request
completion of drawing. The earlier D1 experiment's read barrier was a software
fidelity constraint, not an established hardware requirement to finish every
queued command on CER access. Its negative result applies to that constrained
implementation. A revised asynchronous model must account for actual FIFO,
error/completion visibility and ordering; it cannot simply force ready/no-error.


### Card-back corner dependencies (2026-09-27)

**MEASURED (fresh complete command catalog and scalar Surface proof):** the
79-command / 260-word card back writes 8,652 of its 8,800 bounding-box pixels on
an eligible background. Its four rounded-corner PAINTs read 68 distinct pixels
before the recipe defines them (116 reads total). Colours 1 and 15 are excluded
by the active fill predicate, changing coverage on those backgrounds. A single
unconditional stamp is therefore not equivalent. The approved cache design
checks these corner pixels and retains scalar fallback when they do not qualify;
see `card-back-blit-design.md`. This is model/ROM evidence, not a physical-chip
measurement. Nine of 11 fresh captured destinations pass this guard.


### Guarded card-cache native validation (2026-09-27)

**MEASURED:** the shared exact command matcher and approved 68-pixel guard
allow one masked four-plane blit for the full card back. The ordinary native
live24 sample makes nine hits, with zero resets/errors. Independent Agnus tests
and full-state replay match on both ECS and AGA. CCR-low interrupt-enable writes
must not be mistaken for pixel observations (they occur between card batches).
See `card-back-blit-design.md` for prefix semantics, measurements and limits.

**MEASURED:** cached complete feeds still average 136 ms in the calibrated
ledger experiment, versus 223 ms over uncached complete cards. The DMA job
itself takes 4.00–5.57 ms with AGA hires active. AY writes reach Paula within one
frame, but matched sound sequences still stretch during graphics. These results
establish a rendering improvement, not completion of the native timing work.


### Face-up card common background (2026-09-27)

**DERIVED (original instruction research):** `$1F69C` dispatches face-up card
artwork. It first calls the shared rounded-white-card producer `$2EC6` through
RAM stub `$42012`, then selects a suit and rank routine. `$3012` calls the same
white producer before constructing the red card back. `$4296` appends a
word-sized stack list to the command ring; `$4256` uses long stack slots.
New routine and jump-table entries are recorded in the symbol/entrypoint lists.

**MEASURED (original routine execution):** the local research harness executes
`$1F69C` for suits 1–4 and rank selectors 0–14, retaining original ROM code and
the producer ABI and two original RAM jump stubs while directing the output ring into scratch RAM. All 60 calls
return. Every result starts with the same 29-command rounded-white-card prefix.
Ranks 2–10 and 14 then copy the black/striped 40×54 inset from `(0,-1000)`;
ranks 11–13 instead copy complete 40×54 pictures from x=200/250/300 at y=-1000.
Selector 0 copies an 80×89 special image; selector 1 leaves the white card alone.
Thus the striped inset is not a universal face-up background, and is already
an AGCPY blit where used. Local evidence: `host/face_up_catalog_check.cpp` (`make harness-face-up-check`),
`tmp/faceup-catalog.words`, plus five matching face-up sequences from fresh
normal gameplay in `tmp/card-back-catalog/commands.txt`.

**MEASURED:** the 52 ordinary cards (selectors 2–14, four suits) draw four
17×17 rank/suit copies between the common white prefix and the inset AGCPY
(command 39, counting from one). **DERIVED:** stamping the complete generic
background at prefix completion would expose the inset earlier than the
original stream. The directly reusable optimization is the common white
prefix, preserving the existing inset and rank/suit copy order.


**MEASURED (implementation validation):** the shared 29-command white prefix
has exactly the full card-back coverage, with every covered pixel colour 15,
under both scalar and rectangle semantics. The existing mask doubles as its
white image; the same 68-pixel guard is required. Complete original-producer
outputs match the packed reference across both planar layouts. Paired native
A1200 service cost is 36.63 versus 13.93 ms per white base; full ECS/AGA replay
and live24 remain free of errors/resets. This is a rendering optimization, not
a change to ACRTC status, guest scheduling or sound sequencing. Details and
coverage limits: `card-back-blit-design.md`.


## Shuffle timing investigation (2026-09-28)

**DERIVED (original instruction inspection):** `$1DFA0` performs the sideways
deck shuffle: two passes of successive partial/full card copies through
`$1E08A`, `$1DD5A` and the ordinary graphics submission stub. The loop has no
explicit scheduler/frame delay. This differs from the moving display-window
callback `$1E45A`, scheduled by `$1E40C`. The current synchronous HD63484 model
reports command completion immediately; rendering cost is not emulated chip
execution time. Whether missing chip busy time accounts for the reported rushed
shuffle is under measurement; no gameplay delay has been patched in.

**MEASURED (unmodified instruction timing, current host model):** an ignored
instrumented host binary observes entry `$1DFA0` at cycle 98,730,264 and its
return `$1E088` at 99,055,204: **324,940 cycles / 40.6175 ms** at the configured
8 MHz. `$1E08A` executes 30 times. A partial copy can be submitted only 2,046
CPU cycles (0.256 ms) after the previous one. The caller at `$1AA20` selects
sound 9 immediately after the shuffle returns. This explains why simply making
the graphics backend quicker can expose an almost invisible shuffle; it is not
evidence that a frame-wait instruction was removed. The ordinary build and its
ROM execution were not changed by this probe. Local evidence:
`tmp/shuffle-probe.cpp`, `tmp/shuffle-probe.log`, `tmp/shuffle-host.jpg`.

**MEASURED (startup controls):** with the same timed cabinet input script,
original hardware-test boot and the fast diagnostic-bypass boot both emit 180 AY
register writes during the deal (41.36–42.95 board seconds), with identical
register/value order. Their first writes differ by eight CPU cycles; their last
writes coincide. Rapid automatic setup followed by coin at 11 s and Deal at 12 s
has the same relative sound-update cadence (12.40–13.99 s). Thus neither the
hardware-test bypass nor acknowledgement-driven setup explains the short
shuffle in these captures. Evidence: `tmp/shuffle-{original,fast,auto-deal}-*`.

**INFERRED (external comparison):** the supplied Finnish-machine footage shows
successive sideways deck shapes over roughly a second around 2–3 s, whereas the
host's complete copy loop takes two PAL frames. Exact physical oscillator and
command latency calibration remain open. Contact sheet: `tmp/shuffle-footage.jpg`;
source: `ref/footage/pokeri-200mk-2BI-eUaPCOc.mkv`.

**DERIVED (manual):** the HD63484 User's Manual, AGCPY-1 (printed p. 293), gives
execution cycles `((P+2)*A+10)*B+70`, with dimensions in drawing dots and P
selected by the operation mode. Section 2.2.1 defines interleaved memory access.
Our `tick()` is empty and `statusNow()` always asserts FIFO-ready/empty after
synchronous execution, so these command costs and finite FIFO backpressure
are absent. The conversion of command cycles to elapsed time must be verified
before implementation; a host CPU delay or arbitrary frame wait is not a model
of that hardware.

**PENDING DECISION:** shared command timing/FIFO backpressure was proposed on
2026-09-28. It changes guest-visible status/timing and therefore needs the separate
architectural decision required by `docs/card-back-blit-design.md`. Retain fast
planar/cache rendering, let the original guest observe device completion, and
validate shuffle/sound cadence against footage plus newly timed host/native
replays. No timing change has been enabled by this investigation.
