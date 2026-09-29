# Remaining work

Updated 2026-09-29, after the rendering route audit/fast paths and approved
live PAL-envelope correction and initial release packaging.
This is the current work list. Other performance documents retain dated designs,
experiments and evidence; their older “next”, “pending” and “current” statements
are not additional tasks. Update this page when a task is closed or its scope changes.

## Active Phase 5 work

### 1. Card rendering and audio deadlines

**Open.** Reduce the remaining cost of feeding commands and servicing video
interrupts, and measure sound timing during drawing bursts. Faster individual
services are useful but do not establish that a complete card or sound update
meets its deadline.

**MEASURED:** the latest combined-handler live run has complete landing-back
intervals of **40.512 and 48.704 ms**, against the **20 ms complete-card target**.
These are two observations from a changing live hand, not a controlled mean or
a worst-case bound. The paired handler benchmark saves about **0.86 ms over 26
entry/exit pairs** on A1200; that is not a whole-card speedup measurement.

The Double transition now uses a separately proved cache case for existing
white card borders. Exact ECS/AGA replay and normal live24 pass. Fast-path
profiling reduces excess sound-write delay from 915 to 570 ms; cached complete
feeds still take 55–57 ms in that build. This improves the reported held note
but does not close the audio/card deadline. See [measurements and limits](double-transition-performance.md).

**MEASURED current attribution:** the 0.1 fast-cache ledger completes all 24
inputs with no error/reset or lost records. Typical cached landing intervals
are 55.48–56.94 ms raw, 51.87–53.26 ms after timestamp-read correction; they
contain 749–754 counted operations, about 6.84–7.69 ms inclusive video-command
service and 0–0.039 ms blitter wait. These are profiling observations, not
replacement release timings or a controlled comparison with the older hand.
The same run's post-Ready 54.12 board seconds take 60.03 PAL seconds; this does
not pass the 5% target. Its Double input does not establish a winning/Double
workload, so it supplies no new Double sound-write latency claim.
Local evidence: `.run/release-cost`, `tmp/release-cost-{cards,summary}.txt`.

Next: optimize the remaining command-feeding/interrupt-service cost. Include guard-rejected redraws: their
sequences now stay recognized for presentation, but still render procedurally.
Live Paula envelopes now follow PAL VBI time by explicit approval. The measured
Double fade reaches zero in 195.92 ms instead of remaining at level 7 after
658.92 ms. This fixes decay stretching, while the latest measured sound-write
gap still has 543 ms excess. See [envelope timing](live-envelope-clock-experiment.md).
Check complete cards, delayed AY writes and audible duration against the reference. Keep both
original video interrupts and all required instruction/device boundaries.
Close this item only with complete-card and audio-deadline evidence, not an
isolated blit or handler benchmark.

Evidence: [handler measurements](native-video-handler-plan.md),
[rendering history](native-rendering-followup.md),
[card-cache acceptance](card-back-blit-design.md).

### 2. Startup elapsed time

**Open.** Startup-only fast-forward, build-time card-cache preparation and
retained accounting are implemented. Required original initialization still
costs too much; saved accounting alone does not make native startup immediate.

