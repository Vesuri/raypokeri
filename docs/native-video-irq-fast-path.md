# Guarded native video-interrupt admission

Status: opt-in prototype, not enabled. The accepted release is 33e60d3.
The 20 ms card/audio and sustained real-time objectives remain open.

## Evidence and scope

**MEASURED:** accepted-release landing backs still take about 42 ms. Earlier
read-only attribution found 27 general dispatcher entries at original PC $2EBC
per landing, accounting for 7.6–8.3 ms inside the C dispatcher alone; assembly
save/restore and the original handler are additional costs. The recent layout
and sampled-status experiments save only 5.5/11.3 microseconds per isolated
admission and do not materially improve card feeding. See
native-rendering-followup.md for qualifications and local evidence.

The next experiment targets that general-service transition, not the guest
handler. Keep the same already-executed FIFO-control write, the same interrupt
source/vector/priority, and execute the original handler instructions. No wider
guest hook, IRQ suppression, scheduling policy or clock change is proposed.

## Eligibility measurement

Use the accepted executable and read-only breakpoints for four landing backs.
Observe immediately after nativeClockPause, before later dispatch work. A check
before charging deferred cycles is insufficient: that charge can create a due
timer tick. Log frame/clock state, virtual SR/stack, video source, competing
PIA/ACIA sources, pending display work and completed-card presentation state.
The current verified instruction is nativeDispatch+0xC0 (TST.L after its JSR to
nativeClockPause). Cache begin/hit offsets are +0x2A0/+0x21A. Revalidate all three
against the particular frozen ELF before using the reader; offsets are not API.
The reader enables this breakpoint only inside the selected cards, with the
normal PC sampler off. Debugger expressions must be type-checked offline:
Registers.a is a C array, while video.control uses the freestanding array's
values member. No target function calls or writes are permitted.

## Proposed implementation boundaries

Start only at the existing nativeShortControlPromote path for the verified
post-write PC $2EBC. Existing grants are already revoked and the guest clock is
stopped. Reject replay, profiling, startup/not-Ready, clock calibration, other
PCs, virtual trace state or IPL at/above the video level before attempting
any shortcut. The normal dispatcher remains authoritative for every rejection.

Charge deferred time once through the existing nativeClockPause. Recheck that
there are no live ticks, unserved PAL frames, unspent credit/debt requiring
service, guard-check work, quit, reset, device/board error or drain observation.
Require the actual video IRQ to be pending with its existing vector, no competing
PIA/serial source or higher-priority vector condition, no active shuffle, no
pending presentation, no elapsed normal presentation deadline and no completed
card requiring early presentation. Missing eligibility is an ordinary fallback,
not an error and not permission to assume success.

Before mutating guest state, the physical interrupt mask and a final frame check
must close the race with an Amiga VBI. Device-source state cannot be changed by
an Amiga IRQ; original handlers are never run inside that IRQ. Physical callbacks
can still change frame, input and display bookkeeping, which is why their pending
work must force fallback. Keep these checks explicit rather than treating a
previous pending-bit snapshot as proof that nothing else is due.

For a proved eligible transition, construct exactly the existing virtual 68000
six-byte exception frame on the selected guest supervisor stack, preserving
original SR/CCR and post-write PC. At virtual user entry, save the current guest
USP and select nativeVirtualSsp exactly as setSr/pushException does; supervisor
entry uses the current guest stack. Keep the separate Amiga service stack. Validate
stack and vector mapping/alignment before any store; unsupported cases fall back.
Set virtual IPL/SR, handler PC, live-IRQ state, interrupt count and pending flags
exactly as the full dispatcher would, then use the existing clock restart and
physical return. Preserve every live register and the Amiga exception-frame format.

A failed guard after clock charging must leave guest/register/device state
untouched. Subsequent full dispatch must not double-charge: the existing deferred
totals are drained and nativeClockRunning is clear. This idempotence requires a
specific test; it must not be inferred from a successful fast case.

## Acceptance

1. Capture real post-accounting eligibility and blocked reasons first. If few
   landing admissions qualify, do not build a speculative parallel dispatcher.
2. Execute the actual linked guard/frame/return code on independent 68000 and
   68020 oracles. Cover every CCR/IPL, register/stack preservation, virtual stack
   transitions/refusal, mappings, competing sources, frame/quit arrivals and
   each guard failure. Verify all stores, not just final pixels. Compare time
   charging plus fallback to the existing dispatcher accounting.
3. Measure complete admission and full live landing latency. Include clock
   accounting and exception return; a frame-store microbenchmark is insufficient.
4. Run required headless/hook/FIFO tests, exact ECS/AGA RAM/VRAM/pixel/AY replay,
   cold live24 on both machines, and VBI/sound checks. Diagnostic replay alone
   cannot validate an explicitly live-only shortcut.
5. Keep normal builds restored while experimenting; default activation requires
   measured improvement and all correctness gates. Report the remaining complete-
   card/audio deadline even if this service improvement is accepted.


## Completed eligibility capture

**MEASURED:** amiga/.run/video-irq-eligibility-v3 uses the accepted uninstrumented
release, after its existing clock accounting. Two completed backs (starts 26/27)
take 42.432/41.664 ms in this capture. Their 53 entries at $2EBC include 51 that
pass the observed no-other-work conditions: card 26 has 25/27, card 27 has 26/26.
Two card-26 entries have a due tick; one of those also has no enabled video IRQ.
Every selected entry has no outstanding completed-card presentation, no shuffle,
no board fault/reset/quit, no drain observation, and calibrated clock mode 2.
There is no simultaneous unspent credit and debt. Stack-target/alignment and
actual shortcut correctness still require the implementation proofs above.

