# Borrowed cache-state batching experiment

Status: portable and native boundary proofs pass. The opt-in prototype fails
the live landing performance gate and is not accepted. CACHE_BATCH is omitted
by default. It refines the existing card cache and whole-feed loop, preserving
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


## Portable specification proof (2026-09-29)

**MEASURED:** `make harness-cache-batch-check` passes 2,088 full-state cut cases:
each of 261 word boundaries (including empty and complete), four translated
X positions (-3, 0, 15, 240), and both scalar/rectangle work-count modes. Each
case compares semantic state and the full serialized video state at the cut,
then continues the original stream and compares all VRAM and the final state.
These tests retain a synthetic non-default untouched parameter and initialize
all command counters at their maximum to exercise wrapping. Random backgrounds
satisfy only the existing 68 guard predicates.

Fourteen repeated-split cases (strides 1,2,3,7,16,31,127 in both work modes)
materialize without flushing pixels, so subsequent borrows continue the same
admitted recipe. Every one of the 260 single-word mutations is independently
run to completion/error against ordinary FIFO execution. There were 237,120
accepted words in the cut matrix. No native execution or performance claim
follows from this portable proof. Evidence: tmp/cache-batch-spec-full.log.

Implementation: host/cached_batch_reference.h is an executable specification,
not included in either game build. It matches every word, aggregates only the
CP/DP calculation and flag stores, and scans completed commands to apply WPR
and command counts. It admits only the eight proven command groups and WPR
indices below 12; other recipes fall back. A borrow starts only at an empty
command, and declines a capture buffer. It reconstructs the active partial
command, including a negative variable-length header before its count arrives.
No observer may inspect it before materialization.

Next: native prototype with a locally translated expected-word buffer and a
tight compare/advance acceptance path. Materialization must still happen before
every existing feeder exit and C callout. First test its CPU/boundary semantics
and complete-card cost; only then run native release gates. Signed-coordinate
wrap, invalid grant cases and 32-bit native counter overflow still need the
independent native matrix; the current four translations are not exhaustive.


## First native prototype (not accepted)

**MEASURED:** src/native/CachedBatch.h passes the same 2,088 full-state cut
cases, 260 mutations and repeated-split matrix as the independent host-only
specification. It translates the immutable expected stream once per recipe/
anchor change. The native acceptance path compares/advances a cursor; borrowed
state is materialized on mismatch, model callout and feeder exits. The original
CPU bookkeeping, clock charges, status/branch/write boundaries remain in place.
The arithmetic audit passes.

Paired A1200 synthetic four-card totals at 709,379 Hz:

| Workload | Accepted release | CACHE_BATCH prototype |
|---|---:|---:|
| Full cached backs, complete absolute mode | 64,210 ticks | 51,910 ticks |
| White prefixes, complete absolute mode | 31,801 ticks | 32,026 ticks |

Full backs are **22.63 → 18.29 ms/card (19.2% lower)**; white prefixes are
about 11.21 → 11.29 ms. Both benchmark runs finish status4/error0 with no board
frames/cycles advanced. This is a first isolated result, not a live deadline or
audio improvement. Other cache modes pay the prototype's eligibility overhead
and regress; any retained implementation must address avoidable checks.

Existing linked feeder regressions pass with the batch inactive. They do not
exercise active batch materialization, so they are insufficient for acceptance.
Next add independent 68000/68020 active-cursor and all-exit tests, then native
full-state/live/latency gates. Native counters are 32-bit, unlike the host test.
The current begin scans eligible commands per borrow; precomputing this bound
may be needed, but must not be treated as already measured.

Evidence: tmp/cache-batch-native-spec.log, tmp/cache-batch-feed-check.log,
amiga/.run/cache-batch-{before,after}/gdb-out.log and frozen
tmp/perf/Pokeri-cache-batch-prototype(.elf). Ordinary amiga/out is restored and
matches the accepted clock-inline release in all allocated ELF sections.


## Native proof and live qualification

**MEASURED:** the linked CPU proof passes 14,400 actual compiled materializations
on 68000/68020, including signed/wrapped CP/DP, partial polygon lengths/latch,
32-bit command/used counter wrap, untouched state and C ABI preservation.
The assembly acceptance path passes all 65,536 words in four active/inactive/
limit/mismatch contexts on both CPUs (524,288 cases), including flush-before-
fallback ordering with deliberately clobbered C scratch registers. Another
384 return/promotion/frame-store cases prove materialization before guest
transfer. The independent original feeder oracle passes 50,880 active-batch
frame/IRQ/shuffle boundaries with exact PC/CCR/registers and nominal cycles.
These complement the full shared-model cut tests; the C callback is stubbed
in the CPU boundary proof, while its compiled materializer is tested separately.

