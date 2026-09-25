# Native performance recovery plan

Proposed 2026-09-25, following the startup profile of `167f486`. This is the
execution plan for the remaining Phase 5 performance work, not evidence that
performance is fixed. Changes are to be measured separately and committed as
cohesive steps on main. Phase 6 remains out of scope.

## Objective and evidence

Make `AMIGA_MODEL=A1200 ./run.sh` practically playable, with normal audio and
correct game timing, while preserving a 68000-compatible OCS/ECS path. The
original program still executes natively. Musashi remains host-only.

The current startup sample advances 2.62 seconds of board time in about 325.5
seconds of PAL machine time, with 592,990 native dispatches. Completed drawing
commands take 17.4 seconds; inclusive video-bus services take 75.7 seconds;
Paula takes 1.7 seconds. Command time is inside bus time, not additional to it.
The unexplained remainder has not yet been divided accurately between exception
wrappers, dispatcher bookkeeping, original execution and other Amiga work.
Sampled service totals do not provide that exact split. See `docs/rom-set.md`.

The graphics checksum at $10F2C/$10FA0 repeatedly issues RD at $10FC0, polls
RFR at $10FC6, and reads a FIFO byte at $10FCC. A single original bus instruction
currently enters the general exception/clock/decoder/address-check/scheduler
path. This is the first optimization target. Do not bypass the checksum or
replace its loop with C. Drawing cache work alone cannot solve this sample.

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
  before staging new files. No additional ROM-test bypass is implied here.

## 1. Establish an attributable baseline

Instrument opt-in profiling at the following milestones: entry to preparation,
ROM loaded/patched, takeover, first guest instruction, first visible game frame,
end of graphics checksum, operator setup complete, Deal, cards settled, Hold,
Draw and exit. Distinguish a Copper list being allocated, DMA being enabled and
an actually presented nonblank frame. Do not call a counter increment a visual
verification.

Report host wall time, PAL VBI/E-clock elapsed time and virtual board cycles
separately. Warp-mode host time is not an Amiga performance metric. Also run the
ordinary non-warp launcher to establish what the user experiences. Count live
service dispatches under that name; `nativeInstructions` currently mislabels
this quantity outside replay.

Collect disjoint costs for exception entry/exit, clock bookkeeping, hook
handling excluding the device, device work, scheduler/IRQ dispatch, presentation,
Amiga IRQs and unhooked guest intervals. Use low-overhead sampling or bounded
microbenchmarks where timing every short access would dominate it. Account for
nested interrupts and nested categories, wrap and lost samples. Measure the
observer itself and compare profiling off/on; never infer the residual entirely
as hook cost by subtraction without checking it.

Rank hook sites by frequency and cost. Specifically count RD, status polls,
FIFO reads, PIA/AY traffic, CPU-control hooks, trace traps and blitter IRQs.
Record median, p95 and maximum short-hook latency, command latency, and copy
fallback reasons (alignment, overlap, direction, bounds). Add a clock-ledger
check: original intervals plus excluded work and measured transitions should
explain total time within measured observer uncertainty.

**Deliverable:** a reproducible baseline table for preparation, checksum,
remaining cold boot, idle and one full hand. A bounded checksum-loop sample can
start the next step; a full slow hand need not be rerun after every edit.

## 2. Remove repeated decoding and lookup from hot accesses

Extend `tools/native_tables.py` and loader preparation to produce validated,
site-specific descriptors. Decode instruction shape and immutable operands
once. Resolve relocated absolute addresses after placement, not repeatedly
through virtual bus calls. Include expected device/range, width, direction,
original instruction length/cycle metadata, register effects and CCR behavior.
Guard all source extension bytes now baked into descriptors, not just opcodes.
Generated ROM-derived values stay in `amiga/generated/`.

Dispatch directly from the Line-A index. Specialize the frequent MOVE and BTST
forms first, beginning with the checksum's RD/status/FIFO sites. Dynamic address
registers still get exact bounds/access checks; a site's prior observation does
not make all future addresses safe. Preserve pre/postincrement ordering, A7
byte increments and the original CCR rules. Retain the generic handler for
complex/uncommon supported shapes and retain loud failures for unknown ones.

Call the relevant shared device endpoint directly after validation. Avoid the
second generic Board address decoder and virtual HookBus extension reads on
known sites. Do not create a separate approximate device model. Keep required
surface synchronization for RD, and preserve FIFO high/low-byte state.

Initially retain the existing full save and timing boundary. This isolates the
benefit of removing software layers from any clock or assembly change.

