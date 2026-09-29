# T6: fixed dispatcher cost

Status: the selective ordered clock batch is validated and enabled by default.
IRQ-source reuse is also validated and enabled by default. Pending-work gating is also validated and enabled by default. T5's separately
saved release executable is unchanged. The active scope is T6 in remaining-work.md:
clock accounting, interrupt-source reuse and pending-work gating, with unchanged
instruction effects, interrupt boundaries and timing policy.

## Ordered clock batch

`CLOCK_BATCH_PAUSE=1` is the default; `0` retains the previous implementation. One paused interval can contain
three contributions, in order: deferred measured guest cycles, deferred nominal
instruction cycles, and the running timer's measured interval. Each contribution
must saturate credit and spend available wall debt before the next is added.
Combining their inputs before saturation loses credit and is not equivalent.

The candidate publishes the wall-frame change once and retains those ordered
spending boundaries. It batches only multiple contributions, only with physical
IPL at least 3 (VBI cannot change wall time), only the live bounded clock, and
outside startup and active profiling. Empty pauses return; single contributions,
low-IPL callers, diagnostic replay and other timing modes keep the existing path.
There is no new clock contract or larger credit window.

**MEASURED:** the initial loop-based version was rejected: empty pauses rose
from about 9 to 50 µs, and single contributions also regressed. Unrolling removes
that overhead for multiple contributions. Batching every nonempty pause still
slows single-source cases; the selective candidate retains their old arithmetic.

The expanded benchmark has 48 contexts: timer stopped/running, no/one-frame
wall debt, deferred guest 0/208/4096 cycles, nominal 0/4200 cycles, and phase
0/72000. Each context has a 512-iteration control and measured batch. Both
control and measurement use the full dispatcher's interrupt mask. Dormant
profiling support is absent from the release-style comparison.

**MEASURED, A1200:** selected baseline → selective candidate costs after
subtracting control, rounded to 0.1 µs per pause:

| Contributions, phase 0 | No debt | One frame debt |
|---|---:|---:|
| None | 10.4 → 8.7 | 9.7 → 8.7 |
| Nominal only | 42.1 → 44.0 | 45.1 → 46.0 |
| Deferred guest only (208) | 50.0 → 53.2 | 47.3 → 54.0 |
| Deferred guest + nominal | 81.6 → 62.3 | 80.9 → 71.2 |
| Running timer only | 51.3 → 53.7 | 52.1 → 54.4 |
| Running timer + nominal | 84.2 → 60.1 | 87.4 → 71.5 |
| All three | 119.7 → 74.0 | 122.8 → 86.0 |

Local evidence: `amiga/.run/t6-clock-normal-before` and
`t6-clock-selective`. The smaller single-source regressions are included in the gameplay trace below;
this table alone was not used to justify default activation.

Correctness so far: three million host batches match an independent sequential
wide-arithmetic oracle, alongside the existing 16 million scalar transitions.
40,000 actual linked 68000/68020 pause cases match the independent oracle,
including raw timer charges, IPL 0/2/3/7, saturation, queue limits and counter
wrap. Existing 24,000 assembly-clock and 420,496 IRQ-admission cases pass.
The same new oracle also passes against the unchanged T5 executable.

**MEASURED:** the completed release-style trace (`t5-tail-play` →
`t6-clock-play`) reduces clock pause from 57.5 to 45.8 µs per full dispatch,
and inclusive dispatcher cost from 443.5 to 427.3 µs. The candidate completes
its accepted Double in round 5 without resets or errors. These traces have
different hands and promotion counts (5,807 → 5,301 full dispatches), so their
board/wall ratios are not a controlled whole-game speed comparison.

All common clock-change gates pass:
- headless board, platform and native suites;
- exact ECS/AGA replay: 262,144 RAM bytes, 524,288 VRAM bytes, 172,064 pixels
  and 60 AY writes at 7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs;
- cold/warm live24 on both machines: all 24 inputs, no errors or watchdog resets,
  vectors restored; A1200 board/wall 0.9730/0.9772, ECS 0.2727/0.2767;
- normal-code accepted Double in round 1, 18 inputs, clean exit. AY batch median
  12.5 ms; largest excess sound-write gap 281.872 ms. This different hand does
  not establish an audio-deadline improvement over T5;
