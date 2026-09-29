# Separating profiling support from normal service code

2026-09-29. Validated and enabled by default.
The current acceptance queue is [remaining-work.md](remaining-work.md).

## Why this matters

**DERIVED:** even with `native-measure` absent, the previous normal executable
contains runtime `NativeTiming::active` tests and scope bookkeeping throughout
clock accounting, dispatch and drawing. These branches cannot be removed by
ordinary per-file compilation because another translation unit may activate
the profiler. They increase the instruction footprint and service cost.

`PROFILE_SUPPORT=0` makes the hot-path profiling predicate a compile-time false.
`PROFILE_SUPPORT=1` retains runtime measurement. The benchmark timer remains
available for explicit isolated batches. A profiling-free executable rejects
`native-measure` before original execution with an explanatory error. This
changes no original instruction, IRQ, model observation or timing policy.

The normal dispatcher shrinks from 17,306 to 15,020 bytes, and clock pause from
3,084 to 2,810 bytes (linked symbol sizes). This is executable size evidence,
not a prediction of total speedup. The supported profiling build is not byte-
identical after recompilation; its sampler/counter behavior must be checked.

## Controlled feeder measurement

**MEASURED A1200**, identical prepared streams, four repetitions per cell,
DMA active, timing only at batch boundaries, final blitter completion included.
Setup and clearing are outside timing. These batches exclude original video
handlers and are not complete live-card deadlines. Values are E-clock ticks at
709,379 Hz:

| Path | Back whole | Back 10-word | Back 1-word | White whole | White 10-word | White 1-word |
|---|---:|---:|---:|---:|---:|---:|
| Runtime-disabled profiling | 64,241 | 82,365 | 242,986 | 32,505 | 36,457 | 75,699 |
| Profiling compiled out | 59,751 | 77,801 | 236,619 | 31,086 | 35,296 | 74,094 |

The whole back falls **22.64 -> 21.06 ms**; ten-word chunks **29.03 -> 27.42 ms**.
All cells improve. Both runs finish status 4/error 0 with zero board cycles or
frames advanced. Reproduce with `RASTER_CHUNKS=1` and
`amiga/cached-raster-chunks.gdb`, changing `PROFILE_SUPPORT` after a clean build.
Local fixtures: `.run/feed-local-baseline`, `.run/profile-free-bench`.

### Rejected local parameter path

A separate earlier experiment duplicated the existing intermediate-parameter
store inline in the register feeder. It retained every original status/write
boundary. The original version passed the linked fused-feed and full-loop
matrices (including 3,755,520 whole-feed cases), but the benchmark did not justify
it. Refining it to avoid a duplicate count test still measured 64,045 / 82,423 /
242,997 ticks for whole/ten-word/one-word backs versus the baseline above:
less than 0.4% on the whole stream and no fragmented-feed benefit. White totals
were 32,309 / 36,489 / 75,818. Neither version is retained. The refinement did
not proceed to full validation because its performance gate failed.
Local evidence: `.run/feed-local-{after,baseline}`, `.run/feed-local2`,
`tmp/feed-local-feed.log`, frozen `tmp/perf/Pokeri-feed-local*`.

## Normal executable observations

`amiga/release-timing.gdb` reads PAL frame/beam position at Ready, external key
transitions, complete back-cache boundaries, AY writes and normal return.
There is no in-game observer or recurring timer read. Breakpoints pause emulation;
these numbers are emulated PAL time, not host stopwatch time. Card offsets are
byte-checked against their actual counter members before installing probes.
`host/release_timing.py` summarizes the log, rejects incomplete/error runs, and
explicitly reports whether an accepted Double callback was observed.

**MEASURED:** baseline/candidate each finish all 24 inputs with zero error/reset
and restored vectors. Ready-to-finish is 54.11 board / 56.914 PAL seconds before
and 54.12 / 56.807 after (ratios 0.9507/0.9527). Session endpoint uncertainty is
up to one frame. Different live hands and frame placement mean this is not a
controlled whole-session speedup. Baseline deal/draw ratios are 0.9049/0.9415;
candidate 0.9090/0.9338. Bursts still fail the real-time target.

