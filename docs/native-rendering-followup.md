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

## Measurement qualification (2026-09-28)

**MEASURED:** earlier fixtures described below as “ordinary”, “unprofiled” or
“no optional profiling” used the ordinary non-ledger executable but still
contained `native-measure`. That enables scope counters/context and VBI samples.
They exclude the heavy ledger and per-word counters, but are not observer-free
release measurements. The latest sampler-off pair below explicitly confirms
`NativeTiming::active == 0` and `nativeProfileEnabled == 0`; use it when comparing
normal run.sh behavior. The isolated paired benchmarks retain their stated
measurement configuration on both sides.

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

## Remove optional feed counters from ordinary execution

**DERIVED:** per-access observation increments are now assembled only for
`FEED_COUNTERS=1`, `DISPATCH_PROFILE=1` or `TIME_LEDGER=1`. Guest instruction
accounting, nominal cycles, device state and every existing event boundary are
unchanged. Ordinary debugger counter symbols remain available with zero values;
`nativeFeedCounterMode` is an absolute ELF tag, not a memory address to read.

**MEASURED (paired synthetic A1200):** removing the counters reduces enabled
header-feed time from 55,432 to 52,945 E-clock ticks (4.5%). With header acceptance
disabled in both parameter-isolation runs, enabled parameter feeding changes
from 36,153 to 33,420 ticks (7.6%). Each batch feeds 512 words. The benchmark
now explicitly disables header acceptance while measuring intermediate words;
otherwise the independent header improvement contaminated that comparison.
Evidence: `amiga/.run/feed-counters-{on,bench}/gdb-out.log`.

**MEASURED:** CPU oracles pass in both counter modes, including the complete
2,637,120-case feed-loop matrix. The counter-free warm live24 finishes 24 inputs,
30 shuffle boundaries and 60 in-motion AY writes with zero reset/error, restored
vectors and clean heap teardown. Ready is 921 frames; the next 58.49 board
seconds take 3,180 frames (63.60 s). This remains outside real-time acceptance.
Evidence: `tmp/feed-counters-{check,on-check}.log`,
`tmp/counter-final-oracle.log`, `amiga/.run/feed-counters-live`.
**MEASURED:** exact ECS/AGA replay passes all 262,144 RAM bytes, 524,288 VRAM
bytes, 172,064 pixels and 30 AY writes at the established 7,008,979-instruction
boundary. Evidence: `tmp/counter-{aga,ecs}-compare.log`.

## Shorten the remaining startup diagnostic display dwell

**MEASURED:** the original diagnostic-digit routine performs 39,744 iterations
without drawing game graphics. Retaining one iteration per digit/blank interval
removes 953,784 instructions and about 79,000 watchdog-port writes, with exact
CPU/RAM/VRAM/AY/video equality at its return. The approved hardware-test bypass
now includes these three duration operands; hardware-test mode and historical
replay keep their old counts. Ordinary A1200 cold/warm Ready changes from
41.60/18.76 s to **35.40/12.08 s**. New and historical exact replay, live24,
headless state/recovery tests and build audits pass. See `startup-policy.md`.

**MEASURED (new instrumented cold baseline):** initial artwork construction
takes 16.65 wall / 0.48 board seconds, with 5,576 commands and 28,916 video-write C
calls (including address selections, control writes and partial FIFO words).
Nested video-write time is 6.65 s; residual hooks/IRQs cost 5.19 s. Refill
takes 27.97 wall / 10.08 board seconds, with only 1.92 s of nested command work;
full dispatch, guest execution and hook overhead dominate that stage. The deal
is 10.81 wall / 8.00 board seconds. Cached-card samples still span 52.87–165.56
ms (nine hits, 105.58 ms mean), including startup cards. The whole-run cadence
and complete-card deadlines remain unmet. Measurable AY writes reach Paula
within 18.29 ms, but slow guest sound sequencing remains an open gate.

Evidence: `tmp/dwell-ledger-report.txt`, `tmp/dwell-card-timing.txt`,
`amiga/.run/dwell-ledger`. The analyzer must be supplied the corresponding
`--marks`, `--startup`, `--frames`, `--slow` and `--events` files: its historical
defaults do not select a capture from `--log` alone. The ordinary build's
text/rodata/data/BSS sections match its saved pre-ledger build exactly.


## Address-selector assembly experiment (2026-09-28)

**DERIVED:** an admitted immediate byte MOVE to `$F6000` changes only the
ACRTC address register and clears its two byte-transfer phases. The native
candidate writes those three authoritative model fields directly, invalidates
borrowed FIFO spans, and uses the existing MOVE flags and event-boundary exit.
It neither draws nor changes status/IRQ ordering. Other endpoints and MOVE
forms keep the ordinary path. The validated candidate is now default; `native-no-address-selector` retains
the previous path for comparisons.

**MEASURED (paired synthetic A1200):** 512 actual Line-A/RTE address selections
take 47,066 / 21,629 E-clock ticks with the C / assembly body: 66.35 / 30.49 ms,
a 54.0% reduction. The test includes the common exception and descriptor path.
Evidence: `amiga/.run/selector-bench/gdb-out.log`.

**MEASURED (same executable, ordinary live24):** cold Ready is 1,906 / 1,821
PAL frames (38.12 / 36.42 s), disabled / enabled. Both begin with fresh
accounting and reach zero player credits plus 100 reserve coins. Warm Ready
from the same retained zero-credit fixture is 654 / 591 frames (13.08 / 11.82 s).
All four runs finish 24 inputs with zero reset/error, restored vectors and clean
heap teardown. Each has 30 or 60 shuffle boundaries and 60 or 120 in-motion AY
writes. Hands differ, so post-ready totals are not a controlled drawing-speed
comparison: cold 60.88 / 54.74 s; warm 65.68 / 71.14 s, for about 49 / 59 board
seconds respectively. Real-time and complete-card acceptance remain open.
Evidence: `amiga/.run/selector-{live,warm}-{off,on}`.

**MEASURED:** 1,024 complete model-state and byte-continuation cases, 74,936
linked CPU MOVE cases and the existing full-feed matrix pass, as does the native
arithmetic audit. ECS and AGA replay match all 262,144 RAM bytes, 524,288 VRAM bytes,
172,064 cropped pixels and 60 AY writes at 7,904,804 instructions / 64,000,008
cycles / 8,679 IRQs. Evidence:
`tmp/selector-final-check.log`, `tmp/selector-{aga,ecs}-compare.log`.


## Register-resident feeder accepted (2026-09-28)

**DERIVED:** the verified live loop saves nine additional
registers once per batch. It keeps the cursor, producer/end pointers, PC, CCR
and two descriptors in registers until the existing status/branch/write boundary
requires an exit. Source/ring bounds, shuffle markers, readiness, nominal cycle
charges and C ABI preservation remain unchanged. Both loops call one shared
header/parameter/model word-acceptance body. Diagnostic replay keeps its original
per-instruction path and exercises the extracted helper; an independent CPU
oracle checks the new live loop's original instruction results at every exit.
There is no device-state shadow, new graphics cache, deferred status or clock
change. It is now default; `native-no-register-feed` retains the previous loop.

**MEASURED (paired synthetic A1200, 512 words each):** WPR-heavy feeding costs
53,868 / 44,370 E-clock ticks (75.94 / 62.55 ms), old / register-resident loop:
17.6% less. WPTN-heavy feeding costs 33,559 / 23,815 ticks (47.31 / 33.57 ms),
29.0% less. Both sides include the shared helper refactor. These comparisons
isolate the loop, not the complete-card or whole-game deadline.
Evidence: `amiga/.run/register-feed-bench/gdb-out.log`.

**MEASURED (ordinary A1200, same executable and retained-accounting fixture):**
warm Ready is 555 / 530 PAL frames (11.10 / 10.60 s). After Ready, 59.36 /
59.34 board seconds take 69.34 / 64.38 wall seconds. The hands differ (60 / 30
shuffle steps), so these are functional/cadence observations, not a controlled
whole-game speedup. Both runs finish 24 inputs with zero reset/error, restored
vectors and clean heap teardown; 120 / 60 AY writes occur during shuffle motion.
A separate enabled cold run reaches Ready at 1,656 frames (33.12 s), zero credits
and 100 reserve coins, then completes the same input script without a fault.
Enabled ECS live24 also completes all inputs and 30 shuffle steps, with 32
in-motion AY writes and clean cleanup. ECS performance remains deferred.
Evidence: `amiga/.run/register-feed-live-{off,on,ecs}`, `register-feed-cold`.

**MEASURED (new cold ledger, including the preceding selector improvement):**
initial artwork is 13.19 wall / 0.49 board seconds and refill is 26.29 wall /
10.08 board seconds. The complete deal takes 10.75 wall / 8.00 board seconds.
Nine cached-card samples average 87.55 ms, range 46.32–148.74 ms; eight non-hits
average 172.00 ms. Hands differ from the previous ledger. Original sound writes
reach Paula within 17.97 ms, but original sound sequencing still stretches with
slow board execution. The complete-card and 5% real-time gates remain unmet.
The normal text/rodata/data/BSS sections restore identically after the ledger
build. Evidence: `tmp/register-feed-{ledger-report,card-timing}.txt`,
`amiga/.run/register-feed-ledger`.

**MEASURED (checks so far):** 3,755,520 whole-feed CPU cases pass on 68000/68020,
including real shuffle markers inside the new loop; 1,280 later-word source
guards and 3,072 invalid ring loads preserve exact PC/CCR/cycles/registers.
The prior 502,272 fused-feed and 131,072 header cases pass. Native audit passes.
ECS and AGA replay match all RAM, VRAM, cropped pixels and 60 AY writes at the
current 7,904,804-instruction boundary. The complete CPU matrix also passes with
optional feed counters enabled. Evidence:
`tmp/register-feed-{guards-check,counters-check,aga-compare,ecs-compare}.log`.


**MEASURED (actual release path, sampler off):** the enabled feeder's warm/cold
Ready is 523 / 1,616 PAL frames (**10.46 / 32.32 s**). After Ready, 59.35 / 48.92
board seconds take 63.78 / 53.46 wall seconds. Both runs finish 24 inputs, 30
shuffle steps and 60 in-motion AY writes without reset/error, restore vectors
and clean up the heap. Sampling is explicitly reported inactive. This is closer
to normal run.sh than the earlier non-ledger measurements, but still misses the
5% timing gate. These captures do not independently establish complete-card
latency without the ledger. Evidence: `amiga/.run/register-release-{warm,cold}`.


**MEASURED (release card timing, no sampler/ledger):** six accepted card-back
cache sequences after Ready took approximately 32–86 ms from admission to
successful queued blit. Five cross three or four PAL frame boundaries; one
crosses one. Two read-only debugger breakpoints observe frame/beam positions;
these exclude initial command lead-in and later presentation, and PAL beam
measurement is approximate. They independently confirm that the complete-card
deadline is not merely a ledger artifact. The run finishes all 24 inputs, 30
shuffle steps and 60 in-motion AY writes with zero reset/error. Ready is 526
frames (10.52 s). Evidence: `amiga/.run/card-release-timing/gdb-out.log`.


## Virtual stack-transition assembly (2026-09-28)

**Accepted/default:** `native-no-stack-switch` retains the previous fallback.
The short path admits virtual supervisor-to-user AND/RTE transitions to the short assembly
path. It updates the same authoritative saved user/supervisor stacks as the
full dispatcher, preserving CCR, the popped RTE frame, trace/privilege fallback
and existing interrupt boundaries. It does not change physical user execution,
clock policy or guest instructions. No duplicate stack state is introduced.

**MEASURED:** 1,572,864 CPU-control cases on physical 68000/68020 match
independently executed original 68000 AND/OR/RTE instructions, including all SR
values, both masks/settings and both virtual stack banks. Host/platform/native
checks pass. AGA replay matches every RAM/VRAM/pixel byte and all 60 AY writes
at 7,904,804 instructions, 64,000,008 cycles and 8,679 IRQs. ECS replay also matches those complete-state checks. Enabled ECS live completes
all 24 inputs without reset/error and restores vectors with clean heap teardown.
These checks accept the transition optimization; overall performance remains open.

**MEASURED (512 synthetic transitions, A1200):** saved-register preparation plus
full C dispatch costs 94,764 E-clock ticks (133.59 ms); short assembly with real
Line-A/RTE and per-iteration SR setup costs 23,608 ticks (33.28 ms). The latter
is 75.1% less in this deliberately conservative comparison: the C baseline
omits exception entry/exit, whereas the assembly batch includes them. There are
no per-operation OS clock calls. This is a transition-path measurement, not a
complete-card or overall gameplay claim.

