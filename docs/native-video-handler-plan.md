# Video FIFO handler overhead: measurement and bounded experiment

Status: bounded entry/exit experiment approved by the user on 2026-09-29;
validated smaller entry/exit increments enabled by default. Original IRQs and
the approved timing policy are preserved; the 20ms card/audio target remains open.

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

## Approved experiment design (historical; results below)

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

## Implementation order used

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
7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs. ECS replay also matches the same complete RAM/VRAM/pixel/AY state;
this exit-only stage remained off pending the combined validation below.

Local evidence: `tmp/handler-exit-{headless,regression,regression2}.log`,
`.run/handler-exit-bench`, `.run/handler-exit-cards-{before,after}`,
`.run/handler-exit-live-aga`; frozen `tmp/perf/Pokeri-handler-exit-bench(.elf)`.

**MEASURED VBI qualification:** paired post-service probes report zero late
(scanline >=29) gameplay samples in both variants: baseline 2,886 samples,
maximum line 11; candidate 2,890, maximum line 12. Startup differs: baseline
1,154 samples/max 13/no late samples, candidate 1,151/max65/two late samples.
Do not claim that startup latency is unchanged. Both diagnostic scenarios finish
24/30/60 without error/reset and restore vectors. Evidence:
`.run/handler-exit-vbi-{before,after}`. These probes are absent from normal builds.

## Entry increment scope (implemented)

Keep the original MOVEM save and queue-pointer loads in native guest execution.
First test only $2E30–$2E3A (status BTST, error BNE, FIFO address MOVE), using the
existing status-hook descriptor. This removes the same extra exception as the
wider entry proposal, without adding software guards around already-native RAM
loads. Preserve the error target and both intermediate event boundaries. The
original first register save and last pointer store need no replacement to
remove these exception round trips. Widen further only with a measured reason.

## Entry increment: first measurements

**MEASURED (2026-09-29):** opt-in `HANDLER_ENTRY_FUSION=1` combines only the
verified $2E30 BTST, $2E34 BNE and $2E36 address MOVE. The status and address
operands/descriptors and exact branch encoding are checked before enabling it.
The error branch returns to its original target; every original instruction
boundary remains resumable. The initial register save, software queue loads and
queue-pointer store continue to execute as original native instructions.

The independent linked-code oracle passes 229,376 cases: both CPU models, all
256 status bytes/all CCR combinations, taken error branch, frame/pending-event
injection at every boundary and a rejected next-write address. Registers, both
stacks, physical IPL, original PC, selector phases, debug drain indication and
nominal cycles match. Existing short/whole-feed/control regressions pass with
both new kernels present. The full ECS and AGA cold live24 scenarios complete
24 inputs / 30 shuffle steps, 45 / 60 in-motion AY writes respectively, no
error/reset and an empty heap after static cleanup.

Same-run, DMA-active 512-sequence benchmarks (709,379 ticks/s):

| CPU/display | Entry ordinary/fused ticks | Entry saving | Exit ordinary/fused ticks | Exit saving |
|---|---:|---:|---:|---:|
| A1200/AGA | 40,539 / 34,943 | 13.8% | 48,344 / 41,949 | 13.2% |
| A500+/ECS | 154,162 / 133,322 | 13.5% | 175,660 / 160,293 | 8.7% |

The A1200 entry saving is about 15.4 microseconds per handler. Both increments
save about 0.86 ms over 26 entry/exit pairs in this synthetic batch. This is not
a whole-card timing prediction: interrupt and service alignment also changes.
In the combined live run, completed landing back 24 takes 40.512 ms and back27
48.704 ms; 25/26 have no full cache hit. This different live hand does not support
a paired mean comparison against the exit-only three-card sample. The 20ms
complete-card/audio target remains open. The completed combined gates and activation are recorded below.

Evidence: `tmp/handler-entry-cpu.log`, `tmp/handler-pair-regression2.log`,
`.run/handler-entry-bench{,-ecs}`, `.run/handler-pair-live-{aga,ecs}`;
frozen `tmp/perf/Pokeri-handler-pair(.elf)`.

## Completed gates and activation

**MEASURED:** the final combined binary passes exact ECS and AGA replay: all
262,144 RAM bytes, 524,288 VRAM bytes, 172,064 displayed pixels and 60 AY writes
at 7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs. Replay deliberately
uses the ordinary hooks; active fused semantics are independently covered by
the linked CPU oracles and live scenarios. The exit oracle was repeated against
the combined linked layout and again passes all 2,752,512 cases.

