# Burst performance: card-deal profile and recovery plan

Measured 2026-09-27 on the A1200 profile (`6479fcf` plus the opt-in time ledger
below), FS-UAE cycle-exact 68020/AGA, 1 MB Chip + 8 MB Fast, PAL, warp host
execution, muted debug audio, K=1.5 boot / K=4 play. This document replaces the
*gameplay ordering* of the remaining steps in
[native-performance-plan.md](native-performance-plan.md). Its constraints,
approved decisions and fidelity gates still apply. Items marked **decision**
need explicit user approval before implementation.

## Findings

**MEASURED.** Quiet phases already run at about real time: board/wall 0.96–0.99
in the double/big/lamp intervals. Almost the whole deficit comes from drawing
bursts. In the reference ledger session (77.4 PAL s for 48.05 board-s after
ready), 39 runs of ten or more PAL frames in which board time does not advance
total **14.1 s**.

**MEASURED (host command stream, `tmp/perf-hand-events.txt`).** Each card that
lands is drawn procedurally: one card face is **79 commands / 260 FIFO words**
(10 WPR, 11 AMOVE, 21 RMOVE, 17 RFRCT, 4 CRCL r=7 plus PAINT for the rounded
corners, a 13-point and an 8-point RPLL outline, 4 ELPS plus PAINT pips, a
6-point RPLL plus PAINT). The ROM issues it in 5.3 ms of reference board time.
The deal as a whole is 1,184 commands / 3,801 words in about 2.1 board-s.

**MEASURED (native).** Natively one card face takes about **0.25–0.4 s**.
Throughout that time the guest gets ~0.1 ms per 20 ms frame, and board time
advances only when an occasional 10 ms tick falls due: about 2% of real time.
Nothing is presented, because presentation is paced by board time. That is the
visible freeze. The deal's first burst (INFERRED: the dealt stack; the same face sequence repeats five to six times) stalls for ~2.0 s; each
landing card then stalls for 0.22–0.36 s.

Inside stalls: command execution 60–65% of wall time, short-path C (feed and
PIA, excluding commands) ~8%, full-dispatch C ~5%, assembly hook mechanism and
unscoped interrupts ~20–25%.

### Per-command cost (deal phase, observer-corrected)

| Command (card use) | Native cost | Notes |
|---|---:|---|
| RPLL 13-point outline | ~43 ms (max 60 ms) | per-pixel Bresenham through `patterned()` |
| RPLL 8 / 6-point | 5.6–6.1 / 2.4 ms | |
| PAINT inside ELPS 9×4 pip | 13.6–14.3 ms | ~100 px region |
| PAINT inside CRCL r=7 corner | 8.3–8.9 ms | |
| AGCPY `$E300` (direction 3, 17×17) | 38 ms mean (22–54 ms) | 180° rotated copy, per-pixel fallback; INFERRED to be the card's inverted corner index |
| CRCL r=7 | 4.0–4.6 ms (35 ms first use) | ~44 points, cached outline, ~90 µs per point |
| ELPS 9×4 | 2.2–2.7 ms | |
| RFRCT (blitter fill) | 0.91 ms | CPU cost of setting up four plane blits |
| AGCPY `$E000`/`$EC00` (blitter) | 1.0 ms | same, shifted copy |
| AMOVE / RMOVE / WPR | 0.21 / 0.21 / 0.15 ms | parse/push/execute overhead only |

One card face therefore costs about 190 ms of commands (about 225 ms when its
curve outline misses the cache), plus about 37 ms of per-word feeding. The
synthetic benchmark agrees: a radius-24 circle costs ~110 µs per plotted point.
**MEASURED:** the same benchmark gives identical results with Zorro III instead
of Zorro II Fast RAM, so this is the emulated CPU executing this code (roughly
1,300–1,500 cycles per plotted point), not the memory configuration.

### Whole session and deal breakdown

