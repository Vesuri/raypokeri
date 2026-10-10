# Remaining work

Updated 2026-10-10. This is the only current work queue; the experiment register
in [performance.md](performance.md) records outcomes, not tasks to repeat.

## Release status

Cleanup, alignment, documentation and RAY Pokeri 0.91 packaging are complete.
Host/linked suites, exact ECS/AGA replay, cold/warm live24 on both machines,
A1200 Double, VBI, actual Installer 43 and WHDLoad/standalone save checks pass.
ECS Double had no win in 12 hands; the user accepted AGA coverage instead of
another attempt. Archive fingerprint and qualification scope are in
[release.md](release.md). The user confirmed that version 0.91 fixes the coin-in
crash on their real A1200 with 68040/40 MHz, MMU and 32 MB RAM (2026-10-10).
The hardware retest gate is complete. Evidence and validation details are in
[rom-set.md](rom-set.md#real-a1200-coin-in-crash-uncovered-video-routine-2026-10-10).

## Remaining performance goals

- Complete cards within 20 ms and reduce late original AY writes. Current cold/warm
  complete-card medians are 37.568/34.368 ms; the longer Double scenario reached
  272.5 ms excess between sound batches. Drawing is now the main target. A new measured proposal is
  needed; T13 joined-handler work is complete, not deferred.
- Improve first-start and warm-start experience. The validated alignment fix's
  cold Ready is frame 793 versus 808 before cleanup. This frame count excludes
  early preparation/loading and must not be compared directly with whole-startup
  stopwatch times. SDL cached startup is not a native boot-performance target.
- Establish sustained timing across representative gameplay, reporting input,
  presentation and worst audio/drawing stalls as well as the aggregate ratio.
  Near-real-time A1200 session averages do not prove a stable 50 FPS game.

There is no further approved performance implementation queue behind these
outcomes. Choose a drawing-side proposal before resuming experiments. Timing,
FIFO/busy semantics and new fused interrupt boundaries require explicit decisions.

## Deferred or declined

- T12 guarded startup-artwork cache: declined; do not implement.
- A500/ECS performance tuning: deferred; compatibility checks remain necessary.
- Physical calibration of display, oscillator/noise balance, FIFO and peripheral
  signals; unvisited service/accounting paths remain fidelity limits.
- Yellow stripe phase behind left suits is original ROM behavior. A cosmetic
  correction is deferred. The coin uses the corrected general ellipse renderer,
  with no special artwork patch.

Cash payout/resumed play, queued short keypresses, PAL-time envelope decay,
WHDLoad save/quit support, compact guest windows and the renamed 0.91 packaging
are implemented. Help remains emergency quit without saving; Esc saves. The
precise save-and-exit timing gate was explicitly dropped. No backup files are
created under the current save policy.