The full selected capture has 148 entries. Starts 24/25 do not reach complete
back hits in this run, so their mixed work is excluded from complete-back timing
and eligibility conclusions. Their 47/48 entries include 24 pending-display
cases each, and additional timer/frame work. This confirms that the display guard
cannot simply be omitted to increase the hit rate.

Every entry's virtual SR is 0, 4 or 8: S is clear. A supervisor-only shortcut
would admit none of these cases. Correct user-to-supervisor stack selection is
therefore essential, not an optional later extension. PIA0 control/flags are
$36/$80 and $0E/$00; the latched CA1 flag is disabled. Serial control $95 has an
empty receive queue. Guarding on nonzero raw flags would also reject useful
cases incorrectly; query actual enables/priority using the existing model.
The existing liveIrqActive flag is still set at most entries, but the ordinary
dispatcher clears it when virtual IPL is below 5 before admitting the new IRQ.
The shortcut must preserve that behavior rather than reject a stale active flag.

The complete scenario finishes 24 inputs/30 shuffle steps/60 in-motion AY writes,
status 4/error 0/no reset, sampler off, at 4,076 PAL frames. This remains an
eligibility experiment, not evidence of a performance improvement. Earlier
eligibility fixtures failed on debugger array syntax before collecting samples;
only v3 is evidence. Every reader expression was subsequently type-checked
without a target. The ordinary release remains unchanged.

## Opt-in prototype and first live measurement

`VIDEO_IRQ_FAST=1` admits the already-completed control write through the
existing exception-frame implementation and virtual stack switch. The assembly
return preserves the actual physical frame and registers. `VIDEO_IRQ_COUNTS=1`
adds a success counter for investigation; neither switch is enabled by default.

**MEASURED:** `host/native_video_irq_check.py` extracts the actual linked code
and debug layouts, then runs synthetic states on independent 68000/68020 CPUs.
26,336 cases pass, including 15,390 admissions and 80 late frame/quit races.
The matrix checks CCR/IPL, both virtual stack modes, stack/vector boundaries,
each explicit guard failure, exact stores, ABI/physical return, and deferred
clock accounting followed by fallback with no double charge. This is synthetic
CPU coverage, not evidence of every possible device-state combination.

**MEASURED:** the first A1200 cold live scenario completes all 24 inputs,
30 shuffle steps and 60 in-motion AY writes, with status 4, error/reset zero,
sampler off, and 1,019 shortcut admissions. Completed backs 25–27 take
40.512/42.304/42.432 ms using PAL frame/scanline observations. The accepted
release's earlier four backs took 41.536–42.304 ms. Different live hands and
frame placement prevent treating this as a controlled speedup; the 20 ms
card/audio target is still unmet.

The headless model/platform/native and linked short/feed regression suites pass.
Full ECS/AGA prototype replay, ECS live and VBI/audio gates remain pending;
there is no default activation. The read-only before/after measurement
below determines whether this prototype merits those remaining gates. Local evidence:
`tmp/video-irq-cpu4.log`, `tmp/video-irq-regression.log`,
`amiga/.run/video-irq-prototype`, and frozen
`tmp/perf/Pokeri-video-irq-prototype(.elf)`.

**MEASURED paired admission capture:** `amiga/video-irq-cost.gdb` measures from
`nativeShortControlPromote` to the original video handler, over starts 25–27.
There are 122 admissions in each run. Accepted/prototype median is 320/256 us,
but mean is 333.38/334.16 us, and total 40.672/40.768 ms. PAL scanline timing
has roughly 64 us resolution; these are coarse service observations, not
microsecond-accurate timing. The prototype has 79 entries at 256 us versus 16
before, but 14 at 448 us versus none before, plus larger outliers. This supports
cheaper admitted cases, not a net speedup across admissions. Both full scenarios
complete all 24 inputs without errors/reset. Local runs:
`amiga/.run/video-irq-cost-{before,after}`. A further refinement would need to
reduce rejection cost before this warrants activation or the remaining gates.

Normal builds have been restored and all allocated ELF sections match the
accepted release exactly. Startup fast-forward remains the already-validated
default; this experiment changes neither its policy nor gameplay pacing.

### Early scheduler rejection experiment (not retained)

**MEASURED:** moving additional refusal checks ahead of stack/vector/source
validation passes the same 26,336 linked CPU cases and a cold A1200 live24
(24 inputs, 30 shuffle steps, 60 in-motion AY writes, no errors/resets).
It admits 935 shortcuts. The 122 observed promotion-to-handler intervals total
60.032 ms, mean 492.07 us. One interval is 20.960 ms and therefore includes
scheduled/deferred work rather than just entry overhead. Excluding that interval
only as a separate diagnostic view gives 39.072 ms / 121 = 322.91 us; it is not
a replacement for the full measurement or a controlled overall speedup.
The median is 320 us. There are 60 entries at 256 us, 36 at 320, 16 at 384,
and four at 448. Compared with the first prototype, cheaper refusals trade off
against extra checks on successful cases; there is no compelling net benefit.
The added code was removed. The accepted normal build remains unchanged.
Evidence: `tmp/video-irq-early-cpu.log`, frozen
`tmp/perf/Pokeri-video-irq-early(.elf)`, and
`amiga/.run/video-irq-cost-early/gdb-out.log`.

**DERIVED next investigation:** the FIFO-control endpoint already computes
current video status and interrupt demand immediately before this promotion.
Rather than repeating eligibility checks in another general C function, inspect
whether a strictly single-boundary authorization can reuse those results.
It must be revoked on every fallback and boundary, and must still charge time,
recheck timer/frame work, preserve source priority and build the exact original
exception frame. No cached decision may survive guest execution or device
mutation. This is a proposed implementation investigation, not permission to
remove an IRQ or weaken any guard.
