# Startup interrupt latency (2026-09-29)

## Attribution on the 0.1 code

The earlier post-service VBI observations could not distinguish late interrupt
entry from expensive audio/display work. The new read-only debugger probe samples
three points on the normal executable: the native level-3 entry, nativeVbi entry,
and the return from screen.vbi. It adds no instructions before display service.
It also observes clock-calibration boundaries and stops at Ready.

**MEASURED:** two cold A1200 runs have 1,109 / 1,107 VBI samples. The first
sample enters at scanline 57, reaches nativeVbi at 63 and finishes audio/screen
service at 64. It is the only sample at or after scanline 29. Audio/screen service
spans at most one scanline in these captures. The program has executed zero
board cycles, the display is disabled, and the interrupted PC is its initial
reset entry. Thus the measured late sample precedes card drawing.

**DERIVED, confirmed by boundary observations:** nativeClockCalibrateBegin sets
physical IPL7 for its 1,032 samples, then runs three 8,192-iteration CPU probes.
The pending VERTB request remains set through them and is delivered on return to
the original program. The final CPU probe finishes at line 52, immediately
before the late level-3 entry at 57. This explains the current reproduction;
it does not retrospectively identify each sample in older two-outlier captures.

## Bounded interrupt window

After each calibration exception, the guest timer is already stopped and its
sample has been captured. The calibration handler now briefly lowers physical
IPL to zero, executes a NOP, then returns to IPL7 before preparing the next
sample. The exception frame has already been consumed; Amiga interrupt handlers
use the service stack and return to supervisor code. The native wrappers do not
arm a guest trace or account this supervisor service as guest execution.

The NOP samples and CPU-speed probes themselves remain masked. Their instruction
counts, conversion, overhead subtraction, calibration limits, and the gameplay
clock contract are unchanged. This is not a change to original IRQ delivery or
an added presentation timer. Diagnostic replay has calibration disabled.
Individual speed probes still impose bounded masked intervals; this change
removes the continuous mask across the whole calibration, not all masking.

**MEASURED:** cold candidate runs have 1,111 / 1,112 startup VBI samples. Their
maximum level-3 entry / service entry / service completion lines are
13/17/18 and 12/17/17, respectively. Neither has a sample at line 29 or later.
The first VBI now enters at line zero during calibration. Four VBIs are serviced
before calibration finishes, instead of leaving them pending/coalesced. Frame
counts therefore must not be compared as a startup speedup.

The repeat and full A1200 live scenario report sample minimum/maximum 33/45,
overhead correction 40 board cycles, CPU probe costs 19,634/29,618/39,275, and
CPU ceiling 80 sixteenths. The live scenario completes 480,000,000 board cycles
and all 24 inputs, with zero errors/watchdog resets and restored vectors.

## Reproduction and validation

`amiga/startup-vbi.gdb` finds the screen-service return instruction from the
current executable rather than assuming an offset. Run with POKERI_REPLAY=0,
a fresh private drive, the normal executable, and the muted diagnostic launcher.
Debugger stops pause emulation; these are emulated PAL scanlines, not host time.

Local evidence:
- baseline: `.run/startup-vbi-entry`, `.run/startup-vbi-calibration`;
- candidate: `.run/startup-vbi-window`, `.run/startup-vbi-repeat`;
- full live: `.run/calibration-live`;
- frozen normal binaries: `tmp/perf/Pokeri-calibration-{before,window}`.

Headless model/platform/native checks and the linked short/whole-feed CPU proofs
pass. Exact ECS and AGA replay each match all 262,144 RAM bytes, 524,288 VRAM
bytes, 172,064 pixels and 60 AY writes at 7,904,133 instructions / 64,000,000
cycles / 8,685 IRQs, with restored vectors and no error. These version-7 fixtures
use `native_check.py --live-boot --skip-hardware-tests --auto-setup`; the older
fixed setup inputs and diagnostic display-delay option are not their policy.
Evidence: `.run/calibration-{aga,ecs}` and `tmp/calibration-{aga,ecs}-check.log`.

Cold and warm WHDLoad runs both exit normally and write NVRAM/accounting, with
exact previous images retained as backups (`tmp/calibration-whdload.log`).
The normal build is restored after the startup-only profiling build. The card/
audio deadlines and startup elapsed-time target remain open.

## Current elapsed startup, separately measured

