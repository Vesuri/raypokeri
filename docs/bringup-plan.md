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

Facts this plan stands on: `docs/rom-set.md` (ROM `$00000–$3FFFF`, RAM at `$40000` reached
through `a6`/`d7`, DUART `$C0000`, HD63484 `$F6000`, and the reset code's own RAM relocation).

## Architecture: one board, two CPUs

```
                 ┌──────────── src/board/ (portable C++, shared) ─────────────┐
                 │ Board: address decode → Duart68681, Hd63484, Ay38912,      │
                 │        IoPorts, Nvram   (each: read8/write8/tick/irq)     │
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
  sound and switch tests), plus any real-machine footage.

## Phase 0 — Musashi harness skeleton  *(small)*

- Vendor Musashi in `host/musashi/` with its licence (MIT; confirm on import).  Host build via the
  root `Makefile` (`make harness` → `build/pokeri-host`), clang, no SDL yet.
- Memory: the four chips from `rom/` at `$00000–$3FFFF` (read-only), RAM `$40000–$7FFFF`
  (sized generously until the extent is known), a 20-bit address mask.  Everything else goes to
  a **logging stub** that records `(PC, address, size, R/W, value)` and returns 0 on reads.
- Reset from the vector table and run N instructions or N ms of emulated time.  Report exceptions,
  privilege violations and unmapped accesses, with the PC history leading up to them.
- **Exit:** the harness runs from reset until it stalls, and the trace shows every device
  touched on the way.

## Phase 1 — Boot to idle on the harness  *(medium, research-heavy)*

Replace stubs with minimal models, in the order the boot hits them:

1. **Nvram** (`$D0000–$D7FFF`) as plain RAM, saved to `tmp/`.  Learn the integrity checks
   (magic `$AA55…`) so a cold board initialises its books instead of refusing to run.
2. **Duart68681**: registers, the counter/timer and its IRQ (level-2 autovector).  The IRQ rate
   follows from the timer values the code programs.  Input ports and output port bits are likely
   buttons and lamps.  The serial channels are logged only.
3. **Hd63484**: address register, status (FIFO ready, command end) and the FIFO, accepting and
   *logging* every command with its parameters.  No drawing yet; the goal is that the code never
   waits forever on it.
4. **Ay38912**: register writes logged.  Its I/O port may carry DIP switches.
5. **IoPorts** (`$E0000`, `$FB000`, …): logged with their read values configurable, until each
   is identified.

- Record every identification in `docs/rom-set.md`, with evidence tags, the moment it's found.
- Also pin down the **board clock** (from the DUART baud/timer settings and HD63484 timing) so
  the harness paces instructions against IRQs realistically.
- **Exit:** the program reaches its idle/attract loop and stays there, with a steady IRQ rate and
  no unmapped accesses.  Deliverables: the device map, the HD63484 command histogram (which
  commands and how often), and the AY register usage.

## Phase 2 — Reference output  *(medium–large; the HD63484 is the big one)*

- **Hd63484 drawing:** implement only the commands in the histogram, against a chunky `Surface`.
  Get the display configuration (resolution, bits per pixel, window/scroll) from the code's own
  register setup.  Find the palette hardware from the trace.  MAME's `hd63484.cpp` (BSD-3) is the
  behavioural reference for the command semantics.
- **Frame dumps:** PPM/PNG into `tmp/` (git-ignored).  An optional SDL window (`make harness SDL=1`)
  with key-to-button mapping, so a person can drive the game to any state.
- **Ay38912 → WAV:** tone, noise and envelope rendering (MAME `ay8910.cpp`, BSD-3, as reference).
- **Snapshots:** save and load full board state (CPU, RAM, NVRAM, devices), so a scenario
  (attract, a deal, a win, the double-up, the service menu) can be replayed deterministically.
  Input scripts drive the same scenario from a cold boot.
- **Exit:** the attract screen and a played hand render recognisably; the service-menu display
  test looks right.  This is the fidelity reference for everything after.

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
- **Its level-2 IRQ is virtual:** Amiga level 2 is CIA-A (the keyboard), so the game's handler
  never goes on a vector.  When the modelled DUART timer is due, the Amiga-side interrupt returns
  *into* the game's handler through a synthesised exception frame.  This happens only if the
  interrupted PC is in game code and the game's IPL is below 2; otherwise it is deferred to the
  next hook exit.  This is how a real IRQ lands, and it never calls game code from inside our own
  service routines.
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

Phases 0 → 1 → 2 run in sequence (each needs the previous one's trace).  Phase 3 can start as soon
as Phase 1 reaches idle, using the attract loop as its first scenario.  The Amiga loader and hook
handler (Phase 4) can be built against the harness-proven tables while Phase 2's drawing matures.

## Risks to watch

| Risk | Mitigation |
|---|---|
| Code paths the scenarios never reach hide relocations or device sites | Coverage map; loud guard buffer on the Amiga; grow the scenario suite (service menu, win paths) |
| Self-modifying code or code copied to RAM | The trace sees execution from RAM; handle those sites as they appear |
| HD63484 semantics wrong in both builds (the gate can't catch a shared bug) | ROM self-tests, MAME's device model as reference, real-machine footage |
| The game's SR/IPL use starves Amiga interrupts | Measure the masked durations in the harness; virtualise the IPL through the hook table only if needed |
| Display format beyond an A500's colours/resolution | Know it at Phase 2 exit, and decide with the user before Phase 5 |