Observer-corrected ledger shares of PAL wall time (the corrected observer
itself is 6–7% and absent from normal builds):

| Category | Deal phase (14.7 s wall, 8.0 board-s) | Ready → finish (77.4 s, 48.05 board-s) |
|---|---:|---:|
| Guest (measured original code, mostly the `$2442` delay loop) | 24.5% | 25.9% |
| Command execution (all paths) | 21.1% | 11.9% |
| Short-path C calls excluding commands | 4.6% | 5.7% |
| Full dispatcher C (incl. presentation, ticks, guard) | 15.4% | 19.0% |
| Masked dispatcher C prologue (~114–123 µs per call) | 4.4% | 5.0% |
| Residual: assembly hook entry/exit, exceptions, unscoped IRQs | 22.5% | 25.2% |

**DERIVED.** Excluding C work, a hook entry costs about 87 µs of residual,
about twice the isolated 48 µs status benchmark (instruction cache misses,
interrupts landing in the path). A feed word also costs ~43 µs in C before any
command executes.

**MEASURED.** Composition is not currently a CPU bottleneck on AGA: presentation
costs 0.12 s and blitter waits inside commands 0.04 s over the whole deal phase.
The blitter composes asynchronously. Its Chip-bus contention remains; it
matters more on ECS.

**MEASURED.** 12% of frames in the unmeasured baseline (731 of 5,877) miss the
VBI swap window, because the swap strobes `COPJMP1` and must happen in scanlines
0–7. Any extra VBI work pushes more frames past it. The first ledger attempt,
which sampled before the swap test, deferred every swap.

**MEASURED (quiet phases).** ~215 main-loop passes per board-second versus about
70 in the reference: each pass runs the 7,424-iteration delay at native speed.
There are ~14 hook entries per pass, so about two thirds of quiet-phase hook
entries and guest time come from passes the original would not have executed.
Under the approved clock this is harmless, but it is also wasted work.

## Budget

Under the approved option C (K=4, one-frame credit window), real time requires
services ≤ ~15 ms per 20 ms PAL frame. The guest needs ≥ 5 ms per frame to earn
its credit. No single service may span much more than a frame: wall time beyond
the credit window is discarded (254 whole frames in this session). For a card face to feel like the original (visible within 2–3 frames,
board time continuing), its entire service cost should be **≤ 40 ms**: roughly
≤ 300 µs per command and ≤ 40 µs per FIFO word, including the hook. Today it is
~250 ms.

## Plan

Stages are ordered by expected gain against fidelity risk. Each keeps the
shared HD63484 semantics, exact replay RAM/VRAM/frame/AY equality, the 68000
arithmetic audit and the ECS path. The ledger capture below is the acceptance
measure for every stage: deal-phase board/wall, stall seconds, the per-command
table and per-word feed overhead.

### A. Faster drawing primitives (no semantic change; largest single gain)

A1. **Word-parallel PAINT.** Replace per-pixel eligibility with 16-pixel planar
word tests. For each boundary colour (PR00, PR01, edge), pre-expand its
four-nibble phase pattern into four plane masks. Then `pixel != colour` for 16
pixels is `OR over p of (plane_p XOR mask_p)`, and eligibility is a handful of
word operations ANDed with a visited-span mask for the row. Left/right extent
and the seed runs of rows y±1 come from bit scans. Keep exactly the current span
order, four-seed stack and overflow failure, visited spans, final CP and work
accounting. Count the scalar-equivalent eligibility evaluations arithmetically
so the diagnostic work limit fires identically. Fill short opaque spans with
masked CPU word writes (no blit set-up, no synchronization). Keep the blitter
only for wide spans, and scalar code for patterned or transparent cases. Use
fixed storage instead of per-PAINT `std::vector` allocations. *Estimate:*
0.3–1 ms per card PAINT instead of 8–14 ms.

