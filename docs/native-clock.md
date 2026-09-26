# Native clock and short access boundaries

The user approved timing option C on 2026-09-25. It supplements measured guest
execution with available wall time, subject to a throughput cap. Diagnostic
replay keeps its recorded schedule; it never uses the live policy.

## Accounting

CIA-A timer A measures intervals outside native services. PAL E-clock ticks are
converted to nominal 8 MHz board cycles by 361/32 (+0.034% relative to
8,000,000/709,379). Calibration subtraction and interrupt-entry adjustments use
the same units. The multiplication is a native 16x16 operation; no OS clock
calls, floating point or 32-bit software multiplication/division enter hooks.

Measured guest cycles receive K credit. Hook metadata and the three audited
iteration-counting boot polls receive their reference charges without K. Both
unspent credit and delayed wall time are capped at one PAL frame (160,000
cycles); at most two 10 ms ticks may be queued. Long services cannot accumulate
seconds of catch-up debt. Pending sources still run their original handlers at
safe boundaries before another tick replaces a flag.

The saved resume PC proves that two adjacent hooked instructions have no
original instructions between them. Such intervals receive zero measured guest
cycles, while each emulated access retains its nominal charge. This matters:
CIA quantization and exception overhead had been credited as guest execution,
causing repeated watchdog resets during early higher-rate clock experiments.
The runtime watchdog periods are unchanged. The later approved normal-game
startup policy skips its diagnostic test; research can still execute it.

The loader retains `native-clock-legacy` (old units and service-excluded policy)
and `native-clock-corrected` (correct units, service-excluded policy) for explicit
comparisons. The default request is K=1.5 (24/16). `native-clock-ratio` is an optional one-byte diagnostic setting in
sixteenths, 1..37. CPU calibration before execution and after display activation
can only lower this request. Three synthetic instruction mixes use the same
private calibration context, not replacements for game instructions. Their
8192-iteration reference counts are checked independently against Musashi:
114686, 180222 and 229374 cycles. Calibration reserves 12.5% headroom. CPU probes
alone do not establish the safety of a game workload or its real-time budget.

## Evidence and limits

Paired host milestones are 16,240,564 cycles at the initial timer-test poll,
55,169,940 at the checksum return and 57,851,768 after the subsequent FIFO drain.
Native samples are within a few original instructions of those endpoints;
nominal hook and audited-poll charges are recorded separately. Per-phase ratios
matter: cumulative boot ratios conceal the tighter drain interval. Under the
initial K=1 experiment its residual reference/charged-guest ratio was about
1.75. After the empty-interval correction the paired drain has 2,681,828
reference cycles, 1,831,430 nominal hook cycles and 489,788 measured guest
cycles. Its residual ratio is (2,681,828−1,831,430)/489,788 = 1.736. K=2 thus
passes the functional scenario but exceeds the phase's measured throughput
floor. The default request is K=1.5, originally leaving 13.6% headroom against that tighter
phase; the runtime CPU probes can only lower it. These are estimates from our
reference model, not measured physical-board clocks.

| Paired phase | Reference cycles | Measured guest cycles | Nominal hook/poll cycles | Residual K |
|---|---:|---:|---:|---:|
| Reset to first timer-test poll | 16,240,564 | 2,390,972 | 7,190 | 6.789 |
| Timer-test poll to checksum return | 38,929,376 | 8,509,751 | 10,220,360 | 3.374 |
| Checksum return to drain exit | 2,681,828 | 489,788 | 1,831,430 | 1.736 |

Residual K is (reference minus nominal charges) / measured guest cycles.
Small endpoint offsets and changed live IRQ schedules limit the precision;
use the minimum with headroom, not an average across phases.

Corrected units alone and the first K=2 experiment both failed after repeated
watchdog resets. The empty-interval correction then completes the K=2 full
cold/play test: 612,000,000 board cycles, 1,014,217 dispatches, one expected
watchdog reset, all 24 input transitions, no error and restored vectors. E-clock
duration is about 453.76 PAL seconds. The 36-second play portion takes about
107.5 PAL seconds. These are functional results, not the 5% real-time gate.
Evidence: `amiga/.run/clock-adjacent/gdb-out.log`.

