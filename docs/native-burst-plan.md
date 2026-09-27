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
| AGCPY `$E300` (direction 3) | ~53.7 ms (17×17) / ~22.8 ms (11×11) | 180° rotated copy, per-pixel fallback; INFERRED to be the card's inverted corner index |
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


## Execution: A3 planar lines (2026-09-27)

**DERIVED (implementation).** Opaque, uniform-colour lines on word-aligned
planar rows use incremental Bresenham address/bit steps and bounded batches of
128 word masks. Each batch selects set/clear/XOR once per plane. There are no
per-point virtual calls, synchronizations or pixel-address multiplications.
The endpoint is excluded, phase and scalar work charges are retained, repeated
physical pixels preserve XOR parity, and diagnostic partial failures fall back
to scalar traversal. Other colours, patterns and pitches retain the old path;
axis-aligned blitter lines remain in place.

**MEASURED (code-generation check).** The initial CPU version still emitted a
captured-lambda call per changed word. In the live deal its 13-point outline
cost 22.135 ms; batching removes that repeated call and reduces it to 11.677 ms.
These are raw inclusive slow-command means for nine matching 13-point outlines;
the PAINT-only baseline is 42.402 ms (**72.5% reduction**). The estimate of
3–5 ms is not yet met. The measured retained change is useful without claiming
that the optional blitter-line work is complete.

| Deal measurement | A1 only | A1 + A3 |
|---|---:|---:|
| RPLL, 27 commands, observer-corrected mean | 16.840 ms | 6.541 ms |
| RPLL total | 0.455 s | 0.177 s |
| Deal wall time / 8.00 board-s | 14.55 s | 14.23 s |
| Board/wall | 0.550 | 0.562 |
| Post-ready stalls (same ≥10-interval definition) | 27 / 8.08 s | 26 / 7.73 s |
| Post-ready wall / 48.04 board-s | 66.48 s | 66.06 s |

The controlled synthetic warm face changes only 217.318 → 214.479 ms because
its polygon segments are short. The real-game command profile determines the
priority. Flipped AGCPY remains the largest drawing cost; A4 is next. Timing,
credit, guest hooks and presentation policy are unchanged.

**MEASURED (validation).** All host harness/platform/native checks pass,
including 278,784 independent line cases across octants, alignments, ROPs,
physical aliasing and address wrap, plus full-VRAM comparisons of long
polylines failing at the work limit. Full AGA/ECS replay agrees in all RAM,
VRAM, cropped pixels and 30 AY writes at the established 7,008,979-instruction
boundary. The 24-input A1200 ledger finishes at 480,000,000 cycles with no error
or watchdog reset. Ordinary text/rodata/data/BSS match after toggling the ledger.
Evidence: `amiga/.run/burst-a3b-*`, `tmp/burst-a3b-*`; the initial unbatched
candidate is retained separately as `burst-a3-*` for comparison.


## Execution: A4 flipped planar copies (2026-09-27)

**DERIVED (implementation).** Disjoint `$E300` rectangles with positive source
axes now reverse rows and bit order directly in the four native planes. A
compile-time 256-byte bit-reversal table, shifts and destination word masks
replace per-pixel reads/writes. All four ROPs preserve partial words. Coordinate
wrap, VRAM wrap, overlapping rectangles and negative source axes keep the
sequential scalar path. Source padding never reads before the allocation.
Bounds products use the existing native 16×16 multiply helper; no software
32-bit multiply/divide enters the executable. No per-copy buffer is allocated.

**MEASURED (matching command dimensions).** Ten 17×17 `$E300` copies in each
of the preceding A3 and final A4 deal captures average **53.202 → 2.986 ms**
(raw inclusive, **94.4% less / 17.8× faster**). The new range is 2.526–3.757 ms,
so all of these copies remain above the ledger's 2 ms recording threshold.
Smaller 11×11 copies now partly fall below that threshold; their recorded
subset is not an unbiased mean. The original baseline table above now separates
17×17 and 11×11 dimensions instead of labelling a mixed-size headline as 17×17.