The combined post-service VBI probe has 3,106 gameplay samples, maximum line 12,
none at line 29 or later. Startup has two late samples/max 75; the baseline run
had none/max 13. This startup qualification remains: these observations do not
establish unchanged startup interrupt latency. The combined probe completed two
shuffles (60 steps/120 AY writes), versus one in the baseline, so its whole-run
duration must not be used as a before/after performance comparison. Both runs
complete every scripted input without error/reset and restore all vectors.

The measured sequence-cost savings exist on both CPUs, and all state/cleanup
checks pass. `HANDLER_ENTRY_FUSION=1` and `HANDLER_EXIT_FUSION=1` are therefore
now defaults; either can be set to 0 for comparison after a clean rebuild.
The release has no VBI probes and its allocated ELF sections exactly match the
frozen validated `Pokeri-handler-pair` candidate. This accepts the bounded
improvements, not completion of startup parity or the card/audio deadline.

The wider register-save/queue-load/store fusion is not pursued: these operations
already execute directly on the CPU, and widening to cover them would remove
no additional exceptions. It would add state publication, pointer checks and
software instruction boundaries. The smaller increments remove both targeted
exception round trips while retaining those instructions in guest execution.

Evidence: `tmp/handler-pair-{aga,ecs}-check.log`,
`tmp/handler-pair-exit-cpu.log`, `.run/handler-pair-vbi`,
`.run/handler-pair-replay-{aga,ecs}`, `.run/handler-pair-live-{aga,ecs}`.

## T13 whole-handler boundary map (2026-09-30)

**DERIVED from the original instruction control flow:** the queue's historical
`$2E26–$2EBC` range spans two routines. Normal video service saves D0/D1/A0/A1
at `$2E26` and returns through RTE at `$2E8A`. Its status-bit-7 error branch
starts at `$2E8C`, restores those registers at `$2EA4` and raises TRAP 14 at
`$2EA8`. `$2EAA` is an independent routine called by BSR at `$428C` and `$42D2`;
it saves A0, selects control register 3, writes $81, selects FIFO 0, restores
A0 and returns at `$2EC2`. It is not a continuation after the interrupt's RTE.
No fusion may fall through from one routine to the next.

| Region | Effects / branches that the full proof must retain |
| --- | --- |
| `$2E26–$2E36` | Stack save, A0 setup, bit-7 status test, error branch, FIFO select |
| `$2E3A–$2E50` | Producer/end/consumer loads from A6-relative RAM; empty branch to `$2E70`; initial wrap reload |
| `$2E54–$2E6E` | Consumer comparison, readiness observation, one postincrement word write, end/producer comparisons and ring wrap |
| `$2E70–$2E7A` | Original empty-ring control-disable triplet; it is a distinct IRQ, never suppressed |
| `$2E7E–$2E8A` | Consumer-pointer store, control select, stack restore, virtual RTE |
| `$2E8C–$2EA8` | Error control read/modify/writes, stack restore, original TRAP 14 |
| `$2EAA–$2EC2` | Separate producer-side enable routine; existing triplet fusion remains separate |

The current entry fusion covers only `$2E30–$2E36`; the exit fusion begins at
`$2E82`, leaving the consumer store native. Existing feeder and control fusions
are components, not proof of the entire handler. T13 must additionally prove
all RAM accesses/stack saves and linking boundaries. Its every-instruction gate
must not be inferred from the current feeder's bounded live-tail batching:
that path deliberately collapses several arithmetic/branch instructions.
This map changes no runtime behavior and does not close T13.

## T13 setup-bridge experiment (2026-09-30)

**Implemented, opt-in; not accepted as a performance improvement.**
`HANDLER_SETUP_FUSION=1` connects the existing FIFO selector service at `$2E36`
to the feed/status guard at `$2E58`, the empty-ring control guard at `$2E70`,
or the consumer publication at `$2E7E`. It retains the original queue loads,
comparisons, branches, CCR and nominal cycles, and checks for promotion after
every original instruction. Checked RAM spans fail before consuming that load.
The build generates a local-only complete byte guard for the setup range.
Normal builds compile out the experiment, its descriptors and byte guard.

