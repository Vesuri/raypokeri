# Guarded native video-interrupt admission

Status: investigation, not enabled. The accepted release is 33e60d3.
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
