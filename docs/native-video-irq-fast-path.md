# Guarded native video-interrupt admission

Status: the original C/frame-only prototype remains opt-in. The T5 assembly
admission and bounded-clock path below is enabled by default after its gates; it
has measured service-cost savings, not a complete-card deadline pass.
The experiment used release `33e60d3` as its baseline, not the current release.
See [remaining-work.md](remaining-work.md) for the active queue.
The 20 ms card/audio and sustained real-time objectives remain open.

## Evidence and scope

**MEASURED:** accepted-release landing backs still take about 42 ms. Earlier
read-only attribution found 27 general dispatcher entries at original PC $2EBC
per landing, accounting for 7.6–8.3 ms inside the C dispatcher alone; assembly
save/restore and the original handler are additional costs. The recent layout
and sampled-status experiments save only 5.5/11.3 microseconds per isolated
admission and do not materially improve card feeding. See
native-rendering-followup.md for qualifications and local evidence.

The next experiment targets that general-service transition, not the guest
handler. Keep the same already-executed FIFO-control write, the same interrupt
source/vector/priority, and execute the original handler instructions. No wider
guest hook, IRQ suppression, scheduling policy or clock change is proposed.

## Eligibility measurement

Use the accepted executable and read-only breakpoints for four landing backs.
Observe immediately after nativeClockPause, before later dispatch work. A check
before charging deferred cycles is insufficient: that charge can create a due
timer tick. Log frame/clock state, virtual SR/stack, video source, competing
PIA/ACIA sources, pending display work and completed-card presentation state.
The current verified instruction is nativeDispatch+0xC0 (TST.L after its JSR to
nativeClockPause). Cache begin/hit offsets are +0x2A0/+0x21A. Revalidate all three
against the particular frozen ELF before using the reader; offsets are not API.
The reader enables this breakpoint only inside the selected cards, with the
normal PC sampler off. Debugger expressions must be type-checked offline:
Registers.a is a C array, while video.control uses the freestanding array's
values member. No target function calls or writes are permitted.

## Proposed implementation boundaries

Start only at the existing nativeShortControlPromote path for the verified
post-write PC $2EBC. Existing grants are already revoked and the guest clock is
stopped. Reject replay, profiling, startup/not-Ready, clock calibration, other
PCs, virtual trace state or IPL at/above the video level before attempting
any shortcut. The normal dispatcher remains authoritative for every rejection.

Charge deferred time once through the existing nativeClockPause. Recheck that
there are no live ticks, unserved PAL frames, unspent credit/debt requiring
service, guard-check work, quit, reset, device/board error or drain observation.
Require the actual video IRQ to be pending with its existing vector, no competing
PIA/serial source or higher-priority vector condition, no active shuffle, no
pending presentation, no elapsed normal presentation deadline and no completed
card requiring early presentation. Missing eligibility is an ordinary fallback,
not an error and not permission to assume success.

Before mutating guest state, the physical interrupt mask and a final frame check
must close the race with an Amiga VBI. Device-source state cannot be changed by
an Amiga IRQ; original handlers are never run inside that IRQ. Physical callbacks
can still change frame, input and display bookkeeping, which is why their pending
work must force fallback. Keep these checks explicit rather than treating a
previous pending-bit snapshot as proof that nothing else is due.

For a proved eligible transition, construct exactly the existing virtual 68000
six-byte exception frame on the selected guest supervisor stack, preserving
original SR/CCR and post-write PC. At virtual user entry, save the current guest
USP and select nativeVirtualSsp exactly as setSr/pushException does; supervisor
entry uses the current guest stack. Keep the separate Amiga service stack. Validate
stack and vector mapping/alignment before any store; unsupported cases fall back.
Set virtual IPL/SR, handler PC, live-IRQ state, interrupt count and pending flags
exactly as the full dispatcher would, then use the existing clock restart and
physical return. Preserve every live register and the Amiga exception-frame format.

