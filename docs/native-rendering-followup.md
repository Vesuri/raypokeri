# Startup, artwork copies, scrolling and shuffle sound

User follow-up, 2026-09-28. Continue the existing performance work without
changing game decisions, hiding faults, or trading correctness for speed.
The memory audit is complete in `471b832`; see memory-audit.md.

## Work order and acceptance

1. **Interleaved copies.** Capture actual attract/doubling commands. Combine
   the four compatible plane transfers into one queued blit. Preserve masks,
   shifter prefetch, storage seams, overlap rejection and the 1,023-row OCS
   limit. Run native self-tests and full replay equality on ECS and AGA, then
   compare native benchmarks and live scrolling. No AGA-only dependency.
2. **Cold setup.** Measure initialization and the 100 acknowledged reserve
   coins separately; attribute time to original execution, command feeding,
   raster work and presentation. Reduce repeated artwork construction and
   setup costs. Keep original accounting execution and external acknowledgments.
   Compare fresh starts against the current SDL cold-start experience; do not
   use a warm snapshot as evidence for cold-start performance.
3. **Remaining artwork.** Catalog both fonts, card numbers, large/small suits,
   and J/Q/K/JOKER. Distinguish already resident offscreen images from shapes
   rebuilt procedurally. Optimize the existing copies first; add exact guarded
   sequence/raster caches only for remaining repeated raster work. Include
   colour/pattern/ROP, orientation, state side effects, observation barriers and
   source modifications in the proof. Derive assets locally; never commit them.
4. **Shuffle sound.** Reconcile the visible shuffle with the original sound
   call and callback ordering and the physical footage. The new presentation
   waits prolong a non-reentrant callback. Do not silently move a sound call,
   invent AY writes, or make the whole guest scheduler reentrant. Propose any
   required scheduling change with its safety conditions before adopting it.
5. **Persistence / development fixtures.** Establish retained main-RAM bounds,
   pointer/checksum handling and reset behavior before extending native saves.
   Keep prepared local fixtures for warm development runs; make cold-start
   tests explicit. Preserve live saves between ordinary launches. Do not seed
   a fresh drive with today's all-zero $D0000 file and claim faster startup.

## Evidence so far

- **MEASURED:** current native warm launch with existing `nvram.bin` still
  inserts 100 reserve coins and takes 2,183 PAL frames (43.66 s) to Ready.
  That file contains 32,768 zero bytes. SDL's immediate second launch loads a
  complete clean-start snapshot. Native accounting lives in main RAM; metadata
  includes pointers and checksums, so relocation across launches matters.
- **MEASURED:** the paced host shuffle has zero AY writes over 30 steps.
  **DERIVED:** `$1AA0A` selects sound 9 *after* calling the shuffle. The ROM also
  defers scheduled callbacks while one is active. This is not evidence of a
  Paula DMA malfunction. Physical sound timing still needs comparison.
- **MEASURED:** attract scrolling includes 261 `$EC00` copies of a 211×20 strip
  from offscreen Y=-850, advancing source X one pixel per update; wrap uses
  additional split copies. These transfers are disjoint, not in-place scrolls.
- **MEASURED (earlier full selector enumeration):** ordinary face cards copy
  their striped inset; J/Q/K copy complete picture insets. Four 17×17 suit/rank
  copies precede the inset. Those assets already exist offscreen in the ROM's
  rendering scheme. See card-back-blit-design.md, “Shared white-card prefix”.

Local traces: `tmp/render-attract{.catalog,-copies.log}`,
`tmp/shuffle-check-full-events.txt`, `amiga/.run/warm-start-audit/gdb-out.log`.
The interleaved copies, artwork catalog/font expansion, isolated scrolling
renderer, retained accounting and consumer-paced shuffle have passed their
respective gates below. Cold-start elapsed time, complete-card deadlines and
whole-game real-time/audio sequencing remain open.

Do not run `window-memory-test` again in this session: the user asked to stop
because failures in the installed SDL library loader produced repeated popups.
Use the non-SDL headless harness for research and muted native diagnostic runs.

## Accepted interleaved-copy optimization

**MEASURED:** the candidate combined-plane copy passes the native synthetic
surface/card tests and exact diagnostic replay on both A1200/AGA and A500+/ECS:
262,144 RAM bytes, 524,288 VRAM bytes, 172,064 cropped pixels and 30 AY writes
at 7,008,979 instructions / 64,000,002 cycles / 7,831 IRQs.
Local evidence: `tmp/copy-batch-{aga,ecs}-compare.log`.