| Deal measurement | A1 + A3 | A1 + A3 + A4 |
|---|---:|---:|
| AGCPY group 56 mean, all sizes/directions | 14.835 ms (55 calls) | 1.579 ms (53 calls) |
| Deal wall time / 8.00 board-s | 14.23 s | 13.47 s |
| Board/wall | 0.562 | 0.594 |
| Post-ready stalls (same ≥10-interval definition) | 26 / 7.73 s | 23 / 6.36 s |
| Post-ready wall / board time | 66.06 / 48.04 s | 65.29 / 48.11 s |

The hands and resulting command populations differ; use the identical-size
copy timings for the isolated speedup. The synthetic face contains no flipped
copy, so its warm time remains effectively unchanged (214.479 → 214.399 ms).
The full live run completes all 24 inputs at 480,000,000 cycles without native
errors or watchdog resets. Real-time/card-latency acceptance is still open.
PAINT, curves and command/dispatch overhead remain substantial. Next: A2 curve
stamps, then A5/A6 and B1/B2; C1/C2/D1/D2 are still unapproved.

**MEASURED (validation).** Host harness/platform/native checks and the native
arithmetic audit pass. 46,080 rotated-copy comparisons cover every source and
destination alignment, five widths (including 17), three heights, three pitches
and all ROPs. Full-VRAM integration comparisons cover overlap, both source-axis
signs and source/destination coordinate wrap. The planar tests also pass address
and undefined-behavior sanitizers. AGA/ECS replay matches all RAM/VRAM/pixels/AY
at the established instruction/cycle/IRQ boundary. Normal code/data sections
match before/after toggling the ledger build, and the ordinary build is restored.
Evidence: `amiga/.run/burst-a4-*`, `tmp/burst-a4-*`.

## Decision update (2026-09-27)

The user authorized benchmarking all four previously pending experiments:
C1 delay-loop idle hook, C2 whole command-feed-loop hook, D1 deferred graphics
execution and D2 a larger timing credit window. Each is retained only after
performance improvement and the plan's correctness gates; otherwise record
the measured rejection. This authorization does not declare them implemented.

## Execution: A2 curve stamps (2026-09-27)

**DERIVED (implementation).** The eight-entry outline cache now lazily builds
16-pixel row masks for each used alignment. Opaque constant pattern selection
uses one synchronized word batch, with the programmed colour-word phase and
ROP applied per plane. Logical rows stay distinct even when physical VRAM
aliases them, retaining XOR parity. Nonuniform/transparent patterns, unaligned
pitches and signed-coordinate wrap retain the point traversal. Mask storage is
bounded by eight outlines × sixteen alignments × 512 eight-byte records; only
used alignments allocate memory. Cache eviction invalidates every alignment.
Small planar outlines use bounded 32-bit midpoint arithmetic and comparisons;
the wide packed model remains the comparison oracle. Larger shapes keep the
wide path. Geometry, pattern order, duplicate removal, CP/DP and work counts
remain shared semantics.

**MEASURED (synthetic A1200).** Ordinary-build cold/warm face batches change
175,916/152,090 → 157,267/133,046 E-clock ticks: **248.0/214.4 → 221.7/187.6 ms**.
The 16-circle batch changes 192,760 → 59,771 ticks (**271.7 → 84.3 ms**,
including first-use construction). The mask-only intermediate was
167,585/133,686 ticks per face and 77,223 for the circle batch; bounded midpoint
construction improves cold work further. These batches include command setup
and are not individual warm r=7 circle timings.

**MEASURED (host correctness).** Harness/platform/native checks and ASan/UBSan
pass, including 3,072 packed-versus-planar curve cases across alignments,
colour phases, ROPs, COL modes, pitches, aliasing and coordinate wrap, plus
350 small-ellipse arithmetic/order comparisons. Both A1200/AGA and A500+/ECS
replay match all 262,144 RAM bytes, 524,288 VRAM bytes, 163,008 cropped pixels
and 30 AY writes at 7,008,979 instructions / 64,000,002 cycles / 7,831 IRQs.
Ordinary text/rodata/data/BSS match before and after the separate ledger build.