- post-service VBI probe: 972 startup samples, maximum line 18; 2,783 gameplay
  samples, maximum line 26; none at/after line 29. This does not fix the existing
  calibration-window issue documented under T7.

Local evidence: `t6-clock-{aga,ecs}`, `t6-clock-live-{aga,ecs}`,
`t6-clock-double`, `t6-clock-vbi`, and their `/tmp/pokeri-t6-clock-*.log`
reports. IRQ-source caching is an opt-in experiment; pending-work gating remains unimplemented;
T6's full −30% dispatcher target and the card/audio deadlines remain open.

## Interrupt-source invalidation inventory

**DERIVED from current code:** the native video-word path recomputes the entire
board IRQ after each endpoint write, even though only video state changed.
`Board::irq` costs 20.5 µs per call in the T5 trace, across 26,366 calls.
Its owned device calls are already devirtualized by the compiler; merely adding
qualified C++ names will not remove that cost.

A candidate can cache the PIA/serial portion, combine it with the freshly
computed video status and current interrupt enable, and retain the existing
vector priority rules. Invalidation must cover all of these routes:

- `advanceClock`: the shared Board tick sets PIA flags and serial-peer RX data;
- `PreparedBus` and checked `Bus`: PIA0 ($FB014–$FB017), ACIA0
  ($FB002–$FB003), including reads that acknowledge or consume an interrupt;
- the generic short-I/O read/write endpoints, which bypass the full bus;
- board reset and state restoration;
- replay/input injection into serial0 (`applyInput`), which bypasses Board tick.

Video parameter/header/raster fast paths update authoritative video fields and
cached status directly. They must continue to use that fresh video state; a
cache of the complete IRQ level cannot simply remain valid across those writes.
Amiga VBI/audio callbacks do not mutate the shared PIA/serial models. Synthetic
CPU fixtures that construct device states directly will need explicit cache
initialization, plus independent transaction tests proving the real invalidation
routes. This is an implementation inventory, not a validated cache yet.


## Validated IRQ-source cache

`IRQ_CACHE=1` is the default (`0` retains the old path). It caches only the PIA0/ACIA0 interrupt predicate. Video status and
its interrupt mask remain fresh, and vector selection keeps the shared model's
priority. Diagnostic replay uses the original IRQ query. All tick/reset, external
serial injection and checked/generic bus routes invalidate the derived cache.

**MEASURED:** the first gameplay candidate (`t6-irq-play`) completes its accepted
Double without errors or resets. Against `t6-clock-play`, query cost falls from
20.4 to 17.6 µs and the FIFO control endpoint from 37.6 to 31.3 µs. However,
short peripheral writes rise from 87.2 to 95.3 µs and reads from 63.2 to 67.2 µs.
Inclusive full dispatch changes only 427.3 → 423.2 µs. Different hands prevent
claiming a controlled whole-game gain. This is insufficient evidence for default
activation.

**DERIVED:** invalidating all PIA0 byte writes is unnecessarily conservative.
PIA output/DDR writes and control reads do not change its interrupt predicate;
ACIA status reads and TX writes do not change its predicate either. The refined
byte endpoint invalidates PIA control writes/data reads and ACIA control
writes/RX reads. Generic multi-byte bus accesses retain conservative range
invalidation. This is service bookkeeping, not a change to device semantics.

**MEASURED:** direction-aware invalidation (`t6-irq-byte-play`) removes the
peripheral regression: short writes 87.2 → 75.9 µs, reads 63.2 → 60.5 µs,
IRQ query 20.4 → 11.5 µs, FIFO control 37.6 → 31.3 µs, source helper
26.1 → 20.3 µs and inclusive dispatch 427.3 → 416.3 µs versus the clock-only
baseline. The scenario completes its accepted Double in round 1 without errors
or resets. Different hands still preclude a whole-game speed claim; its 0.924
board/wall ratio alone is not a controlled comparison.

