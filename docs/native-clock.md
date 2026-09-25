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
The original watchdog periods and tests are unchanged.

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
floor. The default request is K=1.5, leaving 13.6% headroom against that tighter
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

## Assembly status path

A guarded Line-A index selects a descriptor containing the exact original site,
expected relocated A0, bit mask and cycle charge. Dynamic address mismatch
falls back to the checked dispatcher, which stops on an unadmitted address.
Only the admitted four-byte immediate BTST status forms take this path.

The live path saves D0/A0/A1, updates only Z in the physical exception frame,
and advances PC by four. It makes no C++ call. The shared HD63484 implementation
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