**MEASURED (live A1200).** All 24 inputs complete at 480,000,000 board cycles,
error zero and watchdog resets zero. Deal CRCL mean is **4.541 → 1.591 ms**
(56 calls); ELPS is **3.025 → 0.780 ms** (36 calls). Both include cold misses.
Deal wall time is 13.47 → **13.24 s** for 8 board-s (ratio 0.604); post-ready
is 64.436 wall-s / 48.06 board-s (0.746). Stalls are 24 runs / 6.884 s versus
23 / 6.363 s in A4, using the same ten-VBI threshold: different hands prevent
attributing the session-wide count to this change. Matched curve costs improve,
but the 0.2–0.4 ms circle estimate and real-time target are not met.

A2 is accepted. A5 front-end work follows; no clock or scheduling change is
included in A2. Evidence: `amiga/.run/burst-a2-{aga,ecs,ledger}`,
`burst-a2b-after`, `tmp/burst-a2-*-comparison.log`, `tmp/burst-a2-ledger-summary.txt`
and `tmp/perf/burst-a2*`.

## Execution: A5 command front end / small fills (2026-09-27)

**DERIVED (implementation).** Fixed commands and polygons up to 64 words stay
in inline storage; larger legal variable commands spill rather than gaining a
new size limit. Length is decoded once, and WPR/AMOVE/RMOVE bypass general
drawing dispatch. Snapshot encoding remains the previous word-vector format,
including partial words and commands. Native command statistics are 32-bit
modulo counters; host reports and snapshots retain 64-bit counts. Statistics
have no chip effect. The existing idle `blitterSubmit` path already writes
prepared register pairs directly, and is retained.

Small fills use masked CPU words only with **no pending DMA** and at most
16 words per plane. Other rectangles retain asynchronous queued blits.

**MEASURED (rejected intermediate).** Unconditionally draining before CPU fills
of up to 64 words improved the isolated face but regressed deal RFRCT from
0.920 to 1.267 ms. Two 57×2 / 65×2 fills each waited about 31 ms behind earlier
work; a 75×6 CPU rectangle cost 2.897 ms. That policy was rejected. The final
empty-queue / 16-word policy eliminates those observed stalls.

**MEASURED (retained candidate, A1200).** Front-end-only synthetic cold/warm
face is 155,779/131,716 ticks; final is 149,246/125,492, against A2's
157,267/133,046 (**221.7/187.6 → 210.4/176.9 ms**). Eight PAINT batches including
borders are 373,988 versus 416,555 ticks (**527.2 versus 587.2 ms**).

| Deal mean | A2 | A5 |
|---|---:|---:|
| WPR | 0.160 ms | 0.152 ms |
| AMOVE | 0.210 ms | 0.177 ms |
| RMOVE | 0.215 ms | 0.187 ms |
| RFRCT | 0.920 ms | 0.937 ms |
| RPLL | 6.564 ms | 6.015 ms |
| PAINT | 3.438 ms | 3.435 ms |

Deal takes **13.03 wall-s / 8 board-s** (0.614). Post-ready is 64.611 wall-s /
48.08 board-s, with 23 stalls / 6.543 s under the common ten-VBI criterion.
Different hands still limit whole-session attribution. The front-end/rectangle
estimates are not met; no real-time claim follows from these improvements.

**MEASURED (gates).** Host harness/platform/native checks pass, with 3,072
independent small-fill cases plus snapshot/abort checks across inline/spill
and half-word boundaries. Exact ECS and AGA replay matches 262,144 RAM bytes,
524,288 VRAM bytes, 163,008 pixels and 30 AY writes at the established
7,008,979 instructions / 64,000,002 cycles / 7,831 IRQs. All 24 live inputs
finish at 480,000,000 cycles with no error or watchdog reset. Normal sections
match before/after the ledger build. A5 is accepted; A6 follows.

Evidence: `burst-a5-front` (front end only), `burst-a5-*` (rejected fill policy),
`amiga/.run/burst-a5b-*`, `tmp/burst-a5b-*-comparison.log`,
`tmp/burst-a5b-ledger-summary.txt`, `tmp/perf/burst-a5*`.

## Execution: A6 synchronized direct CPU access (2026-09-27)

**DERIVED (implementation).** An optional synchronized four-plane view replaces
per-pixel virtual calls in scalar drawing and PAINT word reads. It is acquired
lazily once per CPU drawing section, invalidated before any rectangle/span
submission that may queue a write, and reset at each drawing command. Thus a
polygon alternating CPU and queued axis segments reacquires the view at the
required boundary. Unsupported surfaces retain the previous virtual path.
The VBI does not draw, and no work was added ahead of its swap.

