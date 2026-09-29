# Video FIFO handler overhead: measurement and bounded experiment

Status: measurement complete; wider-hook experiment awaits user approval.
No production behavior or timing policy has changed.

## Current release measurement

**MEASURED (2026-09-29):** read-only guest-PC breakpoints on the uninstrumented
`Pokeri-card-prepared-release` A1200 binary separate original handler entry
($2E26), feeder entry ($2E54), feeder exit ($2E7E), original RTE site ($2E8A)
and return to the interrupted control triplet ($2EBC). Cache begin/hit sites
were verified against this frozen ELF before installing breakpoints. No host
OS timing calls or sampling code execute in the game. The reader uses PAL frame
number and scanline (64 microsecond resolution), so short intervals have
quantization error; frame-boundary approximation adds a small further error.

Only complete back-cache hits are reported. Initial/final handler fragments
outside each card interval are excluded from complete-handler totals. Feeding
includes device execution and final drawing; it is not CPU-only guest time.
The handler totals below exclude the RTE hook itself and interrupt admission.

| Card start | Begin-to-hit | Complete empty bodies (13 each) | Complete feeding bodies (12 each) | Observed feeder intervals |
|---|---:|---:|---:|---:|
| 24 | 42.944 ms | 4.288 ms | 16.960 ms | 15.744 ms (13) |
| 25 | 44.064 ms | 4.352 ms | 18.016 ms | 15.936 ms (13) |
| 26 | 40.960 ms | 4.672 ms | 17.344 ms | 15.040 ms (12) |
| 27 | 41.472 ms | 4.672 ms | 17.760 ms | 14.784 ms (12) |

These are overlapping categories, not an additive budget. A feeder interval
may begin in the first partial handler. A card may finish inside the last
handler. Counts therefore differ without implying lost observations.

**DERIVED from original instructions and observed entry ordering:** after
feeding the available software queue, the handler returns with WFE still
enabled. A subsequent interrupt sees the empty queue and executes the original
interrupt-disable triplet. This explains the paired feeding/empty interrupts.
Their existence is not evidence that they may be suppressed or combined into
one virtual interrupt. The native model must continue to deliver both.

The run completes 24 inputs, 30 shuffle steps and 60 in-motion AY writes,
status 4/error 0, zero watchdog resets. Local evidence:
`amiga/.run/landing-handler-cost/{measure.gdb,gdb-out.log}` and
`tmp/perf/Pokeri-card-prepared-release(.elf)`. Earlier dispatcher attribution
is in native-rendering-followup.md; do not add subtotals from different runs
as though they were a single controlled capture.

## Proposed experiment: bounded handler entry/exit hooks

The existing whole feeder and three-write control hooks remain authoritative.
Benchmark only the surrounding original video-service sequence:

- Entry $2E26–$2E54: save registers, test video status, select the FIFO and load
  the software queue pointers. The error branch at $2E34 falls back to original
  $2E8C handling. The empty branch retains the original $2E70 disable path.
- Exit $2E7E–$2E8C: publish the consumed pointer, select the register port,
  restore the original registers and perform the existing virtual RTE.
- Leave the command producer, game decisions, graphics model, original IRQ
  count and approved feeder/control semantics unchanged. No polling, wait,
  status observation, IRQ acknowledgement or watchdog boundary is removed.

This is a wider fused hook, not merely cheaper device code. The recovery plan
explicitly requires approval before it is implemented. The existing byte-guarded
handler and ordinary hooks remain the reference and fallback. All original
register/memory effects, CCR values, nominal-cycle charges and fault boundaries
must be checked. At every original instruction boundary where a frame, quit,
source, timer or failure requires service, publish the exact original next PC
and state and return to the ordinary scheduler. Never replay an already executed
store. Keep diagnostic replay on the original path.

First implement only a synthetic bounded prototype, not default activation.
Execute the original bytes and the proposed hook independently under both
68000 and 68020. Cover nonempty/empty/wrapped queues, all condition codes and
virtual IPLs, user/supervisor returns, legal stack ends and rejected pointers,
status/error branches, and event injection at every instruction boundary.
Compare complete registers, memory stores, PC and nominal cycles. This is not
permission to turn an arbitrary ROM routine into C or remove its instructions.

Measure identical batches with display DMA active and timing reads only at
batch endpoints. Retain a candidate only with a net measured saving, then
require exact ECS/AGA replay, cold live24 on both, VBI/cleanup gates and
uninstrumented complete-card measurements. Preserve separate build switches.
The measured 4.3–4.7 ms empty-body subtotal bounds only part of the opportunity;
this experiment alone is not claimed to achieve the 20 ms deadline. If the
proof cannot preserve an intermediate boundary, reject that fusion.
