# T6: fixed dispatcher cost

Status: the selective ordered clock batch is validated and enabled by default.
IRQ-source reuse and pending-work gating remain in progress. T5's separately
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
reports. IRQ-source caching and pending-work gating remain unimplemented;
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