A failed guard after clock charging must leave guest/register/device state
untouched. Subsequent full dispatch must not double-charge: the existing deferred
totals are drained and nativeClockRunning is clear. This idempotence requires a
specific test; it must not be inferred from a successful fast case.

## Acceptance

1. Capture real post-accounting eligibility and blocked reasons first. If few
   landing admissions qualify, do not build a speculative parallel dispatcher.
2. Execute the actual linked guard/frame/return code on independent 68000 and
   68020 oracles. Cover every CCR/IPL, register/stack preservation, virtual stack
   transitions/refusal, mappings, competing sources, frame/quit arrivals and
   each guard failure. Verify all stores, not just final pixels. Compare time
   charging plus fallback to the existing dispatcher accounting.
3. Measure complete admission and full live landing latency. Include clock
   accounting and exception return; a frame-store microbenchmark is insufficient.
4. Run required headless/hook/FIFO tests, exact ECS/AGA RAM/VRAM/pixel/AY replay,
   cold live24 on both machines, and VBI/sound checks. Diagnostic replay alone
   cannot validate an explicitly live-only shortcut.
5. Keep normal builds restored while experimenting; default activation requires
   measured improvement and all correctness gates. Report the remaining complete-
   card/audio deadline even if this service improvement is accepted.


## Completed eligibility capture

**MEASURED:** amiga/.run/video-irq-eligibility-v3 uses the accepted uninstrumented
release, after its existing clock accounting. Two completed backs (starts 26/27)
take 42.432/41.664 ms in this capture. Their 53 entries at $2EBC include 51 that
pass the observed no-other-work conditions: card 26 has 25/27, card 27 has 26/26.
Two card-26 entries have a due tick; one of those also has no enabled video IRQ.
Every selected entry has no outstanding completed-card presentation, no shuffle,
no board fault/reset/quit, no drain observation, and calibrated clock mode 2.
There is no simultaneous unspent credit and debt. Stack-target/alignment and
actual shortcut correctness still require the implementation proofs above.

The full selected capture has 148 entries. Starts 24/25 do not reach complete
back hits in this run, so their mixed work is excluded from complete-back timing
and eligibility conclusions. Their 47/48 entries include 24 pending-display
cases each, and additional timer/frame work. This confirms that the display guard
cannot simply be omitted to increase the hit rate.

Every entry's virtual SR is 0, 4 or 8: S is clear. A supervisor-only shortcut
would admit none of these cases. Correct user-to-supervisor stack selection is
therefore essential, not an optional later extension. PIA0 control/flags are
$36/$80 and $0E/$00; the latched CA1 flag is disabled. Serial control $95 has an
empty receive queue. Guarding on nonzero raw flags would also reject useful
cases incorrectly; query actual enables/priority using the existing model.
The existing liveIrqActive flag is still set at most entries, but the ordinary
dispatcher clears it when virtual IPL is below 5 before admitting the new IRQ.
The shortcut must preserve that behavior rather than reject a stale active flag.

The complete scenario finishes 24 inputs/30 shuffle steps/60 in-motion AY writes,
status 4/error 0/no reset, sampler off, at 4,076 PAL frames. This remains an
eligibility experiment, not evidence of a performance improvement. Earlier
eligibility fixtures failed on debugger array syntax before collecting samples;
only v3 is evidence. Every reader expression was subsequently type-checked
without a target. The ordinary release remains unchanged.

## Opt-in prototype and first live measurement

`VIDEO_IRQ_FAST=1` admits the already-completed control write through the
existing exception-frame implementation and virtual stack switch. The assembly
return preserves the actual physical frame and registers. `VIDEO_IRQ_COUNTS=1`
adds a success counter for investigation; neither switch is enabled by default.

**MEASURED:** `host/native_video_irq_check.py` extracts the actual linked code
and debug layouts, then runs synthetic states on independent 68000/68020 CPUs.
26,336 cases pass, including 15,390 admissions and 80 late frame/quit races.
The matrix checks CCR/IPL, both virtual stack modes, stack/vector boundaries,
each explicit guard failure, exact stores, ABI/physical return, and deferred
clock accounting followed by fallback with no double charge. This is synthetic
CPU coverage, not evidence of every possible device-state combination.