The final assembly-cache K=2 experiment extends play to 60 board-seconds:
804,000,000 cycles, 1,086,737 dispatches, 210,741 short status accesses, one
expected watchdog reset, all 24 scripted input transitions, no error and
restored vectors. Total elapsed PAL time is 512.20 s; play takes 162.48 s.
This remains an experimental comparison, not the selected production cap.
Evidence: `amiga/.run/clock-final/gdb-out.log`.

The latest ECS replay executes 184,280 accesses through the actual short
assembly path and matches every one of the 262,144 work-RAM bytes after
6,083,063 instructions, 64,000,000 cycles and 4,307 interrupts. The exhaustive
shifted-blitter test also passes, and vectors restore. The prior A1200 replay
covers its extended exception-frame layout. Evidence:
`tmp/status-cache-replay-comparison.log`, `tmp/short-replay-comparison.log`.

The later normal-game boot omits the diagnostic checksum/drain. K remains
conservative at 1.5 pending new paired gameplay calibration; bypassing that
workload does not silently raise the clock. See [startup-policy.md](startup-policy.md).

## Assembly status path

A guarded Line-A index selects a descriptor containing the exact original site,
expected relocated A0, bit mask and cycle charge. Dynamic address mismatch
falls back to the checked dispatcher, which stops on an unadmitted address.
The status specialization admits the four-byte immediate BTST forms. The
additional compare/test specialization is described below.

The shared live entry saves D0–D1/A0–A1. Status reads update only Z in the
physical exception frame and advance PC by four. It makes no C++ call. The shared HD63484 implementation
publishes its exact status after each full service boundary and board tick;
status reads themselves have no side effects. All video mutations occur in
those full services, so the snapshot is current when the guest resumes. This
is not a second device model or a guessed ready flag.

Replay first advances the shared board to the recorded access time, executes
the same reduced-save assembly, then promotes to the full scheduler without
repeating the read or counting another instruction. Its C preparation call
also preserves D1. A pending live trace declines the short path. Both frame
formats retain their existing handling. `native-no-short-hooks` disables the
assembly path; `native-generic-hooks` also forces the original generic executor.

Short measured/nominal charges are deferred to the next full boundary. Common
CIA intervals use a table prepared from the calibrated overhead; larger
intervals use the same arithmetic directly. The ordinary raw-observation
counter does not include every short interval; use the charged ledgers and
whole-run elapsed clock rather than infer total service time from that counter.

The explicit synthetic `native-benchmark` now also times 512 actual Line-A/RTE
operations against a matched loop control. It temporarily admits its own
synthetic site, executes no game instructions and restores its descriptor.
ReadEClock occurs only at batch boundaries. Latest clocked status cost is
38.65 us (14,172 versus 136 ticks at 709,379 Hz). The earlier no-CIA experiment
was 28.52 us versus 47.23 us for its paired clocked implementation; that unsafe
experiment is retained only as an ignored benchmark binary and is never used
for game execution. It identifies timer boundaries as a substantial remaining
cost. FIFO accesses, drawing completion and ordinary scheduling still use C++.


## Selected default validation

K=1.5 completes direct cold boot and the following 60 board-seconds of scripted
play: 804,000,000 cycles, 1,139,387 dispatches, 211,394 short accesses, all
24 input transitions, one expected startup watchdog reset, no native error and
restored vectors. It takes 545.26 PAL E-clock seconds overall; the play interval
has 8,990 samples (179.80 PAL seconds). The clock cap remains 24/16 after CPU
calibration. This preserves a margin below the measured drain throughput but
still runs gameplay about three times slower than PAL time. It is not the
real-time acceptance gate. Evidence: `amiga/.run/clock-cap15/gdb-out.log`.

## Assembly compare/test path