The existing STARTUP_PROFILE build takes three CIA-A TOD snapshots: preparation
entry, original execution entry, and Ready. No hot-path timing calls are added.
The new `amiga/startup-elapsed.gdb` reads these snapshots. The stub cannot read
CIA registers directly, so an attempted entirely debugger-side clock probe was
unusable and supplies no timing result.

**MEASURED A1200/PAL, 50 TOD ticks per second:**

| State | Preparation | Original initialization | Total to Ready |
|---|---:|---:|---:|
| Cold, no saved files | 0.72 s | 22.30 s | 23.02 s |
| Valid retained NVRAM/accounting | 0.76 s | 8.64 s | 9.40 s |

Both report error=0 and the expected retained flag (0/1). TOD initialization
deltas exactly equal VBI counts (1,115/432). These totals exclude executable
loading and early CRT, and have 20 ms resolution. The warm fixture is a normally
saved zero-credit game from the successful WHDLoad test, not an injected CPU
snapshot. This refresh supersedes the older 24.32/10.46 pair as the current
measurement; it is not a controlled speedup claim for the interrupt-window fix.
Original initialization dominates. SDL first-start parity is still not met.
Evidence: `.run/current-startup-{cold,warm}` and frozen
`tmp/perf/Pokeri-current-startup(.elf)`; measurements contain no recurring ledger.


## T7 completion-window experiment (2026-09-30, rejected)

**MEASURED:** leaving physical IPL zero throughout `nativeClockCalibrateNext`,
then restoring IPL7 before resume, does not remove the late samples. The
comparison has startup/display lines 41/48; the candidate has 41/45, both with
calibration active and a clean 24-input exit. The saved exception frame points
to the NOP immediately after lowering IPL in `nativeClockCalibrationTrap`,
before calling Next. Thus the pending VBI was delayed by the just-completed
masked probe, not by its subsequent completion bookkeeping. The optional
change was removed.

Next: bound the synthetic speed probes themselves while retaining their
instruction-cost measurement and conservative clock calibration. Do not count
unmasking bookkeeping as a solution to this reproduction. Local evidence:
`.run/t7-calibration2-vbi`, `tmp/t7-calibration2-vbi`; comparison
`.run/t10-memset-vbi`. The first `t7-calibration-vbi` build did not pass its
switch to the assembler and is a baseline-only observation.

## Bounded speed-probe candidate (2026-09-30, opt-in)

`CALIBRATION_CHUNKS=1` splits each existing 8,192-iteration synthetic loop
into 32 pieces of 256 iterations. Each piece captures its own elapsed cost,
subtracts the existing measured exception overhead, and uses the existing IRQ
window before the next piece. The three loops and total iteration counts are
unchanged; their accumulated costs drive the same conservative ratio selection.
The reference subtracts two cycles for each final not-taken branch (32 instead
of one). No original game instruction or device timing rule is replaced.

**MEASURED first A1200 cold live24:** maximum observed post-service VBI line is
7 during startup and 10 during play, with zero samples at line 29 or later.
All 24 inputs finish, with no error/reset and restored vectors. Calibration
minimum/maximum/overhead is 33/45/40 board cycles; summed speed probes are
19,307 / 29,148 / 39,136, and the CPU ceiling remains 80 sixteenths.
A repeat reaches startup/play maximum lines 6/10, again with no late samples
and a clean 24-input exit. It reports the same 33/45/40 overhead values and
19,307 / 29,136 / 39,136 speed costs, ceiling 80. The paired unchunked build
reports lines 41/48 and costs 19,634 / 29,618 / 39,275, also ceiling 80.

ECS completes all 24 inputs without error/reset and restores vectors. Its
startup/play maximum lines are 32/41 (one/83 late samples); five late samples
are in calibration and the remainder are ordinary gameplay. Its overhead
minimum/maximum/correction is 124/473/468; costs are 121,641 / 197,064 /
270,396, ceiling 11. This is an ECS correctness observation, not a latency
pass. ECS performance remains separately deferred.

The combined tick-product/chunked candidate's exact replay, cold/warm live24,
Double and trace gates are running. The option remains off by default.
Evidence: `.run/t7-chunks-{vbi,repeat,baseline,ecs}`, frozen
`tmp/t7-chunks-vbi` and `tmp/t10-memset-vbi`.
