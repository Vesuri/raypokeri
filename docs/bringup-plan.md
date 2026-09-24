# Bring-up plan

**Goal:** the original 68008 program runs unmodified on the Amiga's 68000 with only its relocations
and hardware-access sites patched.  The board's devices are reimplemented behind those sites.
**No disassembly-to-C, no transliteration.**  Ghidra is a research aid only; nothing is generated
from it.

**Strategy:** build a host-side board emulator on **Musashi** first, and use it for everything that
is slow or blind on the Amiga: finding the devices, writing their models, discovering the
relocations and access sites, and serving as the reference.  Then run the same code on the Amiga,
with the same device models behind patched access sites, and gate it against the harness by
comparing RAM state.

Facts this plan stands on: `docs/rom-set.md` (the memory map, chip order, device identifications
and boot findings) and `docs/hardware.md` (the board photo and articles).

## Status (2026-09-24)

| Phase | State |
|---|---|
| 0 Harness skeleton | ✅ Done (`host/`, `make harness`, `make harness-check`) |
| 1 Boot to idle | ✅ **Effectively met on the 512 KB video path** (the target), with open items (below) |
| 2 Reference output | ⏭ **Next** |
| 3–6 | Not started |

**Decisions waiting on the user**
1. ~~Video memory configuration~~: **resolved, 512 KB** (user, 2026-09-24; now the harness
   default).  The 2 MB path, which also programs a RAMDAC at `$E0000`, stays available with
   `--video-kwords 1024` for comparison.
2. ~~HD63484 documentation~~: **resolved.**  We have both Hitachi's *HD63484 ACRTC User's Manual*
   (November 1984, the authoritative reference) and the *ACRTC Application Note* (April 1986,
   worked examples), local and git-ignored in `ref/manuals/`, with searchable OCR text in `tmp/`.

## Architecture: one board, two CPUs

```
                 ┌──────────── src/board/ (portable C++, shared) ─────────────┐
                 │ Board: address decode → Pia6821 ×3, Acia6850 ×3, Hd63484,  │
                 │        Ay38912 (behind PIA 0), Nvram, Ramdac (to come)     │
                 │        each: read8/write8/tick/irq                         │
                 │ Hd63484 → Surface interface   Ay38912 → Tone interface    │
                 └───────────▲──────────────────────────────────▲─────────────┘
   host/ (Musashi harness)   │                  Amiga            │
   m68k_read/write_memory ───┘       Line-A hook at each access ─┘
   Surface = chunky framebuffer       Surface = bitplanes (+ blitter)
   Tone    = sample renderer (WAV)    Tone    = Paula
```

- **The device models are written once.**  Debugged at host speed, with tracing, then linked into
  the Amiga build unchanged.  Only the backends (`Surface`, `Tone`, input) are per platform.
- **The CPU side is exact on both.**  Musashi is a well-proven 68000 core, and on the Amiga the
  real 68000 runs the real bytes.  So with the same inputs, **work RAM after N interrupts must be
  byte-identical** between the harness and the Amiga.  That's the strongest regression gate
  available, and it's free.
- The harness is exact on the CPU and **only as good as our device models** on the devices.
  External checks on the models: the ROM's own self-tests (the service menu has display, lamp,
  sound and switch tests), plus real-machine footage (`docs/visual-reference.md`).

## Phase 0 — Musashi harness skeleton  ✅

Musashi vendored in `host/musashi/` (licence kept).  ROM read-only at `$00000–$3FFFF` in address
order 30/38/34/PARA, RAM `$40000–$7FFFF`, a 20-bit mask, loud stops on unknown accesses, traces,
coverage, PC history and CPU-exception capture.

## Phase 1 — Boot to idle on the harness  ✅ (on the 512 KB path)

What the boot needed, in the order it hit them (details and evidence in `docs/rom-set.md`):

1. **Chip order.**  The ROM's own module checksum passes only with 30, 38, 34.
2. **Three 6821 PIAs** (`$FB014–$FB01F`): register tests, the peripheral reset, and three
   periodic edge sources (system tick, input scan, watchdog).  Their rates are **hypotheses**
   (100 Hz / 50 Hz / 400 ms, behind `--system-hz` etc.).  The board has HC4060 dividers from an
   unreadable resonator (`docs/hardware.md`).
3. **Three 6850 ACIAs** (`$FB002–$FB00B`): minimal status.
4. **The AY-3-8912 behind PIA 0**: the ROM's sound self-test reads back through it.
5. **The HD63484** (`$F6000/$F6002`): the 8-bit bus, status, FIFOs, WPR/RPR/ORG and WT/RD/MOD.
   The ROM's video-RAM tests pass, and drawing commands are parsed and counted.
6. **The video-memory probe**, which picks the 512 KB or 2 MB path.

Result on the 512 KB path: a steady interrupt-driven loop.  318 M instructions, 39,478
interrupts, no unmapped access, and the full drawing-command mix (the histogram is in the
`*-devices.txt` capture).