**MEASURED:** the first A1200 cold live scenario completes all 24 inputs,
30 shuffle steps and 60 in-motion AY writes, with status 4, error/reset zero,
sampler off, and 1,019 shortcut admissions. Completed backs 25–27 take
40.512/42.304/42.432 ms using PAL frame/scanline observations. The accepted
release's earlier four backs took 41.536–42.304 ms. Different live hands and
frame placement prevent treating this as a controlled speedup; the 20 ms
card/audio target is still unmet.

The headless model/platform/native and linked short/feed regression suites pass.
Full ECS/AGA prototype replay, ECS live and VBI/audio gates remain pending;
there is no default activation. The read-only before/after measurement
below determines whether this prototype merits those remaining gates. Local evidence:
`tmp/video-irq-cpu4.log`, `tmp/video-irq-regression.log`,
`amiga/.run/video-irq-prototype`, and frozen
`tmp/perf/Pokeri-video-irq-prototype(.elf)`.

**MEASURED paired admission capture:** `amiga/video-irq-cost.gdb` measures from
`nativeShortControlPromote` to the original video handler, over starts 25–27.
There are 122 admissions in each run. Accepted/prototype median is 320/256 us,
but mean is 333.38/334.16 us, and total 40.672/40.768 ms. PAL scanline timing
has roughly 64 us resolution; these are coarse service observations, not
microsecond-accurate timing. The prototype has 79 entries at 256 us versus 16
before, but 14 at 448 us versus none before, plus larger outliers. This supports
cheaper admitted cases, not a net speedup across admissions. Both full scenarios
complete all 24 inputs without errors/reset. Local runs:
`amiga/.run/video-irq-cost-{before,after}`. A further refinement would need to
reduce rejection cost before this warrants activation or the remaining gates.

Normal builds have been restored and all allocated ELF sections match the
accepted release exactly. Startup fast-forward remains the already-validated
default; this experiment changes neither its policy nor gameplay pacing.

### Early scheduler rejection experiment (not retained)

**MEASURED:** moving additional refusal checks ahead of stack/vector/source
validation passes the same 26,336 linked CPU cases and a cold A1200 live24
(24 inputs, 30 shuffle steps, 60 in-motion AY writes, no errors/resets).
It admits 935 shortcuts. The 122 observed promotion-to-handler intervals total
60.032 ms, mean 492.07 us. One interval is 20.960 ms and therefore includes
scheduled/deferred work rather than just entry overhead. Excluding that interval
only as a separate diagnostic view gives 39.072 ms / 121 = 322.91 us; it is not
a replacement for the full measurement or a controlled overall speedup.
The median is 320 us. There are 60 entries at 256 us, 36 at 320, 16 at 384,
and four at 448. Compared with the first prototype, cheaper refusals trade off
against extra checks on successful cases; there is no compelling net benefit.
The added code was removed. The accepted normal build remains unchanged.
Evidence: `tmp/video-irq-early-cpu.log`, frozen
`tmp/perf/Pokeri-video-irq-early(.elf)`, and
`amiga/.run/video-irq-cost-early/gdb-out.log`.

**DERIVED next investigation:** the FIFO-control endpoint already computes
current video status and interrupt demand immediately before this promotion.
Rather than repeating eligibility checks in another general C function, inspect
whether a strictly single-boundary authorization can reuse those results.
It must be revoked on every fallback and boundary, and must still charge time,
recheck timer/frame work, preserve source priority and build the exact original
exception frame. No cached decision may survive guest execution or device
mutation. This is a proposed implementation investigation, not permission to
remove an IRQ or weaken any guard.

### Single status sample experiment (not retained)

