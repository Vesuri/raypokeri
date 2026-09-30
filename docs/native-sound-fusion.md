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
- model fault/IRQ cases beyond the synthetic boundary-promotion matrix;
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
