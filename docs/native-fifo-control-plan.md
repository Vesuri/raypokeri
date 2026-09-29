# Bounded FIFO control-triplet experiment

Status: benchmark approved by the user on 2026-09-29; measured improvement
accepted by default after all correctness gates passed. Scope remains only
these two triplets; no interrupt suppression is approved.

## Evidence and scope

**MEASURED:** normal landing-card intervals execute 26 original video FIFO
interrupt handlers, in addition to 258 status/word-feed pairs. Handler counts
show 13 executions each of the three writes at $2E70/$2E74/$2E7A and
$2EB2/$2EB6/$2EBC. They are 78 of the 182 extra accesses.

**DERIVED:** each triplet selects CCR low through the address port, writes
$80 or $81 through the data port, and restores FIFO address selection. Only
CCR bit 0 (WFE interrupt enable) changes. The first sequence occupies 14 bytes
and ends at $2E7E; the second ends at $2EC0. Their existing short handlers
remain the semantic reference and fallback. No ROM bytes belong in this doc.

## Implementation

- `FIFO_CONTROL_FUSION=1` selects assembly for only these two three-MOVE
  sequences. Preparation verifies every original opcode, immediate and
  displacement, plus the descriptor's address, length, kind and cycle count.
- Reuse authoritative address/byte-phase fields and the current data-write
  endpoint. Preserve each MOVE's flags, original PC, nominal cycles, address
  validation and register values. No drawing, game or accounting substitution.
- Preserve a scheduler/IRQ/quit boundary after each original instruction. In
  particular, enabling WFE can interrupt before the final FIFO address selection.
- Promotion resumes at that exact next original PC without repeating a write.
  Each next effective address is checked before charging/executing it. Invalid
  addresses return to the ordinary single-instruction path and failure state.
- Diagnostic replay retains ordinary hooks. The original handler and RTE still
  execute; no interrupt is suppressed or acknowledged speculatively.
- `FIFO_CONTROL_FUSION=0` retains the comparison path. Maximum theoretical
  removal is 52 physical exception round trips per measured landing, not a
  promised speedup. Pending boundaries can reduce the actual saving.

## Acceptance

1. Independent synthetic triplets on Musashi 68000/68020: all CCRs, each event
   boundary, both control values, invalid/misaligned/outside addresses, register
   preservation and exact cycles. Compare shared-model byte phases/status/IRQ
   effects against three ordinary writes.
2. Isolated native benchmark without per-operation timing reads. Retain only
   a measured improvement; distinguish full completions from boundary exits.
3. Full ECS/AGA replay equality: RAM, VRAM, screen, AY and instruction/cycle/IRQ
   boundary unchanged. Native arithmetic audit and relevant host checks.
4. Cold live24 on both chipsets, no errors/resets, all inputs. Re-measure release
   landing intervals; isolated savings do not close rendering/audio deadlines.
5. Normal release contains no profiling counters or timing scopes. Commit
   and push only a validated improvement; otherwise record rejection.

Separate work may optimize existing one-instruction services and virtual
interrupt admission, provided their observable behavior remains unchanged.
Changing interrupt priority, acknowledgement timing or guest-clock rules
requires a separate decision.

## Results

**MEASURED:** 200,704 independent 68000/68020 CPU/shared-model cases pass:
all CCRs, IPL 0/4/5/7, both control values, error-status variations, all byte
phases, each frame/IRQ stop boundary, wrong/odd/low/overflowing EAs, exact
cycles, frame/clock-resume PC and all preserved registers. Enabling an eligible
video IRQ stops before the final address selection. Existing feeder and
individual-hook CPU matrices and `harness-native-check` also pass.
The repeatable linked CPU target is `make harness-fifo-control-check`, with the
Amiga toolchain on PATH and a native build containing the feature.

**MEASURED paired A1200 benchmark:** 512 synthetic interrupt-disable triplets
take 83,639 E-clock ticks with ordinary hooks versus 64,418 fused, at 709,379 Hz:
**230.28 → 177.36 µs/triplet, 23.0% less**. Status 4/error 0; guest frame
counters 0/0. This batch enables no guest FIFO IRQ, so it measures uninterrupted
completion rather than claiming that every live triplet can complete.

**MEASURED initial release capture:** cold A1200 completes 24 inputs/30 shuffle
steps/60 in-motion AY writes, error/reset 0. Ready/end: 1,557/4,186 PAL frames.
Lighter cards: 17.408/18.272 ms; landings: 42.176/41.024/40.832 ms. Earlier
source-span landings of 42.688/42.752 ms are from a different hand, not a paired
percentage speedup. First/later special intervals of 77.152/70.912 ms remain
above deadline. Audio-generation deadlines remain open.

**MEASURED final live gates:** the final frozen candidate completes both cold
live24 scenarios with all 24 inputs, 30 shuffle steps and no errors or resets.
AGA Ready/end: 1,552/4,192 PAL frames, 60 in-motion AY writes; ECS: 6,242/16,022,
45 writes. Different hands and CPU speeds prevent treating AY totals as a
deterministic comparison. Both AGA and ECS full replay match all 262,144 RAM bytes,
524,288 VRAM bytes, 172,064 cropped pixels and 60 AY writes at 7,904,133
instructions, 64,000,000 cycles and 8,685 IRQs.