**MEASURED (sampler-free paired retained-state fixtures):** disabled/enabled
post-Ready duration is 63.80/63.22 PAL seconds for 59.34/59.32 board seconds.
Both finish all 24 inputs, 30 shuffle steps and 60 in-motion AY writes, with no
reset/error and clean heap teardown. Hands are not guaranteed identical, so the
small whole-run difference is observational. Neither meets the 5% timing gate.
Evidence: `amiga/.run/stack-switch-{off,on,bench,replay-aga}`,
`tmp/stack-switch-{host-check,short-reference,aga-compare,ecs-compare}.log`.
The ECS live run records 30 shuffle steps and 45 in-motion AY writes; those
counts are timing-dependent, unlike the exact replay stream of 60 writes.


## Virtual user TRAP entry (2026-09-28)

**Accepted/default:** the guarded short TRAP path also handles virtual user
entry. It validates the saved supervisor stack before any mutation, saves the
current user stack, pushes the original six-byte SR/PC frame and enters virtual
supervisor mode. The frame retains the original user SR. Trace and invalid
stack/target/opcode cases retain checked fallback. `native-no-user-trap` selects
the previous implementation. Original instructions and timing policy are unchanged.

**MEASURED:** 32,798 TRAP cases cover all vectors/IPL/CCR, user/supervisor modes,
both rollout settings and physical 68000/68020, against independent original
68000 exception execution. Invalid user-stack/target/opcode/trace cases leave
both stack banks and frame memory untouched. All short/feed/host/platform/native
checks pass. ECS and AGA replay match all 262,144 RAM bytes, 524,288 VRAM bytes,
172,064 cropped pixels and 60 AY writes at 7,904,804 instructions / 64,000,008
cycles / 8,679 IRQs. Both chipsets finish live24 without reset/error, restore
vectors and clean up the heap. ECS live has a different hand (60 shuffle steps,
90 in-motion AY writes); A1200 on/off has 30 steps and 60 writes each.

**MEASURED (512 entries, A1200):** saved-register preparation plus full C TRAP
entry costs 96,038 E-clock ticks (135.38 ms), against 25,123 ticks (35.42 ms) for
assembly including exception entry/return and per-iteration SR/USP setup, 73.8%
less. As in the preceding stack benchmark, the C baseline excludes physical
exception entry/exit. Timers run only around whole batches. The sampler-off
paired gameplay runs take 63.20/63.08 PAL seconds disabled/enabled, covering
59.33/59.31 board seconds. This small whole-run difference is observational,
not a controlled same-hand speedup, and still misses the 5% timing gate.

Evidence: `amiga/.run/user-trap-{bench,off,on,live-ecs,replay-aga,replay-ecs}`,
`tmp/user-trap-{short-check,all-checks,aga-compare,ecs-compare}.log`.


## Per-card cost attribution (2026-09-28)

**MEASURED, diagnostic build only:** the new card-boundary snapshots add elapsed
open scopes to cumulative totals. Nested-scope tests cover open/completed scopes,
same-kind nesting, disabled scopes, untimed audio and clock wrap. Normal linked
text/rodata/data/BSS remain identical when TIME_LEDGER is disabled. The initial
256-record capture overflowed and was rejected; the 1,024-record repeat captured
577 endpoints with zero drops and completed live24 without error/reset.

The fastest cached-card samples take 46.4–47.1 ms after clock-reader correction,
with raw inclusive Command time 24.4–25.1 ms, ShortCall 30.1–30.9 ms and BlitWait
0.038 ms. Other cached cards take 75.7–78.4 ms corrected, Command 28.8–29.8 ms,
ShortCall 36.9–38.0 ms, still with only 0.038 ms BlitWait. Guest time within these
intervals is about 0–1.2 ms. Some cached cards instead incur 15.2–32.5 ms blitter
waits, and the first startup card waits 73.3 ms. Endpoint capture itself costs at
most 0.054 ms. Scope-chain bookkeeping and the existing command observer still
add diagnostic overhead: **these are attribution measurements, not release
latency, and inclusive columns must not be summed**. Only the wall column has
clock-reader overhead removed. Normal release measurements remain the authority
for the deadline.

**DERIVED next targets:** command acceptance remains substantial even with the
cached bitmap; inspect that path and the unscoped assembly feeder. Separately
inspect whether global blitter drains wait for unrelated queued work. Do not
skip read-after-write dependencies or weaken original instruction boundaries.
The broad earlier PC-sampling interval mixed multiple cards with main-loop work
and could not establish this per-card split.

Evidence: `amiga/.run/card-cost2/gdb-out.log`, `tmp/card-cost2-report.md`,
`tmp/card-cost2-{boundaries,events}.bin`. Reproduce with
`host/native_card_cost.py --boundaries tmp/card-cost2-boundaries.bin --events
 tmp/card-cost2-events.bin --log amiga/.run/card-cost2/gdb-out.log`.
`amiga/ledger.gdb` now captures endpoints and explicit event drop counters.
The older completed capture has only 2,994 events, below the fixed 16,384-slot
capacity; the event count never resets, so it cannot have overflowed. Full legacy
buffers require an explicit drop counter and are rejected without it.


## Inline live feeder boundaries (2026-09-28)

**MEASURED:** the live register-resident feeder now checks pending PAL frames
and interrupt promotion inline at each of the same three original device
instruction boundaries. It retains the physical interrupt mask and original
nominal cycle charges. Diagnostic replay keeps the old route. `INLINE_BOUNDARY=0`
retains the prior implementation for comparison.

Paired A1200 synthetic batches of 512 words reduce WPR from 44,105 to 41,897
E-clock ticks (5.0%) and WPTN from 23,570 to 21,769 (7.6%). These compare the
register-resident column across builds, not the older feeder control column.
The saving is about 1–1.6 ms per 260-word card, not a whole-game speedup claim.

The CPU oracle passes 3,755,520 whole-feed cases, 502,272 fused-feed cases,
131,072 headers, 64 shuffle-marker exits, 3,072 invalid initial ring loads and
1,280 later-word guards, including physical 68000/68020 and event injection at
every original instruction boundary. Both ECS and AGA replay match all 262,144
RAM bytes, 524,288 VRAM bytes, 172,064 cropped pixels and 60 AY writes at
7,904,804 instructions / 64,000,008 cycles / 8,679 IRQs.

Both live runs complete all 24 inputs without resets/errors and restore vectors
and clean up the heap. A1200 reaches retained-accounting Ready in 521 PAL frames
(10.42 s); after Ready it covers 59.28 board seconds in 63.14 PAL seconds. ECS
reaches Ready in 2,626 frames and covers 59.24 board seconds in 226.12 PAL seconds.
The different live hands are not a controlled cross-build comparison. Neither
result closes the gameplay timing gate. The normal build enables this local
improvement; it does not change timing policy or remove safe points.

Evidence: `tmp/inline-boundary-feed-fast-check.log`,
`amiga/.run/boundary-bench-{before,after}`,
`amiga/.run/inline-boundary-{replay-aga,replay-ecs,live,live-ecs}` and
`tmp/inline-boundary-{aga,ecs}-reference-*`.


## Read-only display DMA (2026-09-28, accepted)

**DERIVED from code:** the shared blit helper marked both VRAM-destination
copies and external display copies as pending writes. CPU reads consequently
drained the entire queue even when pending work only read VRAM. The default
`READ_ONLY_DMA=1` implementation keeps separate pending-access and pending-write
state. CPU writes, mutable plane leases, cache eviction and teardown still drain
all accesses. CPU reads wait for writes. An old write flag can retire before
an external copy only when both the hardware and queue are demonstrably idle.
The complete destination allocation must be disjoint from VRAM, including
masked edge words. No clock or scheduling policy changes.

**MEASURED:** the AGA native self-test passes read overlap, subsequent CPU-write
ordering and a queued writer followed by display composition. AGA replay matches
all RAM/VRAM/172,064 pixels/60 AY writes at the same 64,000,008-cycle boundary.
Both A1200 live24 runs start with identical retained accounting and finish without
reset/error, including vector restoration and heap cleanup. Before/after gameplay
covers 59.31/59.35 board seconds in 62.96/62.80 PAL seconds. Hands can diverge with
live timing: this small difference does not establish a whole-game speedup.

A same-build synthetic benchmark compares forcing a drain before 68 pixel reads
against allowing those reads to overlap a queued 608-by-255 display copy. Across
16 batches, read-path cost is 424,475 vs 28,042 E-clock ticks (37.40 vs 2.47 ms per
batch); total including the final drain is 428,267 vs 394,771 ticks (37.73 vs
34.78 ms, 7.8% less). Timers bracket batches, not individual reads. This isolates
read latency; it does not prove the full-card or sound deadline. No production
allocation or continuous timing instrumentation is introduced.

**MEASURED acceptance:** ECS also passes the native blitter tests and full
RAM/VRAM/pixel/AY replay equality at the same instruction/cycle/IRQ boundary.
Its live24 completes without reset/error and cleans up, with 30 shuffle steps
and 32 in-motion AY writes for its different hand. All required host, platform
and native regression suites pass. The implementation is now default;
`READ_ONLY_DMA=0` retains serialization for comparison. With it disabled, normal
text/rodata/data/BSS exactly match the accepted inline-feeder binary. Benchmark
allocations and timing calls run only in explicit native-benchmark mode.
The gameplay, complete-card and sound deadlines remain open.

Evidence: `amiga/.run/read-only-dma-{replay-aga,replay-ecs,live-before,live-after,live-ecs,benchmark}`,
`tmp/read-only-dma-{aga,ecs}-reference-*`, `tmp/read-only-dma-host-checks.log`.
Reproduce the synthetic comparison with `amiga/read-dma-benchmark.gdb` and
`native-benchmark`; output is `READDMA`.


### Release card timing after read-only DMA

**MEASURED:** the committed normal build (sampler and ledger disabled) completes
live24 with no reset/error, 30 shuffle steps and 60 in-motion AY writes. Four
complete card-back hits observed after Ready take approximately 30.3, 30.3,
54.0 and 52.4 ms from starts-counter increment to successful queued-blit increment.
Two read-only debugger breakpoints capture PAL frame/beam counters; this excludes
the first command's lead-in and does not measure final on-screen presentation.
Different hands and only four hits prohibit claiming a percentage gain against
the older 32–86 ms sample. The 20 ms full-card gate remains unmet.

The current ELF confirms CardBackCache::command offsets 0x2e8 (starts increment)
and 0x262 (successful hits increment); re-inspect these before another build.
Evidence: `amiga/.run/dma-card-release/gdb-out.log`, frozen
`tmp/perf/Pokeri-dma-release.elf`. Next investigate the final-word command path:
intermediate words and headers already stay in assembly, but each cached command
completion still traverses the general C acceptance and status-update path.


## Rejected C cached-completion shortcut (2026-09-28)

**MEASURED:** an opt-in final-word shortcut compared the next already-admitted
raster command against the buffered words, then updated CP/DP, work, FIFO, cache
fallback storage and counters directly. Admission, final blit, WPR/MOVE, command
observers and mismatches retained the ordinary path. It passed 2,337 differential
cases and 43,577 direct completions against ordinary rendering; the original
2,336-case cache suite also passed. This proves that prototype, not a future
assembly implementation.

A same-build A1200 benchmark of four complete cards through the native word
service took 115,262 E-clock ticks with the shortcut disabled and 131,688 enabled
(40.62 vs 46.41 ms/card, **14.3% slower**). The enabled batch accepted 140 raster
completions. Both batches require complete cache hits and include draining the
queued blit. This drives every word through C; it is not the release assembly
feeder's card-latency measurement.

**DERIVED:** checking eligibility by making an extra C call for each word is the
wrong placement. The candidate was removed, not enabled. The next design should
have the model authorize a bounded completion span and let the existing assembly
feeder match/apply it, revoking the grant at every existing observation/scheduler
boundary. It must preserve fallback words, command counters, CP/DP, work, FIFO
latches, error/logging/IRQ effects and every original interrupt opportunity.

Local evidence: `tmp/cached-raster-rejected.patch`,
`tmp/cached-raster-host-test2.log`, `tmp/cached-raster-host-baseline.log`,
`amiga/.run/cached-raster-benchmark/gdb-out.log`. No candidate code remains active.


