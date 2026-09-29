# Native performance recovery plan

**Current work (2026-09-29):** see [remaining-work.md](remaining-work.md).
This document retains implementation detail and dated measurements. Older
“next”, “pending” and performance totals describe their recorded stage, not
additional current tasks or a current-release baseline.

Proposed 2026-09-25, following the startup profile of `167f486`; revised the
same day after review against the code, the existing logs and new host-reference
measurements (below). This is the original Phase 5 recovery plan. Its constraints
and acceptance gates still apply; current tasks are in remaining-work.md. Changes are to be
measured separately and committed as cohesive steps on main. Phase 6 remains
out of scope.

**Historical gameplay reordering (2026-09-27):** a wall-time ledger of the card
deal shows quiet phases already near real time, and every landing card
stalling board time for 0.2–0.4 s while ~79 procedurally drawn commands
execute. Per-pixel drawing (PAINT, RPLL, flipped AGCPY, curves) dominates those
stalls, ahead of the per-word hook cost. That work moved to
[native-burst-plan.md](native-burst-plan.md), whose experiments are now completed
and classified. Their historical costs are not the current card-cache baseline.

**Startup decision superseding the old diagnostic gate:** the user now requests
normal-game boot to skip/pass the ROM's coin-op hardware tests. Preserve required
RAM clearing and game/device initialization; keep the prior test path explicitly
available for research. The checksum/drain startup workload will no longer be a
normal-game optimization target. Implement and verify that policy first, then
use gameplay call counts to prioritize native handlers. This does not change
FIFO model semantics or authorize replacing game/accounting logic.

**Timing decision approved:** the user approved option C on 2026-09-25,
including the E-clock units correction, conservative calibration across workloads,
a bounded credit window, and retention of the old clock for comparison.
Diagnostic replay keeps its recorded schedule. Approval is not validation of K.

**Execution update (2026-09-26):** steps 1–6 have implemented pieces, but the
plan is **not complete**. Prepared operands, shifted blits, exact curve products,
word-span panel drawing, normal hardware-test bypass and acknowledgement-driven
setup are implemented. Reduced-save assembly now handles status, bounded memory
CMP/TST, common virtual SR/RTE operations, frequent PIA/ACIA moves and original
TRAP frames. Shared hardware models and checked fallbacks remain authoritative.
ECS replay matches full RAM, VRAM, displayed pixels and AY writes with these
handlers. See [native-clock.md](native-clock.md) for the successive measurements.

At that stage, K=4 post-ready intervals took **79.18–89.68 sampled PAL seconds for 60
board-seconds**, versus 98.60 with the previous K=1.5 gameplay cap and
blanket display invalidation. The corrected-counter shared-PIA/whole-word-blit repeat is 79.18;
live hands differ, so this is not a controlled per-change speedup. Boot retains K=1.5. The higher gameplay request
is below the conservatively measured workload minimum with 12.5% headroom;
CPU probes can lower it. The corrected workload counter excludes pre-instruction
traces and gives a minimum 5.207 (4.556 after margin) on the measured scenario.
See the post-ready calibration and its coverage limits in native-clock.md.
The isolated clocked status hook still costs 48.44 us, above the 25 us gate.
This is progress, not real-time or 50 FPS acceptance.

Cabinet messages now wait for transport idle and the ROM's completed door
transition. The full 85-board-second play/service scenario completes with
empty request queues, no resets/errors and the normal poker screen. Gameplay
calibration now has phase-specific lower bounds; steady-state release gates
remain open.
The diagnostic boot checksum/drain stays bypassed only in normal gameplay;
its uncertain FIFO semantics have not been changed. The user approved the faster faithful result at the FIFO gate. Command writes
may use reduced-save assembly with unchanged shared-model semantics; RD/read-FIFO
behavior stays untouched because evidence does not justify changing it.

The P2 87 service-door failure is now reproduced on the host and corrected by
waiting for the ROM’s completed door transition before returning status; see
[startup-policy.md](startup-policy.md). Native play/service completes through
85 board seconds and returns to the normal poker screen.

Remaining gates: short-hook latency, sustained real-time timing, input
correctness across schedules, animation deadlines, normal non-warp audio/launch checks and physical
calibration. Full replay equality remains mandatory when affected.