**MEASURED (A1200).** Synthetic cold/warm faces are 145,485/121,921 ticks
(**205.1/171.9 ms**, from 210.4/176.9); the eight PAINT/border batch is 354,351
ticks (499.5 ms), from 373,988 (527.2). The single-DOT batch slightly worsens
154,927 → 158,266 ticks because a one-pixel command cannot amortize acquisition.
The retained benefit is in drawing loops. Live deal PAINT is **3.435 → 3.373 ms**,
RPLL 6.015 → 6.013 ms; the already-batched lines/curves largely bypass this path.
Deal wall/board is 13.08/8.00 s (0.612); post-ready 64.429/48.12 s, with
20 stalls / 5.744 s. Different hands and queue-wait placement still limit
session attribution; this is a modest kernel improvement, not a real-time gate.

**MEASURED (gates).** Host harness/platform/native checks and ASan/UBSan pass.
New tests force queued fills between scalar polygon segments and check exactly
two acquisitions, completed DMA before CPU continuation, and capability fallback
once per command. ECS and AGA replay match all RAM/VRAM/pixels/30 AY writes at
7,008,979 instructions / 64,000,002 cycles / 7,831 IRQs. Live24 finishes at
480,000,000 cycles with no error/reset; normal sections match after restoring
the ordinary build. A6 is accepted; B1/B2 presentation follows.

Evidence: `amiga/.run/burst-a6-*`, `tmp/perf/burst-a6-*`,
`tmp/burst-a6-*-comparison.log`, `tmp/burst-a6-ledger-summary.txt`.

## Execution: B1 wall-time presentation experiment (2026-09-27)

**MEASURED (rejected candidate).** Presenting changed VRAM every second wall
VBI, with a queue-length guard, produced 1,354 wall-only presentations and
skipped 39 busy-queue attempts. In the captured deal there were 86 successive
presentation pairs with unchanged board cycles: progress was visible during
stalls. However, whole-screen composition raised blitter waits inside deal
commands from about 0.03 to **1.93 s**, and deal wall time from **13.08 to
15.46 s** for eight board-seconds. Presentation CPU time rose 0.12 to 0.33 s.
The 24-input run and exact ECS/AGA replay still passed, but performance did not.
This candidate is reverted. B1 must be reconsidered with B3 damage updates;
full recomposition on wall cadence is not the default. Evidence: local
`burst-b1-*` captures and `tmp/burst-b1-rejected.patch`.

## Execution: B2 Copper reload and buffer ownership (2026-09-27)

**DERIVED (Hardware Reference Manual, Copper section 2-5).** COP1LC is
automatically reloaded at vertical blank; COPJMP1 instead restarts immediately.
Completed frames now publish COP1LC without the strobe in the main thread,
between scanlines 8 and 299 with no unserviced VERTB request. Only that short
publication masks interrupts. The next VBI retires the old front buffer; both
buffers remain owned until then. Publishing between VBIs permits one swap per
frame, avoiding the accidental 25 Hz ceiling of arming only in the VBI. No
work is added before the VBI's screen update. Diagnostic forced captures wait
for retirement, while normal presentation does not wait.

**MEASURED (A1200).** All 359 composed frames were armed and swapped, with
zero late-window deferrals. The old early-blanking restriction is removed.
Deal wall/board is **13.12/8.00 s** (0.610), essentially unchanged from A6's
13.08/8.00. Post-ready is 64.677/48.14 s; 19 stalls total 5.424 s under the
same ten-VBI criterion. Different hands prevent an isolated speed claim.
The 24-input run finishes at 480,000,000 cycles without errors or watchdog
resets. B2 is accepted for safe frame publication; the real-time gate is open.

**MEASURED (gates).** Host checks include frame ownership, delayed readiness,
counter wrap and 100 swaps in 100 frames. ECS and AGA replay match all RAM,
VRAM, cropped pixels and 30 AY writes at 7,008,979 instructions / 64,000,002
cycles / 7,831 IRQs. The restored ordinary build's text/rodata/data/BSS match
the saved pre-ledger build. Evidence: `amiga/.run/burst-b2-*`,
`tmp/burst-b2-*-comparison.log`, `tmp/burst-b2-ledger-summary.txt`.