**MEASURED correctness:** `make harness-handler-setup-check` compares the linked
assembly on 68000 and 68020 with an independently assembled synthetic CPU
oracle. All 13,952 cases pass with instruction counting both disabled and enabled:
all CCRs, empty/nonempty/wrapped rings, every intermediate frame/pending-event
boundary, odd/truncated/out-of-range RAM and address wrap. This proof ends at
the existing device guard; it does not prove the whole-handler feed/error/return
paths. A normal opt-in A1200 warm live run completes all 24 inputs at 480M cycles,
with zero errors/resets and restored vectors. Its Ready-to-end ratio is 0.9809;
there is no accepted Double in this run. Cards still take up to 45.888 ms.

**MEASURED performance regression:** the diagnostic-only
`handler-setup-benchmark.gdb` measures 512 synthetic repetitions of each queue
shape, comparing ordinary selector + native setup + next exception with the
setup bridge and the same next device endpoint. At 709,379 timer ticks/s:

| Queue | Ordinary ticks | Fused ticks | Ordinary µs/sequence | Fused µs/sequence |
| --- | ---: | ---: | ---: | ---: |
| Nonempty, no wrap | 41,344 | 72,634 | 113.8 | 200.0 |
| Empty | 44,894 | 59,790 | 123.6 | 164.6 |
| Initial wrap | 41,066 | 77,251 | 113.1 | 212.7 |

Both paths execute the same authored instruction effects and shared device
endpoints; the benchmark is not a captured game stream. The fused setup keeps
publishing guest PC/CCR/cycle state and checking each boundary, whereas the
ordinary setup instructions execute directly. **INFERRED:** that repeated
publication outweighs the exception saving. This isolates a rejected implementation
strategy, not proof that whole-handler fusion cannot win. Keep this experiment
disabled; investigate retaining guest operands in registers and materializing
state only at an actual promotion/device boundary before widening it. Any such
revision must still pass the every-original-boundary oracle. No timing/device
contract or target is relaxed.

Local evidence: `/tmp/pokeri-t13-setup-count-check.log`,
`amiga/.run/t13-setup-warm/gdb-out.log`, `/tmp/pokeri-t13-setup-bench-run.log`;
frozen binaries in `tmp/t13-setup` and `tmp/t13-setup-bench`.

**Repeat and regression gates:** a second paired run returns
41,349/72,636, 44,890/59,788 and 41,063/77,526 ticks, confirming the regression.
`harness-check`, `harness-platform-check` and `harness-native-check` all pass
(`/tmp/pokeri-t13-setup-host-check.log`). The restored normal executable's
allocated ELF sections match the validated T14 candidate exactly. Exact
ECS/AGA replay and the remaining live/Double/trace gates have not been claimed
for this disabled experimental path; they remain required for any adoption.

### Register-resident setup refinement (2026-09-30)

`HANDLER_SETUP_REGISTERS=1` (requires `HANDLER_SETUP_FUSION=1`; both remain
opt-in) retains D0/D1/A1 operands, next PC and nominal accounting in registers.
It publishes them into the ordinary service frame only at promotion or the
next shared endpoint. Every original instruction boundary still checks frame
and pending-service state. A single admission validates the fixed 160-byte
A6-relative field span before any selector effect. An odd/out-of-range span
uses the ordinary selector hook and native setup instructions with no partial
fused effects. The scheduler's stable `seenFrames` value stays in a register;
VBI's `pendingFrames` remains freshly checked at every boundary.

**MEASURED proof:** expanded linked CPU cases cover pointer comparisons across
`$7FFFFFFF` and `$FFFFFFFF` as well as ordinary ring pointers. All 41,856 cases
pass with instruction counts off and on. Early admission refusals additionally
check untouched guest registers/frame, selector, latch phases and grants before
entering the ordinary endpoint. The earlier per-load-guard register prototype
and original stack-state bridge also passed the expanded matrix before the
admission refinement. The final normal build again matches all allocated
sections of the validated T14 candidate; neither experiment is enabled there.

**MEASURED intermediate paired costs:** register state alone reduced nonempty
setup from approximately 200 to 150 µs, still above its paired 115 µs ordinary
path. Hoisting range admission reduced it further to 142.7 versus 112.5 µs;
empty was 129.8 versus 121.0 µs, and wrapped was 147.1 versus 112.4 µs. These
are separate paired synthetic runs, not additive whole-game speedups. The
final stable-frame-register comparison is recorded below.

Evidence: `/tmp/pokeri-t13-register-expanded{,-count}.log`,
`/tmp/pokeri-t13-span-bench-run.log`,
`/tmp/pokeri-t13-register-final{,-count}-check.log`.