The refined helper passes 200,000 host transaction sets including retained-cache
operations, PIA acknowledgments, serial RX/reset, tick/reset and live video
changes. Linked IRQ/clock tests pass 420,496 / 64,000 cases on 68000/68020,
including warm-cache source queries, and 1,411,072 actual FIFO endpoint cases
pass. Headless board/platform/native suites pass. With the flag off, every
allocated ELF section matches the committed clock-only executable exactly.
Cold/warm A1200 live24 pass (board/wall 0.9758/0.9777), as does cold/warm ECS
(0.2745/0.2782), all with 24 inputs, no resets/errors and restored vectors. Normal
Double passes in round 3 with 34 input transitions: median AY batch 11.7 ms,
maximum batch lateness 223.6 ms. These are regression checks across different
hands, not evidence that the audio deadline has been met.

The VBI run exits cleanly but records one late startup sample (line 39 of 946
samples) and one late gameplay sample (line 46 of 2,779). The read-only probe confirms both occur with `nativeClockCalibrating=1`,
at frames 3 and 952 (the latter at guest PC `$2442`). This qualifies the two
samples as the existing T7 calibration issue, not a new FIFO service delay.
Exact AGA and ECS replay both pass all 262,144 RAM bytes, 524,288 VRAM bytes,
172,064 cropped pixels and 60 AY writes at 7,904,133 instructions / 64,000,000
cycles / 8,685 IRQs. With the full common gates passing, the cache is enabled
by default. The pending-work results are below; the estimated overall −30% target was not demonstrated.


## Pending-work gating audit

The next experiment must preserve these boundaries:

- Shuffle service consumes the command-ring marker at `$2E62`, releases the
  current held frame, and clears hold/pointer state even on its final release.
  An inactive-queue shortcut is valid only after that cleanup; queue counts
  cannot replace the final-release work.
- A completed original system tick requests composition. Cache recognition,
  command completion, and the ring-drained check still guard publication.
  VBI can retire a pending buffer during service, so a pending-buffer snapshot
  from dispatch entry must not suppress a newly eligible composition.
- `presentReady()` already checks pending/armed/testing before touching the
  blitter. Adding another copy of that predicate is not inherently a saving;
  asynchronous blitter completion must still get its publication opportunity.
- `AmigaScreen::region` can call `observePixels`, materializing a deferred
  card prefix. A failed fallback changes ACRTC error/status. A reused status
  value cannot cross composition unconditionally. RESET and replay boundaries
  also require a fresh result.

`DISPATCH_WORK=1` is now the validated default; `0` retains the old path. A local
work mask skips inactive shuffle service, gates the existing compose predicate,
and requests a final status refresh for diagnostic boundaries, resets and
composition. The live source scan reuses its just-refreshed status, including
`advanceClock`'s refresh after a tick. Publication retains `presentReady`'s own
fresh pending/armed check; a stale dispatch-entry buffer snapshot is not used.

`DISPATCH_WORK_VERIFY=1` compares reused IRQ/status values against fresh shared
model queries and checks inactive-shuffle cleanup. Its A1200 live24 passes with
no errors or resets. The separate uninstrumented gameplay trace completes its
accepted Double in round 1. **MEASURED:** inclusive dispatch 416.3 → 415.8 µs
against the IRQ-cache baseline, with different hands and dispatch counts.
This is too small to establish a workload-independent gain. An identical-input synthetic comparison resolves the ambiguity: 512 iterations
with the same saved CPU/device context take 78,277 → 71,665 timer ticks;
context-only controls take 16,220 → 16,531 ticks, at 709,379 Hz. Subtracting the
control gives **170.9 → 151.8 µs per dispatch (11.2% lower)**. This supports
retaining the candidate for full release validation. Local evidence:
`.run/t6-work-bench-{before,after}`. Headless suites, exact AGA replay and cold/warm live24 on both models pass.
Normal Double completes in round 1 with 14 input transitions: median AY batch
11.4 ms, maximum batch excess 218.0 ms. The VBI probe finds two late samples
(lines 35/32), both with calibration active, and exits cleanly after 24 inputs.
Exact ECS replay also passes all RAM, VRAM, pixels and AY writes at the same
7,904,133-instruction boundary. Cold/warm live24 board/wall ratios are
0.9746/0.9793 on A1200 and 0.2768/0.2804 on ECS. Work gating is enabled by
default after those gates. T6's three implementation steps are complete, but
the observed overall dispatch means (443.5 → 415.8 µs across differing hands)
do not demonstrate the estimated −30% reduction. Card/audio deadlines remain
open; T7 and the wider authorized T13/T14 fusions address larger remaining costs.