## Execution: C2 whole command-feed loop (2026-09-27)

**DERIVED (implementation).** The existing ready-test/write descriptors now
continue through the verified ring-loop tail in assembly. The first loop-head
comparison still runs in original code; subsequent comparisons, branches and
ring wrapping share the stopped-clock exception. Every word retains the WFR
test, shared device write, A1 postincrement and pending frame/IRQ/fault check.
The source word and ring-start load are range/alignment checked before access.
D0/D1, other live registers, CCR and the precise exit PC are preserved. All
original bytes in `$2E54–$2E6E` are guarded by local generated patch tables.

Diagnostic replay retains every intermediate instruction boundary and exact
counts. The live path collapses only the bounded non-I/O comparison/branch tail
between word boundaries; it charges the original nominal cycles and does not
charge service time as guest execution. A clean ring exit returns directly;
a due event promotes to the scheduler. `native-no-feed-loop` retains the prior
three-instruction fusion; `native-no-feed-fusion` disables both.

**MEASURED (rejected intermediate).** Checking every comparison/branch in live
mode and promoting every clean ring exit cost as much as the exceptions saved:
deal wall time was 13.17 s. That live tail was replaced by the bounded path
above; it remains the diagnostic implementation.

**MEASURED (paired synthetic A1200, ordinary build).** A newly assembled
512-word ring containing 256 synthetic WPR commands takes **76,890 → 67,487
E-clock ticks**, **108.4 → 95.1 ms**, or **211.7 → 185.8 microseconds/word**
(**12.2% less**) versus the previous three-instruction fusion. Both use the
same command parser/device model; this batch runs before raster DMA is enabled.
The controlled warm card batch is 121,996 ticks versus 121,921 after A6,
essentially unchanged, as expected for a feeding optimization.

**MEASURED (live A1200).** The 24-input ledger finishes at 480,000,000 cycles
with no error or watchdog reset. It executes 44,507 loop writes and avoids
**39,660 repeated exception entries** (89.1% of those writes). Deal wall/board
is **13.14/8.00 s**, versus B2's 13.12/8.00. Post-ready is **64.333/48.03 s**
with 20 stalls / 5.585 s, versus 64.677/48.14 and 19 / 5.424 s. Hands differ;
the paired synthetic result establishes the isolated saving, not a whole-game
speedup. The ledger's short-call counter counts completed hooked instructions,
including fused accesses; it is not an exception-entry count. The analyzer now
labels its residual denominator accordingly. Real-time acceptance stays open.

**MEASURED (correctness).** The independent CPU oracle passes 879,040 whole-loop
cases on 68000/68020, covering each diagnostic instruction boundary, live device
boundaries, ring wrapping, exact-end producers, FIFO backpressure, every CCR,
nominal cycles and C scratch-register destruction. Another 2,048 invalid
ring-start cases stop before the original load without reading or changing A1.
The prior 502,272 three-instruction cases and other host/short-hook gates pass.
Both ECS and AGA replay match all 262,144 RAM bytes, 524,288 VRAM bytes, 163,008
pixels and 30 AY writes at 7,008,979 instructions / 64,000,002 cycles / 7,831
IRQs. Restored ordinary sections match the saved pre-ledger executable.

C2 is retained and enabled by default. C1 delay-loop idle handling is next;
C3/C4, B3 with a B1 retry, and the authorized D experiments remain. Evidence:
`burst-c2-*` (first implementation), `amiga/.run/burst-c2c-*`,
`tmp/burst-c2c-*-comparison.log`, `tmp/burst-c2c-ledger-summary.txt`,
`tmp/perf/burst-c2c-*`; `burst-c2-default` validates the default ordinary build.

**MEASURED (ordinary default, non-warp).** `burst-c2-default` completes all
24 inputs at 480,000,000 cycles, 5,235 PAL frames from entry, zero native or
screen errors, zero watchdog resets and restored vectors. Whole-loop default
is confirmed enabled; no replay or ledger is active. Debug audio is muted.

