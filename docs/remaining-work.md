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
- [ ] **T4 — input apply (items 1, 3).** `amigaInputApply` copies and clears three
  128-byte volatile arrays under Disable() at 50 Hz, ≈380 µs per call. Latch/clear
  only the 17 keys it reads, with unchanged read-acknowledged transitions. Gate:
  the input-response short/repeat/overlap tests. Estimate −1.5% gameplay CPU.
- [ ] **T5 — FIFO-empty interrupt delivery (items 1, 2).** Each interrupt costs
  ≈1.26 ms outside word feeding. The fused `$2EB2` re-arm and `$2E82` promote to
  the full dispatcher (≈380 µs) to deliver the next interrupt. Complete admitted
  promotions in assembly: clock pause, IRQ query, frame push, virtual SR/stack;
  keep the dispatcher for any other state. The opt-in VIDEO_IRQ_FRAME_ASM work
  replaced only frame creation (3%). Then trim `$2E30`/`$2E70`/`$2EBC`. Estimate
  −0.25 ms then −0.2 ms per interrupt: −0.5 s boot, −35 ms Double entry, −7 ms per
  landing back.
- [ ] **T6 — full-dispatch fixed cost (items 1–3).** 250–450 µs per full dispatch:
  `nativeDispatch` self ≈100 µs, `nativeClockPause` 30–75 µs with two to three
  inlined `LiveClock::grant` calls, and two to three `Board::irq` scans. Use one
  grant per pause, a cached board IRQ level invalidated by device writes and
  ticks, and one pending-work word gating `shuffleService`, the compose predicate,
  `presentReady` and the second `statusNow`. Target −30%: −5% gameplay CPU, −0.6 s
  boot, −1.5 s refill.
- [ ] **T7 — tick path (items 1, 3).** Tick-handler RTE `$0C3E` costs 731 µs when
  full; `Board::tick` costs 150–190 µs with 64-bit phase arithmetic and model ticks;
  trace-exception tick delivery in the hook-free delay loop costs 565 µs each
  (4.2% of gameplay). Target −0.3 ms per tick, −3% steady CPU.
- [ ] **T8 — drawing hot spots (items 1, 2).**
  - (a) Two card backs per deal are refused by the cache guards and render
    procedurally, ≈110 ms against ≈40 ms cached. Identify their backgrounds and
    prove an admission case, like the white-border case.
  - (b) `copy180` reverses words on the CPU, 3.6–4.1 ms per copy, 13–19 per deal or
    draw. Use table-driven reversal, or a reversed resident copy under the same
    invalidation rules.
  - (c) Boot primitives: 4.9 ms per curve outline, PAINT 1.08 s and pattern-tile
    expansion 0.62 s of warm boot.

  Estimate −130–160 ms of stalls per deal, −40–60 ms per reveal set, −0.8 s boot.
- [ ] **T9 — startup quanta (item 2).** The `$2442` startup delay hook has 14,853
  entries, mostly full, at 1 ms quanta: 40% of the cold refill. Add an assembly
  batch path when no quantum, IRQ or frame is due. Advance directly to the next
  due edge while the serial peer is idle, including watchdog-age deadlines (the V3
  constraint); V3's null result predates dispatch dominating. Estimate −2.5 to
  −4 s cold.
- [ ] **T10 — preparation (item 2).** The 0.8 s before the first original
  instruction is not attributed. Add a trace mode from `nativePrepareInner`
  before targeting it.

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

- [ ] **T11 — ACRTC timing research (decision A).** A bounded study, with no
  default change:
  - Collect the HD63484 datasheet's drawing and FIFO timing, tagged
    DERIVED/INFERRED.
  - Compare with the Finnish reference footage (`2BI-eUaPCOc`, see
    visual-reference.md) for the deal, reveal and Double sequences.
  - Prototype on the host harness only: measure FIFO-empty interrupt counts,
    words per interrupt, board-time progress and AY-write timing under an
    optional command-duration model.

  Deliverable: a concrete proposal with its evidence tags and the
  replay/reference regeneration it would require. Any adoption needs a new
  decision.
- [ ] **T12 — boot artwork design study (decision C).** Using the host harness,
  measure:
  - the complete boot command stream up to door open;
  - which words vary with settings, retained accounting and credits;
  - where it interleaves with other drawing;
  - the prepared data size and the realistic saving.

  Design the proof like card-cache-preparation.md: exact recipe/data proof,
  authoritative VRAM written, and fallback on any mismatch. Stop for a go/no-go
  before implementing.
- [ ] **T13 — fused FIFO-empty interrupt handler (decision D, after T5).** One
  guarded assembly block for `$2E26–$2EBC`, covering entry, status tests, feed
  loop, empty-ring tail and exit. Every original instruction keeps its exact
  effects, order and CCR, and promotion remains possible at each original
  boundary. It uses the unchanged shared device endpoints; RD/read-FIFO
  semantics and the FIFO model question stay unchanged.

  Gates: a Musashi oracle over the whole block (every intermediate boundary,
  interrupt, fault and ring wrap), exact ECS/AGA replay, live24 and a trace
  re-measurement. Target: interrupt overhead from ≈1.26 ms to ≈0.3 ms, a cached
  landing back under ≈20 ms, and a Double-entry busy interval of ≈150 ms or less.
- [ ] **T14 — fused `sound_register_write` (decision D, after T1).** One guarded
  block for the `$0D58` routine, holding its six PIA writes in exact order with
  the shared PIA/AY endpoints and every boundary. Same gates as T13, plus AY
  register order/hash against the reference. Target: the AY register write from
  1.53 ms (≈1.0 ms after T1) to ≈0.2 ms, and note application from 21.7 ms to
  ≈3–4 ms.

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
  seconds.
- [ ] **W2 — inventory and cost (before code).** Count trace entries by what
  armed them and audit the short paths that lower IPL or clear IRQs. Measure
  WHDLoad's per-exception forwarding cost (moved VBR against NoVBRMove) with a
  CIA-timed benchmark build. Audit guest opcodes for 68060-unimplemented
  instructions.
- [ ] **W3 — trace-free live service entry.** The interrupt wrappers redirect
  the frame PC to a Line-A stub, whose exact-PC short-path descriptor restores
  the PC and enters like today's trace. Pending-tick resume uses the existing
  privileged-instruction traps, or an immediate CIA-A expiry if W2 finds a
  remaining case. Diagnostic stepping keeps trace and refuses a moved VBR. Add
  the slave's missing Emul flags. Gates: redirect CPU tests, exact replay,
  live24, `trace.sh`, the Double scenario and the W5 matrix.
- [ ] **W4 — QuitKey (decision).** With a moved VBR, WHDLoad's QuitKey exits
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