The five audited compare/test sites ($616A, $6170, $6186, $61CA, $61E2)
now share the reduced-save entry. Preparation records the original vector value
and the exact operation. The handler validates the site. A null source register
(A2 or A0) selects the immutable vector value; otherwise the effective address
must be even and its entire longword must fit inside owned ROM or RAM. Device
space, odd pointers and boundary crossings fall back to the checked executor.
It performs native CMP.L or TST.L, copies NZVC to the stacked SR,
preserves X and all other SR bits, and advances the original two or four bytes.
D0–D1/A0–A1 are the only scratch registers saved; D4 and A2 are read without being
modified. Normal short execution makes no C++ call and uses the existing clock
and safe-boundary rules. Replay executes the same assembly, then promotes once;
these CPU reads do not consume device-bus replay events.

`make harness-short-check` (with the cross-toolchain in PATH and native build
present) extracts this project's assembled flag-update body, without ROM bytes,
and compares it against independently assembled CMP/TST instructions under
Musashi. All 8,192 cases pass, covering all 32 incoming CCR combinations and
signed-overflow/equality boundaries, with unchanged nonscratch registers and
stacked PC. A further 112 cases execute the assembled address guard against
null sources, ROM/RAM edges, odd pointers, unmapped space and address wrap.
Flag-body tests alone do not validate scheduling or RTE; the whole-handler
replay and live checks below supply the integration evidence.

The first specialization admitted only null sources. An A1200 measurement
showed essentially unchanged short-call counts (52,581 versus 52,576 before),
so it did not remove the measured workload. The five sites usually read real
game RAM; the final implementation admits those bounded reads as well. The
15.18% figure in the earlier call distribution counts both cases, not just null
vector reads. Evidence for the rejected narrow version:
`amiga/.run/sentinel-live/gdb-out.log`.

The bounded-memory version completes the A1200 K=1.5 live scenario through
600,000,000 cycles, all 24 input transitions, zero watchdog resets, no native
error, intact guard and restored vectors. Ready RAM is credits 0/reserve 100;
final accounting is credits 1/reserve 102. Short accesses rise from 52,576 to
98,935 (the live IRQ schedule changes slightly). Ready occurs at 12.06 board
seconds / 135.28 sampled PAL seconds, versus 11.87 / 136.30 before. The first
60 board-seconds after ready take **179.06 sampled PAL seconds**, versus
199.26 in the preceding fast-start build: about 10.1% less time for this input
scenario, still roughly three times slower than real time. This is a live
scenario comparison, not an instruction-identical benchmark. Remaining sampled
costs include dispatch 31.63%, prepared execution 4.65%, clock accounting 4.85%,
64-bit curve multiplication 5.10%, composition 4.08% and blitter waits 3.66%.
Evidence: `amiga/.run/memory-short-live/gdb-out.log` and
`tmp/memory-short-play-profile.txt`.

The ECS replay with the same startup policy also passes: all 262,144 RAM bytes
match at 7,008,979 instructions, 64,000,002 cycles and 7,831 IRQs. It executes
38,230 accesses through the short assembly handler, passes the planar blitter
self-test, preserves the device guard and restores vectors. The host comparison
uses the exact recorded external input times, not the newer automatic-setup
schedule. Evidence: `tmp/memory-short-replay-comparison.log` and
`amiga/.run/memory-short-replay/gdb-out.log`. The final build's instructions
match the tested executable; the 68000 arithmetic audit passes.

## Further reduced-save handlers (2026-09-26)

Preparation now selects guard/body pointers in 32-byte descriptors. CPU-control
sites use direct Line-A indices, with immediate extension bytes verified before
patching. Common virtual OR/AND SR and RTE instructions, four repeated PIA
forms, admitted byte PIA/ACIA moves, and original TRAP entries use assembly
instruction/CCR/stack handling. Unusual privilege, trace, stack, operand or
address cases retain the full checked path. PIA and ACIA accesses still call
the shared device model; no second hardware state is introduced. Original
TRAPs push exactly their architectural six-byte virtual frame onto game RAM;
service calls continue to use the separate private stack.