A2. **Curve stamps.** Rasterize each cached outline into 1-bpp row masks per
16-pixel alignment, lazily on first use and bounded like the outline cache
(only valid when the outline has no duplicates, which the cache already
guarantees). When the pattern is uniform along the curve, draw by applying the
masks per row and plane with the colour and ROP. Other patterns keep the
per-point path. Also cut the first-use miss (33 ms). *Estimate:* 0.2–0.4 ms per
r=7 circle.

A3. **Planar line primitive.** Set up each segment once: address, bit mask,
plane pointers, uniform colour, ROP. Step the Bresenham with incremental address
and mask updates, with no per-pixel calls, virtual dispatch, multiplication or
synchronization. Separately evaluate the Amiga blitter line mode for long
segments. Its error term is twice the model's (`4dy−2dx`, step while ≥ 0), so it
should match exactly, but that must be proven exhaustively against `line()`:
all octants, lengths, the excluded endpoint, repeating 16-bit texture phase,
COL modes and ROP minterms. Colour words with differing nibbles stay on the CPU.
*Estimate:* 3–5 ms (CPU) or ~1–2 ms (blitter) for the 13-point outline instead
of 43 ms.

A4. **Flipped AGCPY (direction 3 and other supported flips).** For
non-overlapping rectangles, reverse each row with a compile-time 256-entry
bit-reverse table plus shifts on whole plane words, then apply masks and ROP.
Overlapping cases keep the sequential fallback. Optionally cache rotated glyphs
as Chip RAM tiles and blit them. *Estimate:* ≤ 1 ms instead of 40 ms.

A5. **Lean command front end and cheaper blit set-up.** Store pending words in
a fixed array rather than `std::vector`; give WPR/AMOVE/RMOVE dedicated fast
handlers; move 64-bit command counters off the hot path. For fills and copies,
write the prepared register block straight to the blitter when the queue is idle.
Use masked CPU word fills when a rectangle is only a few words per plane (most
RFRCT borders). *Estimate:* ~0.2 ms per RFRCT, ~30–50 µs per trivial command.

A6. **One synchronization per command.** Hoist the queued-blit check out of
`plot4`/`pixel4` and devirtualize the planar calls inside drawing loops.

Gate: packed host oracle and planar tests for each primitive (all depths,
origins, ROPs, COL modes, alignments), independent Musashi-free pixel oracles,
exact native replay, and a synthetic *card-face benchmark*. That benchmark
replays the shape of the command sequence above with synthetic parameters and
no ROM data, inside `native-benchmark`, before and after each change.

### B. Presentation (small and independent)

B1. **Present on wall-clock cadence** whenever VRAM or display registers changed,
even while board time is stalled, throttled to every second VBI during bursts
and skipped if the blitter queue is already long. The real ACRTC displays VRAM
live, so showing progress is more faithful than a frozen frame.

B2. **Swap without the Copper strobe.** Write `COP1LC` at any time; the Copper
reloads it at the next frame. Track buffer ownership by frame number, so no
finished frame waits for the scanline 0–7 window (12% of frames today).

B3. **Damage rectangles instead of full recomposition** after VRAM writes:
per-buffer row/column bounds mapped through the three screens and the window.
Put display blits in a lower-priority queue behind drawing blits, in bounded
chunks, so CPU drawing never waits for a whole-screen composition. This mostly
reduces Chip-bus contention now and is required for ECS later.

B4. *(later, ECS)* **Zero-copy display**: point bitplanes straight at the planar
VRAM rows (Copper-reloaded per screen, modulo from MW), and compose only the rows
the window covers. VRAM pitch (76 bytes) permits FMODE 1 at most, never 3.

### C. Fewer hook entries (**decisions**)