The first prototype passes headless model/platform/native suites, linked
short/FIFO regressions, exact AGA and ECS replay (262,144 RAM bytes,524,288 VRAM
bytes,172,064 pixels,60 AY writes at7,904,133 instructions/64,000,000 cycles/
8,685 IRQs), and cold live24 on both chipsets. AGA Ready/end1,195/4,105 PAL
frames; ECS5,825/16,329, both24inputs/30shuffle steps,60/45 in-motion AY writes,
no errors/resets. Replay retains its existing schedule and ordinary hooks;
it does not independently exercise active batching.

**Performance gate failed for the first prototype:** observer-free release
card intervals improve for lighter stages21/23 (18.912/19.424 →13.696/13.696ms),
but interrupt-heavy landing stages24/26 regress (42.304/42.240 →47.456/46.400ms).
Different hands are not paired whole-game comparisons, but these results do
not justify default activation. A complete-card synthetic win alone is
insufficient. Evidence: tmp/cache-batch-card-latency.txt and the two
.run/cache-batch-latency-* captures.

## Precomputed eligibility bounds (second prototype, not accepted)

Eligibility boundaries now rebuild only when the immutable recipe, translation
or grant options change. This removes the remaining-recipe scan from each
interrupt-separated borrow. The host proof additionally reuses the same batch
across owners, X positions and changing control/absolute options. Both portable
and native-ready implementations pass the extended matrix. Linked batch and
active-feed CPU proofs pass again against Pokeri-cache-batch-bounds.

**MEASURED:** complete four-back totals fall to49,090 ticks (17.30ms/card);
white-prefix totals27,884 (9.83ms/prefix), at 709,379 Hz. The second candidate's
live24 capture completes24inputs/30shuffle steps/60AY with no error/reset and
sampler0. Light stages21/23 improve to12.224/12.096ms, but stage26 is still
46.784ms versus the accepted release's42.240ms. Later special-card intervals
are not comparable across hands. Therefore this revision is still **not
accepted**, despite the isolated gain. Attribute the remaining per-interrupt
materialization/admission cost before another change; do not silently enable it.

Current source remains opt-in CACHE_BATCH=1. Normal amiga/out is restored.
Local evidence: tmp/cache-batch-{cpu-exits,active-feed,headless,short,fifo}.log,
tmp/cache-batch-{aga,ecs}-check.log, tmp/cache-batch-bounds-{host,cpu,feed}.log,
tmp/cache-batch-bounds-card-latency.txt and .run/cache-batch-bounds-*.
The original live readers inherited old RAM dump prefixes; corrected copies
are tmp/cache-batch-live-{aga,ecs}-ram.bin. The old clock-inline-live RAM files
were overwritten by these captures and must not be used as baseline evidence.
Replay dumps and benchmark/latency logs use separate correct prefixes.


## Live attribution and inactive-call experiment (2026-09-29)

**MEASURED:** a read-only debugger capture of the second prototype brackets
borrow materialization and the ordinary video endpoint during card drawing.
For completed landing cards 24–27, materialization totals 0–0.192 ms/card;
there are only zero to two non-empty borrows. Ordinary video endpoint service
accounts for about 10.7–11.9 ms over 49–51 calls per card. This is beam/frame
sampling at 64 microsecond resolution, not a high precision per-call timer.
The inspection range (including work outside completed cards) records 3,539
inactive finish calls. All 24 inputs, 30 shuffle steps and 60 in-motion AY
writes complete with sampler off, no error and no watchdog reset.

**MEASURED, rejected:** guarding the two C callers against inactive borrows
passes the linked CPU proof but does not improve live landings: cards 24–27
measure 48.544, 47.392, 47.968 and 47.808 ms. The synthetic back remains
49,093 ticks/four cards (17.30 ms each), and white prefixes 27,882 ticks/four
(9.83 ms each), at 709,379 Hz. This run also completes live24/30 shuffle/60 AY
without errors or resets. Live hands and frame alignment differ, so these
are not instruction-identical whole-session comparisons; they nevertheless
fail to demonstrate an improvement over the accepted ~42 ms landings.
The guard edit is removed. The second opt-in prototype and its proofs remain
available for research, with default activation explicitly rejected.