## Execution: C1 bounded idle-loop experiment (2026-09-27)

**DERIVED (implementation).** Opt-in `native-idle-hook` under clock option C
guards `$2442/$2444` and installs one Line-A hook at the decrement. A pure
assembly kernel updates the saved D6 word, CCR and PC for a bounded number of
original SUBQ/BNE instructions; the final real subtraction supplies overflow,
borrow and X correctly, including an initial zero counter. The exact reference
cycle charge is `14*n-2` for a completed loop. Partial batches can stop after
SUBQ or BNE. Diagnostic batches end at the next replay instruction boundary.

Live batches consume only the current wall-time deficit, split near board-tick
boundaries, and charge reference cycles without K multiplication. When no
budget is available, the supervisor service uses STOP with Amiga IRQs enabled.
The VBI can finish DMA, update audio and wake the service; no original code is
called from an ISR. Pending guest IRQs return through the existing scheduler.
No OS timing/wait calls enter this path. Ledger `IdleWait` distinguishes sleep
from CPU work; its reader also accepts the old 13-kind captures unchanged.

**MEASURED (A1200 live24).** All inputs complete at 480,000,000 cycles with no
error or watchdog reset. During the deal, measured original CPU execution falls
**3.47 → 0.21 s**, with **2.45 s in STOP**. Short-call count falls 33,066 →
26,991, but full dispatcher calls rise to 7,961 because the new scheduler
boundaries use the checked dispatcher. Deal wall/board is **12.97/8.00 s**,
versus C2's 13.14/8.00: elapsed improvement is small. The heavier doubling hand
in this run makes its 71.39/48.03 s whole-session result unsuitable for direct
comparison with C2's quieter hand. Total skipped delay work is 59,479,442
instructions / 416,309,400 reference cycles, with 2,291 STOP waits.

**MEASURED (gates).** The assembly kernel passes 524,356 independent 68000/68020
state comparisons and 458,661 budget checks, including wrap, signed overflow,
partial branches, high D6 bits and unrelated registers. All host and existing
short/feed-hook gates pass. ECS and AGA replay agree in full RAM, VRAM, cropped
pixels and 30 AY writes at 7,008,979 instructions / 64,000,002 cycles / 7,831
IRQs. Restored ordinary sections match the pre-ledger candidate.

**Decision:** retain C1 as a validated **opt-in experiment**, not a new normal
default yet. It frees measurable CPU time but does not fix card latency alone;
evaluate its interaction with D1 deferred graphics before choosing the final
default. C3 lean FIFO and C4 dispatcher costs are next, then B3/B1 and D. The
authorized experiments and real-time acceptance remain unfinished. Evidence:
`amiga/.run/burst-c1-*`, `tmp/burst-c1-*-comparison.log`,
`tmp/burst-c1-ledger-summary.txt`, `tmp/perf/burst-c1-*`.

## Execution: C3 FIFO word path (2026-09-27)

**DERIVED (implementation).** Native word writes to the FIFO use one address
and byte-phase decode, retaining the shared parser and command execution.
Partial high bytes, fault state, read-byte phase and private write latches retain
the previous two-byte behavior. Control-register writes retain the byte path.

**MEASURED (paired synthetic A1200).** The 512-word feed takes **67,487 →
63,071 E-clock ticks**, **95.1 → 88.9 ms** (6.5% less), or 173.6 microseconds
per word. Warm face drawing remains 121,905 ticks (171.9 ms). This improves
feeding without changing drawing semantics or the clock.

**MEASURED (live24).** All inputs complete at 480,000,000 cycles with no error
or watchdog reset. Deal wall/board is **13.10/8.00 s**, versus C2 13.14/8.00;
post-ready is **64.27/48.01 s** with 21 stalls / 5.683 s. Different hands
limit whole-session attribution. Masked dispatcher prologue is still 116 us
per deal call and 125 us over the session; C4 follows. Real-time acceptance
remains open.