C1. **Delay-loop idle point** at `$2442/$2444` (`subq.w #1,d6 / bne`,
D6=`$1D00`). One guarded hook produces the exact D6 and CCR results
(X=N=V=C=0, Z=1) and PC `$2446`. It charges the reference 14n−2 cycles and splits
at pending board events, so interrupts are delivered between iterations as in
the reference. It then waits for wall time to catch up, running deferred work
meanwhile. Expected effect: the main loop runs at the reference pass rate,
about a third of the current quiet-phase pass count, removing most per-pass
hooks. Guest spin time becomes service budget (≈15 → ≈19 ms per frame).

C2. **Feed-loop hook** at `$2E54–$2E6E`: one exception drains the ROM's ring
into the FIFO. It retains the actual per-word WFR test, the exact A1/D0/D1/CCR
and exit PC, and a safe boundary after any word that makes an interrupt, tick
or fault due. That is ~3,800 → ~200 exceptions per deal. This extends the
approved three-instruction fusion to the whole loop.

C3. **Lean FIFO word path**: assemble bytes and words without per-byte generic
`write8`, and call the shared model once per completed command.

C4. **Per-entry cost**: find out why the masked dispatcher C prologue costs
~115–124 µs, and trim the clock boundary. Only after C1–C3, since those remove
most entries.

### D. Scheduling and timing (**decisions**, after A and C1)

D1. **Deferred ACRTC execution**: queue completed commands and execute them at
idle points within a per-frame budget. Synchronize first before any read-type
command (RD, RPR, RPTN, DRD), any status the ROM could observe changing, and any
display-register change that presentation would expose. The model always reports
ready, so this is invisible to the guest; only the displayed VRAM lags, as it
does on the real chip.

D2. **Credit window**: allow banking more than one frame of guest credit, e.g.
two or three frames, so a short burst does not permanently lose board time. This
changes the approved contract's bounded window.

### Order and expected outcome

A1 → A3 → A4 → A2 → A5/A6 (each measured separately) → B1/B2 → C2 → C1 →
C3 → B3 → D as measurements dictate. *Estimates, not results:* after A a card
face should cost ~20–30 ms of commands instead of ~190 ms. After C2/C3 feeding
should cost ~5–10 ms instead of ~37 ms. That puts a face at 1–2 frames, and the
session's 14.1 s of stalls should mostly disappear. Real-time acceptance remains
the existing gate: sustained board time within 5% of PAL time over a heavy
winning/doubling hand, with animation deadlines reported.

## Measurement method (time ledger)

`make -C amiga TIME_LEDGER=1` builds a separate diagnostic executable; normal
builds contain no ledger code (their code, data and BSS are identical). It
reserves a free CIA timer through its resource (CIA-A timer B here), runs it
continuously at the E-clock with its interrupt disabled, and extends it past
the 92 ms wrap at every scope and once per VBI. Scopes are inclusive and
recorded by `[enclosing kind][kind]`: full dispatch, masked prologue, prepared
hook execution, short-path C calls, each FIFO write (a *command* when it
completes one, attributed to its opcode group), presentation, board ticks,
guard, blitter waits and queue back-pressure. Snapshots are taken at every
scripted input mark and at the finish. Per-VBI records are taken **after** the
swap test. Commands ≥ 2 ms keep their first eight words.

CIA-B TOD was tried first and rejected: on this Kickstart it restarts roughly
every frame. Each timestamp costs ~13.9 µs (2,517 E-ticks per 256 reads). The
analyzer removes one timestamp per scope from the scope itself and two per nested
scope from each ancestor, and reports the total observer cost. Interrupts that
land inside a scope count in it; interrupts during guest execution fall into the
residual. The measured sessions differ in dealt hands, so compare phases, not
whole-session totals.