**DERIVED:** repeated materialization is not the principal landing cost in
this capture. The next attribution target is the accepted release's scheduler
and original video IRQ service, retaining every guest instruction and IRQ
boundary. No timing change or IRQ suppression follows from this result.

Evidence: .run/cache-batch-attribution, .run/cache-batch-guard-{benchmark,latency},
tmp/cache-batch-guard-cpu.log and frozen tmp/perf/Pokeri-cache-batch-guard(.elf).
The ordinary build is restored and all allocated ELF sections match
Pokeri-clock-inline-release exactly. The initial guard fixture failed its
local byte-check script before producing a valid capture; the corrected
fixture verified both instruction offsets and produced the results above.


## Partial-command re-entry and deterministic split feeds (2026-09-29)

The opt-in implementation can now re-enter after a partial command. It checks
all existing buffered words against the translated recipe, checks the exact
fixed/variable pending length, and rejects a complete/oversized prefix. An
empty borrow leaves the model unchanged. CACHE_PARTIAL=0 retains empty-command
admission for comparison. CACHE_BATCH is still **off by default**; this extension
has not met its live performance activation gate.

**MEASURED proof:** the native-ready model passes the existing 2,088 full-state
cuts and continuation/mutation matrix. A new retained-recognition matrix admits
510 partial prefixes, rejects 3,399 corrupted-prefix/length/count cases, then
continues 100,440 words with exact final state and VRAM. Valid prefixes must
re-admit after restoration, preventing vacuous rejection tests. Snapshotting
flushes recognition, so these tests compare live protocol fields before final
serialization. The linked 68000/68020 proof passes 56,640 materializations,
including partially entered WPR and variable-length RPLL, zero newly accepted
words, signed/wrapped coordinates and counter wrap. Existing 524,288 acceptance,
384 exit and 50,880 active whole-feed boundary cases pass unchanged.
Evidence: tmp/cache-partial-host2.log, tmp/cache-partial-{cpu,feed}.log.

The A1200 live24 run completes 24 inputs/30 shuffle steps/60 in-motion AY writes,
no errors/resets, sampler off, at Ready/end 1,189/4,099 frames. It has too few matching
completed landing backs to establish a speedup: light cards 21/23 take 12.224/
12.160 ms and later card 44 takes 63.648 ms, while the earlier landing-stage cache
begins do not all reach full-back hits. These different sequences cannot be
compared as instruction-identical hands. No live improvement is claimed.

**MEASURED fragmentation:** RASTER_CHUNKS=1 executes the same descriptor with
four repetitions per workload, either one entire feed, ten-word pieces, or
one-word pieces. Each return materializes/revokes the existing grants. All
three builds include the accepted FIFO endpoint. Context setup and clearing
remain outside timing; final blit synchronization is included. Each completed
back/white prefix must increment its cache-hit counter exactly once. This is
synthetic feeder cost, **excluding original IRQ handlers and control triplets**.

E-clock ticks at 709,379 Hz, totals for four cards:

| Variant | Back whole | Back 10-word | Back 1-word | White whole | White 10-word | White 1-word |
|---|---:|---:|---:|---:|---:|---:|
| No batching |64,244|82,398|239,380|31,744|35,915|74,235|
| Empty-command admission |49,229|83,518|283,454|27,923|36,478|87,213|
| Partial-command admission |49,079|81,792|359,674|27,683|35,768|98,965|

Ten-word backs are 29.04 ms without batching versus 28.83 ms with partial admission:
only 0.7% lower. One-word feeds regress 84.36→126.76 ms. The isolated uninterrupted
back gain (22.64→17.30 ms) is consumed by frequent admission/materialization and
return boundaries. This result is insufficient for default activation; partial
admission remains an opt-in research path. It does not close the 20 ms deadline.
All synthetic runs exit status 4/error 0/frames 0/cycles 0. Normal allocated ELF
sections still exactly match Pokeri-fifo-service-release.

Reproduce with RASTER_CHUNKS=1 plus CACHE_BATCH=0, CACHE_BATCH=1 CACHE_PARTIAL=0,
or CACHE_BATCH=1, and amiga/cached-raster-chunks.gdb. Local evidence:
.run/cache-chunks-{off,whole,partial}, .run/cache-partial-live and frozen
tmp/perf/Pokeri-chunks-{off,whole,partial}(.elf). No new timing/IRQ policy or
wider guest hook was introduced.