**MEASURED:** one paired A1200 synthetic composition run, at 709,379 timer ticks
per second, gives incremental composition 744,465 -> 732,148 ticks (1.65%
less time) and full composition 3,887,914 -> 3,866,151 ticks (0.56% less).
These are composition measurements, not a scrolling or whole-game speed claim.
Evidence: `amiga/.run/copy-bench-{before,after}/gdb-out.log`.
**MEASURED:** A1200 live24 completes 24 inputs, all 30 shuffle boundaries and
480,000,000 total board cycles with zero watchdog resets/errors and restored
vectors. Cold Ready is 95,920,000 cycles / 2,153 PAL frames; the remaining live
scenario spans 2,700 frames. This does not establish a cold-start improvement
against an independently controlled baseline. Evidence:
`amiga/.run/copy-live24/gdb-out.log`. Headless harness, platform and native
checks pass; the native arithmetic audit passes.

**DERIVED (implementation):** compatible interleaved copies at 608-pixel source
pitch and at most 255 logical rows submit one four-plane blit instead of four
plane jobs. Other layouts/heights retain the previous path. Existing bounds,
mask, prefetch, storage-seam and overlap checks remain in force. No new buffers
or chipset-specific feature are required.

## Proposed sideways-shuffle scheduling experiment

The user confirmed the missing sound concerns the sideways deck shuffle.
The user approved continuing this prototype on 2026-09-28. It paces consumption of that shuffle's
commands, instead of suspending the original producer callback at each helper
return. This would allow the callback to return and select its original sound
while queued drawing is still being presented. It is a hypothesis, not yet a
verified fix or an assertion about physical ACRTC execution time.

Safety conditions for a prototype:

- Preserve the original command stream, CPU decisions and sound calls.
- Identify only the verified shuffle boundaries; do not throttle every copy.
- Bound buffering and handle producer-ring wrap/full conditions explicitly.
- At a boundary, release the original feeder IRQ through its normal not-ready
  path. Do not block inside that IRQ or make guest callbacks reentrant.
- Suppress both FIFO-ready and FIFO-empty interrupt indications while held;
  release after presentation and resume through the ordinary device IRQ.
- Keep native cached-status/assembly-feed state coherent with the device model.
- Verify all 30 frames against the existing paced render, verify AY writes
  during motion, and complete watchdog-on live play on both chipsets.
- Leave diagnostic replay unchanged; test snapshots and any new runtime state.

First measure whether the original producer ring can hold the complete shuffle
and where the feeder is when the helper returns. If these preconditions fail,
revise the design rather than assume an unlimited queue. This experiment was subsequently integrated after the gates below passed.

## Consumer-pacing prototype findings (2026-09-28)

**MEASURED:** the headless prototype completes all 30 boundaries without a
watchdog reset, with 45 AY register writes during motion (the producer-wait
version has none). The original callback reaches sound selection with 30 steps
queued and only two consumed. Observed producer-boundary ring occupancy peaks
at 2,288 of 4,008 bytes. This measures one original-code scenario, not every
possible live queue state.

**MEASURED:** command words remain identical through the shuffle. Simply moving
the wait gives only three matching frames out of 30: later display-window
settings are applied to earlier queued drawing. Retaining each marker's display
registers for composition restores byte-for-byte equality of all 30 complete
frames. The prototype temporarily applies that display state only while
composing; guest-visible current registers remain current.

Reproduce with `python3 host/shuffle_consumer_probe.py` (headless, no SDL). This
builds instrumented copies under `tmp/`, leaving normal host/native binaries
unchanged. Captures and generated sources are local-only. The prototype's IRQ
query temporarily masks FIFO-ready/empty enables and its status-read path masks
both ready bits. A production version must put this policy into a single
coherent readiness state rather than retain these research interception points.

## Consumer pacing adopted

**MEASURED:** the shared fixed 32-marker queue stores only 62 scanout-control
bytes per marker. Overflow, reset, ring wrap, display-state isolation and corrupt
snapshot cases pass. Native assembly exits at the exact consumed cursor without
changing guest registers or flags. Native release waits for actual Copper buffer
retirement. Both ready indications and cached status use one model policy.

**MEASURED:** ECS and AGA live24 finish all inputs with zero resets/errors; all
30 retired frames match every reference pixel. AGA has 60 AY writes during 77
held VBIs; ECS has 30 writes during 162. Exact full-state boot replay passes on
both chipsets. The integrated headless test retains all 601 commands, 30 exact
frames and 45 in-motion AY writes, including ring wrap and snapshot continuation.
See [shuffle-pacing.md](shuffle-pacing.md). This resolves the missing scheduled
shuffle sound; it does not claim physical audio calibration or a 50 FPS native
shuffle. Native speed remains part of the performance work.

