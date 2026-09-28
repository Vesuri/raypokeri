# Startup-only fast-forward proposal

Status: proposed, not implemented or enabled. The current native clock remains
unchanged. This addresses the user's request that cold initialization be as quick
as practical SDL startup; it does not relax gameplay timing or graphics fidelity.

## Reason and scope

**MEASURED:** the register-resident feeder candidate reaches ordinary cold Ready
in 33.12 PAL seconds. Its separate instrumented refill interval consumes 10.08
board seconds and 26.29 wall seconds; initial artwork takes 13.19 wall seconds.
The current wall-bounded clock cannot finish that refill in less than its board
duration, regardless of faster rasterization. See native-rendering-followup.md.

**DERIVED:** SDL already runs original initialization and acknowledgment-driven
setup without a wall-time throttle, then opens live playback. A matching native
preparation mode needs a different time policy before Ready. It must not import
a manufactured ready snapshot, reserve balance, CPU state or sound sequence.

## Proposed behavior

1. Enter only for normal fast boot, before Startup::Ready. Keep hardware-test
   research runs, diagnostic replay and explicit clock comparisons unchanged.
2. Execute original initialization, the existing cabinet protocol and original
   accounting code. The retained-accounting path remains a fresh CPU boot and
   preserves the player's existing credits and interrupted-hand recovery.
3. Advance startup device time from bounded guest work and exact nominal hook
   cycles without waiting for PAL wall deadlines. Reuse the verified delay-loop
   CPU-state kernel only at its guarded original site, charging the skipped
   iterations' exact reference cycles. Never skip a loop with other side effects.
4. Split delay advancement at the next device/event deadline. Deliver original
   interrupts at the existing safe boundaries; preserve virtual IPL, source
   priority, acknowledgments and the rule that a pending handler returns before
   a new timer edge can replace its flag. Keep watchdog checks and loud failures.
5. During preparation, publish completed artwork at physical frame cadence,
   with the existing buffer/queue ownership rules. Do not wait for every virtual
   frame to be shown. Keep Paula's state current but begin audible playback at
   Ready, matching SDL's preparation behavior. This mute is startup-only; ordinary
   gameplay and normal run.sh remain audible.
6. At acknowledged Ready, clear startup-only timing debt/credit and establish the
   current PAL frame as the live clock origin. Enable the existing calibrated
   gameplay policy and audio; never replay a burst of missed startup frames or
   timer interrupts into play. Persistence and accounting formats are unchanged.
7. Retain an explicit comparison switch to use today's wall-bounded startup.
   No automatic fallback that hides errors or supplies a guessed ready state.

The implementation must first establish the next-event bound for the existing
startup scheduler. If that bound cannot be established, stop the experiment;
wall-time parity is not permission to guess device timing or overwrite game RAM.

## Acceptance

- Independent 68000/68020 checks for every admitted delay-loop boundary, PC,
  CCR, registers and exact reference-cycle charge, including interruption.
- Headless cold/warm accounting, nonzero credits, interrupted-hand recovery,
  corrupt-save handling, and watchdog/serial acknowledgment checks.
- Existing exact ECS/AGA native replay unchanged, and live cold/warm runs on both
  chipsets through real input, shuffle sound, saved accounting and heap cleanup.
- Compare first launch and retained-accounting launch separately, including
  executable preparation and Ready time. Report remaining native cost honestly;
  an M1 host's absolute startup time is not guaranteed for a stock Amiga CPU.
- Check the Ready transition for timer catch-up, audio bursts, delayed inputs and
  graphics ownership. Re-measure gameplay to show its timing policy is unchanged.

Changing startup's time policy requires the timing-contract approval described
in native-performance-plan.md. Approval would authorize a measured prototype;
only a faithful, validated improvement would become the default.