## Model-granted assembly raster completion

The replacement for the rejected per-word C shortcut uses an explicitly borrowed
view of authoritative state. The model grants it only for an already-admitted
cached raster command, with no observer, partial byte, presentation hold, error,
spill, enabled CED IRQ or unsupported geometry. WPR/MOVE, cache admission, final
blit and mismatches retain C. Every existing inline-grant revocation also revokes
this view. The feeder tries the assembly kernel only where it would otherwise
call C, preserving the original intermediate-word and header paths and all
instruction/scheduler boundaries. Native field offsets are asserted individually.

**MEASURED:** 2,339 differential cache cases and 43,647 grant completions match
ordinary rendering. The standalone kernel and actual linked feeder wrapper each
pass 504,000 independent synthetic 68000/68020 cases: exact state/registers,
nonzero recipe/buffer offsets, length/refusal cases, signed coordinate boundaries
and counter overflow. The full existing feeder matrix passes, including 3,755,520
whole-feed cases. Grant pointers are reused only for the same cache/video/recipe;
card coordinates and work mode refresh on each grant. Cross-object rebinding and
command-observer refusal are covered.

The first paired real-feeder benchmark takes 96,831/92,442 E-clock ticks for four
cards disabled/enabled. With binding reuse and non-raster refusal, it takes
95,585/91,252 ticks (33.69/32.16 ms per card, 4.5% less). Setup and translation
are outside the timer; final blitter drain is included. Each enabled batch has
140 assembly completions. Differences between builds are not a controlled gain;
the paired within-build comparison is the reported result. The card deadline is
still unmet.

A1200 live24 finishes without reset/error, restores vectors and cleans up, with
328 assembly completions, 30 shuffle steps and 60 in-motion AY writes. ECS live24
also finishes without reset/error, restores vectors and cleans up: 405 assembly
completions, 30 shuffle steps and 45 in-motion AY writes (different live hands).
Both ECS and AGA exact replay comparisons pass: 262,144 RAM bytes, 524,288 VRAM
bytes, 172,064 cropped pixels and 60 AY writes at 7,904,804 instructions,
64,000,008 cycles and 8,679 IRQs. Replay intentionally excludes the grant; positive
coverage comes from the independent model/CPU checks and live completions.
The host/platform/native suites and existing short-hook assembly suite also pass.

The measured improvement is enabled by default with the card cache;
`CACHED_RASTER=0` retains the previous feeder for comparison. This does not close
the complete-card or audio-latency targets. The benchmark-only cache-flags query
uses CacheControl(0,0): it changes no flags but flushes caches before the timed
batches, and never runs in live services or interrupts.

Evidence: `tmp/raster-expanded-test.log`, `tmp/raster-reuse-host-checks.log`,
`tmp/raster-integrated-feed-check.log`, and
`amiga/.run/{raster-integrated-benchmark,raster-reuse-benchmark,raster-reuse-live}`.
Reproduce model/kernel checks with `make harness-raster-check` and linked wrapper
checks with `host/native_raster_check.py --elf <candidate.elf>`.

**MEASURED:** a repeat with the benchmark-only cache query takes 95,569/91,190
ticks for four cards (33.68/32.14 ms; 4.6%). Cache flags are `$00000001`:
the 68020 instruction cache is enabled. This rules out disabled instruction
caching, not instruction-cache misses. No cache setting was changed.
The default release's allocated ELF sections match this benchmark candidate
exactly. The query is the only executable change since the live/replay candidate
and runs only in the explicit benchmark.

Reproduce the paired benchmark with a normal build and an isolated debug drive
containing ROMs and `native-benchmark`, using `amiga/cached-raster-benchmark.gdb`.
It prints both four-card totals, accepted completions and cache flags, requires
complete cache admission, and includes final DMA completion. Do not add
`native-replay`, test inputs or retained state to that benchmark drive.
Latest local evidence: `amiga/.run/raster-cache-benchmark/gdb-out.log`,
`amiga/.run/raster-reuse-live-ecs/gdb-out.log`,
`amiga/.run/raster-reuse-replay-{aga,ecs}/gdb-out.log`,
`tmp/raster-default-kernel-check.log`, `tmp/raster-final-host-checks.log` and
`tmp/raster-short-check.log`.


## Cached register/move extension (accepted)

The next candidate admits exact WPR parameter writes except PR12/13 (which also
update RWP), plus exact relative moves, inside the already admitted cache span.
Absolute moves keep the normal path because their translated operands need a
separate match proof. Register writes preserve drawing work, stop and CPU lease;
relative moves update CP/DP from the actual previous cursor, reset drawing work
and stop, and preserve the CPU lease. Admission, final blit, observers and all
scheduler boundaries retain the previous behavior. A per-grant flag permits
paired timing in one binary. The extension is now enabled by default.

**MEASURED:** the model extension passes 2,339 differential cases, including
prefix/mismatch/observation/state checks, with 77,860 direct completions versus
43,647 for raster-only. The first native kernel matrix passes 564,480 cases; expanded register coverage
and linked-wrapper checks also pass. A three-way same-build benchmark
measures 96,179 ticks without raster completion, 93,635 raster-only, and 80,490
with register/move completion, for four cards including final DMA drain:
33.90 / 33.00 / 28.37 ms per card. The extension saves 14.0% against raster-only
in that binary. Neither cross-build speedup nor a 20 ms deadline is established.
`CACHED_CONTROLS=0` retains raster-only processing for comparison.
Local evidence: `tmp/raster-controls-model.log`, `tmp/raster-controls-kernel.log`,
`amiga/.run/raster-controls-benchmark/gdb-out.log`.

**MEASURED:** the verified 79-command card recipe contains ten WPR, eleven
AMOVE, twenty-one RMOVE, three RPLL, four CRCL, four ELPS, seventeen RFRCT and
nine PAINT commands. Within admitted stages 7–77, the extension adds five WPR
and twenty RMOVE completions: 25 more per complete card. The ten inner AMOVEs
still use C. The native benchmark confirms 35 raster-only and 60 extended
completions per card. Derived command counts are recorded here; recipe bytes
remain in ignored generated files.

**MEASURED:** expanded standalone and linked kernel matrices each pass 564,480
cases, covering every supported PR index and signed cursor arithmetic. The
first register-coverage generator missed indices; its coverage assertion failed,
then the generator was fixed to enumerate valid WPR cases independently. No
kernel state mismatch was observed. Existing feeder/short-hook suites and
host/platform/native suites pass. Cold live24 passes both A1200 and ECS with
no error/watchdog reset, restored vectors and heap cleanup: 636/727 direct
completions, respectively; both show 30 shuffle steps and 60/45 in-motion AY
writes (different hands). A1200 Ready is 1,574 PAL frames (31.48 s), ECS 6,255
(125.10 s); these are not paired startup improvement claims. Both exact replay comparisons pass: all 262,144 RAM bytes, 524,288 VRAM bytes,
172,064 cropped pixels and 60 AY writes match at 7,904,804 instructions,
64,000,008 cycles and 8,679 IRQs. As with raster-only, replay intentionally keeps
the ordinary observed path; positive extension coverage comes from the
independent model/CPU proofs and live completions. The measured improvement is
accepted, with complete-card and sound deadlines still open.


### Ordinary card processing and subsequent presentation

**MEASURED:** the accepted control-extension candidate, with sampler/ledger off,
completes another A1200 live24 run with no error/reset, 30 shuffle steps and
60 AY writes. Six post-Ready complete-card samples take 24.736, 24.608, 49.024,
49.888, 48.448 and 77.248 ms from starts increment to successful queued-blit
increment. The first subsequent composed frame retires 53.984, 52.512, 84.960,
90.848, 42.848 and 104.640 ms later, respectively. Total intervals are 78.720,
77.120, 133.984, 140.736, 91.296 and 181.888 ms. Different hands and a small
sample prohibit a percentage comparison with earlier live samples.

Read-only port-code breakpoints: CardBackCache::command +$2E8 (starts), +$262
(successful hit), and AmigaScreen::vbi +$3C (after front/armed/pending retirement).
After a hit the capture waits for screen.frames to advance at least once and
that composed frame to retire; pendingFrames+1 accounts for the VBI's later
counter increment. This measures the first subsequent composition/retirement,
not proof that a particular card pixel is visible: it excludes first-command
lead-in and does not inspect window/source visibility. At 20 ms/PAL frame plus
64 us/beam line, sub-millisecond figures are approximate. Only enable the VBI
breakpoint while awaiting this event. No profile calls or guest writes are added.
Local evidence: `amiga/.run/controls-card-latency/{run.gdb,gdb-out.log}` and
`tmp/perf/Pokeri-controls-live.elf`. The normal default build's allocated ELF
sections exactly match that validated candidate.

**DERIVED:** presentation scheduling deserves renewed measurement alongside
command cost. The broad wall-cadence experiment previously regressed CPU time;
a bounded request after a completed cached card may avoid its repeated partial
work. Before implementing it, establish the card's destination/window visibility,
and distinguish waiting for a new composition from queued DMA and Copper
retirement. Keep guest timing, consumer shuffle pacing and buffer ownership
unchanged. This is a next investigation, not an accepted presentation policy.


### Decomposing card presentation cost

**MEASURED:** `.run/card-pipeline2` confirms that the card destinations intersect
the middle base display at (24,39), (168,147), (264,147) and (360,147), each
88 by 100 pixels in the cropped Amiga image. The moving window can occlude them;
shuffle presentation can also exchange a saved display-register set, so these
are base-region intersections, not final pixel visibility claims.

A subsequent sampler-off live24 capture (`.run/card-pipeline-dma`) completes
without error/reset, with 30 shuffle steps and 60 AY writes. It adds bounded
read-only observations at composition completion and VBI, including actual
DMACONR busy and queued-blit flags. Each observed new composition increments
fullFrames, not partialFrames. Rounded milliseconds for seven samples:

| Card start ID | Hit to free-buffer composition entry | Entry to submitted frame | Submission to publication | Publication to retirement |
|---|---:|---:|---:|---:|
| 18 | 69.70 | 1.86 | 31.42 | 19.62 |
| 21 | 1.98 | 2.05 | 33.86 | 16.35 |
| 23 | 14.88 | 1.54 | 37.31 | 18.91 |
| 25 | 23.20 | 1.95 | 51.20 | 8.22 |
| 26 | 30.34 | 1.60 | 50.24 | 7.33 |
| 27 | 27.14 | 1.60 | 29.79 | 7.90 |
| 44 | 5.92 | 46.34 | 0.06 | 9.18 |

These are different live hands from earlier captures. Publication intervals
include DMA/queue execution and the caller reaching armReady; do not label them
pure DMA duration. Hardware was still busy at consecutive VBIs in samples 25
and 44. Queued work can remain when hardware is momentarily idle between blits.
The shuffle queue owned presentation in sample 21 (15 markers pending); it was
empty in the other six. A blanket immediate-present change would therefore be
incorrect for at least that sample.

**DERIVED from code:** any `surface->changed` invalidates both backgrounds in
AmigaScreen::present, even for a bounded cached 88x100 card. This forces 608x283
base composition before the moving window is added. It is the next graphics
optimization target; normal timing and shuffle ownership need not change.

#### Next experiment: bounded card damage

- Keep generic CPU/VRAM writes conservative. Their existing changed flag means
  unknown damage and requires full composition. Do not add callbacks to every
  plotted pixel or change the CPU plane lease ABI.
- Add a separate optional surface notification for the proven cached-card
  rectangle. Default implementations retain ordinary damage behavior. The Amiga
  backend can retain one known VRAM rectangle; multiple different rectangles
  before collection may conservatively fall back to unknown damage.
- Notify for the cached prefix as well as the completed masked blit. A display
  observer can flush a partial prefix, so its complete proven bounds must be
  included before deciding a repair rectangle. Reads, mismatch fallback, command
  state and the observer barrier remain unchanged.
- Project bounded damage through the current base-region source addresses and
  strides; clip to the cropped screen. Unsupported pitch, source wrapping or
  ambiguous projection must cause a full redraw. The moving window is still
  composed after the repaired base, preserving occlusion and window movement.
- Retain repair bounds separately for both display buffers. Repair the union
  of that buffer's pending card damage and its old window rectangle; retire its
  damage only after successful composition. Never reuse an armed/pending buffer.
  Register changes that currently invalidate the base continue to do so.
