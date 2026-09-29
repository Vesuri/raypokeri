# Double transition and held-note investigation

Measured 2026-09-29, input fix `6a03e8c`. This is a scoped graphics investigation;
[remaining-work.md](remaining-work.md) retains the broader performance goals.

## Baseline

**MEASURED:** a native diagnostic plays ordinary hands, holds pairs (or four
cards of a suit), draws, and presses D only after the original Double-ready
byte `$4112F` becomes 1. All actions use external keyboard input. It changes
no cards, accounting, CPU results or game instructions. The second hand wins.
The earlier probe mistakenly checked the Big/Small-ready byte `$4112E`; that
capture is excluded from the Double evidence below.

The four board seconds beginning at D take **6.26 PAL seconds** in the
instrumented A1200 build. This includes 1,254 commands: 253 RFRCT, 108 PAINT,
36 RPLL and 48 each CRCL/ELPS. Rectangle fills average **0.851 ms**; the long
13-point card outline takes about **14.4 ms**. Total inclusive command work
is **1.06 s**, presentation **0.05 s**, and timestamp-observer cost about
**0.42 s**. Command feeding, exceptions and other services also contribute;
the interval is not one screen-clear blit.

**MEASURED:** the longest consecutive AY-write gap in that interval is
**1,072.41 ms wall / 80 ms board time**, between cycles 330,160,000 and
330,800,000. This is delayed production of sound writes while drawing, not
evidence of a Paula DMA playback-rate error. Diagnostic host audio is muted.

Local evidence: `amiga/.run/double-entry-baseline`,
`tmp/double-entry-baseline-{events,marks,slow,card-cost}.bin`,
`tmp/double-entry-baseline-summary.txt`, and frozen
`tmp/perf/Pokeri-double-entry-baseline(.elf)`.

## White-border cache proof

The previous cache admits only the background predicates established on empty
pixels. A card redrawn over an existing card has white pixels at its rounded
corners; these stop PAINT and reject the original cache guard.

A second preparation proof renders with all guard pixels white. Both scalar
and rectangle-accelerated passes must match the existing bitmap composed over
white, and every previously undefined read must remain within the original
68 guarded pixels. The original image and mask are reused. Each command keeps
its own separately proved current position and drawing-work count, because
corner PAINTs stop sooner on white. The additional table has 79 records
(948 bytes); there is no second Chip bitmap.

Runtime admission requires either all original predicates or all-white guards.
It never mixes these conditions pixel by pixel: partial white barriers could
change flood-fill reachability. Other backgrounds retain ordinary rendering
and sequence tracking. Reads, mismatches and partial sequences preserve the
existing replay/observation rules. No sound scheduling or timing policy changes.

**MEASURED:** 4,908 differential cases pass with scalar reference pixels,
intermediate parameters/work, logs, interruptions and serialized state. These
include arbitrary surrounding artwork with only guarded pixels constrained.
Prepared-table checks also pass, including 14 malformed descriptors rejected
before writes. The normal ECS and AGA replays matches all 262,144 RAM bytes, 524,288
VRAM bytes, 172,064 pixels and 60 AY writes at 7,904,133 instructions,
64,000,000 cycles and 8,685 IRQs. Normal live24 completes with zero error,
zero watchdog resets and restored vectors. The native raster kernel passes 1,249,920 CPU cases, and the short/feed
proof suites pass, including 3,755,520 whole-feed cases.

## Detailed-logger comparison

**MEASURED:** the same five back sequences drop from **140.96–145.52 ms**
to **80.18–82.43 ms** (raw elapsed per complete feed). Total inclusive command
work over the four-second Double window falls from **1.06 s to 0.66 s**.
The largest excess AY-write gap (wall minus board time) falls from
**992.41 ms to 692.65 ms**. The corresponding absolute gaps are
1,072.41/792.65 ms, spanning 80/100 ms of board time. A later intentionally
long interval between sound writes must not be mistaken for drawing delay.

The AY register state before these two gaps is identical. Paula applies the
preceding write batch within **15.84/16.58 ms**. The long wait is for the
original program to produce its next batch. Different live timing changes
random hands; compare the identical card-back sequences and note state, not
whole-session elapsed totals.