Default is enabled; `FIFO_CONTROL_FUSION=0` preserves the old path. Before
activation, the flag-off build matched the accepted source-span allocated ELF
sections exactly. The normal default build matches the frozen validated candidate in every
allocated ELF section and passes the arithmetic audit. The new repeatable CPU
target passes against that default build. There is no optional ledger instrumentation.
Local evidence:
`tmp/fifo-control-{bench-cpu,feed,short,native}-check.log`,
`amiga/.run/fifo-control-benchmark`, `amiga/.run/fifo-control-live-aga`,
`amiga/.run/fifo-control-final-{live,replay}-{aga,ecs}` and
`tmp/perf/Pokeri-fifo-control-bench(.elf)`.


## Dedicated CCR-low service (2026-09-29, accepted default)

FAST_FIFO_VALUE=1 changes only the C endpoint called by the already-approved,
byte-for-byte-guarded middle instruction of each triplet. No fused sequence is
widened. The existing address/size/site guards, CCR construction, cycle charges
and event checks after each original write remain unchanged.

For normal execution with AR=3 and no existing board/video fault, the helper
writes the authoritative CCR-low byte, computes video status once, and checks
the concrete PIA0, video and ACIA0 IRQ sources in their existing order. It
publishes the same pending-event bits and revokes feeder grants. CCR low has no
FIFO, byte-phase, cache flush, display-damage or address-increment side effects.
Diagnostic execution, another AR, an existing fault, or an active experimental
cache borrow uses the original endpoint. The helper is private to the guarded
byte-write site; it is not a new general address decoder. No clock, presentation,
IRQ delivery, or guest instruction policy changes.

**MEASURED:** paired A1200 batches of 512 synthetic triplets at 709,379Hz:

| Implementation | Ordinary triplets | Fused triplets |
|---|---:|---:|
| Accepted release | 86,218 ticks| 65,978 ticks|
| Dedicated helper, virtual device calls | 86,777| 58,020|
| Dedicated helper, concrete device calls | 86,152| 56,492|

The final variant reduces the fused workload by 14.4%, about 26.1 microseconds
per triplet. The original disable-only synthetic workload does not establish
a live IRQ or audio saving. All runs finish status4/error0/frames0/cycles0.

**MEASURED correctness:** host/native_fifo_value_check.py extracts the actual
linked code and obtains member offsets from its debug types. Its independent
Musashi oracle executes the compiled helper and concrete PIA IRQ routine on
68000/68020. All 1,411,072 cases pass: every video enable/status byte, pending
commands and result FIFO flags, every PIA control/flag byte on both sides,
every ACIA control with empty/nonempty receive queues, all IPL/pending-event
combinations, fallback arguments, all memory stores and the C ABI. Only the
general fallback is stubbed; the fast helper itself is actually executed.
The existing independent original-instruction triplet oracle also passes
200,704 cases against the new endpoint wiring, including every event boundary.

The first cold A1200 live run completes 24 inputs / 30 shuffle steps / 60 in-motion
AY writes, no error/reset, sampler off, ending at 4,062 PAL frames. Completed
landing backs 24–27 take41.792 / 41.856 / 41.792 / 43.392 ms. This is a modest live
result, not a 20 ms deadline claim; different hands and frame alignment qualify
comparisons with the earlier ~42–45 ms accepted-release observations.
The remaining native gates below pass. FAST_FIFO_VALUE is now enabled with
FIFO_CONTROL_FUSION by default; setting FAST_FIFO_VALUE=0 keeps the comparison
endpoint. This is a bounded service-cost improvement, not completion of the
20ms card/audio or real-time goals.
Evidence: .run/fast-fifo-value-{before,after}, .run/fast-fifo-direct,
.run/fast-fifo-direct-live, tmp/fast-fifo-direct-{cpu,boundaries}.log;
frozen tmp/perf/Pokeri-fast-fifo-direct(.elf).


**MEASURED release gates:** required headless model/platform/native suites and
linked short/whole-feed regressions pass. Exact AGA and ECS replay matches all
262,144 RAM bytes, 524,288 VRAM bytes, 172,064 pixels and 60 AY writes at
7,904,133 instructions/64,000,000 cycles/8,685 IRQs. Diagnostic replay takes
the existing general endpoint; active fast execution is covered separately by
the compiled CPU oracle and cold live scenarios. ECS live24 completes at
16,167 PAL frames (Ready 5,735),30 shuffle steps / 45 in-motion AY writes, with
no error/reset. The AGA release Ready/end frames are 1,173/4,062, with 24/30/60,
no error/reset. These frame counts exclude pre-execution preparation and must
not be substituted for the preparation-inclusive startup totals.

Paired diagnostic-only A1200 VBI measurements finish live 24/30/60 without errors.
Gameplay has zero samples at scanline 29 or later in both variants: before,
2,896 samples/max 19; after,2,890/max12. Startup has two late samples in each
(max 77/74). The probe runs after audio/screen service and is absent from normal
builds. Evidence: .run/fast-fifo-vbi-{before,after}, tmp/fast-fifo-{aga,ecs}-check.log,
.run/fast-fifo-live-ecs, tmp/fast-fifo-{headless,short,feed}.log. The ordinary
release is rebuilt without timing probes after activation. Its allocated ELF
sections are checked against the validated Pokeri-fast-fifo-direct candidate.