- Validate against full composition with both buffers, multiple card positions,
  unseen/offscreen/occluded rectangles, unknown writes mixed with card writes,
  partial-prefix observations, pending DMA and register changes. Then run native
  exact ECS/AGA replays, cold/live checks and paired composition/card benchmarks.
  Reject the experiment if bookkeeping costs erase its benefit.

At this measurement point no damage optimization or presentation-policy change
was implemented; the bounded repair prototype below follows this experiment.
Diagnostic endpoint offsets in `Pokeri-controls-live.elf`: present +$4BC after
pending/frame assignment, armReady +$9C after COP1LC publication, vbi +$3C after
retirement; verify again for another ELF. The first capture failed on the
freestanding array's unsupported debugger operator; successful captures read
`control.values` directly and never call target functions.


### Bounded card repair prototype (not default)

`CARD_DAMAGE=1` adds a separate known-card notification, leaving the existing
unknown-write flag and CPU plane lease unchanged. The Amiga surface retains one
known 88x100 VRAM rectangle; different rectangles before collection fall back
to unknown damage. The screen projects it through all base regions and retains
a repair union for each buffer. Its old moving-window rectangle is repaired
as before and the current window is still composed last. Prefix notification
precedes possible observer flushing. Unusual pitch, wrapping or row-straddling
projection retains full composition. The normal build leaves the experiment off.

**MEASURED:** 1,313 projections match an independent per-pixel oracle, including
clipping and offscreen cases. Native A1200 tests compare 48 bounded compositions
against full redraws in the same buffers, with varied positions, both buffer
ages, different queued card regions, mixed unknown writes and register/window
changes. They pass. The first fixture incorrectly set bitmap padding beyond
pixel 87; it failed the pixel comparison. Correcting its padding to the existing
card-blit contract (also used by cardBlitTest and immutable generated assets)
resolves that failure without changing the repair algorithm.

A same-build benchmark with real display DMA, 16 card-blit/composition/drain
iterations per mode, measures 544,859 vs 278,712 E-clock ticks: about 48.00 vs
24.56 ms per iteration, 48.85% less elapsed time. This includes the card blit,
base repair, current window and DMA completion. It is not a whole-card feeding
or live presentation deadline measurement. Host/platform/native regressions and
the Amiga arithmetic audit pass. Exact ECS/AGA replay and cold live24 checks are
running; do not enable or commit the prototype as accepted before those pass.

Evidence: `tmp/card-damage-host.log`, `tmp/card-damage-regressions.log`,
`amiga/.run/card-damage-mask/gdb-out.log`,
`tmp/perf/Pokeri-card-damage-mask.elf`. Reproduce the projection oracle with
`make harness-card-damage-check`. Native paired counters are
`screen.cardRepairCases`, `screen.cardRepairTicks`, and `screen.cardRepairMismatch`.
The temporary comparison bitmap/reference allocations occur only in explicit
native composition tests; normal play allocates no new pixel buffer.


**MEASURED validation update:** both exact replays pass 262,144 RAM bytes,
524,288 VRAM bytes, 172,064 cropped pixels and 60 AY writes at 7,904,804
instructions / 64,000,008 cycles / 8,679 IRQs. A1200 live24 passes. One ECS cold
run fails before Ready with `serial transmit checksum` at 84,800,000 cycles;
a same-binary retry completes all 24 inputs, and two further startup-only runs
reach zero-credit Ready at 85,760,000 and 85,280,000 cycles. This does not erase
the failure or establish reliable ECS startup. The failed run's old script
continued to cleanup before examining the peer, so its malformed packet was
not captured. The retry's diagnostic used an incorrect `.values` expression
for a C array only after printing successful completion; later captures fix it.

**MEASURED live presentation:** `.run/damage-card-pipeline` completes live24
without errors/resets, with 30 shuffle steps and 60 AY writes. Four bounded
compositions take 0.576–0.640 ms to submit and 2.368–2.944 ms from submission
to publication; their complete hit-to-subsequent-retirement intervals remain
31.936–34.368 ms. Three other samples conservatively use full composition
(unknown damage or shuffle/register changes). Different hands prevent a direct
percentage comparison with the earlier captures. Pipeline offsets in this ELF
are command +$2F6/+ $266, present +$590, armReady +$9C, vbi +$3C.

**DERIVED, investigation pending:** Startup currently enqueues two status
application packets together at opening/closing confirmation and warm startup.
CabinetInput deliberately sends one packet per observed ROM/peer idle boundary.
SerialPeer can begin the second queued packet when its transport state returns
to zero, without rechecking that ROM-idle predicate. This is a possible overlap
hazard, consistent with an older known failure class; it is not yet the proven
cause of this particular intermittent failure. Do not enable bounded damage by
default or call startup reliable solely because subsequent attempts succeeded.
Evidence: `.run/damage-live-ecs`, `.run/damage-ecs-fault`,
`.run/damage-ecs-boot-{a,b}`, `.run/damage-card-pipeline` and
`.run/damage-replay-{aga,ecs}` under `amiga/`.

### Startup status pacing candidate

**MEASURED:** a new synthetic regression fails the previous Startup implementation
because cold opening, cold closing and retained-accounting boot enqueue both
status packets together. The candidate retains the second packet until a new
main-loop observation and the shared ROM/peer idle predicate. Tests cover all
three paths and every independent busy-link condition; the reference suite and
headless host/platform/native-model suites pass. Headless original-ROM cold setup
reaches zero-credit Ready, and a subsequent accounting-retained boot reaches Ready.
No startup clock policy changes are included.

This eliminates a demonstrated queueing hazard; it does not establish the cause
of the earlier intermittent ECS checksum fault. Native live validation is pending
using frozen `tmp/perf/Pokeri-status-pacing.elf` with CARD_DAMAGE=1. The normal
build is restored with CARD_DAMAGE off. Automatic setup event times change, so
new automatic-setup replay comparisons must use matching input schedules.
The first ECS validation was terminated by a shared debugger-port collision;
it supplies no game-failure evidence. The restarted ECS run uses port 2781,
separate from the A1200 run on 2377. Evidence is in `tmp/status-pacing-*` and
`amiga/.run/status-pacing-{ecs,aga}`.

**MEASURED candidate update:** A1200 live24 finishes at 480,000,000 cycles /
4,443 PAL frames with zero errors/resets, 24 inputs, 60 shuffle steps and 120
shuffle AY writes. Ready occurs at 89,280,000 cycles / 1,578 frames. Different
live hands and shuffle counts prevent attributing this whole-run duration to
the status-pacing change. ECS validation remains active.

### Next presentation experiment: completed bounded cards

**DERIVED from Native.cpp:** ordinary presentation only runs once
`nativeCycles-lastPresentCycle >= 160000`; a finished card can wait for this
board-time boundary even when its bounded repair could already be submitted.
The previous broad wall-time presentation experiment remains rejected. A narrower
opt-in experiment should request early presentation only after an actual complete
cache hit, after Ready, outside diagnostic mode and outside the active shuffle
queue. It must require known bounded damage with no generic unknown-write flag,
a free display buffer, and at most one early submission per PAL frame. Pending
buffers must still retire normally; do not wait synchronously or change board
time. Keep a hit pending until submitted, unless shuffle ownership or unknown
damage requires the existing path.

Measure complete-card start/hit, submission/publication/retirement, plus AY
intervals and ordinary live24 throughput. Confirm that the request does not
repeat for unchanged hits or expose partially drawn prefixes. Validate full
ECS/AGA replay and live shuffle ownership. This experiment is not implemented;
bounded repair acceptance comes first.

**MEASURED fresh reference:** the status-pacing A1200 replay passes all 262,144
RAM bytes, 524,288 VRAM bytes, 172,064 cropped pixels and 60 AY writes at
7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs. Native vectors restore
and the replay exits without error. ECS comparison remains pending.

### Bounded card repair accepted

**MEASURED:** the fresh ECS reference comparison also passes all 262,144 RAM
bytes, 524,288 VRAM bytes, 172,064 pixels and 60 AY writes at 7,904,133
instructions / 64,000,000 cycles / 8,685 IRQs. Together with the A1200 comparison,
48 real-blitter composition cases, 1,313 projection cases, paired 48.00 to
24.56 ms composition benchmark and successful ECS/A1200 live24 runs, this
accepts bounded card repair as the default. `CARD_DAMAGE=0` retains comparison
behavior. Earlier intermittent checksum failure remains documented; paced
startup now avoids the proven two-packet queueing hazard, without claiming its
unavailable failed packet was diagnosed. Whole-card/audio deadlines remain open.

A separate local prototype requests early presentation only for
complete card hits and bounded damage, after Ready and outside shuffle ownership,
with a free valid background buffer and once-per-PAL-frame throttling. It is
not accepted for normal play. It changes no guest clock.

### Completed-card presentation candidate: initial measurement

**MEASURED:** frozen `tmp/perf/Pokeri-card-present.elf` with `CARD_PRESENT=1`
completes A1200 live24 twice (ordinary observation and read-only pipeline
breakpoints) at 480,000,000 cycles / 4,224 frames, zero errors/resets, 24 inputs,
30 shuffle steps and 60 shuffle AY writes. It makes four early submissions.
Three free-buffer bounded updates reach subsequent retirement in 12.960, 15.520
and 28.288 ms. Their submission-to-publication intervals are 6.464, 6.016 and
7.328 ms. A fourth bounded update waits for a pending buffer and takes 32.704 ms.
The observed shuffle-owned update keeps its existing path (54.368 ms).

Earlier bounded updates took 31.936–34.368 ms. These are different live hands,
not a controlled percentage comparison. The early request improves some observed
latencies but does not establish the complete-card 20 ms or sound deadline.
Exact ECS/AGA replays and ECS live validation are running; the experiment remains
disabled by default. Normal executable disassembly is identical to the validated
bounded-repair candidate when CARD_PRESENT is absent.

Verified candidate instruction endpoints: CardBackCache::command +$2F6 (start),
+$266 (hit), AmigaScreen::present +$590 (submitted), armReady +$9C (published),
vbi +$3C (retired). Native diagnostic disassembly stays local. Pipeline evidence
is `amiga/.run/card-present-pipeline/gdb-out.log`; local parser
`tmp/card-pipeline-summary.py` ignores old-frame publication before new submission.

**MEASURED command duration from the same captures:** matching each starts
counter increment to its subsequent hit gives 24.544 ms for two lighter cards
and 49.440 / 48.864 / 48.192 ms for three landing updates in the early-present
run. The prior bounded-repair capture has 24.736 / 24.672 ms and 50.144 /
50.208 / 49.056 ms respectively. Other samples take 68–80 ms. These exclude
the first command's lead-in and do not isolate guest execution, exception work,
interrupts or DMA waits. Different hands prohibit attributing small differences
to the new request. Early presentation does not close this command-processing
deficit; lowering post-hit latency must not be reported as a complete-card
20 ms result. Next profile this residual separately from composition.

**MEASURED candidate validation update:** ECS live24 completes at 480,000,000
cycles / 16,108 PAL frames with zero errors/resets, 24 inputs, 30 shuffle steps,
45 shuffle AY writes and five early submissions. Ready occurs at 82,880,000
cycles / 6,189 frames. The different live hand prevents directly comparing its
AY count to A1200's. A1200 exact replay passes all RAM/VRAM/pixels/60 AY writes
at 7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs. ECS exact comparison
is pending. The request remains disabled.

A separate current-default live profile uses only the existing VBI PC sampler
(`native-measure`, no TIME_LEDGER), with no per-access clock calls. It will
attribute samples during the card-drawing cycle interval; it cannot by itself
prove inclusive timings or accurately sample interrupt-masked work.

### Current default VBI profile

**MEASURED:** `.run/current-card-profile` completes live24 with 4,281 samples,
none dropped, zero resets, 24 inputs, 30 shuffle steps and 60 shuffle AY writes.
The 100M–140M cycle interval contains 306 samples: 145 (47.39%) in guest ROM
$02400, 27 (8.82%) in nativeDispatch, 22 (7.19%) in nativeShortLive, eight
(2.61%) in executePreparedHook and six (1.96%) in PAINT eligibility. The guest
bucket is 75 at $2444, 68 at $2442 and two at $2478. These delay-loop samples
are not evidence that removing pacing would improve faithful gameplay.