**MEASURED:** `python3 host/shuffle_consumer_probe.py --wrap` also passes all
30 frame comparisons, all 601 commands through the final shuffle frame,
45 in-motion AY writes and zero watchdog resets. It deliberately repositions
an empty ring before the original code resumes; this is a boundary-condition
fixture, not an unmodified live-run claim. Both normal and wrap runs observe
2,288 / 4,008 bytes maximum occupancy at producer markers.

## Startup stage ledger (2026-09-28)

**MEASURED, instrumented A1200:** splitting the existing CIA time ledger at
actual setup state transitions gives 31.80 s before the first main-loop/door
step, then 29.14 s inserting the 100 acknowledged reserve coins. The first
interval feeds 44,126 FIFO words / 5,576 commands; refill feeds 19,389 words /
2,408 commands. Estimated timestamp overhead is 5.16 s and 2.95 s respectively.
These diagnostic-build times are not a replacement for the ~43 s ordinary-build
cold-start baseline; profiler overhead and code layout affect execution.

**MEASURED:** initialization command time is 7.84 s inclusive, with PTN the
largest individual group (621 commands / 2.06 s corrected). Refill command time
is only 2.37 s; guest execution is 6.59 s, masked dispatch prologues 2.52 s and
estimated exception/assembly residual 6.44 s. Refill is therefore not primarily
raster work. Reducing unnecessary per-word service work and retaining accounting
are useful alongside artwork acceleration; faster fills alone cannot remove
most of this wait.

Reproduce with the ledger build and `amiga/ledger.gdb`, then
`host/native_ledger.py --startup tmp/ledger-startup.bin --log <capture-log>`.
The nine stage snapshots and stage/coin events exist only in TIME_LEDGER builds;
normal execution has no new checks or allocations. Local evidence:
`tmp/startup-ledger-report.txt`, `amiga/.run/startup-ledger/gdb-out.log`.

## Accounting persistence adopted

**MEASURED:** normal native saves now retain the active pointer-free block and
all three accounting copies. Fresh warm boots use the original initialization
and cabinet status protocol without reserve refill. A1200 cold/warm Ready is
43.72/19.80 s; ECS and AGA warm live24 pass through cleanup with zero error/reset.
Headless recovery/relocation/corruption checks pass. Explicit local warm fixtures
seed only new non-replay drives and never replace existing saves. See
[startup-policy.md](startup-policy.md) for format, evidence and usage.

This closes persistence and warm-fixture implementation. It does not close cold
startup, remaining artwork/scrolling, or the real-time rendering/audio deadlines.


## Remaining artwork catalog

**MEASURED:** all 60 original face-card selector probes consist of the shared
white prefix followed only by resident-image copies (484 total, 192 rotated).
This includes a complete 80×89 Joker image, 40×54 J/Q/K insets, 17×17 suit/rank
indices and 11×11 small suits. The header's 20×14 digits and composed labels/
paytable/scrolling strips also already reside offscreen. A second immutable
copy of these images would need source-write invalidation without avoiding their
existing blits. The useful paths to optimize are PTN expansion and the upright/
rotated copies. See rom-set.md for the complete-selector and atlas evidence.

**MEASURED:** doubling advances a resident 150×20 strip through 101 EC00 copies,
with no PTN commands in that capture. Attract uses a 211×20 strip. A synthetic
native benchmark now measures these dimensions over every source alignment and
physical-row seam, including DMA completion for each step. Its result is recorded below; composition timing alone is not a scrolling measurement.


## Planar-word font tile expansion accepted

**DERIVED:** PTN cache misses now construct whole 16-pixel plane words. The
row mask is reversed/aligned once and combined with pre-expanded colour words;
transparent modes and arbitrary wrapping pattern windows retain their exact
per-pixel meaning. Cache keys, allocations and DMA layout are unchanged.

**MEASURED (paired synthetic A1200):** 512 15×14 expansions take
783,054 -> 230,084 E-clock ticks (**71% less**). The same number of cache misses
including blits takes 1,407,522 -> 812,678 ticks (**42% less**), or
3.875 -> 2.238 ms per tile. These miss-heavy batches isolate this change;
normal cache hits already avoid expansion. Evidence:
`amiga/.run/pattern-bench-{before,after}`.

