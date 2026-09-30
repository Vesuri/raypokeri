# T14: bounded sound-register fusion

2026-09-30. Authorized experiment, **opt-in**, `SOUND_WRITE_FUSION=1`.
The normal default remains zero until paired performance and release gates pass.

## Scope and preserved boundaries

The original `sound_register_write` routine saves D3 at `$0D58`, performs six
PIA writes with four register operations, restores D1/D3 and returns. The new
body starts at the existing `$0D5A` byte-write hook and stops before `$0D80`.
The initial stack save, final D1/D3 restoration and RTS execute natively, without
introducing hooks or replacing stack behavior. All six original byte writes
call the existing `nativeShortIoWriteValue` endpoint in their original order.
No PIA/AY state or IRQ side effects are duplicated in the assembly block.

The six installed descriptors must match the decoded registers, byte widths,
lengths, nominal cycles, displacements and physical port addresses. The four
intervening operations must match their authored instruction encodings. A
shape mismatch fails startup. Each later effective address is checked again
before its write is admitted or charged. The first uses the ordinary short
I/O guard. A bad later address promotes at its exact original instruction PC.

The block keeps the saved D0/D1/A0/A1 frame and live D2–D7/A2–A6 convention.
MOVE.L D1,D3 updates the live D3; byte arithmetic updates only saved D1's low
byte. Each instruction preserves X and records its proper N/Z/V/C before
bookkeeping can change physical flags. PC, resume PC and original nominal
cycles advance separately at each of ten boundaries. The whole span costs
100 reference 68000 cycles, including the first already-admitted 12 cycles.
Every boundary uses the same frame/quit/IRQ promotion check as the existing
handler fusion. No following instruction runs across a declined boundary.

Installation is live-only, like the existing FIFO-control fusion. Diagnostic
replay retains ordinary instruction services. Therefore a replay pass proves
that the experiment does not disturb the reference path; it does not by itself
prove the fused body's intermediate states. The linked CPU oracle and live
checks provide that separate coverage.

## Current evidence

**MEASURED:** `make harness-sound-check`, against the opt-in ELF, passes 78,336
cases on emulated 68000/68020 execution. The independently assembled oracle
uses a different port displacement and the original 68000 nominal cycle model.
The matrix covers all 32 CCR values; byte zero/sign and upper-register cases;
each original instruction boundary; pending-frame and service-promotion events;
ABI scratch-register destruction; bad effective addresses at every write;
exact six-write arguments/order, resulting D1/D3, remaining registers, saved
stack/frame fields, PC/resume PC, cycles and both disabled/enabled live instruction accounting.
Failures exit cleanly rather than invoking a host crash dialog.

The existing `harness-short-check` and `harness-feed-check` also pass against
this opt-in build. Local evidence: `/tmp/pokeri-t14-oracle.log`,
`/tmp/pokeri-t14-shared-oracles.log`. Frozen candidate:
`tmp/t14-candidate/Pokeri{,.elf}`. No original data is included in the tests.

**MEASURED first native smoke:** a frozen opt-in A1200 cold live24 run reaches
Ready at cycle 47,120,000 / VBI 807 and ends at 480,000,000 cycles / VBI 3,578,
with all 24 inputs, no errors/resets and vectors restored. Ready-to-finish is
54.110 board seconds / 55.418 PAL seconds (0.9764). The 33 observed AY batches
have a median application span of 9.2 ms. This is not a paired measurement:
hands/timing differ from previous runs, no accepted Double occurred, and the
largest inter-write excess still reaches 391 ms. Do not claim the sound or
card deadlines passed. Evidence: `amiga/.run/t14-live-aga/gdb-out.log`, analyzed
by `host/release_timing.py`.

The default build has been restored. Its complete allocated ELF sections match
`tmp/t9-final-candidate/Pokeri.elf` exactly, proving the disabled experiment
leaves the normal executable unchanged. The user’s saved T5 executable is also
untouched.

Still required before activation:
- host suites, ECS/AGA replay and cold/warm live24, including vector/heap cleanup;
- accepted Double with original AY order/hash and normal-code excess-delay report;
- trace remeasurement of the six sites, and a VBI latency check;
- a documented retained/rejected disposition against the 0.2 ms/register and
  3–4 ms/note estimates. Reduced exception entries alone do not prove these
  targets or settle the separate drawing-related late-write deadline.

## Paired complete-register benchmark (2026-09-30)