**Final paired result:** ordinary/fused ticks are 40,861/50,264 (nonempty),
44,674/45,973 (empty) and 40,860/51,086 (wrap), at 709,379 Hz for 512 repetitions:
112.5/138.4, 123.0/126.6 and 112.5/140.7 µs per sequence. The benchmark returns
normally with no native error and vectors restored. The revision substantially
reduces the first prototype's overhead but still loses to direct native setup.
**Disposition:** retain only as an opt-in measured building block; do not enable
isolated setup fusion. Further whole-handler work must amortize the admission
and state-publication cost across additional endpoints, or demonstrate another
measured saving without weakening boundaries. Whole-handler, live/replay,
Double and target gates remain open. Frozen final profile binary:
`tmp/t13-register-final`; log `/tmp/pokeri-t13-register-final-bench.log`.

### Deferred setup metadata experiment (2026-09-30)

`HANDLER_SETUP_LAZY_STATE=1`, requiring both earlier setup options, leaves
PC/cycle/instruction-count bookkeeping until an actual promotion or endpoint.
Every original boundary still masks/checks fresh frame and urgent service state;
no interrupt boundary is removed. Boundary-specific exits materialize the exact
original PC, CCR and accounting. The ring-wrap path carries only its additional
14 cycles and one instruction until publication. All options remain disabled.

**MEASURED correctness:** the existing independent linked CPU oracle passes
41,856 cases with instruction accounting disabled and again with it enabled.
This includes every setup boundary, all CCRs, signed/wrapped ring pointers,
empty/wrapped queues and conservative RAM admission failures. These are setup
proofs, not a whole-handler or live/replay qualification.

**MEASURED paired A1200 benchmark:** 512 synthetic repetitions per queue, CIA
frequency 709,379 Hz, normal return, error zero and restored vectors:

| Queue | Ordinary ticks | Deferred ticks | Ordinary µs | Deferred µs |
| --- | ---: | ---: | ---: | ---: |
| Nonempty | 40,861 | 47,061 | 112.5 | 129.6 |
| Empty | 44,717 | 44,946 | 123.1 | 123.8 |
| Initial wrap | 40,821 | 48,530 | 112.4 | 133.6 |

The earlier register-state prototype measured 138.4/126.6/140.7 µs. Deferred
bookkeeping reduces that experimental cost, but remains slower than ordinary
native setup. **Disposition:** do not enable it or claim a gameplay improvement.
Whole-handler work must still amortize state across feed/return endpoints and
pass its larger correctness/performance gates. No production timing changes.
Local evidence: `tmp/t13-lazy`, `amiga/.run/t13-lazy-bench/gdb-out.log`,
`/tmp/pokeri-t13-lazy-check.log`, `/tmp/pokeri-t13-lazy-count-check.log`.

### Whole original-handler reference fixture (2026-09-30)

`make harness-video-handler-reference` verifies the four user-supplied chips,
executes the **unmodified original instructions** at `$2E26` in host-only Musashi,
and emits seven boundary fixtures to ignored `tmp/video-handler-boundaries.jsonl`.
No guest routine is translated to C or embedded in the test. The C++ assertions
check ring contents, ordered device effects, preserved registers, consumer
publication, stack ownership and the resulting original RTE/TRAP frames. The
scripted device is a fixture for readiness/error combinations, not a change to
the production ACRTC model.

**MEASURED:** 63,040 cases cover all producer/consumer positions in an eight-word
ring (including the one-past-end sentinel), readiness capacities zero through
eight, all 32 CCR combinations, supervisor/user return-stack modes and all
256 error control bytes. They traverse 1,931,136 original instruction boundaries.
The error branch restores guest registers and reaches the TRAP 14 vector; it
never falls into the independent producer-enable routine at `$2EAA`.

A further **13,056 real level-7 interrupt/RTE insertions** exercise every original
boundary of seven representative paths with all CCRs and both return modes.
Each transient CPU exception frame is verified, all other RAM is unchanged by
the injected handler, and resumed guest PCs, registers, SRs, nominal guest cycles,
device accesses and original RAM-store streams match uninterrupted execution.
The injected handler contains only an independently authored RTE instruction.

This supplies the whole-block reference for T13, **not a native candidate
comparison or a completed fusion gate**. Malformed spans/address faults,
whole native entry/feed/exit integration, every-boundary native promotion,
shared production endpoints, exact ECS/AGA replay and performance validation
remain required. Local log: `/tmp/pokeri-t13-whole-reference.log`.