**MEASURED (gates).** 16,384 word/byte transitions cover every address register
and both pending-byte phases, including faults and complete serialized state.
Mixed WPR/ORG/pattern/move/draw/read/write streams also match. All host gates
and native arithmetic audit pass. ECS/AGA replay matches all RAM, VRAM, pixels
and 30 AY writes at 7,008,979 instructions / 64,000,002 cycles / 7,831 IRQs.
Restored ordinary sections match the saved build. C3 is accepted. Evidence:
`amiga/.run/burst-c3-*`, `tmp/burst-c3-*-comparison.log`,
`tmp/burst-c3-ledger-summary.txt`, `tmp/perf/burst-c3-*`.

## Execution: C4 dispatcher boundary (2026-09-27)

**DERIVED (implementation).** The masked prologue ran general data-address
canonicalization for the PC, startup-test loop bookkeeping during normal play,
and repeated clock grants with no state change. PC conversion now uses the
contiguous ROM/RAM allocation and retains the existing range stop. Test-loop
corrections run only when hardware tests are explicitly enabled. Deferred clock
totals are cleared only when nonzero. Grants retain their original order and
saturation; common K=1.5/4 arithmetic and no-op boundaries use exact fast forms.
No work was merely moved outside the measured prologue or into an ISR, and the
clock contract is unchanged.

**MEASURED (A1200 ledger).** Masked prologue cost falls **116 → 100 us per deal
call** (13.8%) and **125 → 108 us over the session** (13.6%). Deal wall/board
is **13.01/8.00 s** versus C3 13.10/8.00; post-ready **64.15/48.00 s** versus
64.27/48.01. Stalls are 21 / 5.741 s versus 21 / 5.683 s. Different hands and
interrupt placement limit session attribution; the boundary saving does not
establish real-time play. Live24 completes with no error or watchdog reset.

**MEASURED (gates).** Eight million clock transitions agree with an independent
wide-arithmetic implementation of the previous policy: both credit sources,
saturation, empty/saturated credit, queued ticks, frame wrap and ratios 0–80.
Host harness/platform/native checks and the arithmetic audit pass. Exact ECS
and AGA replay match RAM/VRAM/pixels/30 AY writes at the established boundary.
The first ECS attempt exited via the ordinary quit path without a capture; it
was not counted as a pass, and the rerun completed. Ordinary code/data sections
match after restoring the non-ledger build. C4 is accepted; B3/B1 and D remain.
Evidence: `amiga/.run/burst-c4-*`, `tmp/burst-c4-*-comparison.log`,
`tmp/burst-c4-ledger-summary.txt`, `tmp/perf/burst-c4-*`.

## Execution: B3 damage queue and B1 retry (2026-09-27)

**DERIVED (experiment).** A 4 KB physical damage map retained separate buffer
ages at 64-pixel granularity. Presentation mapped it through the three screens,
merged adjacent dirty rows, restored the prior window and overlaid the new one.
All planar CPU paths and queued drawing marked damage. A lower-priority display
queue submitted only when drawing DMA was idle, bounded to 512 words per plane
and 64 rows per chunk. No original instructions or work entered the ISR.

**MEASURED (rejected intermediate).** Scanning every display row even without
new VRAM writes made the 128-window synthetic batch rise from about 0.728M to
2.880M E-ticks. Skipping clean buffers, bounding the occupied map and merging
rows reduced this to 0.800M. The controlled small-VRAM-write batch took
**1.830M versus 3.718M ticks** for full recomposition (50.8% less). However,
damage bookkeeping raised the warm synthetic face from 121,905 to 124,965
ticks (171.9 → 176.2 ms), and each bounded chunk pays another blitter setup.

**MEASURED (live B3).** Deal wall/board worsened **13.01 → 14.04 s / 8 board-s**.
Post-ready was 66.11/47.92 s, with 22 board-time stalls / 5.640 s. Command
blitter waits fell from about 0.06 s to below 0.01 s during the deal, but damage
planning and chunk submission outweighed this. All 403 composed frames retired;
2,041 chunks covered 20.90M pixels. The initial ledger attributes chunk pumps
outside `present()` to full dispatcher time; total dispatcher time still includes
them. The later retry times these pumps explicitly under presentation.

