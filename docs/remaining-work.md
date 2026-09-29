# Remaining work

Updated 2026-09-29, after the validated handler changes in `0923a85`.
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

Next: attribute the remaining complete-card time on the current release, then
optimize the dominant measured costs. Check complete cards, delayed AY writes,
envelope progression and audible duration against the reference. Keep both
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

**MEASURED:** the last preparation-inclusive A1200 pair was **24.32 s cold /
10.46 s warm**. These measurements precede the later pattern-tile and handler
optimizations and exclude executable loading/early CRT. They must not be
presented as a fresh total for `0923a85`. The recorded SDL comparison is
**2.90 s cold / 0.11 s cached** on this Mac; SDL's cached launch restores a
snapshot, whereas native warm launch still boots the original CPU program.

Next: refresh cold and retained-accounting totals, separating loading/preparation,
original initialization, artwork and refill. Target the remaining measured cost
and compare the first-run experience with SDL. Preserve original accounting,
nonzero-credit and interrupted-hand recovery behavior; do not substitute a warm
fixture for a cold-start result. Startup parity is not yet demonstrated.

Evidence: [cache preparation](card-cache-preparation.md),
[startup fast-forward](startup-fast-forward-design.md),
[startup policy](startup-policy.md).

### 3. Startup VBI outliers

**Open investigation.** The combined candidate's post-service VBI probe recorded
two startup samples as late as **scanline 75**; this baseline run had no late
samples and a maximum of line 13. Gameplay had **3,106 samples, maximum line 12,
none at line 29 or later**.

Next: identify what accounts for the startup outliers and whether the difference
is reproducible. These are post-service samples; they do not alone distinguish
late interrupt entry from time spent inside the service. Do not claim that
startup interrupt latency is unchanged or already fixed.

Evidence: [combined validation](native-video-handler-plan.md#completed-gates-and-activation).

### 4. Sustained gameplay and final timing validation

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

- Consumer-paced shuffle and in-motion sound scheduling on SDL and Amiga.
- Retained native accounting, save preservation and warm FS-UAE fixtures.
- Memory ownership/cleanup audit and the identified Guru regression fix.
- Artwork catalog, font expansion, guarded card-back/white-prefix caches and
  optimized copies of resident ranks, suits and picture-card assets. This does
  **not** mean every image needs or has a separate startup cache.
- Interleaved copy/scrolling paths and pattern-tile blits. Their implementation
  gates pass; whole-game timing still belongs to item 4 above.
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
- **Phase 6 release:** not started. Packaging/WHDLoad and the exposed coin,
  credit and operator features need their own scope decisions; see the
  [release section](bringup-plan.md).

There is no outstanding approval request for the completed handler experiment.
This list does not reopen measured/rejected experiments or authorize new timing
or hardware-model changes.