Ten dispatcher samples occur at +$EC, immediately after `move #$2000,SR`.
**DERIVED:** pending VBI delivery makes this an attribution boundary for preceding
interrupt-masked work, not evidence that the following timing-active read costs
200 ms. The narrower 114M–124M interval has only 93 samples, including 19
dispatcher and 18 guest-delay samples. No single drawing routine dominates this
small sample; do not infer inclusive costs or rank tiny differences. The existing
C1 delay hook already failed to improve animation latency despite freeing CPU.

Evidence: `tmp/current-card-profile.bin`, matching ELF `amiga/.run/current-card-profile/Pokeri.elf`
and its gdb-out.log. The next residual measurement needs to separate masked
entry/accounting from command completion, with observer cost explicitly bounded.

### Completed-card presentation accepted

**MEASURED:** ECS exact comparison passes all 262,144 RAM bytes, 524,288 VRAM
bytes, 172,064 cropped pixels and 60 AY writes at 7,904,133 instructions /
64,000,000 cycles / 8,685 IRQs, matching the A1200 result. Combined with
successful ECS/A1200 live24 and the bounded live latency measurements above,
this accepts the request by default when card caching and bounded repair are
enabled. `CARD_PRESENT=0` retains the previous presentation schedule. Disabling
CARD_DAMAGE or CARD_CACHE disables its default too. No guest clock or shuffle
consumer policy changes. This is a measured local latency improvement, not
completion of the whole-card, audio or ECS real-time targets.

### Aggregate ledger with assembly completion preserved (in validation)

**DERIVED from code:** TIME_LEDGER installs video.commandLog to attribute every
command. CardBackCache::rasterGrant refuses a non-null commandLog, so the full
ledger cannot attribute current normal cached assembly completion. Reader-cost
subtraction cannot correct this changed code path. Earlier full-ledger card
figures must not be treated as the current optimized command path's cost.

The diagnostic-only `TIME_LEDGER=1 LEDGER_FAST_CACHE=1` variant omits that logger,
retains aggregate scopes, and records cache recognition and successful blit
endpoints. Recognition timing is after the first command is accepted because
inline header feeding bypasses push(); this excludes its lead-in. No per-opcode
histogram is available in this mode. A regression verifies exactly one start/hit
without disabling completion grants, alongside full cached-renderer equivalence
(`make harness-fast-cache-ledger-check`). Native capture also prints grant hit
counts and commandLog to verify the intended path is exercised.

Normal executable disassembly matches the accepted build byte-for-byte.
The isolated diagnostic is running under `amiga/.run/fast-cache-ledger`; its
reader correction still does not remove all scope/counter bookkeeping, so
release latency captures remain the deadline authority.

**MEASURED first aggregate capture:** native cached raster/control flags are on
and 632 assembly completions occur. Model regression passes 2,340 differential
cases / 77,920 completion grants, including the new exactly-once timing boundary
check. The native run completes live24, but the old ledger script aborts at an
empty slow-command dump before card endpoints. Its earlier phase dumps are
usable; the script now guards an empty list and a fresh repeat is running.

The 8-board-second deal occupies 10.30 diagnostic wall seconds: corrected
inclusive full C dispatch 2.01 s (3,743 calls), masked prologue 0.32 s (87 us
per full call), short-path C 1.40 s (10,834 calls), C video-write scopes 0.87 s
(2,933 calls), presentation 0.13 s and command blitter waits 0.02 s. Unscoped
hook/exception/IRQ residual is 2.71 s; timestamp observer cost is 0.67 s. Nested
columns must not be summed. This is an instrumented attribution result, not a
release deadline measurement. The analyzer identifies unavailable opcode counts
and unscoped assembly completions rather than showing zero completed commands.
Evidence: `tmp/fast-cache-ledger-model.log`, `tmp/fast-cache-ledger-summary.txt`,
`amiga/.run/fast-cache-ledger/gdb-out.log`; repeat `.run/fast-cache-ledger2`.

**MEASURED complete aggregate repeat:** live24 exits cleanly with 659 assembly
completions and 98 card endpoints, none dropped. Lighter cached cards take
29.122–29.507 ms after timestamp-reader correction, with about 9.6–9.7 ms raw
inclusive C Command scopes and 0–0.038 ms BlitWait. Four slower landing cards
take 59.263–59.944 ms corrected, C Command 13.8–14.2 ms, ShortCall 17.7–18.1 ms,
Service 6.0–6.2 ms and Prologue about 4.0 ms, with zero BlitWait. Inclusive
columns overlap; the remaining instrumentation is not corrected away. Release
measurements (24.5 and 48–49 ms) remain the actual deadline evidence.

The lighter intervals contain 516 hooked operations / 516 short calls; slower
landing intervals have 698–700 hooked operations / 698 short calls. These are
not physical exception counts because the feeder combines operations. The
report now displays both counters. No claim that a single routine accounts for
the entire difference follows. The ten admitted AMOVEs still dispatched to C
are the next bounded assembly-completion experiment, with exact translated
coordinate checks and refusal before mutation required.

Evidence: `.run/fast-cache-ledger2/gdb-out.log`, `tmp/fast-cache-ledger2-cards.txt`
and matching card-cost/events binaries. The first profile's missing endpoint
file is not reused. `LEDGER fastcache=1` in future standard captures identifies
this histogram-free mode; the first captures use the explicit FASTCACHE line.

### Translated cached AMOVE completion (opt-in validation)

`CACHED_ABSOLUTE=1` adds exact translated AMOVE completion for the ten inner
moves of an already admitted card. The initial anchor-setting move, cache
admission, final blit and all observer/scheduler boundaries retain C. Before
mutation the kernel checks length, exact opcode and both signed coordinate
differences against the anchor; overflowed translations and mismatches refuse.
Actual supplied coordinates update CP/DP, reset drawing work/stop and preserve
the CPU lease like ordinary AMOVE. RasterGrant grows from 88 to 92 bytes with
the optional absolute flag at offset 88; native layout assertions cover it.

**MEASURED:** 2,339 model comparisons pass with 89,669 granted completions. Both
standalone and linked-feeder CPU matrices pass 624,960 cases each on 68000/68020,
including coordinate extrema, overflow, changed opcode/X/Y and refusal before
state mutation. Same-binary A1200 benchmark (four cards/mode, final DMA drain
included) measures 95,876 / 92,459 / 79,134 / 73,788 E-clock ticks for no cache
completion / raster / controls / absolute respectively. Controls to absolute
is 27.888 → 26.005 ms/card, a 6.76% reduction. The 660 assembly hits correspond
to 35 + 60 + 70 completions per card across four trials. This does not meet the
20 ms target or prove a live latency gain.

Normal build is restored with CACHED_ABSOLUTE off. Full regression suites,
ECS/AGA exact replays and live validation are running; do not enable yet.
Evidence: `tmp/absolute-{model,kernel,linked}.log`,
`amiga/.run/absolute-benchmark/gdb-out.log`, frozen `tmp/perf/Pokeri-absolute.elf`.

### Translated cached AMOVE accepted

**MEASURED:** standalone and linked CPU matrices each pass 624,960 cases,
including independent absolute/control enable flags. Host/platform/native/short/
feed regressions pass (3,755,520 whole-feed cases). Both ECS and AGA exact
replays match 262,144 RAM bytes, 524,288 VRAM bytes, 172,064 pixels and 60 AY
writes at 7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs.
Both live24 scenarios finish at 480,000,000 cycles with 24 inputs, 30 shuffle
steps and zero errors/resets. A1200 finishes at 4,224 PAL frames (Ready 1,583);
ECS at 16,100 (Ready 6,304). Different live hands prevent an end-to-end speed
claim from these runs.

The 27.888 → 26.005 ms paired isolated benchmark and correctness gates accept
CACHED_ABSOLUTE by default when CACHED_RASTER is enabled. Setting it to zero
retains the earlier command set. Normal builds contain no ledger instrumentation.
The 20 ms whole-card/audio deadline remains open. Evidence: absolute-linked-final.log,
absolute-regressions.log and .run/absolute-{replay,live}-{aga,ecs} (under tmp/ or
amiga/ as appropriate); full-state comparisons use tmp/absolute-{aga,ecs}-reference.

### Precomputed background-guard addresses (unaccepted prototype)

The next candidate stores each guard's bottom-left-relative 608-pixel-row offset
when preparing the cache. Admission still checks the same 68 pixels in the same
order and the same allowed-colour masks, after the existing context/bounds gate.
Guard storage size is unchanged. Host regression adds 7,594,400 address comparisons
against pixelAddress across signed anchors, origin offsets and frame seams;
renderer comparisons pass with ordinary/raster/control/absolute completions.

**MEASURED:** the first candidate native benchmark exits with `native PC outside
ROM/RAM` before recording any card times. The unchanged accepted executable
repeats 95,876 / 92,459 / 79,134 / 73,788 ticks exactly. Do not treat the failed
candidate as a speed result or enable it before diagnosing the failure and
completing native equality/live gates. Evidence: .run/guard-{before,after}.
A follow-up read-only capture is .run/guard-after-debug. The first attempt at
that follow-up omitted fixture inputs and was stopped using its scoped PID;
the prepared rerun is the useful capture.

**MEASURED repeat:** prepared .run/guard-after-debug completes status 4/error 0
with 660 assembly hits. Four-card batches measure 90,794 / 87,509 / 74,487 /
68,805 ticks (none/raster/controls/absolute). With absolute completion this is
26.005 → 24.248 ms/card, 6.75% less, including final DMA drain. The initial
failure remains unexplained and is not hidden by this successful repeat.
Standalone CPU tests pass after sourcing the toolchain; the first Make invocation
had reached all four passing model variants but could not find the assembler.
ECS/AGA exact replays and A1200 live validation are now running in
.run/guard-{replay-aga,replay-ecs,live-aga}. Do not accept the candidate yet.

### Guard-address validation and benchmark race investigation

**MEASURED:** host/platform/native regressions pass. AGA exact replay matches
all RAM/VRAM/172,064 pixels/60 AY writes at 7,904,133 instructions / 64,000,000
cycles / 8,685 IRQs; ECS replay reaches the same endpoint, comparison pending.
Cold A1200 live24 reaches Ready at 89,920,000 cycles / 1,588 frames and exits at
480,000,000 cycles / 4,216 frames. Cold ECS reaches Ready at 85,120,000 cycles /
6,278 frames and exits at 16,137 frames. Both have zero errors/resets, 24 inputs
and 30 shuffle steps. The first A1200 fixture inadvertently copied accounting.bin:
its successful 536-frame Ready is a warm-start check, not cold-start evidence.

**MEASURED:** release endpoint capture .run/guard-card-latency verifies cache
start at command+$2FA and hit at +$26A in the frozen ELF. Lighter cards take
21.280/21.024 ms; three landing updates take 44.800/44.864/44.864 ms. Later update
46.304 ms; first shuffle-owned case 87.104 ms. All24 inputs finish with no error,
60 shuffle AY writes and sampler disabled. Earlier release values were ~24.5/
48–49 ms; different live hands prohibit a controlled percentage speed claim.
The isolated 24.25 ms benchmark and live intervals measure different work.

**MEASURED:** three additional fault-breakpoint repeats all finish status4/error0,
with absolute batches 68,855/68,751/68,806 ticks (24.265/24.229/24.249 ms/card).
No synthetic escape is captured. Logs: tmp/guard-fault-repeat-{1,2,3}-gdb.log.

**DERIVED race:** shortIoCompleted reads pendingFrames then compares seenFrames
with interrupts enabled (frozen ELF instructions $12FC/$1302). Benchmark VBI
increments both. A VBI between these two reads makes old-pending differ from
new-seen and spuriously requests a general guest boundary, although the running
PC is a synthetic benchmark address. This can produce the observed pre-card
`native PC outside ROM/RAM` stop. The original failed interleaving was not
captured, so its attribution remains INFERRED. Normal gameplay does not update
seenFrames inside VBI. Fix the benchmark by leaving guest scheduling counters
unchanged while its display/audio interrupts still run; do not suppress normal
runtime guards or claim the failed measurement was valid.

**MEASURED acceptance:** ECS comparison also passes every RAM/VRAM byte,
172,064 pixels and 60 AY writes at the identical endpoint. Together with both
cold live24 runs, warm A1200, host regressions and repeatable isolated savings,
this accepts precomputed guard offsets. The diagnostic-only counter race is a
separate open fix; no failed capture contributes to the speed result. The
normal executable is restored to the validated guard candidate. Whole-card
20 ms/audio and overall real-time targets remain open.

### Isolated benchmark frame-counter fix (in validation)

