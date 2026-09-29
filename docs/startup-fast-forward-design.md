# Startup-only fast-forward proposal

Status: approved by the user on 2026-09-29; validated and enabled by default.
Gameplay and diagnostic replay retain their existing timing policies. This addresses the user's request that cold initialization be as quick
as practical SDL startup; it does not relax gameplay timing or graphics fidelity.

## Reason and scope

**MEASURED:** the register-resident feeder candidate reaches ordinary cold Ready
in 33.12 PAL seconds. Its separate instrumented refill interval consumes 10.08
board seconds and 26.29 wall seconds; initial artwork takes 13.19 wall seconds.
The current wall-bounded clock cannot finish that refill in less than its board
duration, regardless of faster rasterization. See native-rendering-followup.md.

**DERIVED:** SDL already runs original initialization and acknowledgment-driven
setup without a wall-time throttle, then opens live playback. A matching native
preparation mode needs a different time policy before Ready. It must not import
a manufactured ready snapshot, reserve balance, CPU state or sound sequence.

## Proposed behavior

1. Enter only for normal fast boot, before Startup::Ready. Keep hardware-test
   research runs, diagnostic replay and explicit clock comparisons unchanged.
2. Execute original initialization, the existing cabinet protocol and original
   accounting code. The retained-accounting path remains a fresh CPU boot and
   preserves the player's existing credits and interrupted-hand recovery.
3. Advance startup device time from bounded guest work and exact nominal hook
   cycles without waiting for PAL wall deadlines. Reuse the verified delay-loop
   CPU-state kernel only at its guarded original site, charging the skipped
   iterations' exact reference cycles. Never skip a loop with other side effects.
4. Split delay advancement at the next device/event deadline. Deliver original
   interrupts at the existing safe boundaries; preserve virtual IPL, source
   priority, acknowledgments and the rule that a pending handler returns before
   a new timer edge can replace its flag. Keep watchdog checks and loud failures.
5. During preparation, publish completed artwork at physical frame cadence,
   with the existing buffer/queue ownership rules. Do not wait for every virtual
   frame to be shown. Keep Paula's state current but begin audible playback at
   Ready, matching SDL's preparation behavior. This mute is startup-only; ordinary
   gameplay and normal run.sh remain audible.
6. At acknowledged Ready, clear startup-only timing debt/credit and establish the
   current PAL frame as the live clock origin. Enable the existing calibrated
   gameplay policy and audio; never replay a burst of missed startup frames or
   timer interrupts into play. Persistence and accounting formats are unchanged.
7. Retain an explicit comparison switch to use today's wall-bounded startup.
   No automatic fallback that hides errors or supplies a guessed ready state.

The implementation must first establish the next-event bound for the existing
startup scheduler. If that bound cannot be established, stop the experiment;
wall-time parity is not permission to guess device timing or overwrite game RAM.

## Acceptance

- Independent 68000/68020 checks for every admitted delay-loop boundary, PC,
  CCR, registers and exact reference-cycle charge, including interruption.
- Headless cold/warm accounting, nonzero credits, interrupted-hand recovery,
  corrupt-save handling, and watchdog/serial acknowledgment checks.
- Existing exact ECS/AGA native replay unchanged, and live cold/warm runs on both
  chipsets through real input, shuffle sound, saved accounting and heap cleanup.
- Compare first launch and retained-accounting launch separately, including
  executable preparation and Ready time. Report remaining native cost honestly;
  an M1 host's absolute startup time is not guaranteed for a stock Amiga CPU.
- Check the Ready transition for timer catch-up, audio bursts, delayed inputs and
  graphics ownership. Re-measure gameplay to show its timing policy is unchanged.

Changing startup's time policy requires the timing-contract approval described
in native-performance-plan.md. Approval authorizes a measured prototype;
only a faithful, validated improvement would become the default.

## Prototype and next-event bound (2026-09-29)

`STARTUP_FAST_FORWARD=1` builds the prototype. `native-startup-wall` disables
it for comparison. Replay, coin-op hardware-test runs, legacy/corrected-only
clock modes and the isolated benchmark retain their existing paths.

**DERIVED:** the supported native profile is fixed at 8 MHz, 100 Hz system,
50 Hz input, 400 ms watchdog warning/450 ms reset, and a 1 kHz serial peer.
Starting from zero phases, all interrupt-producing timed edges lie on 8,000
cycle boundaries. AY envelope edges are handled inside the backend and do not
produce CPU interrupts. Startup device advancement therefore uses 1 ms quanta,
checking for an eligible IRQ before each one; a pending handler must return
before the next quantum. Cabinet setup observations remain every 10 ms.