**MEASURED:** a service-local status sample replaced repeated video IRQ/status
queries inside the opt-in helper. Explicit PIA and serial tests remained, as
did the CB2 vector-priority condition (including its disabled/output flag case).
The sample did not cross any guest instruction or device mutation. Expanded
linked CPU tests pass **420,576 cases**, including exhaustive enable/flag bytes
for both PIA sides and video, and every serial control byte with empty/nonempty
receive queues. The independent predicates check source refusal, vector
priority, accepted status, all stores and time accounting on 68000/68020.
There are 296,526 admissions and the existing 80 late frame/quit races.

The cold A1200 live run completes 24 inputs/30 shuffle steps/60 in-motion AY
writes, no errors/resets, and 949 shortcut admissions. Its 122 measured entries
total **40.800 ms**, mean **334.43 us**, versus baseline 40.672 ms/333.38 us.
Completed backs 26/27 take 42.240/40.608 ms; start 25 has no full-back hit and
is not included. This does not establish a net performance improvement.
The status-reuse code was removed; the broader CPU source-coverage tests are
retained and rerun against the original prototype. Production remains unchanged.
Evidence: `tmp/video-irq-status-sources.log`,
`amiga/.run/video-irq-cost-status/gdb-out.log`, frozen
`tmp/perf/Pokeri-video-irq-status(.elf)`.

**DERIVED:** the FIFO endpoint's existing demand flag cannot itself authorize a
video-only shortcut: its boolean OR does not identify the pending source and
short-circuits later source tests. Reusing it without extra proof would weaken
priority checks. Repeated status queries are not the dominant remaining cost.
The next entry-path design should examine which configuration/immutable-vector
checks can be proved once and which stack/source/timer/frame checks must remain
at each boundary, before attempting an assembly fast path. No such invariant
cache is currently implemented or assumed by the tests.

### Direct assembly frame prototype

`VIDEO_IRQ_FRAME_ASM=1` alongside `VIDEO_IRQ_FAST=1` keeps every existing C
admission guard and clock-charge/fallback rule, but builds the accepted frame
with a leaf assembly wrapper. It writes the exact six-byte virtual exception
frame, saves virtual USP on user-to-supervisor entry, publishes the validated
SSP/handler PC/SR, and preserves all non-scratch physical registers. Admission
remains physically masked through these stores. The original guest handler and
RTE execute normally. The original C implementation remains the comparison.
No configuration guard is cached or removed.

**DERIVED pointer invariant:** native preparation assigns `rom`, `romBase` and
`nativeRomBegin` to the same aligned board-memory allocation. The isolated
benchmark changes nativeRomBegin only before gameplay and restores it; it
cannot pass the Ready guard. The outer physical-entry guard also requires saved
PC = nativeRomBegin + $2EBC, while C requires PC = romBase + $2EBC. Thus the
assembly's vector load uses the same ROM allocation validated by C. No guest
instruction or device mutation intervenes between validation and the load.

**MEASURED:** 420,576 linked CPU cases pass with the assembly selected, including
296,526 admissions and 80 late frame/quit races. The oracle checks every store,
full CCR/IPL coverage, both virtual stack modes, mapping ends, source/vector
priority, fallback idempotence and the physical return on 68000/68020.

The first cold A1200 run completes all 24 inputs, 30 shuffle steps and 60
in-motion AY writes, with no errors/resets and 1,001 shortcut admissions.
122 promotion-to-handler samples total **39.392 ms**, mean **322.89 us**,
versus baseline 40.672 ms/333.38 us (3.1% lower in this capture). Of those,
24 are 192 us and 66 are 256 us; fallback/scheduler outliers remain. Completed
backs 24–27 take **40.384/40.128/41.280/40.704 ms**. This is a modest candidate
improvement, not a controlled whole-card percentage or closure of the 20 ms
card/audio target. All timings use the existing coarse PAL scanline reader.

Full ECS/AGA replay, ECS live, hook/FIFO and VBI checks pass as recorded below;
this candidate is not enabled by default. Evidence: `tmp/video-irq-frame-cpu.log`,
`amiga/.run/video-irq-cost-frame`, and frozen
`tmp/perf/Pokeri-video-irq-frame(.elf)`.

#### Assembly-frame verification and measurement limits

