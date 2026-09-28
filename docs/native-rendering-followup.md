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
The interleaved copy implementation passed the gates below. Cold startup,
remaining artwork and retained-RAM persistence remain open. Shuffle consumer pacing is now integrated and validated below.

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