The verified delay kernel batches only up to the next quantum, allowing the
indivisible four-cycle SUBQ to cross it by at most three cycles. Original
registers, flags and nominal cycles remain those of that kernel. Ordinary
native work retains the existing calibrated boot ratio, but no PAL debt limit;
nominal hook/delay cycles are not scaled. Presentation is limited to physical
frame cadence. Paula registers/envelopes keep updating while output is muted.

At Ready, the prototype restores the original SUBQ word (unless the user chose
the separate gameplay idle hook), clears its instruction cache once, clears
startup queued time/debt/credit and restores the existing gameplay clock.
Audio begins from current AY state, without replaying preparation sounds.

**MEASURED first draft:** Ready at 1,675 PAL frames (33.50 s), versus 1,552
(31.04 s) for the accepted FIFO-control build. It passed all 24 inputs, 30
shuffle steps and 60 in-motion AY writes, with no errors/resets. At Ready,
startup mode/mute/phase/queued ticks/credit/debt were all zero and the original
delay opcode was restored. This draft mistakenly used unscaled guest work;
it is slower and is not accepted. Its finer serial schedule also changes the
board-time endpoint, so elapsed board time alone is not a performance result.

The corrected second draft preserves calibrated guest-work scaling. Its
portable budget checks pass 83,886,080 scaling cases and 48,000 next-edge delay
budgets. The independent native delay kernel passes 524,356 CPU cases and
458,661 cycle budgets. The second cold AGA run is in progress; warm, ECS and
full replay gates remain pending. Normal builds are restored after freezing
each candidate. Local evidence: tmp/startup-budget-check.log,
tmp/startup-fast-delay-check.log, amiga/.run/startup-fast-cold-aga and
amiga/.run/startup-fast-v2-cold-aga.

### Measured prototype iterations

- V2 retains calibrated scaling: cold AGA Ready 1,676 PAL frames (33.52 s),
  all 24 inputs/30 shuffle steps/60 in-motion AY writes, no errors/resets.
- V3 groups empty serial intervals: Ready 1,675 frames, no material benefit.
  That experiment was removed. Future grouping must also account for watchdog
  deadlines relative to arbitrary kicks, not just the global 100 Hz phase.
- V4 keeps 1 ms scheduling and limits preparation presentation to 5 Hz. Cold
  AGA Ready is **1,251 frames / 25.02 s**, versus accepted 1,552 / 31.04 s.
  It reaches 47,120,000 board cycles, exactly as V3, with 125 presented frames
  instead of 522. Full cold live24 passes, error/reset zero.
- V4 warm AGA Ready: 535 frames / 10.70 s (earlier normal ~10.4 s). Saved
  credits **1 → 1** and reserve **102 → 102** match at Ready; retained=true,
  coins inserted=0. All 24 inputs/30 shuffle steps/60 AY writes pass.
- V4 cold ECS Ready: 6,483 frames / 129.66 s (accepted 6,242 / 124.84 s).
  All 24 inputs/30 shuffle steps/45 AY writes pass without errors/resets.

These mixed results do not justify default activation. All three V4 Ready
transitions have fast/mute/phase/queued ticks/credit/debt zero and restore the
original delay instruction. Exact V4 ECS/AGA replays both pass.

V5 changes only preparation presentation to at most once per second; Ready
still requests an immediate refresh and gameplay keeps normal presentation.
Cold ECS/AGA and warm AGA measurements are running from frozen
`tmp/perf/Pokeri-startup-fast-v5(.elf)`. The normal executable is restored and
its allocated sections have been checked against the accepted release.

### V5 completed live checks

**MEASURED:** cold AGA Ready 1,184 frames / **23.68 s** (accepted 31.04 s);
warm AGA 482 / **9.64 s** (earlier ~10.4 s); cold ECS 5,840 / **116.80 s**
(accepted 124.84 s); warm ECS 2,568 / **51.36 s**. All four full live24 runs
complete with no errors/resets and all 24 inputs. Shuffle steps/in-motion AY
writes: cold AGA 30/60, warm AGA 60/120, cold ECS 60/94, warm ECS 30/45.
Different live hands do not define an exact AY-total equality check. Both warm
starts preserve saved credits 1 and reserve 102 with zero inserted coins.
All Ready transitions clear startup debt/queued ticks/mute and restore SUBQ.