**MEASURED:** an authored synthetic sequence performs the same six PIA writes
and four register operations through the ordinary hooks and the combined hook.
Both paths use the shared Board/PIA/AY endpoint, with output DDRs enabled and
real falling-edge select/data strobes. Each batch writes mixer register 7 = 255
512 times, with the Paula backend attached. Four trials alternate mode order;
the timer is read only around each complete batch. Register/write-count and
PIA-output checks pass, with no fault, no pending frame and restored vectors.
This is a repeated muted mixer write, not a changing tone/envelope workload.

At the measured 709,379 Hz E-clock, ordinary/combined batch ticks are:

| Trial | Ordinary | Combined |
|---|---:|---:|
| 1 | 305049 | 242874 |
| 2 | 305240 | 242811 |
| 3 | 305002 | 242818 |
| 4 | 305007 | 243040 |

The means are **840.0 → 668.7 µs per complete register write (20.4% lower)**.
That saves 171.2 µs but does not reach the 200 µs/register estimate. The result
includes real exception entry/exit and shared model work; it is not a per-site
or drawing-related AY lateness measurement. Evidence:
`amiga/.run/t14-sound-bench/gdb-out.log`, frozen `tmp/t14-benchmark/`, and the
read-only `amiga/sound-benchmark.gdb`. Build with
`PROFILE_SUPPORT=1 SOUND_WRITE_FUSION=1`, and use `native-benchmark` on a fresh
non-replay drive. The synthetic program contains no ROM-derived bytes.

The `LIVE_INSTRUCTION_COUNTS=1 SOUND_WRITE_FUSION=1` linked oracle separately
passes all 78,336 cases, checking the count at every stop alongside the saved
state and nominal cycles. Evidence: `/tmp/pokeri-t14-counts-oracle.log`, frozen
`tmp/t14-counts/`. Normal flags were restored afterward; allocated ELF sections
still match the T9 release candidate exactly. T14 remains opt-in pending the
remaining release gates and an explicit disposition against its missed target.

## Real-model boundary checks (2026-09-30)

**MEASURED:** 672 additional cases execute the independently assembled ordinary
sequence and the linked fused body against the shared Board/PIA/AY models. They
cover all 16 register selections, a PIA CA1 edge before each of the six writes,
and guest IPL 0/5/7 on both 68000 and 68020 execution. IPL 0 promotes after the
write that observes the level-5 IRQ; IPL 5/7 keeps executing. Selecting register
15 faults on the data strobe and stops before the final write. The comparison
checks write order, exact stop PC/CCR/D1/D3/cycles and serialized full device
state, including internal latches. Fault state/reason is checked separately
because the snapshot format intentionally refuses faulted boards.

The endpoint ABI is intercepted by the host test: it performs the shared Board
write and IRQ-cache query, and returns the promotion bit to the linked assembly.
It does not execute the native C endpoint itself or the subsequent native full
dispatcher. That distinction keeps this evidence separate from the live/replay
release gates. IRQ-cache results are also checked against the uncached Board
IRQ query after every access. The external edge explicitly invalidates the
cache, as a source-changing board tick does.

Both instruction-accounting variants pass these 672 cases and the existing
78,336 instruction/operand cases. Logs:
`/tmp/pokeri-t14-model-oracle.log` and
`/tmp/pokeri-t14-counts-model-oracle.log`. No production code changed in this
follow-up; T14 remains opt-in while the other release gates run.

**MEASURED regression gate:** `make harness-check harness-platform-check
harness-native-check` passes after the real-model oracle extension; log
`/tmp/pokeri-t14-host-gates.log`. Two native AGA replay attempts ended in an
externally SIGKILLed emulator before producing comparison data. They are not
passes or established game failures. A fresh run uses dedicated debug port
3187 to exclude default-port reclamation; native comparisons remain pending.

**MEASURED AGA replay:** the dedicated-port run completes with error zero and
vectors restored. All 262,144 RAM bytes, 524,288 VRAM bytes, 172,064 cropped
pixels and 60 AY writes match the host reference at 7,904,133 instructions,
64,000,000 cycles and 8,685 IRQs. Evidence:
`amiga/.run/t14-replay-aga3/gdb-out.log`,
`/tmp/pokeri-t14-aga-compare.log`. As noted above, diagnostic replay intentionally
uses ordinary hooks; the fused live path has separate CPU/model/live gates.
ECS replay and the remaining live-performance gates are still pending.

