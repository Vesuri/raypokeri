# Borrowed cache-state batching experiment

Status: design for the next bounded implementation experiment; not implemented
or enabled. It refines the existing card cache and whole-feed loop, preserving
their observation and interrupt boundaries. It changes no timing policy.

## Why

**MEASURED:** native-rendering-followup.md records a synthetic feeder-only floor
of 7.08–7.28 ms per 260 words versus a roughly 22.7 ms isolated cached card.
The current cache completion kernel repeatedly verifies and materializes each
command's counters, CP/DP and work flags. Cached pixels already stay deferred
until the card/prefix is completed or an observation forces a flush.

**INFERRED:** aggregating private, unobserved state changes inside an existing
borrow could save a material fraction of this difference. It must be measured;
the synthetic endpoint delta is not a predicted real-card saving.

## Proposed bounded mechanism

1. Admit only after the existing exact context, translated position and
   background guards have accepted the card. Use the existing raster-grant
   restrictions: no byte phase, observer, enabled CED interrupt, pending spill,
   error, presentation hold, origin/width change or RWP write. A missing proof
   takes the current path. Diagnostic replay keeps ordinary execution.
2. Precompute aggregate prefix descriptors during existing startup rendering,
   using locally derived recipe data. Record only fields actually written by
   the verified commands. Preserve uncontrolled parameter registers. Bound all
   tables and reject unsupported command classes instead of guessing effects.
3. Match every incoming word, including exact signed translated AMOVE operands.
   Retain original FIFO status reads, source guards, PC/CCR/register updates,
   nominal cycles, physical interrupt windows and pending-event tests. No
   ring words are skipped and no original instruction sequence is widened.
4. Keep accumulated progress private while the model cannot be observed. Before
   any promotion, mismatch, callback, pixel observation, end of ring, final card
   command or return to guest, materialize precisely the accepted prefix and
   partial command. Materialize before the event, not after its handler.
5. Aggregate only proven state: changed parameter words; translated CP/DP;
   per-group command-count deltas; last raster/move work and CPU-access flags;
   cache matched/used offsets; pending words/count/length and write-high byte;
   status bits. Handle counter wrap and partial headers/variable counts exactly.
   Do not copy a whole parameter snapshot over untouched live state.
6. On a mismatch, materialize only earlier accepted words, then hand the
   mismatching word to the existing endpoint exactly once. Keep observation
   flushes and final masked blit on the authoritative shared path.
7. VBI may continue Paula, swap and quit work, but may not inspect the borrowed
   device state. Audit every callback/IRQ path before permitting a borrow. Any
   such observation requires materialization or makes the path ineligible.

## Proof and measurement gates

- First build a portable executable specification. Compare against ordinary
  FIFO execution at every word cut, at every mismatch position and after
  arbitrary borrow splits, with translated/wrapped coordinates, mutated
  untouched parameters, both rectangle-work modes and counter overflow.
- Compare serialized device state and subsequent continuation, not only final
  pixels. Exercise observation barriers, byte continuation, error and aborted
  commands. Confirm no device state remains borrowed after refusal/return.
- For the native path, independent 68000/68020 CPU tests must preserve the
  saved state and exact original boundary for physical frame/IRQ/quit events,
  plus the existing source/wrap guards and short/whole-feed/FIFO-control tests.
- Benchmark complete back and white-prefix separately with the same synthetic
  fixture and final DMA drain. Reject regressions; report startup table cost.
- Required headless suites, exact ECS/AGA RAM/VRAM/pixel/AY replay, cold live24,
  release landing/AY latency and VBI service checks still apply before default
  activation. Keep a flag-off comparison and restore the ordinary release
  while experimental captures use frozen executables.

No permission is inferred to weaken the 20 ms card/audio deadline, suppress
video interrupts or change the clock. If exact materialization cannot be
proved, stop this experiment and retain the existing implementation.