Interrupted-hand recovery also completes without errors/reset or injected
inputs. All retained bytes match the original-code host result except the
original pseudo-random state, explained exactly by differing update counts;
see rom-set.md. Credits zero/reserve 101 agree. CPU hook/feeder/FIFO matrices
pass against the frozen V5 candidate.

AGA exact replay passes all 262,144 RAM bytes, 524,288 VRAM bytes, 172,064
pixels and 60 AY writes at 7,904,133 instructions/64,000,000 cycles/8,685 IRQs.
ECS replay also passes every RAM/VRAM/pixel/AY comparison at the same instruction/cycle/IRQ boundary. The V4 replay binary and V5 candidate have
identical allocated sections except two immediate bytes: preparation threshold
9 becomes 49 at `$AB13` and `$B89B`. Disassembly verifies both are guarded by
`startupFast`, which remains false throughout diagnostic replay. This proves
that the cadence change does not alter the replay path; live tests use V5.

The remaining activation gates were subsequently completed below. The 23.68 s
native cold start still falls well short of SDL; this is an improvement, not
completion of the startup/performance objective.

## Default activation and full preparation timing

`STARTUP_FAST_FORWARD=1` is now the default; `=0` retains the earlier build.
`native-startup-wall`, legacy/corrected-only modes and explicit
`native-clock-ratio`, `native-clock-play-ratio` or `native-clock-window` files
disable acceleration. Three native override runs verify fast=false, mute=false,
the original delay instruction and clean post-destructor exit. Research clocks
therefore keep their previous startup contract. Normal runs need no new files.

**MEASURED paired A1200 runs**, using the same frozen profiling executable and
starting accounting. `STARTUP_PROFILE=1` samples CIA-A TOD only three times: at
entry to native preparation, before original execution, and at Ready. The PAL
source has 50 ticks/second; it includes six elapsed ticks missing from the
software VBI counter in each initialization measurement. All times below
include native preparation and exclude executable loading and the CRT work before native preparation.

| Policy | Preparation | Original initialization/setup | Total to Ready |
|---|---:|---:|---:|
| Old cold | 3.18 s | 31.60 s | 34.78 s |
| New cold | 3.12 s | 23.76 s | **26.88 s** |
| Old warm | 3.16 s | 10.56 s | 13.72 s |
| New warm | 3.14 s | 9.74 s | **12.88 s** |

These are 20 ms-resolution native measurements, not host stopwatch estimates.
The three timestamp calls allocate no sampling buffers and are compiled out of
normal builds. Evidence: amiga/.run/startup-profile-{cold,warm},
amiga/.run/startup-profile-old-{cold,warm}, tmp/perf/Pokeri-startup-profile(.elf).
The matching measured runs all reach the verified CRT epilogue after static
destructors with heapHead=0, no watchdog reset and no Exec Alert.

**MEASURED failure cleanup:** a corrupted accounting file is rejected before
any guest instruction with “invalid retained accounting”; its bytes remain
unchanged. The process reaches the post-destructor epilogue without an alert,
heapHead=0, no Board allocation, closed DOS/timer/audio resources and no
installed Paula servers. Evidence: amiga/.run/startup-final-corrupt. Existing
unit tests separately cover every single-bit save corruption.

The release selector change only disables acceleration in explicitly requested
clock comparisons. All ordinary paths retain the validated V5 startup behavior;
its full cold/warm ECS/AGA scenarios, CPU matrices and exact replay evidence
remain applicable. Preparation still takes about 3.1 s and cold initialization
another 23.8 s: the larger cold-start and gameplay performance objectives remain
open. This activation does not claim SDL-equivalent startup or 50 FPS gameplay.

## Build-time card-cache preparation (2026-09-29)

**MEASURED:** moving the existing two-pass card-cache preparation into the build
reduces paired A1200 cold Ready from 26.70 to **24.32 s**, and retained-accounting
Ready from 12.86 to **10.46 s**, including native preparation but excluding
executable loading/early CRT. Original initialization takes 23.62 / 9.70 s;
this change does not alter its timing policy. Exact ECS/AGA replay, prepared-data
equality and cold/warm live cleanup pass. `CARD_PREPARED=1` is now the default;
see [card-cache-preparation.md](card-cache-preparation.md) for measurement scope,
comparison controls and proof. Startup parity and gameplay deadlines remain open.
