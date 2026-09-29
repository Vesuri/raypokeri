# Native presentation from original game ticks

## Decision and scope (2026-09-29)

The user requested removal of invented presentation timing. Reproducing the
physical board's VSYNC phase relationship is not an acceptance prerequisite.
Ordinary native presentation now follows completion of the original system-tick
handler; it does not use an independent elapsed-cycle timer or a card-hit trigger.
The existing user-approved shuffle markers still preserve shuffle animation
steps, and explicit diagnostic captures still request their own images.

## Request and completion boundaries

The original PIA system interrupt is vector $43, entering $0C06 and returning
through $0C3E. Its callbacks may run in virtual user mode and admit nested ticks.
Record the outermost exception frame when injecting this original interrupt.
Only the RTE consuming that frame creates a pending composition request. Nested
interrupt returns do not release an unfinished outer callback. The marker stores
the physical guest frame address. The assembly RTE guard promotes only a return
consuming that address to the checked dispatcher; other returns retain the fast
path. This bookkeeping
changes no guest instruction, register, interrupt source or tick frequency.

Requests coalesce while waiting for all of the following:

- The original tick's callbacks have returned.
- The original graphics producer/consumer pointers at $41326/$4132A are equal.
- The HD63484 has no partial FIFO word or incomplete command.
- Card recognition is idle, including tracking-only scalar redraws.
- A display buffer is available and no shuffle marker owns presentation.

The callback-completion state is sampled before injecting the next original
interrupt. A just-injected handler has executed no instructions, so it must not
prevent composition of the previously completed update. This avoids starvation
when ticks are queued. A callback already executing still blocks composition.

Composition queues its blits after earlier graphics work. The Amiga VBI only
publishes a ready buffer, using the existing buffer ownership rules. There is no
new frame-rate divider or wait in original game execution. Game pixel reads,
clock accounting, audio and input scheduling retain their existing semantics.

Startup fast-forward coalesces requests until Ready instead of maintaining a
separate one-second progress timer. Diagnostic replay has no periodic composition;
its explicit end capture remains authoritative and materializes required pixels.
CPU/peripheral reset clears both the active tick-frame marker and pending request.

## Removed mechanisms

- `lastPresentCycle` and the 160,000-cycle presentation deadline.
- The completed-card early refresh path, `CARD_PRESENT`, and its counters.
- `startupPresentFrame` and its separate progress cadence.

The other cycle constants in clock/guard accounting are unrelated to presentation
and remain part of their existing approved policies. This change neither claims
physical VSYNC equivalence nor establishes the outstanding 20 ms graphics/audio
or sustained real-time performance targets.

## Validation (2026-09-29)

**MEASURED:** headless harness, platform and native suites pass. The linked
68000/68020 hook proofs pass, including 384 added cases checking outer versus
nested/unrelated RTE frames, both restored privilege states and every CCR
combination. The optional video-IRQ path passes 420,536 admission/race cases
with the pending-refresh guard replacing its removed timer/card-hit guards.
The normal build was restored and its allocated sections match the frozen
candidate used for native validation.

**MEASURED:** the A1200 cold live scenario completes all 24 input edges, 30
shuffle steps and 60 shuffle AY writes, with zero watchdog resets, no alert,
and an empty heap after cleanup/destruction. It records 4,552 completed outer
system-tick requests, 1,225 nested ticks and 3,715 composition admissions;
unchanged images are filtered by the display backend, yielding 257 prepared
frames including shuffle frames. Every admission asserts a completed callback,
an empty original command ring, a complete HD63484 command and idle card
recognition. These are correctness counts, not an FPS measurement.

**MEASURED:** both A1200/AGA and A500+/ECS replay match all 262,144 RAM
bytes, 524,288 VRAM bytes, 172,064 cropped pixels and 60 AY writes against the
host reference at 7,904,133 instructions, 64,000,000 cycles and 8,685 IRQs.
Both restore the vectors and finish without a native error. The explicit replay
capture remains exact after removal of periodic diagnostic composition.

The first candidate's live test exposed the optimized RTE bypassing C++ tick
completion. That candidate produced no ordinary refresh requests and was
rejected. The assembly guard and positive request/admission assertions above
cover that failure; only the corrected candidate is retained.

Local evidence: `tmp/tick-presentation-host.log`,
`tmp/tick-presentation-rte-hooks.log`, `tmp/tick-presentation-videoirq-test.log`,
`amiga/.run/tick-presentation-rte-live-aga`,
`amiga/.run/tick-presentation-rte-replay-{aga,ecs}`,
`tmp/tick-presentation-rte-{aga,ecs}-check.log`, and frozen
`tmp/perf/Pokeri-tick-presentation-rte(.elf)`.
