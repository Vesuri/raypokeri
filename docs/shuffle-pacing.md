# Sideways shuffle pacing

Status (2026-09-28): **consumer pacing integrated and validated**. Normal SDL and bounded-clock Amiga play pace the graphics consumer,
allowing the original sound scheduler to continue. This is an approved port
presentation policy, not cycle-accurate HD63484 execution timing.

The original routine at $1DFA0 queues full and partial card drawing. Its helper
returns at $1E0C2 from 15 verified call sites, traversed twice. That return now
records the original producer cursor and the display configuration, then executes
the original RTS semantics immediately. No sound call or game decision moves.

The shared fixed queue holds at most 32 markers, about 2.2 KB of ordinary RAM.
Each captures only the 62 control bytes read by scanout, plus its ring cursor;
no bitmap or command stream is duplicated. The original 4,008-byte command ring
retains its normal producer backpressure. Unknown layouts/cursors and marker
overflow are loud failures. Reset discards all markers and releases readiness.

At a marked consumed cursor, the video model withholds both WFR and WFE. The
original feeder returns through its ordinary not-ready path. Native assembly
promotes at the completed write boundary, preserving its PC, registers and CCR,
before another FIFO word can be sent. Its cached status mirrors the same model.
The guest main loop and IRQ/sound callbacks continue normally.

Each composed frame uses its producer-time display configuration. This matters:
the ROM changes window enable/position before the queued drawing finishes.
Applying its later settings to earlier frames changes the visible shuffle.
Guest-visible current registers remain current; the saved configuration is
exchanged only during composition and restored immediately afterwards.

SDL releases the held consumer after presenting the frame at its normal 50 Hz
boundary. The native path releases it only after the composed buffer has reached
the Copper; an unchanged image is held through another VBI. Ordinary presentation
is suppressed while shuffle markers remain, preventing intermediate clears from
being shown. Physical VBI and Paula updates continue throughout.

## Watchdog, sound and state

Consumer pacing needs **no watchdog exemption**. The original callback can return,
its existing sound selection runs, and the main loop can service the watchdog.
The old producer-pacing comparison alone retains its previously approved narrow
exemption. Neither path makes the original callback dispatcher reentrant.

The optional host snapshot extension SHV2 retains the marker queue, current hold,
display settings and selected policy. A snapshot saved under the legacy SHV1
producer policy requires the explicit producer comparison option. Existing clean
startup caches have no active shuffle extension and remain usable.

## Current validation

**MEASURED:** integrated headless consumer and producer runs execute the same
601 commands through the shuffle and produce all 30 byte-identical frames.
Consumer pacing produces 45 AY writes during motion, versus none under producer
pacing. A synthetic empty-ring relocation repeats this across a ring wrap.
Observed producer-boundary occupancy peaks at 2,288 / 4,008 bytes. A mid-shuffle
snapshot resumes to byte-identical full state. Normal watchdog settings remain
active and no reset occurs.

**MEASURED:** native live24 completes 24 inputs and all 30 steps without reset or
error on A1200/AGA and A500+/ECS. The actual retired screen buffers match all
172,064 reference pixels in each of the 30 frames on both chipsets. The measured
AGA run has 60 AY writes across 77 held VBIs; ECS has 30 writes across 162 VBIs.
These are workload observations, not a 50 FPS or physical audio calibration claim.
Evidence: `amiga/.run/shuffle-consumer-frames-{aga,ecs}/gdb-out.log`,
`tmp/shuffle-native-{aga,ecs}-frame-check.log`.

**MEASURED:** bounded queue tests cover overflow, reset, pointer guards, marker
wrap, display-state isolation and corrupt snapshots. Linked assembly checks cover
64 marker exits with every CCR on 68000/68020, plus the existing fused/whole-feed
instruction-boundary and register-preservation cases.

**MEASURED:** final diagnostic replay remains exact on ECS and AGA at
7,008,979 instructions / 64,000,002 cycles / 7,831 IRQs: all 262,144 RAM bytes,
524,288 VRAM bytes, 172,064 cropped pixels and 30 AY writes match. Full host
harness/platform/native checks pass and SDL compiles (no SDL runtime test was
launched). Evidence: `tmp/shuffle-consumer-{aga,ecs}-compare.log` and
`tmp/shuffle-final-checks.log`.

## Controls and reproduction

- Normal SDL: consumer pacing enabled. `--no-shuffle-vblank` disables it.
- Research host: `--shuffle-vblank` enables consumer pacing;
  `--shuffle-producer-vblank` selects the historical comparison.
- `--shuffle-frames` explicitly captures individual frames under the `tmp/`
  output prefix. Normal play produces no such captures.
- Amiga: `native-no-shuffle-vblank` disables pacing. Legacy/corrected clock
  comparison modes leave it off. Diagnostic replay keeps original timing;
  an explicitly requested live handoff installs pacing afterwards.
- `make harness-shuffle-check` verifies cadence and snapshot continuation.
- `python3 host/shuffle_consumer_probe.py [--wrap]` compares integrated host
  frames/commands and in-motion AY writes against producer pacing.
- `host/shuffle_native_capture_check.py` compares retired native screen captures.

## Historical producer-wait evidence

- **MEASURED:** the requested zero-credit SDL revision `0e23404`, using its own
  clean startup state, takes 324,940 emulated cycles / 40.625 ms wall for this
  shuffle. `09ae232` and the pre-wait current build take the same cycles, with
  SDL wall times 40.121 and 40.574 ms. The reported earlier slower SDL behaviour
  has **not been reproduced**. A removed frame wait or historical rendering
  slowdown has not been established as its cause. The user subsequently noted
  that their recollection of earlier correct SDL pacing may have been mistaken.
- **MEASURED:** with normal watchdog settings, the host completes 30 steps at
  20 ms intervals (within IRQ-entry jitter), with IRQ progress between every
  pair. A mid-shuffle snapshot resumes to byte-identical complete state.
  `make harness-shuffle-check` reproduces this in `tmp/shuffle-check-*`.
- **MEASURED:** native live24 passes on A1200/AGA and A500+/ECS: 24 inputs,
  30 completed shuffle boundaries, zero resets/errors, restored vectors.
  The waits span 93 and 158 physical VBlanks respectively. These are native
  workload measurements, not a 50 FPS claim or physical-machine calibration.
  Evidence: `amiga/.run/shuffle-visible-{aga,ecs}/gdb-out.log`.
- **MEASURED:** the first native prototype stalled at boundary 29 when no new
  damage remained to publish. The current-image/VBI path fixes that case;
  both completed live scenarios exercise it.