**MEASURED identical synthetic entry state:** the same linked CPU oracle counts
6,532 → 5,602 instruction cycles on 68000 and 2,768 → 2,428 on 68020
(14.2%/12.3% less). Both include guard/accounting/frame/physical return; neither
includes hardware memory waits or display DMA, and neither measures the full
accepted general dispatcher. This proves a saving relative to the C shortcut,
not that complete live cards meet their deadline. The expanded source/CPU
matrix passes against both linked implementations.

**MEASURED interval qualification:** the broad 122-entry reader also includes
work after a cache hit and before the next cache start. For actual completed
backs only, the prior status-sample capture has 26 admissions each at means
305.23/251.08 us (cards 26/27); the assembly frame capture has 26 each at
274.46/270.77/286.77 us (cards 25/26/27). These variable samples do not establish
a controlled complete-card improvement. The coarse timing and different hands
remain limitations; the broad 3.1% difference is not an activation claim.

**MEASURED full replay:** ECS and AGA match all 262,144 RAM bytes, 524,288 VRAM
bytes, 172,064 cropped pixels and 60 AY writes at 7,904,133 instructions,
64,000,000 cycles and 8,685 IRQs. Replay explicitly declines the live shortcut;
the independent CPU matrix and live runs cover its active behavior. ECS live
completes 24 inputs/30 shuffle steps/45 in-motion AY writes, status 4/error 0,
no resets, Ready/end frames 5,786/16,174, and 1,186 shortcut admissions.
The linked short/feed/FIFO regression matrices pass, including all 200,704
FIFO triplets and each intermediate event boundary.

**MEASURED paired VBI probes:** with shortcut counters disabled, before/after
both complete A1200 live 24/30/60 without errors/reset. Gameplay samples are
2,877/2,880, maximum scanline 11 in each, zero at line 29 or later. Startup has
two late samples in each, maxima 65/80. Ready/end frames are 1,169/4,046 before
and 1,181/4,061 after; these runs do not show a whole-session speedup. Probes
run after audio/screen service and are absent from normal builds.

Normal allocated ELF sections match the accepted release after restoration.
Local evidence: `tmp/video-irq-frame-cycles-{before,after}.log`,
`tmp/video-irq-frame-{aga,ecs}-compare.log`, `tmp/video-irq-frame-hooks.log`,
`amiga/.run/video-irq-frame-{replay-aga,replay-ecs,live-ecs}`, and
`amiga/.run/video-irq-vbi-{before,after}`. The next entry-path optimization must
improve overall workload cost before default activation; cheaper frame stores
alone are insufficient.

The required headless model/platform/native regression suite also passes
(`tmp/video-irq-frame-headless.log`). All emulator runs above are terminal;
no diagnostic process is left running. The normal build is restored, and the
assembly experiment remains opt-in pending an overall-workload win.


## T5 assembly admission and deferred clock (2026-09-29)

`VIDEO_IRQ_ASM=1` (default) selects assembly scheduler admission at the existing verified
post-write/returned-handler PC `$2EBC`. It implies the previous entry and frame
switches; `VIDEO_IRQ_ASM=0` retains the ordinary dispatcher or either explicitly
selected historical prototype. The original guest instructions, intermediate
boundaries, virtual six-byte frame and return implementation are unchanged.
The fused `$2E82` exit can return to this same PC, so this admission benefits
both that exit and the `$2EB2` re-arm sequence.

C++ emits assembler aliases to the owning scheduler fields. They are addresses,
not snapshots or duplicate state, and add no runtime preparation or allocation.
Stack/vector/configuration and pending-work guards execute in assembly. A small
query retains the shared PIA/ACIA/video model predicates, including disabled
flags and the encoder's CB2 priority rule. Device state cannot change in physical
interrupt callbacks. Display/frame/quit state can, and is checked again after
clock accounting with IPL7 restored. No authorization crosses guest execution.

