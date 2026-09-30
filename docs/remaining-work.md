# Remaining work

Updated 2026-09-29, after the cycle-exact instruction-trace profile of startup,
deal/draw and an accepted Double, and the WHDLoad default-options plan (item 4).
This is the current work list. Other performance documents retain dated designs,
experiments and evidence; their older “next”, “pending” and “current” statements
are not additional tasks. Update this page when a task is closed or its scope changes.

## Active Phase 5 work

### Implementation queue (T1–T14)

Derived from the [instruction-trace profile](trace-profile.md). Its measurements
have no in-game observer and are the current attribution for items 1–3; each
item's measured basis is in its candidate table. The queue is ordered by measured
benefit per risk; T1–T4 are small, surgical admissions.

Every item keeps the original instructions, the shared device endpoints, the
existing promotion rules and every interrupt boundary. It changes no clock
contract or device model. Common gates:
- linked CPU/oracle matrices for any new assembly form
- headless suites
- exact ECS/AGA replay
- cold/warm live24 on A1200 and ECS
- `amiga/trace.sh` re-measurement of the affected phase
- for gameplay items, normal-code AY lateness (`host/release_timing.py --scenario double`)

Record the measured result here when closing an item. Estimates are FS-UAE A1200
figures.

- [x] **T1 — AY strobe writes (item 1), completed 2026-09-29.** Admit
  D3–D7 for peripheral byte writes, retaining existing read forms and promotion
  boundaries. **MEASURED:** `$0D68`/`$0D7C` cost 444/427 → 136/139 µs;
  630 full dispatches at each site become 2/7. Normal-code accepted-Double AY
  batch median 21.7 → 12.6 ms. Its 323/358 ms late sound-write gaps remain;
  this is not an audio-deadline pass. Headless and linked short/feed matrices,
  full ECS/AGA state equality and cold/warm live24 on both machines pass.
  [Evidence and limits](trace-profile.md#t1-ay-strobe-register-admission-2026-09-29).
- [x] **T2 — card-window callback (item 1), completed 2026-09-29.** Checked
  absolute ACRTC byte reads and byte/word writes use the shared endpoints with
  unchanged promotion boundaries. **MEASURED:** the sum of 20 access-site means
  is 8.183 → 2.514 ms against T1, above the proposed 1.5 ms estimate. Only
  18/3,164 accesses promote to full dispatch. Cold/warm A1200/ECS live24, exact
  replay, headless/assembly matrices and accepted-Double checks pass. Normal
  Double's largest sound-write excess is 267 ms; the deadline remains open.
  [Evidence and limits](trace-profile.md#t2-absolute-video-register-accesses-2026-09-29).
- [x] **T3 — serial ISR (item 2), completed 2026-09-29.** Admit A1 port
  MOVEs/bit tests and checked byte postincrement sources from A2. **MEASURED:**
  the nine serial sites take 3.313 → 1.290 s in cold-start traces; normal cold
  Ready is 2.050 s earlier than T2 from the same VBI origin. Shared endpoints,
  flags and promotion boundaries remain intact. Headless/CPU matrices, exact
  ECS/AGA replay, cold/warm live24 and accepted-Double regressions pass.
  [Evidence and limits](trace-profile.md#t3-serial-interrupt-operands-2026-09-29).
- [x] **T4 — input apply (items 1, 3), completed 2026-09-29.** Snapshot only
  the 15 consumed event keys, retaining Escape's level and the original
  read-acknowledged queues. The old code copied/cleared two 128-byte event
  arrays, not three. **MEASURED:** 438.6 → 123.0 µs/call (72% lower), about
  1.58% CPU saved at 50 Hz. Synthetic short/repeat/overlap/overflow and 50,000
  reference events pass, along with headless, full ECS/AGA replay, cold/warm
  live24 and accepted-Double gates. Double still has 242 ms excess sound-write
  delay; audio deadlines remain open.
  [Evidence and limits](input-response.md#t4-compact-event-snapshot-2026-09-29).
- [x] **T5 — FIFO-empty interrupt delivery (items 1, 2), completed 2026-09-29.**
  Default assembly admission and exact bounded deferred clock reduce the two
  targeted means from 278/325 to 247/266 µs. The follow-up inlines unchanged
  instruction-boundary checks and clears the aligned address-phase pair in one
  store: isolated entry/exit/triplet batches improve 2.9%/6.2%/4.9%; gameplay
  entry/empty-tail sites improve 93.6/146.7 → 87.4/139.9 µs. Every original
  instruction and IRQ boundary remains. CPU/headless, exact ECS/AGA replay,
  cold/warm live24 and accepted Double pass. VBI probes identify two late
  samples during existing calibration, not FIFO execution; see T7. Complete
  cards remain typically 35–43 ms and Double sound-write excess reaches 229 ms.
  Those targets remain open; T6 and T13 address the remaining overhead.
  [Evidence and limits](native-video-irq-fast-path.md#t5-handler-boundary-and-address-phase-completion-2026-09-29).
- [x] **T6 — full-dispatch fixed cost (items 1–3), completed 2026-09-30.**
  Default ordered clock batching preserves each credit-spending boundary;
  the peripheral IRQ cache invalidates on every source-changing route; a local
  work mask skips inactive shuffle service and redundant status refreshes.
  Composition, reset and replay still refresh status, and publication still
  checks fresh buffer/blitter state. **MEASURED:** clock pause 57.5 → 45.8 µs;
  IRQ queries 20.4 → 11.5 µs; peripheral writes 87.2 → 75.9 µs. Controlled
  dispatcher work-gating cost is 170.9 → 151.8 µs (11.2% lower).
  Observed inclusive gameplay dispatch means are 443.5 → 415.8 µs across
  differing hands, so the estimated −30% target was not demonstrated. The
  larger card/audio targets remain open under items 1–3 and T7/T13/T14.
  Linked/host oracles, live invariant checks, exact ECS/AGA replay, cold/warm
  live24 and normal Double pass. Its latest AY batch median is 11.4 ms and
  largest excess batch delay 218 ms. The two late VBI samples are proved to
  occur during calibration, retained as T7 work.
  [Evidence and constraints](native-dispatch-fixed-cost.md).
- [ ] **T7 — tick path (items 1, 3).** Tick-handler RTE `$0C3E` costs 731 µs when
  full; `Board::tick` costs 150–190 µs with 64-bit phase arithmetic and model ticks;
  trace-exception tick delivery in the hook-free delay loop costs 565 µs each
  (4.2% of gameplay). Target −0.3 ms per tick, −3% steady CPU.
  Also bound the existing masked calibration work: T5's VBI probe
  catches one pre-game and one display-recalibration sample at lines 32–40,
  both with `nativeClockCalibrating=1`; ordinary FIFO gameplay has no late sample.
  **MEASURED follow-up:** the saved frame locates these delays at the end of
  the masked speed probe, before completion bookkeeping. Unmasking that
  bookkeeping alone was tested and rejected. Default `CALIBRATION_CHUNKS=1`
  bounds probes to 256 iterations; repeated A1200 live24 has no late samples
  (maximum startup/play lines 7/10). Exact ECS/AGA replay, cold/warm live24,
  Double and trace gates pass. ECS remains correct but has late VBI samples.
  **Partial completion (2026-09-30):** default `TICK_PRODUCT=1` passes
  200,450 CPU cases, exact ECS/AGA replay, cold/warm live24 and accepted
  Double; observed tick cost is 192.4 → 156.5 µs. Default `TICK_RETURN=1` passes CPU/boundary matrices, exact ECS/AGA replay,
  cold/warm live24, Double and VBI gates; observed full tick-return cost falls
  589.2 → 516.9 µs. The estimated −0.3 ms/−3% improvement remains unproved,
  and trace-exception service still uses the shared scheduler. [Evidence](trace-profile.md#t7-tick-arithmetic-default-2026-09-30).
- [x] **T8 — drawing hot spots (items 1, 2), completed 2026-09-30.**
  - (a) **Completed 2026-09-30:** default `CARD_RIGHT_WHITE=1` admits the
    separately proved left-eligible/right-white background. 5,369 differential
    cases and borrowed FIFO tests pass; the live trace has zero guard refusals
    in its captured deal/draw/Double workload. It reuses the original bitmap
    with exact per-command state; other mixtures still fall back.
  - (b) **Completed 2026-09-30:** default `COPY180_WORD_PLANES=1` shares copy
    geometry across planes. Pixel-oracle tests pass; observed core mean drops
    3.67 → 2.20 ms. The combined (a)/(b) candidate passes headless suites, exact
    ECS/AGA replay, cold/warm live24, Double and VBI checks. Normal Double has
    an 11.6 ms median AY batch and 222.4 ms maximum excess batch delay. These
    are scoped improvements, not whole-card/audio-deadline closure.
  - (c) Boot primitives: historical 4.9 ms per curve outline, PAINT 1.08 s and
    pattern-tile expansion 0.62 s of warm boot. **Completed:** current warm
    capture attributes 1.15 s to PAINT, 0.54 s to curves and 0.24 s to pattern
    expansion (capture extends past Ready). Default `SOLID_COLOR_PLANES=1`
    passes exhaustive colour/tile checks and reduces observed tile mean 571 →
    527 µs. Default `SMALL_FILL_WORD_PLANES=1` passes pixel oracles and lowers
    observed small-fill mean 343 → 150 µs. Bounded `DENSE_CURVE_STAMPS=1`
    passes packed/planar comparisons and lowers observed stamp mean 2.13 →
    0.83 ms (whole curve 4.88 → 3.54 ms). All three are default after exact
    ECS/AGA replay, cold/warm live24 on both machines, Double, VBI and headless
    gates pass. Cold/warm Ready measured 0.377/0.361 s earlier from the same
    VBI origin. Double still has 195.7 ms maximum excess AY batch delay.

  The original estimates (−130–160 ms stalls/deal, −40–60 ms/reveal set,
  −0.8 s boot) were not demonstrated as combined whole-workload gains. T8
  implementation and its correctness gates are complete; the larger card,
  startup and audio deadlines remain open under items 1–3.
- [x] **T9 — startup quanta (item 2), completed 2026-09-30.** Default
  `STARTUP_QUIET_BATCH=1` advances on the existing 1 ms grid only to the next
  serial/system/input/watchdog/cabinet boundary. Default
  `STARTUP_DELAY_SHORT=1` returns through assembly when an entire original
  delay loop fits before that quantum, preserving flags and promotion checks.
  **MEASURED:** quiet batching saves 1.835 s cold versus T8c; three paired
  assembly probes save another 19.7–23.3 ms. Board cycles and 100 reserve
  coins are unchanged. The estimated 2.5–4 s saving was not demonstrated.
  Linked CPU/policy oracles, headless suites, exact ECS/AGA replay, cold/warm
  live24 on both machines, accepted Double and VBI checks pass. Final startup
  instruction tracing confirms the new path. Normal allocated ELF sections
  match the validated candidate exactly. A1200 cold/warm gameplay ratios are
  0.9764/0.9820; accepted Double has 11.6 ms median AY batch span and 231.8 ms
  maximum excess batch delay. Startup parity and audio/card deadlines remain
  open. [Evidence](trace-profile.md#t9-default-activation-2026-09-30).
- [x] **T10 — preparation attribution (item 2), completed 2026-09-29.**
  Added `trace.sh prepare` from `nativePrepareInner` and a vector-installation
  boundary marker. **MEASURED:** approximately 0.70–0.74 s; a preparation-only
  720 ms window spends 51.18% in bytewise `memset`, 35.40% in Kickstart and
  4.84% in preparation's own code. This closes the attribution/tooling task,
  not the startup-speed target. An aligned wide fill is the concrete small-win
  candidate; required initialization must remain.
  [Evidence and reproduction](trace-profile.md#t10--preparation-attribution-2026-09-29).

  **Follow-up completed 2026-09-30:** default `FAST_MEMSET=1` preserves every
  write while filling aligned words. 49,586 CPU cases pass; paired preparation
  drops 740–760 → 420–440 ms. Exact ECS/AGA replay, cold/warm live24 on both
  machines, accepted Double and gameplay traces pass. Calibration still has
  two late VBI samples, tracked under T7; overall startup remains open.

**Expected after T1–T9 (estimate, re-measure):** warm Ready 9.2 → ≈6.5 s; cold
22.5 → ≈13 s; cached landing back 38–40 → ≈25 ms; Double entry busy 420 →
≈250 ms; Double sound lateness ≈540 → ≈250–300 ms. The 20 ms card and
no-stretch goals are not reachable by these alone on the A1200 preset; see the
derived floor in the profile.

**Decisions made by the user (2026-09-29).** Background is in
[trace-profile.md](trace-profile.md#decisions-2026-09-29).
- A. ACRTC drawing/FIFO timing: research first. No model or default change; see
  T11.
- B. Automatic reserve coins: keep 100. First launch only; T3 and T9 address its
  cost.
- C. Build-time boot artwork: design study now (T12). Implementation needs a
  separate go/no-go after T5, T6 and T8c are re-measured.
- D. Wider fused sequences: authorized for the FIFO-empty interrupt handler (T13)
  and `sound_register_write` (T14) only. `move_card_window_tick` is not
  authorized; T2 covers it.

- [x] **T11 — ACRTC timing research (decision A), completed 2026-09-30.**
  [Study, measurements and adoption proposal](acrtc-timing-study.md) include
  the verified manual table, finite eight-word FIFO prototype, explicit
  command-duration/geometry hypotheses and startup/deal/reveal/accepted-Double
  sweeps against physical footage. Zero-duration runs match the ordinary
  reference exactly; four timing scenarios and capture repeats pass without
  errors/resets. At 3M hypothetical table cycles/s, startup video IRQs fall
  5,870 → 3,422; Double's first two seconds use 178 vs 590 services. Animation
  phases do not establish a unique rate, and original AY writes also move.
  **Disposition:** keep the prototype host-only and production timing unchanged.
  This completes the bounded study, not physical calibration. Adoption would
  require a new decision, full FIFO/latch semantics and versioned regenerated
  snapshot/replay references; the study specifies those gates.
- [ ] **T12 — boot artwork design study (decision C).** Using the host harness,
  measure:
  - the complete boot command stream up to door open;
  - which words vary with settings, retained accounting and credits;
  - where it interleaves with other drawing;
  - the prepared data size and the realistic saving.

  **Study complete; implementation decision pending:**
  [Boot-artwork study](boot-artwork-study.md) captures the
  same 5,576-command / 23,888-word pre-operator stream for fresh, retained-zero
  and retained-three-credit fixtures. Bus barriers and a 117 KB sparse planar
  payload estimate are recorded. Exact bus replay also measures a 377 KB
  planar command-delta alternative, before recipe/state records. Pricing
  variants at coin values 2 and 11 also match the exact first-main prefix.
  Native A1200 copy medians are 63 ms final-image / 263 ms command deltas
  with display DMA. The guarded delta proposal estimates about 450 KB of
  data and a plausible 1–1.8 s startup saving, below a derived 1.93 s ceiling;
  the speedup is not yet implemented or measured. The document specifies
  exact intermediate-state/fallback proof and the release gates.

  Design the proof like card-cache-preparation.md: exact recipe/data proof,
  authoritative VRAM written, and fallback on any mismatch. Stop for a go/no-go
  before implementing.
- [ ] **T13 — fused FIFO-empty interrupt handler (decision D, after T5).** One
  guarded assembly block for `$2E26–$2EBC`, covering entry, status tests, feed
  loop, empty-ring tail and exit. Every original instruction keeps its exact
  effects, order and CCR, and promotion remains possible at each original
  boundary. It uses the unchanged shared device endpoints; RD/read-FIFO
  semantics and the FIFO model question stay unchanged.

  **Scope clarification:** normal IRQ return is `$2E8A`; error TRAP 14 is
  `$2EA8`. `$2EAA–$2EC2` is a separate producer enable routine. Keep its
  existing triplet separate; see the [boundary map](native-video-handler-plan.md#t13-whole-handler-boundary-map-2026-09-30).

  **Opt-in setup experiment (2026-09-30):** a bridge over `$2E36–$2E58`
  passes 13,952 linked CPU cases with and without instruction accounting and
  warm A1200 live24. Its paired synthetic setup sequences regress from
  114/124/113 to 200/165/213 µs (nonempty/empty/wrapped), so it remains disabled.
  Repeated state publication is the next implementation issue to resolve;
  this does not satisfy whole-handler proof or the performance target.
  Register-resident state plus one-time RAM admission now passes 41,856 cases
  in both count modes and lowers fused setup to 138/127/141 µs, still slower
  than its paired ordinary 113/123/113 µs. Both variants remain opt-in; the
  next whole-handler step must amortize setup/publication across endpoints.
  Deferred PC/cycle metadata now passes the same 41,856 cases in both count
  modes and measures 130/124/134 µs against paired ordinary 112/123/112 µs.
  It also remains disabled: setup-only fusion has not demonstrated a win.
  [Experiment and evidence](native-video-handler-plan.md#t13-setup-bridge-experiment-2026-09-30).

  Gates: a Musashi oracle over the whole block (every intermediate boundary,
  interrupt, fault and ring wrap), exact ECS/AGA replay, live24 and a trace
  re-measurement. Target: interrupt overhead from ≈1.26 ms to ≈0.3 ms, a cached
  landing back under ≈20 ms, and a Double-entry busy interval of ≈150 ms or less.
- [x] **T14 — fused `sound_register_write` (decision D, after T1).** One guarded
  block for the `$0D58` routine, holding its six PIA writes in exact order with
  the shared PIA/AY endpoints and every boundary. Same gates as T13, plus AY
  register order/hash against the reference. Target: the AY register write from
  1.53 ms (≈1.0 ms after T1) to ≈0.2 ms, and note application from 21.7 ms to
  ≈3–4 ms.

  **Completed/default 2026-09-30:** bounded six-write fusion preserves all
  original instruction effects and boundaries. Linked CPU matrices (78,336
  cases in both accounting variants), 672 shared-device IRQ/fault cases,
  headless suites, exact ECS/AGA replay, cold/warm live24 on both machines,
  accepted Double, VBI and instruction-trace gates pass. All 630 captured live
  AY writes match original call arguments and order, with identical hashes.
  Paired complete mixer writes improve 840.0 → 668.7 µs (20.4%); the 200 µs
  estimate is **not met**. Accepted Double's AY batch median is 9.3 ms, maximum
  excess batch delay 244.5 ms; this does not close the audio deadline. Normal
  default allocated ELF sections exactly match the validated candidate; sound
  benchmark code is excluded from normal builds. [Evidence](native-sound-fusion.md).

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

**MEASURED normal executable:** compiling out dormant profiler branches lowers
identical isolated whole/split cached-back feeds from 22.64/29.03 to 21.06/27.42 ms.
The candidate's observed complete landing backs are 38.59–39.94 ms (baseline
40.45–48.83 ms); neither is a worst-case bound or a controlled live-hand mean.
The 20 ms target remains unmet. Normal cold/warm live24 and full ECS/AGA state
checks are recorded in [profiling separation](native-profile-build.md).
Neither normal live hand accepts Double, so this does not replace the earlier
Double-write-gap evidence.

**MEASURED follow-up:** separating the general instruction executor reduces
controlled C interrupt/hook dispatch batches by 6.94%/5.96%. Observed landing
backs are 37.86–39.42 ms, still above 20 ms; cold A1200 overall ratio is 0.9531,
deal/draw 0.9091/0.9436. This hand also declines Double. Full validation status
and measurement limits are in [dispatcher separation](native-dispatch-separation.md).

**MEASURED accepted Double (normal code, keyboard-only `DOUBLE_SCENARIO`):**
the two AY write batches across Double entry are 262.9 and 276.4 ms late; each
face-up reveal adds ≈100 ms. The traced entry spends 420 ms at 97% services for
110 board-ms, 40% of it in FIFO-empty interrupt overhead. See
[Double workload](double-transition-performance.md#keyboard-only-double-workload-2026-09-29).

Next: T5, T6 and T8 above, then T4 and T7, then T14 and T13; T11
runs in parallel. Live Paula envelopes now follow PAL VBI time by explicit approval. The measured
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

**MEASURED current A1200 pair:** **22.48 s cold / 9.22 s warm**, including
preparation but excluding executable loading/early CRT. Preparation is 0.80 s
in both; original initialization takes 21.68/8.42 s. Three boundary-only CIA-A
TOD reads avoid recurring observer cost. The warm fixture was saved normally
with zero credits. The previous current pair was 23.02/9.40 s; this is a modest
improvement, not startup parity. See [measurement scope](native-profile-build.md#startup).

The recorded SDL comparison is **2.90 s cold / 0.11 s cached** on this Mac;
SDL's cached launch restores a snapshot, whereas native warm launch still boots
the original CPU program. Startup parity is not demonstrated.

**MEASURED (trace):** warm boot spends 8.1 s on 0.5 board-s: FIFO feed and
drawing take 58% (drawing 2.6 s) and FIFO-handler hooks 31%. The cold refill
takes ≈12 s for 100 coins: delay-loop hook 40%, serial ISR 25%.

Next: T5, T6 and T8c for boot; T3 and T9 for the cold refill; T10; the T12
design study. The 100-coin refill is retained (decision B). Preserve nonzero-credit and interrupted-hand recovery; keep loading/early CRT
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

**MEASURED normal builds:** the cold candidate completes 54.12 board seconds
in 56.807 PAL seconds (ratio 0.9527); warm completes 59.42 in 62.015 (0.9582).
Deal/draw intervals remain around 0.91–0.94, and neither hand accepts Double.
The keyboard-only `DOUBLE_SCENARIO` workload now supplies an accepted Double for
normal-code timing (round 3: 71.78 board / 77.16 PAL seconds, ratio 0.930). Use
it with the 24-input script when closing this gate.
These are scoped observations, not closure of representative workload coverage
or burst deadlines. The read-only probe and analyzer now distinguish an
accepted Double callback from calls to its shared drawing helper.

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

### 4. WHDLoad with default options (W1–W5)

**Open.** Release 0.1 requires the NoVBRMove and NoWriteCache tooltypes. Goal:
run and save correctly under WHDLoad's defaults on every supported system, with
no significant performance cost, and make both tooltypes optional. The plan,
evidence and gates are in [WHDLoad compatibility](whdload-compatibility.md).

**DERIVED:** NoVBRMove is needed because live service entry uses trace
exceptions (`$24`), which a moved VBR never forwards. The interrupt wrappers arm
T on return to the guest, and the dispatcher resumes with T while a tick is
pending. The slave also lacks the Emul flags for the other vectors the runner
installs. **MEASURED:** NoWriteCache avoids an exit-time hang inside WHDLoad
whose cause is unknown.

- [ ] **W1 — exit hang root cause (first).** Reproduce without NoWriteCache
  (cold/warm, PRELOAD on/off) and locate the hung PC read-only. Bisect the save
  pattern, the runner's exit state and a runner-free kickfs program. Fix the
  cause: the port's cleanup, a cache-safe save pattern, or `ws_DontCache` as
  the fallback. Record exit duration; WriteDelay makes physical writes cost
  seconds. **2026-09-30 progress:** PRELOAD-on cached run captured inside
  WHDLoad's inferred cache-node cleanup; matched NoWriteCache cold/warm runs
  pass both saves/backups. Further controls locate the hang after both successful
  saves and DOS close. PRELOAD off passes three normal-game launches but does
  not exercise caching of new files. Moved VBR, NoResInt, WHDLoad 20.0, direct
  callbacks and ws_DontCache patterns do not fix the cold creation case.
  **Pre-existing files pass:** three normal-game launches with all four genuine
  saves/backups already present retain exact backups with PRELOAD/cache enabled.
  The [fresh save-slot proposal](whdload-save-slots-design.md) awaits user approval;
  cold placeholder equivalence, full version matrix and exit-duration gates
  remain open. No release save/option change has been made.
- [x] **W2 — inventory and cost, completed 2026-09-30.** Count trace entries by what
  armed them and audit the short paths that lower IPL or clear IRQs. Measure
  WHDLoad's per-exception forwarding cost (moved VBR against NoVBRMove) with a
  CIA-timed benchmark build. Audit guest opcodes for 68060-unimplemented
  instructions. **Opcode inventory complete for the measured scenarios
  (2026-09-30):** 110,694,439 instruction entries across fast cold/warm boot,
  research play including Double/choice, and service; 17,966 PCs including 107
  in RAM, zero MOVEP and zero non-68000 opcode words. Full observed-play state
  and events match with the observer disabled. This does not prove unseen
  paths or full 68060 compatibility. **Trace inventory/source audit complete:**
  current cold/warm captures have 134/68 traces; the accepted-Double window has
  782 (763 interrupt-wrapper arms, 19 dispatcher resumes), all attributed.
  SR-lowering/RTE paths retain promotion mask 3; device endpoints refresh
  eligible IRQ work. Pending ticks can remain even with IPL already low, so
  W3 must explicitly service that backlog, not wait solely for an SR change.
  **Forwarding benchmark complete:** three fixed/moved-VBR launches each,
  four 128-exception batches/type, exact counts and clean exit. Added median
  cost is 5.8 µs Line-A, 11.8 µs TRAP, 45.3 µs privilege, 33.7/24.0 µs
  level 2/3. These are isolated costs, not a whole-game improvement. Privilege
  forwarding required placing the test handler in reserved BaseMem; W3/W5
  must verify the actual runner's handler locations.
  [Evidence](whdload-compatibility.md#w2-executed-opcode-inventory-2026-09-30).
- [ ] **W3 — trace-free live service entry.** Primitive implementation started:
  524,288 CPU cases pass for the exact assembly redirect/consume code, with
  saved PCs, all SRs, extension bytes and registers checked, plus 452 actual
  nested IRQ injections. Another 1,573,184 cases test the linked wrappers,
  exact-PC lookup, clock ABI clobbers and slot faults. Opt-in
  `SERVICE_REDIRECT=1` now connects interrupt
  wrappers to the exact-PC stub; warm A1200 live24 passes with zero reset/error
  and restored vectors (0.9822 ratio, no accepted Double). Pending-tick resumes
  still use T; pending-work interrupt choice and full runtime gates remain.
  Four-model CPU extension (000/020/030/040) passes 1,048,576 primitive,
  904 nested-IRQ and 3,146,368 linked-entry cases; this does not cover 68060
  or whole-game MMU/cache behavior.
  ECS/AGA exact replay now matches full RAM/VRAM/display/AY state at 7,904,133
  instructions, 64,000,000 cycles and 8,685 IRQs on both chipsets. No live
  scheduling default change. Diagnostic moved-VBR refusal is now implemented
  and tested; a discovered startup-wrapper bug is also fixed so main's error
  code survives destructors. Negative refusal and normal WHDLoad save/exit
  tests pass, as do 28 four-CPU linked startup cases.
  The interrupt wrappers redirect
  the frame PC to a Line-A stub, whose exact-PC short-path descriptor restores
  the PC and enters like today's trace. Pending-tick resume uses the existing
  privileged-instruction traps, or an immediate CIA-A expiry if W2 finds a
  remaining case. Diagnostic stepping keeps trace and refuses a moved VBR. Add
  the slave's missing Emul flags. Gates: redirect CPU tests, exact replay,
  live24, `trace.sh`, the Double scenario and the W5 matrix.
- [ ] **W4 — QuitKey (decision).** **Measured:** WHDLoad 19.2 resolves the
  current zero slave key to F10 (`$59`); explicit F10 and a QuitKey override
  behave as specified. Three diagnostic probes exit cleanly. This does not
  test actual keypress/persistence behavior. With a moved VBR, QuitKey exits
  without the game's save. Set `slv_keyexit` explicitly (F10 recommended) and
  choose: document "quit with Esc", add checkpoint saves (separate decision), or
  keep NoVBRMove optional.
- [ ] **W5 — package and matrix.** Remove both tooltypes from the icon,
  installer, ReadMe, slave info and test defaults, but keep testing them as
  user options. FS-UAE matrix: 68020/030+MMU/040/060, with defaults and each
  option, PRELOAD on/off, cold/warm saves. Proposed gate: warm/cold Ready
  within 2% of NoVBRMove, and no worse Double AY lateness. **INFERRED:**
  forwarding costs ≈0.3–1% at the measured ≈3,300 gameplay entries/s.

## Completed implementation — not remaining tasks

- The general instruction executor is separated from the common native
  scheduler. Controlled dispatch cost improves with unchanged instruction
  effects and interrupt boundaries; model/CPU tests, exact ECS/AGA replay,
  cold live24 on both machines and scoped VBI checks pass. A broader compiler
  setting was measured and rejected for mixed gameplay benefit. See
  [dispatcher separation](native-dispatch-separation.md).

- Normal service code no longer tests dormant profiling state throughout hot
  paths. Profiling remains an explicit build, automatically selected by the
  counter/ledger variants. The local parameter-word inlining experiment was
  rejected for negligible/no split-feed benefit. See [measurements and limits](native-profile-build.md).

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
- **Broader game-scope changes:** initial 0.1 packaging/WHDLoad is implemented,
  and removing its tooltype requirements is item 4 above. Current coin, credit
  and operator behavior is retained. Any redesign of those
  features remains a separate decision. See [release checks](release.md).

There is no outstanding approval request for the completed handler experiment.
This list does not reopen measured/rejected experiments or authorize new timing
or hardware-model changes.