**MEASURED ECS replay:** the A500+ run also completes with error zero, vectors
restored and exactly the same full RAM/VRAM/pixel/AY comparison counts and
instruction/cycle/IRQ endpoint as AGA. Evidence:
`amiga/.run/t14-replay-ecs/gdb-out.log`, `/tmp/pokeri-t14-ecs-compare.log`.
Both chipset replay gates now pass. The accepted-Double live scenario and the
remaining cold/warm live, trace and VBI gates are still required before default
activation; the measured 200 µs/register estimate remains unmet.

## Accepted Double follow-up (2026-09-30)

**MEASURED:** the normal-code `SOUND_WRITE_FUSION=1 DOUBLE_SCENARIO=1` run
accepts Double in round 3, completes all 32 generated key transitions and exits
with status 4, error/reset counts zero and vectors restored. Ready-to-finish is
72.030 board seconds / 73.907 PAL seconds (0.9746). Across 114 AY batches the
median first-to-last application span is 9.3 ms. Maximum batch-to-batch excess
is 244.5 ms; the largest consecutive-write excess is 234.432 ms (694.432 ms
PAL versus 460 ms board time). Those are delayed writes, not audible envelope
duration. Cached-back intervals still range roughly 16.8–57.9 ms; the 20 ms
complete-card target is not established. Different hands prevent treating this
as a paired comparison against T9's Double run.

Evidence: `amiga/.run/t14-double2/gdb-out.log`, analyzed with
`host/release_timing.py --scenario double`. The earlier `t14-double` fixture
omitted `native-test-inputs` and only ran attract mode; it was interrupted and
is excluded from validation. `release-double.gdb` now rejects that omission at
Ready. The valid test has no `native-live` cycle-limit file: the keyboard driver
ends it after the accepted Double scenario. Remaining live24/trace/VBI gates
and AY-order qualification remain open; the sound/card deadline estimates have
not passed.

## Native latency, trace and warm-start gates (2026-09-30)

**MEASURED VBI:** the instrumented live24 run completes 24 inputs with no
error/reset and vectors restored. Startup: 807 samples, maximum scanline 6,
zero samples at/after line 29. Gameplay: 2,776 samples, maximum line 10, zero
late samples. No BLITHOG samples occurred. The read-only probe instruction at
`nativeVbi+0x84` was checked against the frozen ELF before running. Evidence:
`amiga/.run/t14-vbi/gdb-out.log`, frozen `tmp/t14-vbi/`.

**MEASURED trace:** nine 100-field captures cover deal, draw and an accepted
Double in round 3. The scenario ends cleanly at 623,840,000 board cycles with
no reset/error. In 18 PAL seconds, `$0D5A` has 630 entries averaging 655.9 µs
own service time, excluding nested IRQs; eight promote to full dispatch. Rare
continuations occur at `$0D78` (4 entries, 176.3 µs mean) and `$0D7C` (5 entries,
124.4 µs mean); the other three original write sites have no outer Line-A entry.
Inside the `$0D5A` service, 3,763 shared byte-endpoint calls take 299.64 ms of
413.2 ms total (72.5%), averaging 79.6 µs each. Its Board write, completion and
IRQ-query costs overlap this total and must not be added again. This identifies
why removing five exception round trips alone does not reach 200 µs/register.

Trace evidence: `amiga/.run/t14-trace`, `/tmp/pokeri-t14-trace-report.log`,
`/tmp/pokeri-t14-trace-sound.log`, `/tmp/pokeri-t14-trace-sites-all.log`.

**MEASURED warm A1200 live24:** Ready at cycle 4,640,000 / frame 390; finish at
480,000,000 / frame 3,412, all 24 inputs, no error/reset, vectors restored.
The AY batch median is 9.2 ms; maximum batch excess is 272.2 ms. The run retains
saves from the candidate's earlier cold run. Evidence:
`amiga/.run/t14-warm-aga/gdb-out.log`. Cold/warm ECS live checks and the final
activation/AY-order qualification remain; these passes do not close the global
sound/card deadlines.

**MEASURED cold ECS live24:** all 24 inputs complete with no errors/resets and
vectors restored at 480,000,000 cycles / frame 13,292. Ready-to-finish is
53.860 board seconds / 189.786 PAL seconds (0.2838). This is a correctness pass,
not an ECS real-time claim. Evidence: `amiga/.run/t14-live-ecs/gdb-out.log`.
The warm ECS run uses the saved NVRAM/accounting from this run and is pending.