These builds use the detailed command observer, which disables cached-raster
FIFO grants. A separate `LEDGER_FAST_CACHE=1` comparison retains that production
path and is required before presenting these numbers as normal-path latency.

## Production FIFO path comparison

**MEASURED:** `TIME_LEDGER=1 LEDGER_FAST_CACHE=1` retains cached-raster grants
and omits the detailed command logger. With the old admission policy the
largest excess sound-write gap crossing Double is **914.99 ms** (1,224.99 ms
wall versus 310 ms board). The white-border candidate reduces this to
**570.00 ms** (680.00 ms wall versus 110 ms board), a **37.7% reduction in
excess delay**. The AY register state at the beginning of both gaps is identical;
Paula applies it after 15.77/13.62 ms. These are still profiling builds, not
uninstrumented release timings or a worst-case bound.

The five admitted Double-entry backs in the candidate take **54.79–57.50 ms**
each, including original command feeding. This is not just the blit duration.
They cannot be compared directly against the detailed-logger card times above:
the latter disable FIFO grants. The remaining note extension and whole-card
20 ms target are **not solved**. Command feeding/exception overhead remains,
and no timer, interrupt, original instruction or sound sequence was removed to
obtain this improvement.

The first light candidate run never reached Double and is excluded. The repeat
uses five external coin taps and two seconds for accounting before Deal;
the baseline uses one coin and half a second. Both accepted runs win their first
hand, but different timing changes the hand. Consequently these are scoped
same-note/same-card-command observations, not controlled whole-session totals.
Local accepted captures are `amiga/.run/double-fast-baseline` and
`amiga/.run/double-fast-white2`, with matching `tmp/double-fast-*-{events,marks}.bin`
and frozen executables in `tmp/perf/`. Both finish without errors or watchdog
resets. All diagnostic source/input changes are restored afterward.

Normal-build `.text`, `.rodata`, `.data` and `.bss` match the frozen replay
candidate. The new admission case is enabled by default; it allocates no new
Chip bitmap and keeps fallback for unproved backgrounds.

## Live envelope correction (2026-09-29)

The user approved PAL-clock envelope progression independently of guest
slowdown. The measured shape-9 fade now reaches silence after 195.92 ms wall
time (90 ms board time); previously it was still level 7 at 658.92 ms wall.
This fixes the held decay, not late subsequent notes. The new live capture
still has a 683.04 ms sound-write gap spanning 140 ms board time (543.04 ms
excess). See [clock implementation and measurement limits](live-envelope-clock-experiment.md)
and the [rendering route audit](rendering-path-audit.md).

## Keyboard-only Double workload (2026-09-29)

`DOUBLE_SCENARIO=1` builds a diagnostic `native-test-inputs` driver
(`src/native/DoubleScenario.h`) that supplies only keyboard edges. Each round
inserts a coin, deals, and holds like a player reading the screen: a pair, else
four of a suit, else the highest card. The ROM does not accept Draw with nothing
held; a blind variant stalled with coins accumulating. The driver then draws and
presses D only once the ROM's own Double-ready byte `$4112F` is set. It chooses
Big after 4 s and stops the session 8 s later. After 12 losing rounds it stops
with an error. No card, credit, CPU or RAM value is supplied. Normal builds
contain none of this.

`amiga/release-double.gdb` is `release-timing.gdb` plus the scenario result;
summarize it with `host/release_timing.py --scenario double LOG`. Besides the
cached-back intervals and accepted-Double count, the report gives AY write-batch
statistics. One original sound update writes several registers at one board
cycle. The report gives the batch application span and the batch-to-batch
lateness: PAL minus board time.

**MEASURED (normal code, A1200, `amiga/.run/pa-double-release`):** accepted in
round 3, 36 key transitions, status 4, zero errors, zero resets, vectors
restored. Ready to finish 71.78 board / 77.16 PAL seconds. The two batch gaps
across Double entry are 262.9 and 276.4 ms late. 47 of 143 gaps are more than
50 ms late. Applying one batch takes a median 21.7 ms. A separately traced run
(round 5) attributes this cost in [trace-profile.md](trace-profile.md).