The benchmark candidate now leaves pendingFrames/seenFrames unchanged in VBI,
instead of incrementing both. Normal gameplay still increments pendingFrames;
Paula and screen VBI run first in both modes. This removes the two-read race
without masking interrupts, relaxing guest-PC guards or changing game timing.
The benchmark script now prints both scheduling counters and display frames.

**MEASURED:** .run/counter-benchmark exits status4/error0, 660 assembly hits,
68,721 ticks for four absolute-completion cards (~24.22 ms/card). Guest counters
remain 0/0 while screen.frames reaches 580: display work and interrupts continue.
ECS/AGA replays and consecutive cold A1200/ECS live24 validation are running in
.run/counter-*; candidate is not yet accepted. Frozen executable:
tmp/perf/Pokeri-benchmark-counter(.elf). Normal amiga/out remains the accepted
5576d48 guard optimization until these checks finish.

**MEASURED counter-fix acceptance:** ECS and AGA comparisons match all 262,144
RAM bytes, 524,288 VRAM bytes, 172,064 pixels and 60 AY writes at the unchanged
7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs. Cold A1200 live24 finishes
at 4,228 PAL frames (Ready 1,597); cold ECS at 16,115 (Ready 6,250). Both complete
24 inputs and 30 shuffle steps with no error/reset. This accepts the isolated
benchmark-counter fix. Normal release is the frozen Pokeri-benchmark-counter.

**Verification correction:** a fresh build found the new guardAddressCheck test
calling a private pixelAddress helper; the previous test run had compiled before
that addition. The earlier independent Python arithmetic proof and native replay
results remain valid, but did not prove the new C++ regression built. Commit
7b68efc repairs it through public AMOVE execution/resulting DP registers and sets
4-bpp mode explicitly. A fresh complete harness-card-cache-check/harness-raster-check
now passes all four renderer variants, each including 7,594,400 address probes,
and 624,960 standalone 68000/68020 CPU cases. Evidence:
tmp/guard-address-test-repair.log. Avoid editing test sources during a running build.

### Reusing planar words in background checks (in validation)

The candidate reads a synchronized four-plane word once for consecutive guards
in that word, then extracts their colours locally. It preserves guard order,
allowed-colour predicates and early failure; no writes occur between reads.
An unsupported readPlanes4 falls back to scalar pixel4 for that admission.
Scratch storage is four words plus a last-word key, with no heap allocation.
The differential matrix now exercises both planar and forced-scalar reads,
both storage layouts, all16 alignments and all18 background classes. Ordinary
mode passes 2,912 cases; each grant mode passes 2,915, with 123,269 completions
in absolute mode. All run the repaired public-AMOVE address probe. Standalone
CPU tests pass 624,960 cases. Evidence: tmp/guard-word-model.log.

**MEASURED:** identical A1200 benchmark configuration changes four-card absolute
batch from 68,721 to 66,666 ticks: 24.219 → 23.494 ms/card (2.99%). Counter freeze
remains 0/0 with580 display frames, status4/error0, 660hits. This is not yet a
live latency result or acceptance. Candidate tmp/perf/Pokeri-guard-word(.elf),
.run/guard-word-benchmark. ECS/AGA exact replays, both cold live24 and broader
host regressions are running. Normal release remains the accepted counter fix.

**MEASURED startup preparation scope:** accepted build .run/startup-cache-cost
records nativeCardPrepareTicks=1,991,910 at709,379Hz (~2.808s), before guest
execution. This is separate from the Ready PAL-frame count, which starts later.
The measured run exits cleanly. An independent startup target is Canvas::fill:
its preparation-only rectangle loop currently calls plot4/index (including
coordinate division) for every pixel. Resolving rectangle coordinates once
could reduce preparation without changing the startup clock or requiring
prebuilt artwork. This has not been implemented or measured yet.

**MEASURED planar-word acceptance:** both ECS and AGA exact comparisons match
all262,144 RAM bytes,524,288 VRAM bytes,172,064 pixels and60 AY writes at
7,904,133 instructions /64,000,000 cycles /8,685 IRQs. Both cold live24 runs
finish all inputs with30 shuffle steps, no errors/resets: A1200 4,221 frames
(Ready1,588), ECS16,083 (Ready6,276). Host/platform/native suites also pass.
This accepts planar-word reuse; normal release is frozen Pokeri-guard-word.
The23.494ms isolated card result remains above20ms. No new live card-endpoint
latency claim is made from these functional runs.

### Startup canvas coordinate reuse (in validation)

The candidate resolves rectangle coordinates once before filling local pixels
and coverage bits, preserving packed-colour nibble phase and zero extents.
Invalid rectangles still fail preparation; their discarded partial canvas is
not observable. A small last-word coordinate cache also shares index arithmetic
between reads/writes of the same four-pixel word. Its key includes the origin
and frame-mask effect through the normalized address delta; uncommon shifts
retain the full calculation. This affects preparation only, not guest timing.

A new synthetic test includes the private canvas implementation in its test
translation unit; no production API is exposed. Its independent scalar oracle
uses ordinary floor-division coordinates, without the candidate index cache.
All69,888 rectangle cases pass, including origins, alignments, nibble phases,
existing coverage, boundaries and zero extents. The full2912-case renderer
comparison also passes; remaining grant/CPU/platform suites are running.

**MEASURED:** accepted baseline preparation is1,991,910 E-clock ticks (2.808s).
Rectangle-only prototype is1,760,479 (2.482s), combined candidate1,715,503
(2.418s), saving0.390s before guest execution. This is not a0.390s reduction
in the later Ready PAL-frame count, nor completion of the cold-start target.
All three scoped startup captures exit cleanly. Evidence: .run/startup-cache-cost,
startup-canvas-cost and startup-prepare-cost; tmp/card-canvas-model.log.
Frozen combined candidate: tmp/perf/Pokeri-card-prepare(.elf). Both exact
replays, both cold live24 and regression suites are running under card-prepare
prefixes. Normal executable remains the accepted guard-word release.

Startup-only fast-forward still needs approval. The explicit question was
presented again while independent preparation/rendering work continues; no
startup clock or audio policy has been changed.

**MEASURED preparation acceptance:** both exact ECS/AGA comparisons match all
262,144 RAM bytes,524,288 VRAM bytes,172,064 pixels and60 AY writes at
7,904,133 instructions /64,000,000 cycles /8,685 IRQs. Cold live24 passes with
no errors/resets and all24 inputs/30 shuffle steps: A1200 finishes4,221 frames
(Ready1,595), ECS16,114 (Ready6,271). Synthetic canvas, four cache/grant modes,
CPU, host, platform and native suites pass (tmp/card-prepare-regressions.log).
This accepts the preparation changes. The saved0.390s is preparation time only;
original initialization/refill and the pending timing-policy decision remain.
Normal release is the frozen Pokeri-card-prepare build.

### Complete-card ledger preserving assembly grants (2026-09-28)

**MEASURED:** the isolated benchmark now records complete-card boundaries with
TIME_LEDGER=1 LEDGER_FAST_CACHE=1, including the final DMA wait, without disabling
assembly grants or enabling opcode logging. Inner cache endpoints are suppressed
only during this benchmark. Fast-cache mode omits the redundant header observer;
the model retains successful-recognition events for live measurements. Diagnostic
mode metadata is retained by a volatile read: the former unreferenced variable
was removed by the linker, making its DWARF-only GDB value unreliable.

The corrected capture (.run/raster-ledger-benchmark2) completes with status4,
error0, fastcache1, 660 assembly hits,32 endpoints,33 events and no drops. Guest
frame counters remain0/0 while display frames reach580. Its four absolute-mode
cards take27.31–27.84ms raw,26.55–27.09ms after clock-read correction. Inclusive
short C calls account for5.42–5.87ms (command work4.61–5.06ms is nested within
that), and final blitter waits3.16–3.24ms. Do not sum nested scopes. These
instrumented totals are NOT the23.49ms release result: subtracting clock reads
does not remove all observer overhead, nor does this synthetic feeder measure
whole-game scheduling. Command feeding remains a target, not a proven complete
attribution of the remaining release time.

Linked validation passes3,755,520 whole-feed cases,502,272 fused-feed cases,
131,072 header cases, boundary/source-guard cases, and624,960 raster cases each
in standalone and linked modes. Evidence: tmp/raster-ledger2-{feed,raster}-check.log
and tmp/raster-ledger2-{card-cost,events}.bin. The normal build has been restored;
all allocated ELF sections match the accepted Pokeri-card-prepare executable.
This diagnostic change does not close the rendering/audio or cold-start gates.

### Rejected short-command copy experiment

**MEASURED:** direct one/two/three-word copies in the cached assembly kernel
pass624,960 standalone and624,960 linked CPU-state cases, but the isolated
A1200 absolute-mode four-card batch takes66,911 ticks versus66,666 baseline:
23.581 versus23.494ms/card (0.37% slower). This does not establish a benefit;
the candidate is discarded. Extra dispatch branches are a plausible cost, not
an independently measured attribution. Evidence: .run/short-copy-benchmark,
tmp/short-copy-linked-check.log; frozen candidate tmp/perf/Pokeri-short-copy.
The normal release remains unchanged.

### White-prefix reconstruction experiment (not retained)

The isolated card benchmark now additionally feeds exactly29 prefix commands,
flushes the cached white image and waits for DMA completion, in each of the
same four assembly-grant modes. Input preparation stays outside timing.

**MEASURED:** returning immediately after a successful white stamp at the exact
prefix boundary, instead of reconstructing unused shadow state, passes3032
differential cases including all60 original face-up selectors. It does not
improve the complete native operation: the absolute-mode four-prefix batch
changes32,162 to33,967 ticks,11.335 to11.971ms per prefix. Both benchmark runs
finish status4/error0 with856 assembly completions and580 display frames.
Full backs remain23.470/23.491ms. The runtime shortcut is removed.

**DERIVED:** shadow reconstruction follows asynchronous blit submission and can
overlap DMA, so its CPU cost is not necessarily additive elapsed time. The
precise reason for the measured regression has not been isolated; no claim
that all the difference is intrinsic to the shortcut is made. Evidence:
.run/white-boundary-{base-benchmark,benchmark}, tmp/white-boundary-model.log.
The new white-prefix measurement is retained to distinguish this workload from
complete backs. Rendering/audio deadlines remain open.

### Temporary drain priority rejected: VBI deadline regression

**MEASURED:** setting BLITHOG only inside blitterDrain and restoring its prior
value reduces the isolated absolute-mode back batch66,595 to65,317ticks
(23.470 to23.019ms/card,1.92%). White prefixes change32,162 to32,031ticks
(0.41%). AGA replay still matches all262,144RAM bytes,524,288VRAM bytes,
172,064pixels and60AY writes at7,904,133instructions/64,000,000cycles/8,685IRQs.

**REJECTED:** matched A1200 cold live tests with optional VBI_LATENCY sampling
show a display/audio servicing regression. The sample reads the beam AFTER
Paula and screen VBI work, before the remaining scheduler work; it measures an
upper bound on entry lateness, not pure interrupt-entry latency. Both builds
use identical instrumentation, absent from normal builds. Rows separate
startup/gameplay and whether BLITHOG is set at sampling.

Baseline gameplay:2,624 samples, maximum line11, zero at line29 or later.
Candidate gameplay:2,514 ordinary samples (max11,zero late),115 priority samples
(max207,72 at line29 or later). Both complete24inputs/30shuffle steps/60in-motion
AY writes with no error/reset. Baseline startup has2late samples (max174);
candidate startup has2ordinary late (max134) plus14priority late (max184).
Thus the small local blit saving does not satisfy the display/audio constraint.
The runtime change is removed; do not enable it based on replay equality alone.

Evidence: .run/drain-priority-benchmark, .run/drain-vbi-{off,on}-aga,
tmp/drain-priority-aga-compare.log. ECS comparisons started before the AGA
regression was known; results are supplementary, not a gate to reverse rejection.
Normal executable remains the accepted Pokeri-card-prepare build. The extended
white-prefix benchmark and optional post-service VBI sampler are retained as
diagnostics. No release performance improvement is accepted from this experiment.