**MEASURED (B1 retry with final B3).** Adding every-second-VBI presentation
worsened the deal further to **15.90/8.00 s**, and post-ready to **72.46/47.99 s**.
35 board-time stalls total 13.834 s. Actual VBI retirement records show 196
displayed frames during the deal, including **107 adjacent swaps at unchanged
board time**; the largest gap between deal swaps is 23 PAL frames (460 ms).
Progress is visible during stalls, but responsiveness/total work fail acceptance.
The run submitted and swapped 1,561 frames / 6,012 chunks. Deal presentation CPU
cost is 2.41 s; drawing waits remain below 0.01 s.

**MEASURED (gates).** Both candidates complete live24 without errors or watchdog
resets. Exact ECS/AGA replay matches all RAM/VRAM/pixels/30 AY writes. Host tests
cover buffer ages, address projection and all CPU drawing paths; ASan/UBSan pass.
Native comparisons check incremental versus full output, clipped windows,
register changes, visible VRAM writes and the real bounded chunks. Ordinary
sections match after the separate ledger build, including a ledger-only observer
that records actual swaps after screen VBI processing.

**Decision: reject and revert B3 and this B1 retry.** They reduce Chip-bus work
but cost more CPU time on the current A1200 target. No slower presentation policy
becomes the default. The tested patch is retained locally for future ECS work or
a materially cheaper submission path. D1 deferred commands and D2 credit-window
experiments follow; they remain required, and real-time acceptance is open.
Evidence: `amiga/.run/burst-b3{,b,c,d}-*`, `amiga/.run/burst-b1b-*`, matching
`tmp/*-comparison.log` / ledger summaries, `tmp/burst-b3-b1-rejected.patch`.

## Execution: D1 deferred commands (2026-09-27)

**DERIVED (experiment).** A bounded 4,096-word queue accepted completed supported
commands from the shared parser. Reads, read-type commands, control changes and
captures drained the prior prefix; oversized commands ran in place. The original
executor remained authoritative. Queue wrap/backpressure retained command order
and partial input. `native-deferred-video` enabled the verified C1 idle hook;
its worker had a conservative 200-PAL-line budget, measured from the beam/VBI
without OS calls. Faults at read barriers propagated as loud stops.

**MEASURED (first full run).** Live24 completes without error/reset. Deal is
**12.66/8.00 s**, versus C1's 12.97/8.00 and the C4 default's 13.01/8.00. This
also includes C3/C4 changes absent from the earlier C1 run, so the small elapsed
saving cannot be attributed to deferral. The heavier doubling hand takes
70.07/48.03 s post-ready (C1 71.39/48.03), with 31 stalls / 8.413 s.
Crucially, **all 13,066 queued commands execute at 4,498 barriers; zero execute
at idle**. Therefore the intended scheduling benefit is absent. Command scopes
here time execution rather than FIFO writes, so their means must not be
compared directly with earlier parse-plus-execute command scopes.

**MEASURED (barrier investigation).** A 112M-cycle boot/deal sample has 3,486
control-write drains and one status-read drain. CCR-low writes change only IRQ
enables, so a refined candidate avoids their drain while forcing non-WFE/WFR
status tests through the shared synchronizing read. The same bounded sample
still has zero idle executions: all 3,487 drains are now status reads. The
FIFO IRQ's CER test forces queued work before returning to main-loop idle.
See `rom-set.md` for the instruction evidence. Avoiding that barrier would
relax this plan's explicit observable-status ordering rule, so it is not done.

**MEASURED (gates).** The first candidate passes exact ECS/AGA replay in all RAM,
VRAM, pixels and 30 AY writes at the established boundary. Host tests and
ASan/UBSan cover byte phases, queued/synchronous equality, snapshots, control
and read barriers, large commands, ring wrap/backpressure and loud faults.
The existing idle/short/feed CPU oracles pass, including 879,040 whole-feed
cases and invalid-address stops. Ordinary sections match after the ledger
build. The refined candidate passes the queue tests and the bounded native
probe; it is not represented as a fully accepted native replay candidate.

**Decision: reject and revert D1.** Its required barriers leave no work for the
idle worker, and no isolated performance benefit is established. C1 remains
opt-in pending D2/final selection. The locally saved experiment is
`tmp/burst-d1-rejected.patch`; evidence is `amiga/.run/burst-d1-*`,
`burst-d1b-barriers`, `burst-d1c-barriers`, the matching comparison logs and
`tmp/burst-d1-ledger-summary.txt`. D2 and final acceptance remain.