Completed landing backs 24–27 measure 48.832/41.792/42.016/40.448 ms before and
38.784/39.936/39.872/38.592 ms after. Light backs 21/23 are 18.784/18.176 versus
17.024/16.832 ms. These are begin-to-cache-hit observations, with roughly 64 us
beam resolution, not final display publication. No 20 ms landing claim follows.

Both runs request Double with its ready flag zero. The first reader observed
$1DD5A, but that drawing helper is shared and its calls do **not** prove Double
coverage. The corrected reader observes $1818A, after the accepted callback's
$18180 flag check/$18186 refusal branch. Thus the early logs' `double_enter`
records are deliberately ignored by the analyzer. No new Double deadline or
audible-duration claim is made. Largest generic AY gaps are reported separately
and can include service-door work and intentionally silent/long intervals.
Evidence: `.run/release-timing2`, `.run/profile-free-live`,
`tmp/release-timing-before.txt`, `tmp/profile-free-live.txt`.

## Startup

**MEASURED:** three existing `STARTUP_PROFILE=1` TOD snapshots, no recurring
instrumentation, 20 ms resolution. Totals exclude loading/early CRT.

| Profile support | Cold preparation/init/total | Warm preparation/init/total |
|---|---:|---:|
| Previous current measurement | 0.72 / 22.30 / 23.02 s | 0.76 / 8.64 / 9.40 s |
| Compiled out | 0.80 / 21.68 / 22.48 s | 0.80 / 8.42 / 9.22 s |

The warm fixture is the same normally saved zero-credit WHDLoad state used by
the previous measurement, not a CPU snapshot. Both reach Ready without error
and with the expected retained flag. This is a modest improvement; original
initialization still dominates and SDL startup parity is not established.
Evidence: `.run/profile-free-startup-{cold,warm}` and
`tmp/perf/Pokeri-profile-free-startup(.elf)`.

## Completed validation and default selection

Host model/platform/native and linked short/feed CPU checks pass. Full ECS and
AGA replay each match all 262,144 RAM bytes, 524,288 VRAM bytes, 172,064 displayed
pixels and 60 AY writes at 7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs.
Cold ECS and cold/warm AGA live runs complete all 24 inputs without error/reset
and restore vectors. Cold/warm WHDLoad both return normally, write both save files
and retain exact previous-image backups. The startup VBI probe records 1,084
samples: maximum entry/service/completion 15/19/19, none at line 29 or later.
The clock calibration remains minimum/maximum 33/45, overhead 40 cycles.

The warm normal run covers 59.42 board seconds in 62.015 PAL seconds (0.9582),
but its deal/draw intervals are 0.9172/0.9416. It also does not accept Double;
full representative timing acceptance remains open.

The explicit supported build reaches Ready with `active=1`, 1,159 PC samples
and nonzero dispatch counters. A normal build with `native-measure` refuses
before any guest instruction, sampler disabled, with the documented message.
The first refusal probe incorrectly waited for guest return despite preparation
failing; the corrected probe stops at `nativePrepared` and checks the Boolean
return's low byte (zero). Both private test emulators were closed.

Normal builds now select `PROFILE_SUPPORT=0`. Set `PROFILE_SUPPORT=1` for the
runtime `native-measure` selector; `DISPATCH_PROFILE=1` and `TIME_LEDGER=1` select
support automatically. Explicitly combining either with support disabled is a
build error. Clean after changing flags. The restored default's allocated ELF
sections exactly match the validated normal candidate. The release archive has
not been repackaged by this performance change.

Evidence: `tmp/profile-free-{headless,hooks,aga-check,ecs-check,whdload}.log`,
`.run/profile-free-{aga,ecs,live,live-ecs,warm-live,vbi}`, and
`.run/profile-{support-check,free-marker-check2}`. The cold timing script in the
first two normal captures predates the corrected Double boundary; the warm
capture uses it. None provides accepted-Double timing evidence.