**Latest continuation:** the approved combined command-feed hook is now the
default (`native-no-feed-fusion` disables it). Shared endpoint inlining reduces
the controlled combined pair from 204.37 to 179.61 us. Reused pixel addresses
and queued solid PAINT spans reduce synthetic DOT/circle work by about 6% and
PAINT by 50%, with strict replay equality. Latest live timing is 78.04 PAL
seconds for 60 game-seconds; a heavier winning/doubling run is 88.68 seconds.
The latter gives a conservative throughput floor of 4.346 after margin, so
K=4 remains unchanged. Steps 3/7 still fail their latency/real-time gates;
50 FPS is not established. The subsequent bounded exact curve cache reduces
controlled repeated-circle work by 66.29%, with 146/148 post-ready curve hits.
The cache-only live sample takes 75.96 PAL seconds for 60 game-seconds; K=4 remains
unchanged and the real-time gate stays open. Exact one-point pattern selection
then saves another 23.94% in the circle batch and 10.13% for DOT; a heavier
winning/doubling live run takes 83.54 seconds, so it is not a whole-game
comparison with the quiet hand. Short-hook accounting and full scheduling
remain major sampled service costs. Full evidence is in native-clock.md.


**Card animation pass (2026-09-27):** per-buffer background repair reduces the
controlled moving-window composition batch by 79.90% on A1200 and 74.75% on
ECS, with hires DMA active. Prepared repeating-pattern selection and a direct
planar point path reduce controlled DOT/circle batches by 11.38% / 21.37%.
The early live deal sample falls from 8.12 to 6.96 PAL seconds; differing hands
prevent an exact whole-game attribution. All 96 synthetic composition cases
on each machine, twelve live frames and strict A1200 replay RAM/VRAM/frame/AY
comparisons pass. A synthetic patterned-fill optimization was discarded because
it was unused in live play. The clock is unchanged; dispatch/clock accounting,
PAINT boundary search and remaining point drawing still need work. Steps 3/7
and the 50 FPS gate remain open. Details and reproducible evidence are in
[native-clock.md](native-clock.md#card-animation-graphics-pass-2026-09-27).

**Burst follow-up (2026-09-27):** the current execution order is in
[native-burst-plan.md](native-burst-plan.md). A1 word-parallel PAINT lowers the
live deal mean from 8.026 to 3.414 ms; A3 batched planar lines lower the 13-point
outline from 42.402 to 11.677 ms. Strict ECS/AGA replay and full live scenarios
pass. A4 then lowers matched 17×17 flipped copies from 53.202 to 2.986 ms.
The drawing estimates and real-time gate remain unmet; curve stamps are next. Clock and pending hook/scheduling decisions are unchanged.

## Objective

Make `AMIGA_MODEL=A1200 ./run.sh` practically playable, with normal audio and
correct game timing, while preserving a 68000-compatible OCS/ECS path. The
original program still executes natively. Musashi remains host-only.

## Evidence

### Startup profile

**MEASURED** (`amiga/.run/current-cost/gdb-out.log`): 2.62 board-seconds
(20,960,000 cycles) take 16,277 profiled VBIs (≈325.5 PAL seconds) and 592,990
native dispatches. `NativeTiming::Service` samples one dispatch in 64: 9,266
samples, mean 347.9 E-ticks (≈490 µs). Extrapolated, the C dispatch interior
(including Amiga interrupts taken during services) accounts for ≈291 s, about
89% of the sample. Video-bus scopes time every access: 398,498 accesses, mean
134.7 ticks. Board tick, presentation, guard, Paula and blit waits total 5.7 s.

**DERIVED (observer inflation):** every timed scope calls timer.device
`ReadEClock` twice, and command logging adds one call per command: 982,360 calls
in this run. The `AyTick` scope brackets almost no work yet averages 44 ticks,
which bounds one call at ≲44 ticks on this A1200 (the earlier A500+ probe
measured 91). The observer therefore costs roughly 60 of the 325.5 seconds and
about a third of every timed video access. The published 75.7 s video-bus and
17.4 s drawing totals are observer-inflated, not production costs.

**DERIVED (attribution):** with nested observer calls removed, a dispatch costs
≈230 ticks (≈330 µs, ≈4,600 cycles at 14 MHz). Timed device accesses account for
≈60 ticks of that. The original program ran for only ≈3 s of the sample (2.62
board-s ÷ 0.887; see the clock scale below). **INFERRED:** exception entry/exit,
two MOVEMs and five CIA accesses are ≈10 µs per dispatch, about 3% of the
current cost. The first target is the C++ dispatch interior (step 2), not the
exception or timer mechanism.

**MEASURED (hot loops, host reference):** the checksum at `$10FA0` executes
76,152 iterations of RD `$10FC0`, RFR poll `$10FC6` and one FIFO byte read at
`$10FCC`. Only one byte is read per RD word, so at the checksum exit (`$10FD8`)
the modelled read FIFO holds 38,076 words (host snapshot). `$11030` then drains
it with 76,152 iterations of `TST.B` data `$1103C` and `BTST` RFR `$11040`.
Checksum plus drain are ≈380,760 dispatches, 64% of the startup sample; the
drain alone is 26%.

### Hook density by phase

**MEASURED** from a cycle-stamped replay of `relocation-play.inputs` (69.5 s;
local `tmp/review-play.replay`, bucketed by `tmp/review_rates.py`). Counts are
hooked bus instructions per reference board-second. CPU-control hooks (at least
one RTE per IRQ) and TRAPs add dispatches that this stream does not record.

| Phase (reference seconds) | Bus hooks/s | Virtual IRQs/s | Dominant sites |
|---|---:|---:|---|
| RAM and watchdog tests (2–5) | 36k–118k | 0–3.7k | `$20BE` poll, `$13E2/$13E6` PIA `$FB01E` |
| Graphics checksum and drain (6–7) | 106k–283k | 100–470 | `$10FC0/$10FC6/$10FCC`, `$1103C/$11040` |
| Ready, idle (8–17) | ≈480 | 100 | `$0C40/$0C48` tick acknowledge |
| Coin refill during setup (20–24) | ≈28k | ≈1.9k | `$2E58/$2E5E` FIFO feed |
| Deal, hold, draw, double (25–68) | 0.4k–8.4k | 100–520 | `$2E58/$2E5E`, `$2E30/$2E36` |

Gameplay is 30–500× less hook-dense than startup. The ACRTC FIFO interrupt at
`$2E26` spends two dispatches per command word (BTST WFR, then MOVE.W), and the
model always reports WFR. Most hot sites come in pairs with identical counts.

**MEASURED (native full hand, unprofiled,** `amiga/.run/cache-live/gdb-out.log`**):**
1,170,859 dispatches over 76.5 board-seconds in 41,787 VBIs (≈836 PAL s). About
578k dispatches (≈7.8k per board-second) follow the startup sample. **DERIVED:**
those ≈74 board-seconds still take roughly 570 wall seconds, about 1 ms per
dispatch. Gameplay is therefore dominated by device work (drawing and
composition), not dispatch overhead alone. This matches the earlier play profile
(58% of PAL time in video services; single commands up to 9.1 s). Gameplay has
not been profiled on the current build.

### Clock observations

**DERIVED (scale defect):** `nativeClockEnter` multiplies CIA E-ticks by 10. The
E-clock is the Amiga CPU clock ÷ 10 (709,379 Hz), so ×10 gives 7.09 MHz Amiga
cycles. `accountGuestCycles` and `Board` count 8 MHz board cycles (80,000 per
10 ms tick), and the hook metadata adds Musashi 8 MHz cycles. Board time
therefore runs at 88.7% of measured guest time before overhead subtraction. The
exact factor is 11.2776; for example `mulu.w #361` then `lsr.l #5` gives
11.281 (+0.03%) without 32-bit software arithmetic.

**MEASURED/DERIVED (compression):** the host reaches the checksum exit at
55,172,422 cycles (6.90 board-s) after 78,204 RDs. The native profile had
completed the same 78,204 RDs by 2.62 board-s. Under the service-excluded clock
this A1200 therefore gives the guest at least 2.6× the reference instruction
budget per board-second. Consequences: host-derived times (the 40.5 s setup
script) do not match the same ROM progress natively. Polling loops also issue
proportionally more dispatches per board-second than the reference.

**DERIVED (overhead subtraction):** each interval subtracts the calibration
maximum (`nativeClockOverhead` = 36 in the full-hand run). The resulting
undercount is small at gameplay rates but reaches tens of percent at checksum
rates. The step-1 clock ledger must quantify it.

### Budget model

The original program never idles. No STOP is covered, and the reference
executes about 1.1M instructions per board-second even when idle. Under the
service-excluded clock, board/wall ≈ 0.887·g/(g+s), where g is guest time and
s is service time. Real time requires s≈0. A 5% target leaves ≈50 ms per second
for all services, or ≈6 µs per dispatch at the current native gameplay rate,
drawing included. No hook optimization reaches that.

## Timing contract (decide after step 1, before setting step 2–5 budgets)

Present these options with the step-1 measurements. Any change needs explicit
user approval. Diagnostic replay keeps its recorded schedule in every option.

- **A. Keep the service-excluded clock.** It is safe for the watchdog, but play
  runs slow by the whole service fraction. Step 7 would have to be restated as
  a service-fraction target.
- **B. Correct the ×10 scale.** A units fix that is needed under every option.
  It changes live timing by 12.8%, so it is listed here, not applied silently.
- **C. Throughput-floored real time** (recommended for evaluation). Board time
  follows PAL wall time, but never advances faster than K′·g. K′ is a
  conservative calibrated ratio of native to reference speed, no higher than the
  measured ≥2.6. Services no longer stall board time while the guest still has
  CPU to spare, yet the guest always receives at least the reference
  instruction budget per board-second. With K′≈2.3, services may use ≈55% of
  wall time before board time slows. That is ≈70 µs per dispatch, drawing
  included, at 7.8k/s, instead of ≈6 µs under A. Requirements:
  - a bounded credit window, so banked guest time cannot later starve the main
    loop;
  - exact reference accounting for ROM loops that count iterations against board
    events (the three audited boot polls today; audit for others);
  - K′ calibrated from at least three paired host/native milestones, including
    the checksum exit.
- **D. Plain wall time.** This previously starved the main loop (watchdog
  expiry). Not recommended.

## Constraints

- Preserve original instruction results, CCR, registers, access order and device
  side effects. Do not batch away game instructions or manufacture FIFO/status
  results. Retain byte guards on every patched instruction and baked operand.
- Retain physical user mode and virtual supervisor state. Exception frames use
  the private service stack. No helper calls that quietly write return addresses
  into game RAM; no original game handlers invoked inside Amiga interrupts.
- Keep shared Board/device semantics and a checked fallback. An optimization
  may decline a supported access; an unknown access must still stop loudly.
- Live and diagnostic modes remain separate. No replay loading, instruction
  tracing, frame/audio capture, SHA calculation or full guard sweep in normal
  startup. Diagnostic replay retains complete RAM equality, not masked fields.
- Keep Paula hardware loops. Keep AGA fetches conditional on actual chipset
  detection. Retain the 68000 arithmetic audit and native ECS regression.
- Debug output is silent; normal audio stays enabled. ROM-derived tables,
  captures and retained state remain ignored. Check `git status --ignored`
  before staging new files. The explicit normal-game diagnostic bypass decision
  above supersedes the former no-bypass requirement.
- No `ReadEClock` or other OS calls in per-access or per-command paths, even
  in profiling builds. One call costs more than the dispatch budget.

## 1. Establish an attributable, observer-free baseline

Use three instruments in place of per-scope `ReadEClock` timing:

- **PC sampling.** Record the stacked PC into a Fast RAM histogram from the
  existing level-3 wrapper (50 Hz). Optionally add a second CIA timer, allocated
  through the resource like the guest clock, at about 1 kHz. gdb reads the
  histogram at exit. Attribute samples to guest ROM/RAM, the assembly
  entry/exit, each C function (ELF symbols) and OS interrupt code. This yields
  the flat profile without touching the hot path.
- **Counters only** in hot paths: dispatches by Line-A index and kind,
  CPU-control and TRAP dispatches, traces, IRQ deliveries, commands by opcode,
  copy fallback reasons (alignment, overlap, direction, bounds), queue stalls.
- **Ablation microbenchmarks.** Extend the NOP/Line-A calibration code to a
  synthetic hook that runs the full path. Then remove one layer at a time:
  dispatch bookkeeping, `executeHook`, `Bus::access`, Board decode, device.

Milestones: entry to preparation, ROM loaded/patched, takeover, first guest
instruction, first visible game frame, end of checksum and drain, operator setup
complete, Deal, cards settled, Hold, Draw and exit. Distinguish a Copper list
being allocated, DMA being enabled and an actually presented nonblank frame. Do
not call a counter increment a visual verification.

Report host wall time, PAL VBI/E-clock elapsed time and board cycles
separately. Warp-mode host time is not an Amiga performance metric; also run the
ordinary non-warp launcher. Rename the live `nativeInstructions` counter to a
dispatch count; outside replay it counts dispatches.

Add a clock ledger: original intervals, excluded work and measured transitions
should explain total time within observer uncertainty. It also quantifies the
scale and overhead-subtraction errors above. Compare profiling off and on;
never infer the residual entirely as hook cost by subtraction.

Host-side additions (Musashi only, no native change):

- Keep the phase-rate table above current and add CPU-control/TRAP counts.
- Add a per-phase PC histogram to measure idle headroom, i.e. the share of
  reference instructions spent in delay and poll loops. It bounds how far the
  guest may be slowed without changing behaviour. It also decides OCS/ECS
  feasibility: an A500 runs at roughly 0.9× the reference's Musashi 68000 timing.
- Derive K′ milestones for the contract decision.

Workloads: checksum plus drain is the per-dispatch microbenchmark. The
acceptance workload is a gameplay window (coin refill, deal/hold/draw, double).
Profile both, separately; they have different bottlenecks.

**Deliverable:** a reproducible baseline table for preparation, checksum and
drain, remaining cold boot, idle, and one full hand. Add the contract-decision
inputs (service fraction per phase, K′, idle headroom). Then stop for the timing
contract decision. A full slow hand need not be rerun after every later edit.

## 2. Remove repeated decoding and lookup from hot accesses

Extend `tools/native_tables.py` and loader preparation to produce validated,
site-specific descriptors. Decode instruction shape and immutable operands
once. Resolve relocated absolute addresses after placement, not repeatedly
through virtual bus calls. Include expected device/range, width, direction,
original instruction length/cycle metadata, register effects and CCR behavior.
Guard all source extension bytes now baked into descriptors, not just opcodes.
Generated ROM-derived values stay in `amiga/generated/`.

Dispatch directly from the Line-A index. Specialize the frequent MOVE and BTST
forms first, beginning with the checksum, drain and FIFO-feed sites. Dynamic
address registers still get exact bounds/access checks; a site's prior
observation does not make all future addresses safe. Preserve pre/postincrement
ordering, A7 byte increments and the original CCR rules. Keep the generic
handler for complex or uncommon supported shapes, and keep loud failures for
unknown ones.

Call the relevant shared device endpoint directly after validation. Avoid the
second generic Board address decoder and virtual HookBus extension reads on
known sites. Do not create a separate approximate device model. Keep required
surface synchronization for RD, and preserve FIFO high/low-byte state.

Per-dispatch items visible in the current code, all removable without
semantic change:

- the duplicate CIA stop (the `stopclock` macro, then again in `nativeClockEnter`);
- the diagnostic `$20BE` poll-sample code in `nativeDispatch`;
- up to three `board->irq()` evaluations per dispatch;
- virtual `HookBus` reads for extension words;
- per-byte `Board::read8/write8` decoding of word accesses;
- the linear access-table scan;
- repeated `canonical()` calls.

Initially keep the existing full save and timing boundary. This isolates the
benefit of removing software layers from any clock or assembly change.

**Gate:** generated descriptors cover all admitted forms; differential hook
tests against Musashi include every register and CCR bit, address boundaries,
aliasing and fault cases. Native replay reaches the same instruction boundary
with all RAM equal. Re-measure the checksum/drain workload and its hot sites.
Engineering target: the dispatch interior falls from ≈4,600 to ≤500 cycles for
the specialized forms.

## 3. Add a minimal fast exception path

If descriptor dispatch still misses the budget of the approved contract, use a
small set of handwritten 68000 assembly handlers selected by validated
descriptors. Keep the two-byte Line-A patch mechanism first; the patch remains
one original access. For hot status/FIFO/byte operations, save only registers
the handler actually clobbers. Construct the correct returned CCR in the
physical exception frame and keep virtual SR consistent with the established
lazy synchronization rules. A C call must obey its entire clobber ABI, not just
the apparent callee source.

Ordinary short accesses return directly when no scheduler work is due. Command
completion, reset, faults, relevant IRQ changes, due virtual deadlines or a
complex form promote to the full saved-context path. Promotion must capture the
original unmodified context and cannot repeat a device read or increment.
Interruptible service work remains on the private stack; Amiga IRQ wrappers must
not mistake a partially saved context for resumable guest state.

Exercise both 68000 six-byte and 68020 format-0/format-2 exception handling,
TRAP/RTE, virtual stack changes, traces at interrupt boundaries, and exit.
Keep a diagnostic switch that forces the generic path so the same replay can
compare generic versus fast behavior. Replay must also exercise the optimized
handler's semantics, not merely bypass it and test the old implementation.

**Gate:** unchanged CPU/device/access results and full-RAM replay equality.
Absolute A1200 targets replace the earlier 10× relative target: at most ≈25 µs
per short hook including its device model, so checksum and drain complete in
≲10 s of wall time. Under contract C, the mean gameplay dispatch stays within
the budget derived from the step-1 service fraction. Stop at this gate rather
than rewriting wider game instruction sequences if it fails.

## 4. Separate fast access completion from scheduling

Keep the current clock for the first fast-handler comparison, so hook gains are
measured under a fixed contract. Then remove unnecessary scheduler work using
explicit due flags/deadlines:

- VBI requests presentation/input/guard work; it does not force that work after
  every bus byte. Apply work at safe guest boundaries and coalesce presentation.
- Deliver a newly asserted eligible device interrupt at the next required safe
  boundary. Preserve virtual IPL, source priority, acknowledgement and the
  rule that a handler returns before a new tick can overwrite a pending source.
- Run guard slices, status output and input scanning at their established
  cadence. Do not make queue progress depend on the original program polling.
- Trace only when needed to reach a safe point from an otherwise hook-free
  loop or for explicit replay. Count and test tracing to detect accidental
  continuous tracing in normal operation.

Then implement the approved timing contract, including the scale correction
(B). Keep exact reference cycle accounting for ROM loops that calibrate against
board events, whatever the contract. Timer I/O needs its own measured choice.
First keep short assembly CIA boundaries and remove duplicate stops and
bookkeeping. If CIA I/O then dominates, prototype a continuously running
elapsed-time measurement with excluded service intervals subtracted, with proper
nesting and overflow accounting. An earlier PAL-beam probe was abandoned for
variable access latency; revisit it only with the clock ledger. Never guess a
constant duration for arbitrary handlers. A calibrated constant is admissible
only for a bounded, proven path with a measured error bound, and never for DMA
waits or command execution.

Use separate startup/steady-state tests of clock drift, short polling loops,
watchdog kick/reset timing, long interrupt handlers, delayed/multiple source
edges and prolonged graphics commands. The original watchdog startup test is now research-only; normal fast boot
expects zero watchdog resets. Do not lengthen watchdog periods to hide
slowdown. The nominal original-board oscillator remains an estimate.

## 5. Complete useful blitter coverage

Rank remaining device work from the gameplay profile. If PC sampling shows
drawing and composition dominate gameplay, as the evidence suggests, run this
step in parallel with step 3 rather than after it. Include display composition
in the ranking: moving windows currently use CPU word shifts after draining
preceding blits.

Implement shifted source-to-destination planar copies using blitter shifts,
masks and modulos. Choose ascending/descending traversal only when it preserves
ACRTC sequential semantics. ACRTC overlapping-copy behavior is not automatically
memmove behavior. Keep a correct fallback for cases without a proof, and
measure its use.

Test all 16x16 source/destination alignments, one-word and boundary widths,
first/last masks, all logical operations, screen pitches, VRAM edges, positive
and negative scan directions, overlap and queued dependencies. Prevent shifted
prefetch from reading outside valid plane storage; use a bounded safe edge path
if extra words are required. Test on the real ECS blitter in FS-UAE, then AGA.

Use the same verified shifted-copy machinery for display-window composition
where appropriate. Keep interrupt-driven queue draining and synchronize only
at genuine read/write dependencies or resource reuse. Track queue stalls rather
than removing correctness barriers speculatively.

Only optimize curves, PAINT and cache policy further if they rank meaningfully
in the new profile. Candidate work is eliminating unnecessary wide products,
per-point allocation/sorting and repeated address calculation while preserving
pixel order and command semantics. Do not approximate original drawing output.

**Gate:** independent packed-pixel oracle, queued native tests and matched
VRAM/frame captures pass; the real game's frequent copy shapes reach the blitter.
Report residual fallbacks and their total time, not just a blit submission count.

## 6. Correct startup policy independently of speed

First measure genuine reset/graphics initialization separately from our external
operator setup. The 40.5-second setup script currently runs on every direct boot.
Its times come from the host reference; natively the ROM reaches the same point
at least 2.6× sooner in board time, then idles until the scripted actions.
Replace unconditional scheduling with observed cold/ready states and necessary
external actions. Retain required minimum protocol timing; do not merely shorten
constants until the game happens to accept them. Controls should become enabled
when the ROM is actually ready, not because a fixed timer expired.

There is an important limit: existing retention experiments show accounting and
settings in main RAM, while the separate $D0000 NVRAM is untouched. Therefore
`nvram.bin` existence or validity alone cannot select warm startup. Reproduce
reset with valid retained RAM, invalid/partial state and no state in the host,
then natively. Record which ROM tests determine initialized state and whether
that removes operator setup, graphics reconstruction, or both.

For the first performance milestone, retain genuine cold boot and make it fast
enough; do not import CPU registers or a prepared framebuffer. If persistent
warm startup is wanted, present a separate decision about emulating the evidenced
retained RAM versus a full ready-state snapshot. Retaining all work RAM has an
unconfirmed hardware scope and relocation issues; a host snapshot is not a
portable native image. Neither is silently authorized by this plan.

Any approved persistence design must specify relocation/version validation,
atomic saves, corrupt-file recovery, no ROM bytes, cold-reset override and the
player-credit policy. Do not fabricate zero credits by overwriting accounting
RAM or refill the reserve on every launch. A snapshot must not conceal failure
to execute cold boot correctly. The graphics-checksum bypass is now explicitly approved for normal startup.

## 7. Performance and release gates for Phase 5

Use fresh-state and repeat-start runs, normal audio, the exact normal launcher,
and both profiled and unprofiled binaries/settings. Capture the following table
after each meaningful improvement: preparation time; first visible frame;
checksum/drain completion; ready-for-input; hook distribution/latency; command
totals; board-time versus PAL-time ratio; service fraction; input latency;
missed animation presentations; watchdog behavior; and retained-state result.

Targets for A1200 under contract C (to be verified, not claimed in advance). If
the user keeps contract A, board-time targets become service-fraction targets:

- Sustained board-time advancement within 5% of PAL elapsed time during a
  representative 60-second play interval, without accumulating timing debt.
- Correct 50 Hz display cadence, with updates ready for their intended VBI in
  normal animation. Repeatedly scanning an unchanged buffer at 50 Hz is not
  proof that the game itself is running at 50 FPS. Report missed updates.
- Normal input delivered by the next applicable 50 Hz scan; no lost short
  presses, and visible response comparable to the paired host scenario.
- Cold startup adds bounded service overhead instead of stretching original
  time by orders of magnitude. Report first-frame and ready times separately;
  an authentic timed initialization is not promised to finish in two seconds.
- Coin/deal/hold/draw/collect/double, service controls, restart, normal exit and
  error exit work without unexpected watchdog resets or guard corruption.

The actual machine remains the final performance/display/audio authority;
FS-UAE results are reported with CPU/chipset/memory settings and do not become
physical-board clock calibration. OCS/ECS retains functional coverage and builds
without 020/AGA-only instructions; its real-time target follows A1200 bring-up
and depends on the idle-headroom measurement from step 1.

Correctness gates remain host hook/model tests, identical-schedule native full
RAM comparison, paired VRAM/frame/AY results where affected, and live tests.
Live timing changes can legitimately change the RNG/hand; do not demand
byte-identical live final RAM across different real-time schedules, or use that
as a reason to weaken the deterministic replay gate.

## Model question (fidelity, not an optimization)

**INFERRED (open):** the ROM reads the ACRTC data port only at `$F6002`: 14
byte-read sites and four word-read sites, and never a byte at `$F6003`. The
checksum reads one byte per RD word, and the drain reads one byte per RFR poll.
The model sequences FIFO bytes with a high/low toggle that ignores address
bit 0. It also bounds the read FIFO only by RFF status at eight words, without
suspending RD. The checksum therefore leaves 38,076 words, and on the Amiga the
ring buffer grows to 65,536 entries. Under the same byte sequencing, a 16-byte
physical read FIFO would fill after 16 iterations. The ROM's access pattern
suggests that on the board one byte read at `$F6002` may consume one RD result.
Glue that latches the word and presents the low byte at A0=1 would do that; the
existing WPTN word-count evidence points the same way.

The 152,304 drain dispatches, and possibly the checksum value, depend on this
unresolved behaviour. Do not specialize RD/FIFO fast paths on the current
semantics until the user decides whether to investigate it. Any model change
must regenerate the host reference and replay; it is not a performance
shortcut.

## Options held in reserve (each needs explicit approval)

- **Wider fused hook sequences:** beyond the bounded benchmark approved below,
  one Line-A executes an audited, straight-line
  sequence of adjacent original instructions with exact semantics and byte
  guards, e.g. BTST/Bcc/MOVE at `$2E58–$2E5E` or the RD/poll/read triple. This
  removes one or two exception round trips per pair. It also emulates extra
  original instructions inside the hook, which current rules restrict ("do not
  transliterate game loops"). Consider it only if steps 2–3 miss the approved
  budget.

## Execution order and stop conditions

Implement 1 → timing-contract decision → 2 → 3 and 5 as the gameplay profile
dictates → 4 → 6 → 7, checking the overall budget after every stage.
Startup-state research can proceed between measurement runs, but architectural
changes must not be mixed into hook-performance tests. Once a representative
bounded test passes, use a longer play test only where new behavior or timing
changes justify it. Keep generic and optimized diagnostic paths until the
optimized path is established.

Stop for a decision after step 1 (timing contract), before specializing RD/FIFO
paths (model question), if the measured fast-hook ceiling cannot meet the
approved budget, if the timing contract must change again, or before
introducing native state snapshots or inferred battery-backed memory. Do not
silently relax correctness beyond the approved diagnostic policy, remove the
runtime watchdog, transliterate game
loops, or declare success from a scripted hand that still takes many minutes.
Each completed step records its before/after numbers and remaining uncertainty
in the Phase 5 notes.

## Approved benchmark: a bounded command-feed fusion experiment

The latest isolated assembled status path still costs about 48.44 us
(17,736 minus 141 E-ticks, 512 accesses at 709,379 Hz), above step 3's 25 us
acceptance gate. Reducing the shared PIA call layers and whole-word blitter
traffic does not remove that fixed exception cost. This is a measured miss,
not proof that every possible single-instruction optimization is exhausted.

The user approved benchmarking this bounded sequence on 2026-09-26:

- Limit the first experiment to the command feeder at `$2E58`: status BTST,
  its conditional branch at `$2E5C`, and the FIFO MOVE at `$2E5E`. End at
  `$2E62` when writing, or the original `$2E7E` branch target when not ready.
  Do not incorporate the surrounding queue loop, RD, game logic or accounting.
- Guard all original instruction/extension bytes during preparation. Keep the
  existing one-instruction implementation available for comparison and fallback.
- Use an assembly handler and the same shared HD63484 write endpoint. Test the
  actual ready bit; never assume success. Preserve all registers/CCR, original
  source postincrement, byte ordering, address guards and failure PCs.
- Retain a safe boundary after **each original instruction**. If a clock,
  interrupt, trace, replay event or invalid dynamic operand prevents continuing,
  promote at the next original PC without repeating any completed effect. Replay
  must count/charge the three instructions separately and admit recorded events
  at their original boundaries.
- Differentially execute the independent original three-instruction sequence
  in Musashi: all ready/CCR combinations, taken/not-taken branch, source bounds,
  postincrement, ABI scratch clobbers, faults and each intermediate interrupt
  boundary. Run exact native full RAM/VRAM/frame/AY replay on both frame formats.
- Compare the assembled pair latency and the same live workload before enabling
  it by default. Reject it if the gain is immaterial or any fidelity gate fails.

The benchmark is authorized; wider sequences and production enablement are not
implied. The experiment is selected explicitly with `native-feed-fusion`; normal
runs retain one-instruction hooks. Compare correctness and measured benefit
before proposing whether to enable it. FIFO semantics remain unchanged.


**Benchmark completed (2026-09-26):** two controlled A1200 repeats reduce the
ready-test/branch/write pair from 227.72–227.74 us to 204.37 us (10.25–10.27%).
The linked assembly passes 502,272 differential cases and exact ECS/AGA full
RAM/VRAM/frame/AY replay. Live first-60-second intervals are 79.80 s off and
79.00 s on; differing presentation/copy counts prevent attributing that entire
1.0% difference to fusion. Both complete the play/service scenario without
errors or resets, but neither meets the real-time gate. Retain the experiment
opt-in; there is no production-enablement recommendation from this small live
difference alone. See [native-clock.md](native-clock.md#bounded-command-feed-fusion-benchmark-2026-09-26)
for measured counters, reproduction and limits.


**Default enabled (user approval, 2026-09-26):** the verified `$2E58/$2E5C/$2E5E`
combined sequence now runs by default. Place `native-no-feed-fusion` beside the
Amiga executable to retain separate hooks for comparison. The old positive
`native-feed-fusion` marker is no longer needed. `native-no-short-hooks` and
`native-generic-hooks` still force their existing slower paths. This supersedes
the benchmark's opt-in policy above; it does not authorize wider fused sequences
or change the bounded clock, RD/FIFO semantics or replay schedule.

**MEASURED continuation (2026-09-27):** A2 curve stamps pass exact ECS/AGA replay.
Deal circles/ellipses average 1.591/0.780 ms; deal ratio is 0.604 board/wall.
A5/A6 follow. The user authorized benchmarking the burst plan's C1/C2/D1/D2
experiments; none is included in the A2 drawing change.

**MEASURED continuation (A5, 2026-09-27):** command setup and small-fill changes
pass full ECS/AGA replay and live24. Small CPU fills require an empty queue;
eager draining caused 31 ms waits and was rejected. Warm synthetic face is
176.9 ms, deal board/wall 0.614; A6 and presentation work follow.

**MEASURED continuation (A6, 2026-09-27):** synchronized direct-plane loops
pass exact ECS/AGA replay and live24. The warm synthetic face reaches 171.9 ms,
but live deal board/wall remains 0.612. B1/B2 presentation and the now-authorized
C/D experiments remain in the current burst plan.

## Authorized burst pass disposition (2026-09-27)

The burst plan's A1–A6, B2, C2–C4 and D2 changes are retained; C1 remains opt-in.
B1/B3 and D1 were measured, checked and reverted rather than enabled despite
negative results. Gameplay now banks three PAL frames, boot one; outstanding
board ticks remain bounded to one frame, with unchanged calibrated K.

**MEASURED:** ordinary A1200 live24 takes 55.54 PAL seconds for 48 game-seconds;
all inputs complete without fault/reset, but the 5% real-time gate fails. The
warm synthetic face is 171.9 ms, also above the burst plan's ≤40 ms target.
This closes the authorized experiment list, not Phase 5 or release acceptance.
Further changes to status-observation ordering or clock calibration require
the decision gate above. [native-burst-plan.md](native-burst-plan.md) is the
current evidence and verification index.
