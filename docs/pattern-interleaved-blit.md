# Single-blit planar pattern tiles

Status: validated and enabled by default (2026-09-29).

## Evidence and scope

**MEASURED:** the current prepared-cache A1200 cold startup attempts 745 PTN
Surface draws, all 15×14 pixels at stride 608. Of these, 730 qualify for the
native cache (291 hits, 439 misses); the remaining 15 decline to the existing
renderer. The same run reaches Ready at frame 1,175 and completes all 24 inputs
with no native error or watchdog reset. Curve counts are 63 hits / 45 misses.
Evidence: `amiga/.run/startup-pattern-layout/gdb-out.log`.

The existing PTN path uses one mask plane plus four separate colour planes,
submitting four blits even though destination VRAM is interleaved. The prototype
prepares interleaved source rows and repeats each mask row four times. One
masked blit then covers all four destination planes at the normal 608-pixel
pitch. Other pitches retain per-logical-row operations; noninterleaved storage
retains separate plane operations. Bounds, ROP, cache keys, eviction ordering,
DMA synchronization and damage publication remain unchanged.

`PATTERN_INTERLEAVED=1` is the default; `=0` retains the comparison path. The fixed
64-entry cache grows from 20,480 to 32,768 Chip bytes, an extra 12,288. Allocation
and release use the same compile-time size. There is no per-draw allocation.
The ordinary layout remains available for comparisons. This is a device drawing
optimization, not a wider guest hook or changed timing contract.

## Validation and initial measurement

**MEASURED:** all 51,456 host pattern cases compare every ordinary and
interleaved mask/colour word against the independent scalar oracle. Alignment,
dimensions, modes, wrapping windows and patterned colours are covered; sentinel
words check output bounds. Native self-tests now exercise both 64- and 608-pixel
strides, every alignment, all four ROPs and all three colour modes, including
repeated cached draws. Existing replay checks exercise complete game output.

**MEASURED first candidate:** paired 512-miss A1200 batches, including final
DMA completion, take 807,331 → 717,756 E-clock ticks at 709,379 Hz: 11.1% less,
about 2.223 → 1.976 ms per tile. The independent ordinary-layout expansion
batch stays essentially unchanged (230,960 → 231,256 ticks). These are synthetic
miss-heavy batches, not a startup or complete-card percentage.

The first candidate's cold live24 reaches Ready at 1,163 PAL frames, versus
1,175 for the baseline: original execution is about 0.24 s shorter. Both reach
Ready at 47,120,000 board cycles. Candidate gameplay completes 24 inputs,
60 shuffle steps and 120 in-motion AY writes with no reset/error; the final
post-destructor boundary has heapHead=0. Different resulting hands are not a
same-workload gameplay comparison. Startup preparation/loading is outside this
VBI interval. Evidence: `.run/pattern-oneblit-{before,after,live-aga}` and frozen
`tmp/perf/Pokeri-pattern-oneblit(.elf)`.

A second candidate avoids clearing active interleaved output words that are
immediately assigned during expansion. Unused rows remain zero. The full host
scalar comparison passes again. Its native miss-heavy benchmark measures 625,541 ticks versus the baseline
807,331: **22.5% less**, or about **1.722 ms/tile**. The ordinary expansion
control is 231,212 ticks. The final native correctness gates are recorded below; the isolated result
alone was not used to activate the change.

**MEASURED first-candidate AGA replay:** all 262,144 RAM bytes, 524,288 VRAM
bytes, 172,064 cropped pixels and 60 AY writes match at 7,904,133 instructions /
64,000,000 cycles / 8,685 IRQs. This also exercises the extended real-blitter
self-test. Evidence: `tmp/pattern-oneblit-aga-compare.log`.

## Activation gates

Require exact ECS/AGA RAM/VRAM/frame/AY replay, native synthetic blitter checks,
cold live24 on ECS/AGA, clean resource/heap teardown and arithmetic audit for
the final selected candidate. Compare the final ordinary startup as well as
isolated tile cost. Keep diagnostic output out of the normal executable.
The 20 ms landing/audio and sustained real-time objectives remain open.

## Final candidate validation

**MEASURED:** the final candidate passes exact AGA replay at the same full
RAM/VRAM/pixel/AY and CPU boundary above. Both cold live24 scenarios complete
without reset or alert and with heapHead=0 after the verified CRT destructors.
AGA: Ready frame 1,156 at 47,440,000 cycles, 30 shuffle steps / 60 in-motion AY
writes. ECS: Ready frame 5,529 at 47,760,000 cycles, 30 steps / 45 AY writes.
Startup mute, timing debt and queued ticks are zero at Ready. These are original
execution frame counts, excluding executable/native preparation; live workload
differences prevent a same-hand gameplay speedup claim. The final ECS replay also matches all RAM/VRAM/pixels/AY at the exact
instruction/cycle/IRQ boundary above. Evidence: `.run/pattern-oneblit-v2-live-{aga,ecs}` and
`tmp/pattern-oneblit-v2-{aga,ecs}-compare.log`.

The host model/platform/native suites and native software-arithmetic audit pass.
Normal activation changes only the build default; no profiling code is added.
Both layout expansion paths retain the scalar oracle tests, and allocation/free
sizes remain paired. The broad startup parity and gameplay latency goals are
not complete. The wider original video-handler experiment remains separately
approval-gated in native-video-handler-plan.md.
