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

**DERIVED: build the native visible frame at 576 × 292 pixels, 4 bits per pixel
(16 colour indices).** Its backing row is **608 pixels / 152 words / 304 bytes**,
not 576 pixels. The full scan, including blanking, is equivalent to 768 × 304
pixel periods; it is not the size of the visible frame. Pixel aspect ratio and
physical CRT overscan are not established by these registers.

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
- HDR width = `$47 + 1` = 72 memory cycles = **576 visible pixels**, or 144 words.
- MWR = `$098` = 152 words = **608 pixels**; eight words / 32 pixels of each
  backing row lie beyond the visible background width.

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
| Window / 3 | `$D8–DF` | `$04B00` (`$09600`) | nominal x=0–87, y=44–143; **88 × 100** | 152 words |

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

Window offsets: HWS=HDS=`$09`, so x=0; VWS=`$32`, VDS=`$06`, so y=50−6=44.
The width follows HWW+1 = 11 cycles and the height follows VWW = 100 rasters.
**Unresolved manual conflict:** §5.6 (p. 74) requires even horizontal widths in
interleaved/superimposed access. The background's 72 cycles obey this; the window's
11 do not. Preserve the programmed nominal **88-pixel** window width for the
reference configuration and flag its last-fetch/right-edge behavior for later
hardware/frame comparison; do not silently round it to 80 or 96. This does not
make the 576 × 292 output size ambiguous.

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
