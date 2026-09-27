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
