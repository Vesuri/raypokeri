# Video FIFO handler overhead: measurement and bounded experiment

Status: bounded entry/exit experiment approved by the user on 2026-09-29; implementation and validation in progress.
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
requires explicit approval; the user has now authorized this bounded experiment. The existing byte-guarded
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

## Implementation order

Start at the existing exit hook $2E82: leave the original queue-pointer store
at $2E7E in guest execution, then combine the address selection, register restore
and virtual RTE. This smaller first increment can prove intermediate boundaries
and stack handling before widening entry. It removes one exception round trip
per handler; it is not the completed entry/exit design or a default activation.

## Exit increment: first measurements

**MEASURED (2026-09-29, opt-in `HANDLER_EXIT_FUSION=1`):** the smaller
$2E82–$2E8A exit passes 2,752,512 independent linked-code cases. Both physical
68000/68020 execute the same synthetic reference instructions, covering all
CCR/IPL combinations, user/supervisor returns, frame and pending-event injection
after each instruction, legal stack ends, overflow/odd pointers and conservative
RTE fallbacks. All registers, PC, virtual/physical stack state and nominal cycles
match; the fused path makes no guest RAM stores. Diagnostic replay keeps the
ordinary path. The original queue-pointer store at $2E7E still executes normally.

The paired A1200 benchmark executes 512 identical synthetic exits in physical
user mode, with display DMA active, using real Line-A entries and returns. Both
variants have identical register/frame setup; the reference executes its MOVEM
normally. A temporary benchmark-only TRAP returns to the caller and is restored
before leaving the benchmark. Timer reads occur only at batch endpoints.
The measured ticks are 47,442 ordinary / 41,902 fused at 709,379 Hz: about
130.62 / 115.37 microseconds per exit, **11.7% less**. This saves approximately
0.40 ms across 26 exits; it does not remove either original video interrupt.

Read-only complete-card observations on the accepted pattern-tile release and
this candidate give:

| Completed landing back | Ordinary exit | Fused exit |
|---|---:|---:|
| 24 | 41.792 ms | 40.832 ms |
| 25 | 43.424 ms | 41.920 ms |
| 26 | 41.856 ms | 41.600 ms |
| Mean | 42.357 ms | 41.451 ms |

Card 27 has no complete cache hit in these runs and is excluded. Different live
RNG/timing and scanline quantization limit attribution: the isolated batch is
the controlled exit-cost result. Both live observations finish 24 inputs,
30 shuffle steps, 60 in-motion AY writes, no error/reset. A separate AGA cold
run also reaches full static cleanup with an empty heap. Headless model/platform/
native suites and existing short/feed/control CPU regressions pass. ECS live24
also completes with 30 shuffle steps / 45 in-motion AY writes, no error/reset,
and an empty heap after static cleanup. AGA exact replay matches all 262,144
RAM bytes, 524,288 VRAM bytes, 172,064 displayed pixels and 60 AY writes at
7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs. ECS replay is still running;
the switch remains off by default, and the entry experiment remains open.

Local evidence: `tmp/handler-exit-{headless,regression,regression2}.log`,
`.run/handler-exit-bench`, `.run/handler-exit-cards-{before,after}`,
`.run/handler-exit-live-aga`; frozen `tmp/perf/Pokeri-handler-exit-bench(.elf)`.

**MEASURED VBI qualification:** paired post-service probes report zero late
(scanline >=29) gameplay samples in both variants: baseline 2,886 samples,
maximum line 11; candidate 2,890, maximum line 12. Startup differs: baseline
1,154 samples/max13/no late samples, candidate 1,151/max65/two late samples.
Do not claim that startup latency is unchanged. Both diagnostic scenarios finish
24/30/60 without error/reset and restore vectors. Evidence:
`.run/handler-exit-vbi-{before,after}`. These probes are absent from normal builds.

## Next entry increment

Keep the original MOVEM save and queue-pointer loads in native guest execution.
First test only $2E30–$2E3A (status BTST, error BNE, FIFO address MOVE), using the
existing status-hook descriptor. This removes the same extra exception as the
wider entry proposal, without adding software guards around already-native RAM
loads. Preserve the error target and both intermediate event boundaries. The
original first register save and last pointer store need no replacement to
remove these exception round trips. Widen further only with a measured reason.