**Gate:** generated descriptors cover all admitted forms; differential hook
tests against Musashi include every register and CCR bit, address boundaries,
aliasing and fault cases. Native replay reaches the same instruction boundary
with all RAM equal. Re-measure the fixed checksum workload and its hot sites.

## 3. Add a minimal fast exception path

If descriptor dispatch still misses the measured budget, use a small set of
handwritten 68000 assembly handlers selected by validated descriptors. Keep the
two-byte Line-A patch mechanism first; the patch remains one original access.
For hot status/FIFO/byte operations, save only registers the handler actually
clobbers. Construct the correct returned CCR in the physical exception frame
and keep virtual SR consistent with the established lazy synchronization rules.
A C call must obey its entire clobber ABI, not just the apparent callee source.

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

**Gate:** unchanged CPU/device/access results and full-RAM replay equality;
known short-hook overhead reduced substantially (initial target at least 10x
against its own baseline, excluding heavy command execution). This is an
engineering target, not a claim that 10x alone guarantees real-time gameplay.
Do not proceed to wider rewriting of game instruction sequences if it fails.

## 4. Separate fast access completion from scheduling

Keep the approved service-excluded clock for the first fast-handler comparison.
Then remove unnecessary scheduler work using explicit due flags/deadlines:

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

Timer work needs its own measured choice. First keep short assembly CIA
boundaries and remove duplicate stops/bookkeeping. If CIA I/O then dominates,
prototype continuously measuring elapsed time and subtracting excluded service
intervals, with proper nesting and overflow accounting. This must preserve
service-exclusion semantics; it is not permission to guess a constant duration
for arbitrary handlers. A calibrated constant is admissible only for a bounded,
proven path with a measured error bound, never for DMA waits or command execution.

Use separate startup/steady-state tests of clock drift, short polling loops,
watchdog kick/reset timing, long interrupt handlers, delayed/multiple source
edges and prolonged graphics commands. Retain the original watchdog test and
its one expected cold-start reset. Do not lengthen watchdog periods to hide
slowdown. The nominal original-board oscillator remains an estimate.

**Decision checkpoint:** after measuring optimized hooks, compare the remaining
cost and drift. Retain the current clock architecture if it meets the target.
If a different guest-time basis, interrupt schedule or patching architecture is
needed, present measured alternatives for user approval before changing that
contract. No assumption that a new clock alone makes the CPU do more work.

## 5. Complete useful blitter coverage

After hook costs are under control, rank remaining device work. Implement shifted
source-to-destination planar copies using blitter shifts, masks and modulos.
Choose ascending/descending traversal only when it preserves ACRTC sequential
semantics. ACRTC overlapping-copy behavior is not automatically memmove behavior.
Keep a correct fallback for cases without a proof, and measure its use.

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
to execute cold boot correctly. No new graphics-checksum bypass is proposed.

## 7. Performance and release gates for Phase 5

Use fresh-state and repeat-start runs, normal audio, the exact normal launcher,
and both profiled and unprofiled binaries/settings. Capture the following table
after each meaningful improvement: preparation time; first visible frame;
checksum completion; ready-for-input; hook distribution/latency; command totals;
board-time versus PAL-time ratio; input latency; missed animation presentations;
watchdog behavior; and retained-state result.

Targets for A1200 (to be verified, not claimed in advance):

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
without 020/AGA-only instructions; its real-time target follows A1200 bring-up.

Correctness gates remain host hook/model tests, identical-schedule native full
RAM comparison, paired VRAM/frame/AY results where affected, and live tests.
Live timing changes can legitimately change the RNG/hand; do not demand
byte-identical live final RAM across different real-time schedules, or use that
as a reason to weaken the deterministic replay gate.

## Execution order and stop conditions

Implement 1 -> 2 -> 3 as required -> 4 -> 5 -> 6 -> 7, checking the overall
budget after every stage. Startup-state research can proceed between measurement
runs, but architectural changes must not be mixed into hook-performance tests.
Once a representative bounded test passes, use a longer play test only where
new behavior or timing changes justify it. Keep generic and optimized diagnostic
paths until the optimized path is established.

Stop for a decision if the measured fast-hook ceiling cannot meet the target,
if the timing contract must change, or before introducing native state snapshots
or inferred battery-backed memory. Do not silently relax correctness, remove
checksums/watchdogs, transliterate game loops, or declare success from a scripted
hand that still takes many minutes. Each completed step records its before/after
numbers and remaining uncertainty in the Phase 5 notes.