```
cd amiga && . ./env.sh && make clean && make TIME_LEDGER=1   # copy out/ aside, then rebuild normally
# isolated run dir with rom/, native-live (480000000), native-measure, native-test-inputs
POKERI_REPLAY=0 POKERI_RUN_DIR=.run/perf-ledger GDBSCRIPT=ledger.gdb \
  POKERI_EXE=<ledger exe> POKERI_ELF=<ledger elf> EXTRA_ARGS=--warp_mode=1 ./diag_run.sh 900
python3 host/native_ledger.py --log amiga/.run/perf-ledger/gdb-out.log \
  [--frame-window CYCLE0 CYCLE1] [--slow-window CYCLE0 CYCLE1]
```

Evidence (local, ignored): `amiga/.run/perf-ledger/`, `tmp/ledger-*.bin`,
`tmp/perf/`. The Z2/Z3 comparison used `amiga/.run/perf-bench-{z2,z3}`.

## Execution: A1 word-parallel PAINT (2026-09-27)

**DERIVED (implementation).** Planar eligibility compares three colour words
against four plane words for 16 pixels together, including the origin's nibble
phase and visited-span mask. Nibble scans advance over eligible/ineligible runs
and charge the exact scalar work count. The four-seed stack, visit ordering,
pattern fallback, coordinate guard, work-limit failure and final CP stay intact.
Short opaque spans use masked CPU writes; spans of at least 16 pixels retain
the existing blitter path. The seed stack and first 128 visited spans use inline
storage; unusually large fills may spill to the existing vector, rather than
introducing a new size limit. Queued writes finish before CPU plane access.

**MEASURED (synthetic A1200, ordinary build).** The explicit benchmark now
includes an independently constructed 79-command / 260-word card workload with
the measured command mix, no game artwork or ROM parameters. Clearing is outside
the timer; queued completion is inside it. At 709,379 ticks/s:

| Workload | Before A1 | After A1 | Reduction |
|---|---:|---:|---:|
| First synthetic face (cold outlines) | 441,598 ticks (622.5 ms) | 177,955 (250.9 ms) | 59.7% |
| Repeated face (warm outlines) | 418,510 ticks (590.0 ms) | 154,161 (217.3 ms) | 63.2% |
| Eight 48×24 PAINTs, including border setup | 1,923,044 ticks (2,710.9 ms) | 415,394 (585.6 ms) | 78.4% |

These synthetic faces are a repeatable command mix, not an estimate of a real
card's absolute duration. The live ledger completed all 24 inputs at 480,000,000 board cycles, no error
or watchdog reset. Deal-phase PAINT is **8.026 → 3.414 ms** per call (101 calls
in each run, **57.5% less**, 0.811 → 0.345 s). RPLL remains 17.026 → 16.840 ms;
AGCPY group 56 increases from 37 to 65 calls with the different hand, so its
0.653 → 0.921 s total is not a regression measurement. Deal board/wall is
0.545 → 0.550 (14.68 → 14.55 s for 8.00 board-s): the real-time gate remains
open. Recomputing both ledgers with a common criterion of at least ten
consecutive VBI intervals without board-cycle advance gives 41 runs / 14.47 s
before and 27 / 8.08 s after, over 77.4 / 66.48 s post-ready wall time. Different
hands prevent attributing that whole-session change solely to PAINT.

**MEASURED (correctness).** Host harness/platform/native checks pass, including
3,072 extra packed-versus-planar PAINT cases and 2,048 independent span cases.
Seed overflow, coordinate wrap, inline-storage spill and opaque/transparent
work-limit failures match the scalar result, including partial VRAM and CP/DP.
Both A1200/AGA and A500+/ECS replay match all 262,144 RAM bytes, 524,288 VRAM
bytes, 163,008 cropped pixels and 30 AY writes at 7,008,979 instructions,
64,000,002 cycles and 7,831 IRQs. Normal-build text/rodata/data/BSS are identical
before and after toggling the ledger build. A1 is accepted; A3 is next.

Local evidence:
`amiga/.run/burst-a1-{before,word-after,replay-aga,replay-ecs,ledger}` and
`tmp/perf/burst-a1-*`.

No new C1/C2/D1/D2 approval has been given; those changes remain pending.