The restricted assembly clock handles unchanged PAL frame, no queued tick,
no running guest timer, ratio 64/16 and a three-frame credit window. All other
states call the existing C clock. Guest and nominal grants remain separate and
ordered: combining them before saturation would lose credit in some cases.
A brief masked section makes the calculation atomic; the existing post-clock
frame/quit check closes its interrupt race. It uses neither OS calls nor wide
multiply/divide. Its phase normalization has at most two iterations.

**MEASURED correctness:** 420,536 profile-enabled and 420,496 normal-code linked
IRQ cases pass on independent 68000/68020 CPUs, with 296,526 admissions and
80 late-frame/quit cases in each build. A further 24,000 clock cases compare
all clock state with the existing linked C calculation, including saturation,
zero/large deferred sources, frame wrap, queued ticks and alternate ratios/windows.
Headless model/platform/native and short/feed matrices pass. The feed fixture
previously overlapped BSS at its synthetic final source word; moving all fixtures
above native allocations and explicitly checking that separation fixes the test
itself. Both unchanged baseline and candidate pass the repaired matrices.

**MEASURED A1200 external instruction traces**, T4 `t4-input-play` versus
`t5-video-play` (different live hands, nearly equal service counts):

| Original service site | Calls before / after | Mean before / after | Full dispatches before / after |
|---|---:|---:|---:|
| `$2E82` fused exit | 1,249 / 1,235 | 278.2 / 246.7 µs | 695 / 280 |
| `$2EB2` re-arm | 783 / 776 | 325.2 / 266.0 µs | 595 / 155 |
| `$2E30` entry | 1,249 / 1,234 | 94.2 / 93.6 µs | 14 / 11 |
| `$2E70` empty tail | 596 / 589 | 148.1 / 146.7 µs | 2 / 3 |
| `$2EBC` address write | 596 / 589 | 48.8 / 47.8 µs | 2 / 0 |

The two targeted site means improve by about 11% and 18%. This is a retained
small improvement; it does not reach the original T5 saving estimate. The
remaining entry/tail costs are still present and need further work. The clock
helper's initial trace call count also includes its local grant subroutine;
do not interpret the printed 15 µs/call as a complete clock-pause measurement.
That subroutine now has a distinct diagnostic symbol without changing its code.

**MEASURED coarse delivery reader:** all three runs finish 24 inputs without
error/reset. Baseline / assembly guards / guards+clock+early rejection each have
122 promotion-to-handler intervals, totaling 35.392 / 33.344 / 44.000 ms.
The last run contains one 20.832 ms scheduling interval. Its remaining 121
samples total 23.168 ms, but that is a separate diagnostic view, not a substitute
for the full total. Instruction-trace site costs above establish the execution
saving without discarding that wait. Local runs: `t5-irq-cost-{before,asm,clock}`.

**MEASURED gameplay:** all cold/warm live24 runs pass with status 4, error/reset
zero and restored vectors. AGA board/PAL ratios are 0.9721 / 0.9760; ECS ratios
are 0.2703 / 0.2736 (compatibility, not real time). The normal-code Double run
accepts round 1 with 18 key transitions, ratio 0.9558, AY batch median 12.3 ms,
and largest consecutive sound-write excess 239.184 ms. Typical complete back
feeds remain about 35–42 ms, with slower outliers. No complete-card or audio
latency target is closed. Evidence: `tmp/t5-video-*-report.txt` and the matching
`amiga/.run/t5-video-*` directories.

ECS and AGA full-state replay pass the usual 262,144 RAM bytes, 524,288 VRAM bytes,
172,064 pixels and 60 AY writes at 7,904,133 instructions / 64,000,000 cycles /
8,685 IRQs. Paired VBI probes complete all 24 inputs with restored vectors and
no error/reset. Before/after gameplay samples are 2,786 / 2,782, both with maximum
scanline 26 and none at line 29 or later; startup maxima are 20 / 18. This gate
covers the assembly clock's bounded masking as well as ordinary service work.
Local probes: `amiga/.run/t5-vbi-{before,after}`. The ordinary build now uses the
validated assembly path; profiling remains absent unless explicitly requested.

## T5 handler-boundary and address-phase completion (2026-09-29)