**Supplementary ECS completion:** exact replay also matches all RAM/VRAM/pixels/AY
at the same endpoint (tmp/drain-priority-ecs-compare.log). Both live24 runs finish
all inputs/30shuffle steps/45in-motion AY writes with no error/reset. Baseline
Ready/end6,271/16,158PAL frames; candidate6,290/16,065. Different live hands and
schedules prevent attributing the whole-run difference to the local change.
Baseline gameplay post-service VBI samples:9,887,maxline55,5,977atline29+.
Candidate without priority:9,598,max66,5,950late; with priority:177,max303,167late.
ECS already has considerable post-service VBI cost; temporary priority makes
the worst observations substantially later. The experiment remains rejected.
All its diagnostic processes have completed.

### Packed drawing-position update accepted

**DERIVED:** DP high/low together encode origin plane bits, the20-bit word
address shifted left4 and the within-word dot shift. The cached assembly kernel
now constructs this32-bit value directly and stores it as one aligned long,
instead of splitting and recombining the two register words. No command guards,
IRQ boundaries or clock policy change.

**MEASURED:** paired four-back batches66,595/66,608baseline versus66,418/66,464
candidate ticks show a small consistent0.05–0.06ms/card saving (~0.2–0.3%).
White-prefix batches show no consistent gain.624,960standalone and624,960linked
CPU cases preserve exact state/registers. Exact ECS/AGA replay matches262,144RAM
bytes,524,288VRAM bytes,172,064pixels and60AY writes at7,904,133instructions,
64,000,000cycles and8,685IRQs. Cold live24 completes with zero errors/resets and
30shuffle steps on both: AGA Ready1,590/end4,230frames, ECS6,305/16,144.
These live runs are functional evidence, not a whole-game speed comparison.

Evidence: .run/packed-dp-{benchmark,repeat,base-repeat,replay-aga,replay-ecs,
live-aga,live-ecs}; tmp/packed-dp-{aga,ecs}-compare.log and
tmp/packed-dp-linked-check.log. This accepts only the packed-position update.
The separate recipe-reconstruction/no-copy candidate remains under test.
Normal executable is the frozen Pokeri-packed-dp. Card/audio20ms and sustained
real-time targets remain unmet.

### Repeated cached-back PC sampling

Optional RASTER_SAMPLES=1 executes512complete backs with all assembly grants
enabled and samples interrupted PCs with the existing VBI wrapper. Sampling
is enabled only during command feeding and final DMA completion, excluding
context setup/clearing. No per-word timestamps are added. The ordinary build
retains its original four-mode/four-trial benchmark.

**MEASURED, experimental recipe-reconstruction candidate:**602samples,zero
drops,35,840assembly completions, status4/error0, guest frame counters0/0.
The isolated batch takes8,459,989ticks. Symbol buckets: source-ready132(21.9%),
feed-head110(18.3%), blitter wait90(15.0%), cached completion70(11.6%).
These are sampled-PC buckets, NOT inclusive function times. In particular,
115samples hit offset$2B160 and91hit$2B084, the instructions immediately after
unmasking interrupts in this frozen ELF. Those206samples (34.2%) include
deferred observations of masked work; attributing them to the following ADD
instruction would be wrong. Masked/higher-priority work remains biased.

**DERIVED next investigation:** register boundary0 masks interrupts, checks
frame/pending state; branch bookkeeping then reaches boundary1 without any
unmask or device call. Establish whether rechecking unchanged scheduler state
there is redundant, including every exit/CCR/source-guard case. No removal or
new scheduling policy is implemented by this sampling change.

Evidence: .run/cached-raster-samples, tmp/cached-raster-samples.bin,
tmp/perf/Pokeri-raster-samples.elf; analyze with host/native_profile.py. The
recipe-reconstruction optimization itself is still under validation and is not
accepted by these measurements.

### Reconstruct accepted cache commands instead of copying them

**DERIVED:** accepted commands exactly match the immutable prepared recipe,
except AMOVE coordinates verified against the common anchor. Fallback can
therefore reconstruct each accepted word from that recipe and anchor. The cache
no longer copies each command into a520-byte private buffer. The native grant
keeps an optional capture destination for its independently tested capture path;
production uses null and skips that copy. Exact comparisons, pending FIFO bytes,
position/work/register updates and scheduler boundaries are unchanged.

**MEASURED:**3,032 differential cases include all60face-up selectors; each of
three grant modes passes2,915cases with mutation/observation/snapshot coverage.
Independent standalone and linked CPU matrices each pass1,249,920cases covering
null/captured destinations, every refusal case, registers and full scratch state.
Host/platform/native regression suites pass. Both exact ECS/AGA replays match
262,144RAM bytes,524,288VRAM bytes,172,064pixels and60AY writes at the unchanged
7,904,133instructions/64,000,000cycles/8,685IRQs. Both cold live24 runs complete
without error/reset: AGA Ready1,591/end4,238PAL frames, ECS6,304/16,129.
Each has24inputs/30shuffle steps; AGA60andECS45in-motion AY writes.

The isolated absolute-mode four-back batch is66,001ticks (~23.26ms/card),
versus66,418/66,464 for the preceding packed-position build; white prefixes
31,827 versus32,168/32,114. This small isolated saving is not a whole-game
speed claim. Evidence: .run/recipe-replay-{benchmark,replay-aga,replay-ecs,
live-aga,live-ecs}; tmp/recipe-replay-{model,grants,linked-check,regressions}.log
and tmp/recipe-replay-{aga,ecs}-compare.log. This accepts recipe reconstruction.
Normal executable becomes Pokeri-recipe-replay; duplicate-poll removal remains
a separate opt-in experiment. Full-card/audio and sustained-real-time gates
remain open.

### Joined masked branch check accepted

**DERIVED:** live register-feed boundary 0 sets physical IPL7 and checks both
scheduler fields. The intervening branch bookkeeping neither unmasks interrupts
nor calls a device. Boundary 1 can therefore reuse that check. The default
`JOIN_BRANCH_BOUNDARY=1` path removes only the duplicate poll; original
PC, CCR, cycle updates, source guards and remaining boundaries stay intact.

**MEASURED:** the isolated four-back batch takes 63,600 E-clock ticks versus
66,001 for recipe reconstruction: **23.260 -> 22.414 ms/card (3.64%)**. White
prefixes take 31,452 versus 31,827 ticks per batch. This is not a whole-game
speed claim and still misses the complete-card 20 ms target.

**MEASURED:** the independent original-instruction oracle passes 3,755,520
whole-feed cases for both candidate and unchanged baseline with physically
queued interrupt events. An event requested inside the IPL7 branch interval
becomes visible only after unmasking, with assertions on the mask and unchanged
fields before delivery. The unchanged baseline also passes the older abstract
event matrix. Source guards, shuffle markers and fused-entry checks pass.

Both cold live24 runs finish with zero errors/resets and all 24 inputs/30 shuffle
steps: AGA Ready/end 1,591/4,224 PAL frames, 60 in-motion AY writes; ECS
6,240/16,038 frames, 45 writes. Both ECS and AGA replays match all 262,144 RAM bytes,
524,288 VRAM bytes, 172,064 pixels and 60 AY writes at 7,904,133 instructions,
64,000,000 cycles and 8,685 IRQs. JOIN_BRANCH_BOUNDARY=0 retains the old poll;
INLINE_BOUNDARY=0 also defaults to that comparison path. Frozen validated build:
tmp/perf/Pokeri-join-boundary.
Evidence: .run/join-boundary-{benchmark,replay-aga,replay-ecs,live-aga,live-ecs};
tmp/join-boundary-{physical-on,physical-off,abstract-off,aga-compare,ecs-compare}.log.

The normal default rebuild has identical allocated ELF sections to the frozen
validated candidate. The native arithmetic audit passes. No profiling code is
enabled by this change.

### Source-span guard reuse accepted

**DERIVED:** within the borrowed live register feeder, the source cursor advances
only by two-byte reads, except at an explicit ring wrap. Physical ROM/RAM bounds
are fixed throughout this scope. FEED_SOURCE_SPAN=1 (default on) retains the
complete guard for the first read and caches the admitted region's end minus
one in preserved D7. Subsequent unsigned cursor values below that exclusive
bound remain even, above the already checked lower bound, and leave room for
the whole word. Zero is an invalid initial bound. Wraps clear it; promotion
restores D7 and discards the borrowed span. A region crossing goes through the
complete guard again before any read. No scheduling boundary is removed.

**MEASURED:** the extended independent original-instruction oracle passes on the
unchanged joined-boundary baseline. Its 1,152 added cases read several valid
words before even/odd physical region ends, invalid low/odd/overflowing wrap
targets, and valid wraps into ROM or RAM. Invalid reads are additionally trapped
in the host memory callback; output, PC, CCR, cursor, nominal cycles, registers
and stack must match. The existing 3,755,520 whole-feed cases also pass.
The candidate passes the same complete CPU matrix. The isolated four-back
batch measures 61,034 ticks versus 63,600: **22.414 -> 21.510 ms/card (4.03%)**.
White-prefix batches measure 30,822 versus 31,452 ticks. Status/error are 4/0,
856 cache hits, with guest frame counters frozen at 0/0 and 580 display frames.
Both ECS/AGA full replay comparisons and cold live24 validation pass.
FEED_SOURCE_SPAN=0 retains the measured comparison path. Evidence: tmp/source-span-{baseline,candidate}-check.log and
.run/source-span-benchmark. Frozen candidate: tmp/perf/Pokeri-source-span.
Normal out remains the accepted joined-boundary build.

**MEASURED live validation:** both source-span cold live24 runs complete with
status4/error0, zero resets, all24 inputs and30 shuffle steps. AGA Ready/end
1,582/4,211 PAL frames with60 in-motion AY writes; ECS6,298/16,019 with45.
Exact replay comparisons remain pending. A separate release endpoint capture
is running in .run/source-span-card-latency: verified command offsets +$2A0
after starts increment and +$21A after successful cardBlit/hits increment.
This measures actual gameplay intervals, not the isolated synthetic batch.

**MEASURED, completed release capture:** source-span-card-latency shows
lighter matching-card intervals of17.472/18.336ms and landing intervals of
42.688/42.752ms, using PAL frame count plus beam position at the verified
endpoints. The initial shuffle-owned interval is78.592ms; a later update is
62.944ms. All24 inputs/30 shuffle steps finish with60 in-motion AY writes,
status4/error0/reset0 and sampler disabled. These are elapsed recognition-to-hit intervals, not blitter-only
times; they include intervening work/waits and exclude the initial admission
snapshot and final DMA retirement. Different live hands prohibit a controlled
percentage comparison to the older21/45ms measurements. Landing/audio deadlines
remain open even when a lighter interval is below20ms.

A1200 source-span exact comparison passes: all262,144 RAM bytes,524,288
VRAM bytes,172,064 pixels and60 AY writes at7,904,133 instructions,
64,000,000 cycles and8,685 IRQs (tmp/source-span-aga-compare.log). ECS
replay remains running. A separate TIME_LEDGER=1 LEDGER_FAST_CACHE=1
FEED_SOURCE_SPAN=1 live capture is running in .run/source-span-ledger to
attribute the remaining longer updates. It is diagnostic only; normal out
was restored to the accepted joined-boundary build after freezing it.

**MEASURED final gate:** ECS also matches all262,144 RAM bytes,524,288 VRAM
bytes,172,064 pixels and60 AY writes at the same7,904,133 instructions,
64,000,000 cycles and8,685 IRQs. Source-span reuse is accepted by default.
The20ms complete-card and gameplay/audio deadlines remain open.

Normal default allocated ELF sections exactly match the validated frozen
source-span candidate; the build arithmetic audit passes.

**MEASURED diagnostic attribution:** source-span-ledger completes24 inputs,
30 shuffle steps and60 in-motion AY writes with error/reset0. Cache grants
remain enabled (751 completions, commandLog=null). No event/card records drop.
Lighter cached intervals contain516 hooked operations; normal landings698–699,
and the later slow interval749. After subtracting timestamp-reader cost, light
intervals take21.45–21.71ms and normal landings50.50–52.52ms. Raw inclusive
C-command time is4.37–4.66ms for light updates,8.76–9.70ms for landings;
normal landing blitter waits are0–0.044ms. These scopes overlap and observer
overhead remains; release timings above remain authoritative.

AY write-to-Paula application peaks at19.157ms in this run. That measures only
application after the guest writes, not delayed generation of the write. The
312.069ms maximum envelope-only change gap is not a reference deadline proof.
Evidence: tmp/source-span-ledger-{cards,audio}.txt and .run/source-span-ledger.
Next attribution target is the extra~182 accesses during normal landings.

### Landing overhead identified: FIFO interrupt service