**MEASURED:** 51,456 cases agree with an independent scalar oracle across every
alignment, dimension, colour mode, wrapping window and patterned colour phase.
All headless suites and the native arithmetic audit pass. ECS and AGA replay
match all 262,144 RAM bytes, 524,288 VRAM bytes, 172,064 pixels and 30 AY writes
at the established instruction/cycle/IRQ boundary. AGA fresh live24 completes
all inputs, 30 shuffle steps and 60 in-motion AY writes, with zero reset/error,
restored vectors and clean heap teardown. Cold Ready is 2,120 PAL frames
(42.40 s), followed by 2,683 frames (53.66 s) to 480,000,000 board cycles.
This does not meet cold-start parity or the real-time/card deadline. Captures:
`pattern-replay-{aga,ecs}`, `pattern-live-aga`, `tmp/pattern-*-compare.log`.


## Scrolling copy measurement

**MEASURED:** the paired synthetic A1200 benchmark performs 256 copies of each
observed strip size with every source alignment and row-seam case represented.
It waits for DMA completion after each copy. Separate-plane versus combined-
plane results (PAL E-clock 709,379 Hz):

| Strip | Four jobs: ticks / batch | Combined: ticks / batch | Combined mean / update |
|---|---:|---:|---:|
| Attract, 211×20 | 475,024 | 470,069 | 2.589 ms |
| Doubling, 150×20 | 369,792 | 364,402 | 2.007 ms |

The accepted interleaved path is 1.0% / 1.5% faster in this test. Each copy is
well below a 20 ms frame; the original resident strip is already transferred by
the blitter, including shifted edges. An extra immutable strip cache would
not remove that transfer. Full ECS/AGA copy correctness was established above.
This settles the isolated scrolling-renderer measurement, not smooth wall-time
cadence during unrelated game/command-feed stalls. That remains part of the
live scheduling/performance gate. No new copy policy or temporary baseline
switch remains in the source. Evidence: `amiga/.run/scroll-bench-{separate,together}`;
`amiga/graphics-benchmark.gdb` retains the reproducible synthetic benchmark.


## Updated gameplay profile and audio measurement correction

**MEASURED, instrumented A1200 with retained accounting:** the deal takes
11.24 wall seconds for 8.00 board seconds. Exclusive totals are 3.01 s guest
execution, 2.24 s full-dispatch C, 0.53 s masked prologue, 1.69 s short-path C,
2.90 s residual hooks/IRQs and 0.88 s observer overhead. Completed command
execution is nested inside those totals: 1.08 s (9.6% of the interval).
VBI samples put 225/566 PCs in the guest's main-loop/delay page at $02400.
This supports remeasuring the authorized idle-loop experiment with the current
renderer; adding more artwork caches alone cannot eliminate most elapsed time.

**MEASURED:** complete cached backs average 105.6 ms (11 samples, range
62.8–119.8 ms); non-hits average 179.6 ms. These include command feeding and
subtract measured observer cost. They remain above the complete-card target.
The warm run lasts 68.53 wall / 58.47 board seconds after Ready; it is not the
same hand/interval as the previous cold profile. Captures:
`amiga/.run/followup-ledger`, `tmp/followup-{ledger-report,card-timing,deal-samples}.txt`.

**MEASURED (profiler race):** VBI applied write 510 and logged it 45 E-clock
ticks before the writer recorded its own event. The old analyzer skipped that
application and paired the write with an envelope update five frames later,
reporting a false 94 ms delay. Pairing by monotonic write sequence identifies
that timestamp race instead. The other measurable writes reach Paula within
18.36 ms. Future diagnostic writers timestamp before publishing their state;
old raced pairs are explicitly excluded. Regression tests reproduce the event
order, and normal text/rodata/data/BSS remain byte-identical after this fix.
This corrects instrumentation; delayed original sound sequencing during slow
board execution remains part of the open performance gate.


## Idle-loop retest and next feeding work

**MEASURED (same ordinary executable and retained accounting fixture):** the
approved idle hook remains opt-in. With it off/on, Ready is 949/946 PAL frames
at the same 12,080,000 board cycles. The subsequent 58.49 board seconds take
63.94/64.66 wall seconds. Both complete all 24 inputs and 30 shuffle boundaries
with 60 in-motion AY writes, zero error/reset, restored vectors and clean heap
teardown. The new renderer therefore still supplies no measured elapsed-time
reason to enable this hook. Evidence: `amiga/.run/followup-idle-{off,on}`.