Live short services permit Amiga interrupts while the guest clock is stopped.
They promote after one completed instruction when a frame, eligible IRQ,
quit/fault or relevant clock deadline requires full scheduling. The original
access is never repeated during promotion. Four scratch registers cover the C
ABI. Diagnostic replay executes the same assembly effects and then promotes.
The optional instruction counters count a promoted short access only once.

The actual assembled-code tests pass 8,192 CMP/TST flag cases, 112 address
bounds cases, 196,608 CPU-control cases, 32,768 PIA cases, 106,496 generic
peripheral byte cases, and 4,106 TRAP cases. TRAP expectations come from
independent Musashi TRAP execution, including every vector/IPL/CCR combination;
invalid stack, target, opcode and privilege cases must decline before writes.
ECS replay additionally matches all 262,144 RAM bytes, all 524,288 video bytes,
163,008 cropped pixels and 30 AY writes at 7,008,979 original instructions,
64,000,002 cycles and 7,831 IRQs. Vectors restore and the blitter self-test passes.
Evidence: `tmp/trap-short-final-check.log`, `tmp/trap-replay-comparison.log`.

Successive A1200 K=1.5 normal-build measurements (1 MB Chip / 8 MB Fast,
PAL, debug audio muted) use the first 60 board-seconds after each run's actual
ready milestone. These are live-schedule comparisons, not identical CPU traces:

| Implementation | Sampled PAL seconds for 60 board-seconds |
|---|---:|
| Bounded-memory compare/test | 179.06 |
| Plus virtual SR/RTE | 161.44 |
| Plus four PIA forms | 132.62 |
| Plus exact curve products, panel spans and interruptible PIA service | 118.02 |
| Plus prepared handler routes | 117.38 |
| Plus general PIA/ACIA byte moves | 109.20 |
| Plus reduced-save TRAP frames | 108.58 |

The last run reaches ready at 94,240,000 board cycles / 87.06 sampled PAL
seconds, then completes through 600,000,000 cycles with all 24 scripted input
transitions, zero watchdog resets and restored vectors. Its profile puts 7.81%
of samples in `nativeDispatch`, versus 31.63% in the memory-short build. The
main-loop ROM bucket is now 50.99%; delayed IRQ sampling prevents treating that
as an exact service/guest split. Source logs and samples are under
`amiga/.run/{control-short-live,pia-ready-live,hotpaths-live,routes-live,
peripheral-live,trap-live}` and matching ignored `tmp/` captures.

The final isolated status benchmark costs **49.84 us** per exception after
subtracting its matched loop: (18,244 - 141) / 512 / 709,379 seconds. This is
still above the 25 us target. The earlier direct-route build measured 47.05 us;
layout/calibration and workload changes make the isolated numbers distinct
from end-to-end speedups. Evidence: `amiga/.run/short-final-benchmark/gdb-out.log`.

Neither these improvements nor one successful live scenario complete release
acceptance. A K=37/16 diagnostic run and a K=1.5 counter-instrumented run both
stopped with `serial transmit checksum` around service-door input. This is not
established as a clock-cap failure: normal input, unlike startup, could initiate
peer work while the ROM still had an outgoing packet. Input pacing is being
validated separately. The production cap remains 24/16. Normal non-warp audio,
input latency, missed animations and real-time acceptance remain open.


The subsequent normal-input queue correction completes both failing schedules:
K=1.5 takes 108.96 sampled PAL seconds for the first 60 game-seconds, K=37/16
101.54. Both deliver 24 key transitions, empty the retained cabinet requests,
restore vectors and report no error or watchdog reset through 600,000,000
cycles. This rules out treating the earlier checksum failures as proof of an
unsafe K. It also does not calibrate K: 24/16 remains the default. Evidence:
`tmp/cabinet-live-play-profile.txt`, `tmp/cabinet-cap37-play-profile.txt` and
matching `amiga/.run/cabinet-*` logs. See [startup-policy.md](startup-policy.md).