**MEASURED current A1200 pair:** **23.02 s cold / 9.40 s warm**, including
preparation but excluding executable loading/early CRT. Preparation is only
0.72/0.76 s; original initialization takes 22.30/8.64 s. Three boundary-only
CIA-A TOD reads avoid the recurring ledger's observer cost. The valid warm
fixture was saved normally with zero credits. These replace the older
24.32/10.46 measurements; they do not establish a controlled per-change speedup.
See [measurement scope](startup-interrupt-latency.md#current-elapsed-startup-separately-measured).

The recorded SDL comparison is **2.90 s cold / 0.11 s cached** on this Mac;
SDL's cached launch restores a snapshot, whereas native warm launch still boots
the original CPU program. Startup parity is not demonstrated.

Next: target original initialization/accounting and artwork service cost.
Preserve nonzero-credit and interrupted-hand recovery; keep loading/early CRT
separate in any fuller timing capture. Do not substitute a warm run for cold.

Evidence: [cache preparation](card-cache-preparation.md),
[startup fast-forward](startup-fast-forward-design.md),
[startup policy](startup-policy.md).

### 3. Sustained gameplay and final timing validation

**Open acceptance gate.** After the burst work, measure the normal release over
representative deal/hold/draw, win, doubling and attract workloads. Report board
time versus PAL time, presentation cadence, worst drawing/audio delays and input
response. The recovery plan's real-time target remains within 5% of PAL time;
sustained 50 FPS has not been established by passing scripted gameplay.

Do not reuse the older 55.54-PAL-second/48-game-second burst result as a current
release measurement. Do not compare whole-run totals from different hands or
different numbers of shuffles as a controlled speedup. The latest VBI comparison,
for example, exercised two shuffles in the candidate and one in the baseline.

Keep headless model checks, active-path CPU proofs, exact ECS/AGA replay and live
cleanup/watchdog checks as required by each change. Rejected experiments are not
pending implementation; changing the clock or fidelity contract still requires
its existing decision gate.

Evidence: [performance constraints and gates](native-performance-plan.md),
[clock policy](native-clock.md), [completed burst experiments](native-burst-plan.md).

## Completed implementation — not remaining tasks

- Startup calibration now admits Amiga interrupts between completed timing
  samples. Read-only probes identify the late first VBI before game execution;
  maximum post-service scanline falls from 64 to 18/17 in two cold runs, with
  none at line 29 or later. Sample/probe masking and gameplay timing stay intact.
  Exact ECS/AGA replay, live24 and cold/warm WHDLoad saves pass. This resolves
  the reproduced calibration outlier, not all possible interrupt latency.
  See [startup interrupt measurements](startup-interrupt-latency.md).

- Read-acknowledged native keyboard transitions preserve short and repeated taps
  until guest input reads. The failing three-tap test now selects all three
  intended cards; queue/CPU tests and exact ECS/AGA replays pass. Original
  hold-ready gating remains unchanged. See [input response](input-response.md).

- Native composition is requested by original system-tick completion, with no
  independent presentation timer or card-hit trigger. It waits for the command
  ring to drain and for exact card recognition, including when
  background guards reject the cached bitmap and ordinary drawing is required.
  Real game pixel observations remain authoritative. The original live24 check
  covered admitted matches only; follow-up tests cover scalar redraws too. This
  presentation policy does not establish the 20 ms rendering deadline. See
  [native presentation](native-presentation.md). Physical VSYNC phase matching
  is explicitly not a prerequisite for this policy.
- Consumer-paced shuffle and in-motion sound scheduling on SDL and Amiga.
- Retained native accounting, save preservation and warm FS-UAE fixtures.
- Memory ownership/cleanup audit and the identified Guru regression fix.
- Artwork catalog, font expansion, guarded card-back/white-prefix caches and
  optimized copies of resident ranks, suits and picture-card assets. This does
  **not** mean every image needs or has a separate startup cache.
- Interleaved copy/scrolling paths and pattern-tile blits. Their implementation
  gates pass; whole-game timing still belongs to item 3 above.
- Startup diagnostic bypass, acknowledged setup and startup-only fast-forward.
- Bounded video-handler entry/exit fusion, now default. CPU proofs, exact ECS/AGA
  replay and active cold live24/cleanup pass. No validation run is pending for
  that change; its broader timing limitations are listed above.

See [rendering history](native-rendering-followup.md),
[shuffle pacing](shuffle-pacing.md) and [memory audit](memory-audit.md).

## Later or explicitly deferred

- **Physical fidelity:** compare palette/display timing and audio balance,
  noise and envelope behavior with physical-machine evidence. Digital equality
  between ports cannot validate a shared hardware-model assumption.
- **Hardware research:** unresolved FIFO semantics and other device-model
  qualifications remain documented in [rom-set.md](rom-set.md). They are not
  permission to change the reference or suppress interrupts for speed.
- **A500/ECS performance tuning:** deferred until A1200 gameplay performance is
  satisfactory. ECS compatibility and correctness checks remain required now.
- **Yellow stripe alignment:** discuss a separate cosmetic correction to the
  original-ROM artwork after the current work. No correction is authorized by
  this work list. Keep the coin on the corrected general ellipse renderer,
  with no coin-specific artwork adjustment.
- **Broader game-scope changes:** initial 0.1 packaging/WHDLoad is implemented;
  current coin, credit and operator behavior is retained. Any redesign of those
  features remains a separate decision. See [release checks](release.md).

There is no outstanding approval request for the completed handler experiment.
This list does not reopen measured/rejected experiments or authorize new timing
or hardware-model changes.