**MEASURED:** the DISPATCH_PROFILE-only read-only endpoint capture contrasts
516-access light cards (258 WFR tests plus258 FIFO writes) with a698-access
landing: the same258 writes, plus26 handler status tests,104 address-register
writes,26 data-register byte writes and26 virtual RTEs. Full C promotions
rise from1 to29;26 push the original virtual interrupt. See rom-set.md for
WFE/WFR terminology and PCs. This is the next service-path optimization target;
no interrupt suppression or handler replacement is authorized by this finding.
The capture completes24 inputs with status4/error0/reset0; this hand runs
60 shuffle steps and120 in-motion AY writes. Nine completed card pairs
reconcile: four light cards have516 accesses and no virtual exception, while
normal longer cards have698–701 accesses and26–27 virtual exceptions. Its
elapsed times are not release timings. Local report:
tmp/card-handler-counts-report.txt; capture .run/card-handler-counts.

### Bounded FIFO-control fusion accepted

**MEASURED:** the user-approved two three-write control sequences now share
one exception when no intermediate event is due. Every original MOVE retains
its exact IRQ/stop boundary; enabling WFE may still interrupt before write three.
The isolated 512-triplet batch improves from 230.28 to 177.36 µs per triplet
(23.0%). Release landing intervals are 40.832–42.176 ms; different hands prevent
a paired speedup claim. The 20 ms complete-card/audio deadline remains open.

The independent CPU/model matrix (200,704 cases), existing feeder/short-hook
regressions, native host suite, exact ECS/AGA RAM/VRAM/pixel/AY replay and both
cold live24 scenarios pass. Default `FIFO_CONTROL_FUSION=1` retains `=0` for
comparison. See native-fifo-control-plan.md for results and evidence. The next
remaining target is the cost of admitting the 26 original FIFO interrupts per
landing, without suppressing or deferring any of them.

### Isolated IRQ-service attribution

**MEASURED:** optional `IRQ_BENCHMARK=1` adds a pre-game batch only; normal
builds contain no code/data for it. With the accepted FIFO-control fusion,
512 synthetic pending-video IRQ admissions take 85,522 E-clock ticks through
`nativeDispatch(11)`. Context setup alone takes 16,376 ticks; context plus
`pushException` takes 30,930; context plus IRQ/vector lookup takes 28,711.
At 709,379 Hz these are 235.47, 45.09, 85.16 and 79.05 µs/call respectively.
The approximate context-subtracted full-dispatch cost is 190.38 µs, or 4.95 ms
for 26 calls. This excludes physical exception entry/exit and accumulated
guest/nominal clock grants. It is not an inclusive live IRQ cost or a claim
that the remaining landing deficit is explained. No per-operation timer calls
are added. Source selection, exactly 512 admissions, no board/frame advance
and no fault are checked; the synthetic stack and source state are restored.
Local evidence: amiga/.run/irq-attribution/gdb-out.log,
tmp/perf/Pokeri-irq-attribution(.elf). The normal build is restored.

The user approved startup-only fast-forward on 2026-09-29. Its implementation
and validation are recorded below; gameplay timing is unchanged.

### Startup-only prototype (2026-09-29)

**MEASURED:** approved startup-only unpaced time, bounded delay execution,
muted preparation audio and 1 Hz progress presentation reduce cold A1200 Ready
from 31.04 to 23.68 s; warm A1200 reaches Ready in 9.64 s and cold ECS in
116.80 s (previous 124.84 s). All cold/warm ECS/AGA live24 scenarios pass;
saved nonzero balances and original interrupted-hand recovery are verified.
Exact ECS/AGA replay remains identical. See startup-fast-forward-design.md for
its proof, failed intermediate experiments, preserved pseudo-random state
semantics and activation gates. This initially remained build-opt-in pending the activation gates below.
The remaining cold-start gap and gameplay rendering/audio deadlines stay open.

**Activation:** startup fast-forward is now the normal policy; explicit clock
comparisons keep the prior behavior. Paired preparation-inclusive A1200 totals
are 34.78→26.88 s cold and 13.72→12.88 s warm. Native corrupt-save rejection
leaves the file unchanged and reaches post-destructor cleanup without an alert
or leaked tracked memory. Three diagnostic boundary timestamps are absent from
normal builds. See startup-fast-forward-design.md for scope and remaining gaps.

### Enabled-source IRQ query experiment (rejected)

**MEASURED:** querying only enabled HD63484 sources avoids assembling unused
read-FIFO flags, but did not demonstrate a useful full-dispatch saving. Matched
512-admission A1200 batches (709,379 Hz) measured before/after ticks:
full dispatch 86,706/85,531; context-only 17,527/16,432; source selection
29,666/28,344; frame construction 31,373/31,470. Context-subtracted full
dispatch is 69,179/69,099 ticks, only 0.12% different. Source selection alone
falls from 12,139 to 11,912 corrected ticks (1.87%); differing context costs
limit attribution. This does not justify retaining the extra branch-heavy
implementation. The original query is restored; no gameplay timing changes.

The independent synthetic regression remains: all 256 status values and
256 enable masks across empty/partial/full/overfull read queues, pending
commands and presentation holds (1,572,864 cases). Both packed/planar test
runs and the complete headless model/platform/native suites pass.
Evidence: tmp/irq-mask-checks.log, tmp/irq-mask-host.log,
amiga/.run/irq-mask-{before,after}/gdb-out.log. Initial incomplete-fixture
runs were stopped and are retained as missing-rom logs, not measurements.

**DERIVED next target:** disassembly of pushException shows six byte stores
and bytewise vector reconstruction. An aligned native word/longword frame
path can reduce this work without changing interrupt selection or delivery;
unaligned cases must preserve the existing behavior and all stack checks.

### Native exception-frame word operations (accepted)

The default `EXCEPTION_FRAME_WORDS=1` keeps pushException's virtual SR/stack
switching, bounds rejection, IRQ vector selection and scheduling unchanged.
A C-ABI assembly helper writes SR/PC and reads the target using word/longword
operations when both pointers are even; odd frame/vector pointers retain the
bytewise behavior on both 68000 and 68020. `EXCEPTION_FRAME_WORDS=0` excludes the helper and retains the comparison path.

**MEASURED:** 131,072 independent linked-kernel CPU cases pass, covering every
SR value, even/odd frame and vector addresses, exact six-byte frame, surrounding
memory, target PC and preserved C-ABI registers. The first prototype performed
alignment selection in C and caused extra register saves; its corrected frame
batch was 15,105 ticks versus the original 13,846, so it was rejected. Moving
selection into assembly preserves the smaller original C prologue. The revised
512-call frame batch takes 30,423 ticks including 17,527 context ticks, versus
31,373/17,527 originally: corrected 12,896 versus 13,846, **6.86% less**.
Full dispatch improves only from 86,706 to 86,276 ticks (0.62% after context
subtraction), about 1.18 microseconds per admission at 709,379 Hz.
That is only about 31 microseconds for 26 landing interrupts; the rendering
deadline remains unmet. This is no claim that frame stores dominate landing.
Evidence: tmp/exception-frame-v2-cpu.log,
amiga/.run/exception-frame-v2/gdb-out.log, baseline irq-mask-before.

The release A1200 cold live24 run passes with 24 inputs, 30 shuffle steps,
60 in-motion AY writes and no errors/resets (Ready/end 1,186/4,090 PAL frames).
ECS cold live24 also passes (Ready/end 5,834/16,429 frames, 24 inputs,
30 shuffle steps, 45 AY writes, no error/reset). Linked short/whole-feed/FIFO
CPU regressions pass. AGA exact replay matches all 262,144 RAM bytes,
524,288 VRAM bytes, 172,064 pixels and 60 AY writes at 7,904,133 instructions,
64,000,000 cycles and 8,685 IRQs. ECS exact replay matches the same complete
state and counters. Both compare logs are tmp/exception-frame-{aga,ecs}-check.log.
The optimization is accepted by default as a small service-cost reduction;
the card/audio and real-time deadlines remain open. The flag-off build was
verified to match the previously accepted allocated sections exactly.

### Deferred clock accounting attribution

**MEASURED:** optional `CLOCK_BENCHMARK=1` runs 24 pairs of 512-call batches
before original execution. Each pair resets identical clock/deferred state;
one invokes nativeClockPause and the other measures setup only. Workloads
combine guest cycles 0/208/4096, nominal cycles 0/4200, wall debt 0/160000,
and queued phase 0/72000, using the accepted gameplay ratio/window (4×/3).
Only batch endpoints read time. The benchmark checks exact resulting phase,
queued ticks, credit/debt and drained counters, with zero board cycles, wall
frames or faults, then restores the saved state. It changes no clock policy.

At 709,379 Hz, context-subtracted empty calls cost about 8 microseconds;
nominal-only calls about 51, guest-only about 59, and both sources about
99–102 microseconds. The 208-guest/4200-nominal/debt160000/phase0 case costs
36,042 corrected ticks per 512 calls, **99.24 microseconds each**, or about
2.58 ms for 26 calls. The earlier zero-deferred IRQ batch excluded this work.
These synthetic contexts do not prove every live interrupt has these amounts;
they identify a potentially material cost without hot-path timer probes.

Evidence: amiga/.run/clock-attribution/gdb-out.log,
tmp/clock-attribution.csv and tmp/perf/Pokeri-clock-attribution(.elf).
The diagnostic exits with status4/error0/frames0/cycles0. The normal build's
allocated sections exactly match the accepted exception-frame release with
the option omitted. Reusable reader: amiga/clock-benchmark.gdb.

### Clock compiler inlining (accepted)

**MEASURED:** the native clock path calls accountGuestCycles, which calls the
unchanged LiveClock::grant algorithm. Three compiler-only variants retain all
branches, two deferred-source grants, volatile frame observations and policy
calculations. The 24 paired batch assertions pass in every variant.
For the representative guest208/nominal4200/debt160000/phase0 row:

| Inlining | Corrected call time | Relative to baseline |
|---|---:|---:|
| Baseline | 99.234 µs | — |
| Grant only | 105.272 µs | 6.1% slower |
| Accounting only | 89.804 µs | 9.5% faster |
| Both | 87.813 µs | 11.5% faster |

The combined variant is accepted by default. Options
CLOCK_INLINE_GRANT=1 and CLOCK_INLINE_ACCOUNT=1 apply always_inline only;
either can be set to 0 for comparison.
no timing calculation, IRQ priority or service boundary is changed.
The independent clock oracles pass 8 million prior-policy and 8 million
bounded-window transitions. Native arithmetic audits pass. Both cold live24
and exact ECS/AGA replay, plus linked CPU regressions, are being validated
against tmp/perf/Pokeri-clock-inline-release(.elf). Prepared release card
latency comparisons must determine whether the larger code improves live
landings; the isolated 11.5% number is not a gameplay speedup claim.
Evidence: tmp/clock-inline-comparison.csv, tmp/clock-inline-oracle.log,
amiga/.run/clock-inline-{grant,account,both}/gdb-out.log.

**Completed live/CPU gates:** AGA Ready/end 1,176/4,075 PAL frames; ECS
5,763/16,240. Both finish all 24 inputs and 30 shuffle steps without errors
or watchdog resets, with 60/45 in-motion AY writes respectively. Linked
short, whole-feed and FIFO-control CPU matrices pass. Exact replay and
release card-latency captures remain running. The ordinary build is restored
and matches the accepted exception-frame release's allocated sections.

**Release latency/AGA replay follow-up:** both release timing captures finish
24 inputs/30 shuffle steps/60 AY writes with error/reset0 and sampler0.
Light intervals are 18.784/19.616 ms before and 18.848/19.488 ms after.
Corresponding landing stages 24/27 measure 44.960/42.944 ms before and
43.744/42.432 ms after. Different hands and beam phases prevent an exact
whole-game speedup claim; later special intervals are not paired. These
observations support the isolated reduction without closing the 20 ms goal.
Reader breakpoints at command+0x2a0/+0x21a were verified in both frozen ELFs.
Evidence: tmp/clock-inline-card-latency.txt and
amiga/.run/clock-inline-latency-{before,after}/gdb-out.log.
Both AGA and ECS replay exactly match all RAM/VRAM/pixels/60 AY writes at
the required 7,904,133 instructions, 64,000,000 cycles and 8,685 IRQs.
Both compare logs are tmp/clock-inline-{aga,ecs}-check.log.
All acceptance checks for this bounded compiler change pass; larger
rendering/audio and real-time deadlines remain open.