**Open items carried into Phase 2**, each to be settled by what the frames show or by the code:
- **Is the loop really attract/idle?**  The run never touched the battery RAM (`$D0000`) or
  `$C0000`.  An idle game would be expected to consult its books, so this may be an
  error/attention screen instead (e.g. cash memory missing or uninitialised).  The first
  rendered frames answer this.
- `$C0000` (the "DUART") is not on the processor board, and it isn't accessed on this path.
- RAM extent: the board has 16 KB (`$40000–$43FFF`), but the code writes `$47000` (`$13EC`).
  Decide between a latch and mirrored RAM before narrowing the harness RAM.
- The tick/scan/watchdog rates and the CPU clock are unmeasured.  They affect pacing, not logic.

## Phase 2 — Reference output  ⏭ next  *(medium–large; the HD63484 is the big one)*

In this order:

1. ✅ **Verify the HD63484 model against the documentation.**  Done: status bits, CCR interrupt
   enables, GBM = 4 bpp, all command lengths, the bus protocol, the RWP layout, MOD with MASK.
   One ROM-vs-manual conflict (WPTN's count) is recorded in `docs/rom-set.md`.  Remaining details
   (drawing registers and command semantics) are read from the manual as each command is built: the command-length table,
   the register map (drawing parameters CL0/CL1/CCMP/EDG/MASK, the pattern and area registers,
   OMR/DCR/CCR bits), and which CCR bits enable which interrupts.  Fix the model and extend
   `make harness-check`.
2. **Decode the display configuration the ROM programs**: OMR `$CD28` (colour depth, access
   mode), DCR `$FF3F` (which screens are enabled), the timing registers `$82–$9D`, and the screen
   start-address and memory-width registers `$C0–$DF`.  The four RWP display-select banks the ROM
   uses (split at word addresses `$2300`/`$4B00`/`$B000`) line up with the base, upper, lower
   and window screens.  Output: resolution, bits per pixel and screen layout, written into
   `docs/rom-set.md`.
3. **Implement the drawing commands the histogram shows**, and only those: AMOVE/RMOVE,
   RLINE, APLL/RPLL, CRCL, ELPS, RARC/REARC, RFRCT, PAINT, DOT, WPTN/PTN, AGCPY, CLR.  Include
   the colour, pattern, area and logical-operation modes they use.  Each gets a synthetic test.
   MAME's `hd63484.cpp` (BSD-3) and the manual are the semantic references.
4. **Compose and dump frames**: build each visible frame from the screen registers into a
   chunky image, written as PPM/PNG into `tmp/` every N frames.  **Palette**: on the 512 KB board
   the palette hardware is still unknown (the 2 MB path's RAMDAC table is a likely match to test), so start with a clearly marked placeholder palette
   (distinct colours per index) and work out the real one from the code and the footage.
5. **Look at the frames** against `docs/visual-reference.md`.  This settles whether the loop is
   attract or an error screen, and shows what the program is waiting for.
6. **Inputs and a window**: identify the button, coin and service inputs on the PIA ports from
   the code, then add an optional SDL window (`make harness SDL=1`) with keys for them.  Insert
   coins and play a hand.  Model the battery RAM behaviour the code expects as it starts using it.
7. **AY → WAV**: tone, noise and envelope rendering (MAME `ay8910.cpp`, BSD-3, as reference).
   Compare by ear with the footage's audio.
8. **Snapshots and scenarios**: save and restore full board state; input scripts that drive
   attract, a deal, a win, the double-up and the service menu from a cold boot.

- **Exit:** the attract screen and a played hand render recognisably against the Finnish footage,
  the service-menu display test looks right, and the scenario scripts replay deterministically.
  This is the fidelity reference for everything after.

## Phase 3 — Relocation and hook tables, derived by running  *(medium)*

The harness finds the patches instead of us reading them out of a disassembly:

- **ROM relocation:** load the ROM at a *different* base in the harness, leave `$00000–$3FFFF`
  unmapped, and run the scenario suite.  Every fetch or read that lands in the old range is a
  missed relocation, and the trace gives its PC and the operand.  This is backed up by a static
  scan for `abs.l` operands and pointer tables inside the ROM range, restricted to bytes the
  coverage map shows as executed code (a pointer inside data shows up when it is dereferenced).
  Run at two different bases and diff the traces: a value that differs by exactly the base delta is
  a relocated pointer, and anything else is not.
- **RAM relocation:** set `d7` (the reset code's own mechanism) to the Amiga RAM offset, and treat
  every access that still lands in the original `$40000` range as a miss, as above.
- **Hardware-access sites:** every PC that touched device space in the trace.  Each site records
  the device register, size, direction, and the effective-address form it used.
- **Coverage:** the scenario suite's union of executed PCs is the completeness measure.  A device
  access or relocation outside the covered code is a known gap and gets listed explicitly.
- **Output:** committed tables of **offsets and operation descriptors only, with no ROM bytes**
  (in the manner of Rescue on Fractalus's `rof_data_recipe.h`).  They are valid for exactly the
  checksummed ROM set, which the loader verifies first.
- **Exit:** the harness runs the whole scenario suite with the ROM at an arbitrary base, RAM at an
  arbitrary base, and devices reached *only* through the hook table: every other device address
  is unmapped and loud.  That is the Amiga configuration, proven on the host.

## Phase 4 — The original code on the Amiga, booting to idle  *(medium)*

- **Loader:** read the four chips from disk (WHDLoad later), verify the checksums, place the 256 KB
  ROM image and the RAM, apply the relocation table, and patch every access site with a Line-A
  opcode (`$Axxx` = hook index).  Line-A is 2 bytes and every device access is at least 4, so a
  site always fits.  The handler returns past the original instruction.
- **Hook handler:** decodes the recorded operation, calls the shared `Board`, writes the read
  result into the right register, and sets the CCR as the original instruction would have
  (N/Z/V/C for moves and compares).  A shared routine is proven for each EA form used.
- **Device-base guard:** the `movea.l #device` immediates are relocated to a guard buffer in memory
  that is checksummed every frame, so an unhooked write fails loudly instead of corrupting chip
  RAM.  (Without this, `$C0000–$FFFFF` on an A500 *is* chip RAM.)
- **CPU context:** the program runs in supervisor mode (it uses `move usp`, `ori #$700,sr`).  It
  runs under full takeover, as Vette does.  For the duration we own the TRAP #0–#15, Line-A and
  exception vectors, and put the OS's back on exit.
- **Its interrupts are virtual.**  The game takes level 5 with device vectors (`$40` HD63484,
  `$43`/`$46` PIA sources) and has handlers for levels 2 and 7.  None of them goes on an Amiga
  vector: Amiga level 2 is CIA-A (the keyboard).  When a modelled source is due, the Amiga-side
  interrupt returns *into* the game's handler through a synthesised exception frame with the
  right vector.  This happens only if the interrupted PC is in game code and the game's IPL is
  below the level; otherwise it is deferred to the next hook exit.  This is how a real IRQ lands,
  and it never calls game code from inside our own service routines.
- **Exit:** the Amiga boots to the idle loop, and work RAM matches the harness byte for byte at
  the same interrupt count.  A `diag_run.sh` probe dumps it and a host tool diffs it.

## Phase 5 — Amiga devices for real  *(large)*

- **Video:** `Surface` on bitplanes, using the blitter for the HD63484 fills, copies and lines
  where the command histogram says it pays.  Depth and resolution are chosen once the real
  display format is known (an A500 gives 32 colours lowres or 16 hires; a scaling or colour
  decision may land here).  Gate: state-paired frame compare against the harness's frames.
- **Audio:** `Tone` on Paula, with square loops per AY channel and noise and envelope in
  software, following the Rescue on Fractalus POKEY→Paula precedent.  Gate: register-stream
  compare against the harness.
- **Input and lamps:** keyboard/joystick to buttons, lamps shown in the UI.  **NVRAM:** saved to disk.
- **Performance:** the 68008's 8-bit bus makes the A500 faster at *running the code*.  The risk is
  the HD63484 drawing cost, so measure it on real command streams before optimising.

## Phase 6 — Game scope and release  *(later, user decisions)*

The coin/credit model, how much of the operator side (books, hopper, service menus) stays
reachable, the WHDLoad install, and the release packaging.

## Order of work and parallelism

Phases 0 → 1 → 2 run in sequence (each needs the previous one's trace).  Phase 3 can start now,
in parallel with Phase 2, using the steady 512 KB boot as its first scenario: relocation and
access-site discovery don't need drawing.  The Amiga loader and hook handler (Phase 4) can be
built against the harness-proven tables while Phase 2's drawing matures.  Within Phase 2, steps 1–2
come first; steps 6–8 can overlap with 3–5 once the first frames exist.

## Risks to watch

| Risk | Mitigation |
|---|---|
| Code paths the scenarios never reach hide relocations or device sites | Coverage map; loud guard buffer on the Amiga; grow the scenario suite (service menu, win paths) |
| Self-modifying code or code copied to RAM | The trace sees execution from RAM; handle those sites as they appear |
| HD63484 semantics wrong in both builds (the gate can't catch a shared bug) | ROM self-tests, MAME's device model as reference, real-machine footage |
| The game's SR/IPL use starves Amiga interrupts | Measure the masked durations in the harness; virtualise the IPL through the hook table only if needed |
| Display format beyond an A500's colours/resolution | Known after Phase 2 step 2; decide with the user before Phase 5 |
| The steady loop is an error screen, not attract | The first frames (Phase 2 step 5) show it; then model what it's waiting for |
| Pacing hypotheses (tick rates, clock) are wrong | Logic doesn't depend on them; compare animation timing with the footage before Phase 5 |