Next ordinary-code optimization: extend the proven intermediate-parameter
bridge to validated command headers. The current path still calls C once for
every opcode, even when its only effect is opening a pending command. The
shared model must provide the decoder metadata and authoritative field pointers;
there must be no second opcode definition. Admit only a whole-word FIFO state
with no pending command, fault, presentation hold or enabled CED interrupt.
One-word commands and invalid opcodes retain the ordinary model call. A header
updates the pending opcode/length/count, high-byte latch and cached CED status;
its final word still performs command execution and all IRQ/fault handling in
the model. As with parameter spans, invalidate every grant at scheduler/control
boundaries. Preserve the existing per-word readiness/event/CCR checks and
reference-cycle charges. No status-ordering or live-clock change is proposed.

Acceptance before default use: per-word full-model state equality including
variable lengths, all opcode bits, CED enable, byte phases and abort/reset;
the linked CPU oracle; a paired header-heavy WPR feed benchmark and live play;
exact ECS/AGA replay; arithmetic audit. This is planned, not implemented yet.
The complete-card and cold-start gates remain open.

## Accepted command-header bridge

**DERIVED (implementation):** the shared opcode table now supplies both word count
and reserved-bit mask. A live empty-command grant lets the linked assembly
accept a valid multiword header directly into the model's fixed pending buffer.
It excludes partial bytes, control-register accesses, presentation holds, faults,
pending commands and enabled CED interrupts. One-word commands, invalid words,
variable count words and final parameters still call the model. All scheduler
and return boundaries invalidate the grant. No new graphics buffer, guest-code
patch, status ordering or clock policy is involved. Diagnostic replay uses the
ordinary path. Ledger builds retain the card-start timestamp callback.

**MEASURED (A1200):** the paired synthetic 512-word WPR feed takes
68,766 / 55,999 E-clock ticks with header acceptance disabled/enabled: 96.94 /
78.94 ms, or 18.6% less time. It directly accepts 255 headers; the initial word
has no preceding grant. The independent intermediate-parameter benchmark is
60,435 / 36,917 ticks. Evidence: `amiga/.run/header-bench/gdb-out.log`.
This is an isolated feed result, not a complete-card or startup deadline claim.
The bridge is enabled by default after the gates below; `native-no-header-feed`
retains the previous path for comparisons.

**MEASURED (paired A1200 live24 with optional counters enabled):** disabled/enabled Ready
is 956/948 PAL frames. After Ready the same 480M-cycle stop spans 3,214/3,194
frames (64.28/63.88 s), for 58.47/58.48 board seconds. Each run completes all
24 inputs, 30 shuffle boundaries and 60 in-motion AY writes with no reset/error,
restored vectors and clean heap teardown. Enabled accepts 5,234 headers and
16,120 intermediate parameters. The small whole-run difference is not a
same-hand drawing comparison: timing changes can change the hand. Neither run
meets the 5% real-time gate. Evidence: `amiga/.run/header-live-{off,on}`.

**MEASURED (gates):** all 65,536 opcode words preserve authoritative pending
fields and the independent reserved-bit contract; full-state samples, all CED
enables, byte phases, variable counts/spills and abort barriers pass. The linked
CPU oracle passes 131,072 header cases and 2,637,120 complete-loop cases, including
all tested interruption points, source guards, CCRs and nominal cycle charges on
68000/68020. The full headless suites and native arithmetic audit pass. ECS and
AGA replay match 262,144 RAM bytes, 524,288 VRAM bytes, 172,064 cropped pixels
and 30 AY writes at 7,008,979 instructions / 64,000,002 cycles / 7,831 IRQs.
Default ECS live24 also completes all inputs and 30 shuffle steps, with 35
in-motion AY writes, zero error/reset, restored vectors and clean heap teardown.
Evidence: `tmp/header-{final-check,aga-compare,ecs-compare}.log`,
`amiga/.run/header-live-ecs`.

**MEASURED (no optional profiling):** A1200 warm/cold Ready is 938/2,080
PAL frames (18.76/41.60 s). Warm/cold post-ready intervals are 3,201/2,672
frames for 58.49/48.00 board seconds. Both finish 24 inputs, 30 shuffle steps
and 60 in-motion AY writes without error/reset. This confirms that disabling
the optional counters does not resolve the remaining elapsed-time deficit.
The separate TIME_LEDGER build restores to identical normal code/data sections.
Evidence: `amiga/.run/header-unprofiled-{warm,cold}`.