**Enabled by default after validation.** `HANDLER_BOUNDARY_INLINE=1` expands
exactly the existing live pending-frame/pending-work check at the entry, exit
and FIFO-control instruction boundaries. Diagnostic replay retains its shared
boundary endpoint. No guest instruction boundary or interrupt is removed.
`PAIRED_ADDRESS_PHASES=1` clears the two adjacent FIFO phase bytes with one word
store. A native compile-time layout assertion requires one-byte booleans,
even alignment and exact adjacency; CPU fixtures check neighbouring sentinels.

**MEASURED:** paired 512-iteration A1200 batches, with display DMA, at a
709,379 Hz measurement clock (`amiga/.run/t5-tail-bench-{before,after}`):

| Optimized batch | Before ticks | After ticks | Reduction |
|---|---:|---:|---:|
| Handler entry | 34,107 | 33,131 | 2.9% |
| Handler exit | 41,888 | 39,307 | 6.2% |
| FIFO control triplet | 53,364 | 50,757 | 4.9% |
| Address selection | 21,049 | 20,713 | 1.6% |

**MEASURED:** completed gameplay traces (`t5-video-play` → `t5-tail-play`)
put the entry site `$2E30` at 93.6 → 87.4 µs and the empty-tail site `$2E70`
at 146.7 → 139.9 µs. Other raw means are mixed with different full-dispatch
promotion frequencies: `$2E82` is 246.7 → 249.3 µs (280/1,235 → 420/1,456
full), and `$2EB2` is 266.0 → 288.2 µs (155/776 → 305/887 full).
**DERIVED:** removing the full-dispatch contribution from the rounded trace
aggregates gives approximately 167.9 → 154.3 µs and 228.2 → 221.9 µs for
those two short paths. These are different live hands, not paired whole-card
measurements. Both traces complete their scripted accepted Double without a
reset or error.

The linked CPU matrices pass: 229,376 entry cases, 2,752,512 exit cases,
200,704 FIFO triplets, all short/feed suites, 420,496 interrupt-admission cases
and 24,000 bounded-clock comparisons. Headless board/platform/native suites
also pass. Exact ECS/AGA replay matches all 262,144 RAM bytes, 524,288 VRAM
bytes, 172,064 pixels and 60 AY writes at 7,904,133 instructions / 64,000,000
cycles / 8,685 IRQs. Cold and persisted warm live24 complete without resets or
errors and restore vectors: A1200 board/wall ratios 0.9715/0.9762, ECS
0.2707/0.2743 (compatibility, not a performance pass). Normal-code Double is
accepted in round 3, completes all 32 input transitions, and exits cleanly.
Its AY batch median is 12.4 ms; the largest sound-write excess is 228.736 ms.
Typical completed cached backs remain roughly 35–43 ms, with outliers: the
20 ms complete-card and audio deadlines remain open.

The first paired VBI probe records baseline startup/play maxima 18/23 lines
with no sample at or after line 29; candidate maxima 32/39 with one late sample
in each phase. The repeat records baseline maxima 27/26 with no late samples and candidate
40/40 with one in each phase. A read-only breakpoint on the late-counter
instruction identifies both as **active clock calibration**, at frames 3 and
976, with `nativeClockCalibrating=1`; neither runs the FIFO handler. No further
late samples occur across the completed runs. Samples are after audio/screen
service, not interrupt-entry timestamps. This qualifies the VBI gate: steady
gameplay shows no delay, but the existing masked calibration completion remains
sensitive to timing and is retained as a T7 follow-up. Do not report this as
zero late VBI across startup. Evidence: `t5-tail-vbi-{before,after}`,
`t5-tail-vbi-repeat-{before,after}` and `t5-tail-vbi-late`.

Both options can still be disabled for comparison. The updated sentinel fixtures
also pass against the frozen pre-tail executable. T5 implementation is complete;
its original numerical estimates were not acceptance guarantees. The remaining
whole-handler exception overhead belongs to T13, and fixed dispatcher cost to T6.
