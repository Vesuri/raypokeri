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
