# Live AY envelope clock

The rendering optimizations continue independently. This experiment would change
only the native audio output clock, not the original CPU, device interrupts,
register writes or board timing policy. The user approved benchmarking on 2026-09-29 and then explicitly directed
that live decay follow real time. The implementation enables PAL envelopes at
live Ready; `native-board-envelope` retains the old clock for comparison.
Validation results are recorded below.

## Evidence

**MEASURED:** fresh-accounting A1200 fast-FIFO ledger, `render-fast-cold`, reaches
Double on its first hand and completes without a fault/reset. One note has a
663.014 ms wall gap between register writes spanning only 100 ms of board time
(563.014 ms excess). Before the gap, all three voices use envelope volume;
the envelope period is 768 and shape is 9 (descending, then hold zero).
The backend's actual applied levels are:

| Wall ms after last write | Board ms | Applied level |
|---:|---:|---:|
| 18.52 | 30 | 13 |
| 58.46 | 40 | 12 |
| 98.48 | 50 | 11 |
| 318.84 | 70 | 10 |
| 439.19 | 80 | 9 |
| 599.27 | 90 | 8 |
| 658.92 | 100 | 7 |

**DERIVED:** in the current 1 MHz AY model, this envelope's full descent lasts
196.608 ms (16 levels × 768 × 16 AY-clock periods). Paula's oscillator DMA already
runs on PAL hardware time, while its volume envelope follows slowed board time.
That mismatch stretches the audible decay during command feeding. Moving the
envelope clock cannot make the original program produce its next note sooner;
any resulting silence must not be reported as reduced AY-write latency.

## Bounded experiment

- Add a native backend mode advancing the existing integer envelope
  state once per PAL VBI, using one frame's equivalent AY clocks.
- Original AY writes, period rewrites and shape restarts remain authoritative.
  No prerecorded song, inferred next note, sample mixer or injected writes.
- Keep preparation and diagnostic replay on the existing board-time clock.
  Enable the experiment only after normal live Ready, and retain a comparison
  switch. Preserve state at the handoff; quantify one-VBI onset uncertainty.
- Ensure shape restarts cannot race the VBI envelope update. No OS calls or
  per-sample work in the hot path.
- Compare all shapes/period changes with the existing scalar envelope over the
  same elapsed AY clocks. Verify register order/hash and original game state.
- Measure native applied volume at the actual Double note, including the time
  to silence, gap before the next note, VBI cost and missed-frame behavior.
  Report AY-write lateness separately. Retain only after user approval and
  measured improvement; this does not close command-feed performance work.

## Implemented and measured (2026-09-29)

Live Ready switches the Paula backend to one 160,000-board-cycle-equivalent
update per PAL VBI. Board ticks no longer advance this output envelope. The
existing phase/state is preserved, and original register 13 writes still restart
the shape. Register publication/restart briefly masks VERTB using INTENA (safe
from physical user mode); it restores only the previously enabled bit. There
are no OS calls or sample calculations. Preparation and diagnostic replay keep
board-time progression. The comparison marker is `native-board-envelope`.

**MEASURED:** `wall-envelope`, muted A1200 fast-FIFO ledger, reaches Double and
exits with error=0, watchdog resets=0. The same period-768/shape-9 envelope is
applied at levels 14,12,11,9,7,6,4,2,1,0 at 15.75,35.66,55.73,75.77,95.80,
115.79,135.82,155.85,175.89,195.92 ms after the last write. At silence only
90 ms of board time has elapsed. This fixes the stretched decay: the previous
board-clock example was still level 7 after 658.92 ms.

The next sound write arrives after **683.04 ms wall / 140 ms board**, or
**543.04 ms excess**. This is not a controlled command-feed speedup comparison:
live hands and interrupt phases differ. The approximately 487 ms silence after
the decay is still late game-generated sound, not an envelope bug. It remains
part of the rendering/command-feed performance work.

The stall contains 34 captured VBI samples with intervals of 19.87–20.17 ms;
no skipped VBI is evident in that interval. Onset is quantized to a VBI, and this implementation
does not reconstruct VBIs lost under prolonged interrupt masking. Audio scopes
are intentionally not timed by the ledger before the screen swap, so no claim
of a directly measured per-VBI cost is made from these logs.

Host tests cover all 16 shapes, period rewrites and restarts against the scalar
AY model, plus this specific 196.608 ms fade with only 100 ms guest progress.
The normal native build and no-software-multiply/divide audit pass.
The final ECS and AGA replays each match all 262,144 RAM bytes, 524,288 VRAM bytes,
172,064 pixels and 60 AY writes at 7,904,133 instructions / 64,000,000 cycles /
8,685 IRQs. Normal cold live24 completes at 480,000,000 cycles with all 24
inputs, zero errors/resets and restored vectors (`envelope-live`).
