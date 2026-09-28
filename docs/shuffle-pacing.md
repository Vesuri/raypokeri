# Sideways shuffle pacing

Status (2026-09-28): **implemented; enabled for normal SDL and bounded-clock
Amiga play**. The user approved VBlank waits at verified visible shuffle
boundaries, then approved excluding those added waits from watchdog time.
This is a presentation policy, not cycle-accurate HD63484 timing.

The original routine at $1DFA0 repeatedly clears/draws full cards and copies
partial cards. The partial-copy helper returns at $1E0C2. There are 15 verified
call sites, traversed twice. Waiting there preserves the original RTS registers
and condition codes, and avoids pausing after a transient clear. Existing
scheduler-paced window movements have no added waits.

The host supplies an authored branch-to-self at this instruction fetch only;
it never changes ROM storage. Musashi executes the wait and ordinary interrupt
handlers. Once the ROM's video producer/consumer pointers match and its command
is complete, it waits through the next 50 Hz presentation boundary. Optional
snapshot metadata preserves an in-progress wait. Legacy research snapshots
remain readable; clean startup caches are independent of the pacing option.

The native version replaces that RTS with a guarded Line-A hook. The ordinary
dispatcher delivers guest interrupts, presents the completed copy, and returns
only after its composed buffer has reached the Copper. If the completed image
was already presented, it holds that image through another VBI. Physical Amiga
interrupts stay enabled. It sleeps only when no pending list needs publication;
otherwise it would risk waking only inside the unsafe Copper publication window.
Added idle time contributes reference-time credit under the existing bounded
clock. Normal execution and graphic rendering remain fast.

## Watchdog and sound

The added waits hold the main loop that normally services the cabinet watchdog.
The first watchdog-on host prototype consequently reset after 21 steps.
The approved correction advances AY, serial, system and input clocks normally,
but excludes only presentation waiting from watchdog age. Normal execution
still ages the watchdog; its expiry and reset behaviour are unchanged.

The host exempts synthetic wait-branch cycles. Native clock delivery has existing
10 ms granularity: quanta delivered at the blocked return boundary, including
its release, are exempt. Time delivered at unrelated guest/IRQ locations is not.
This is a port policy, not a claim about the original cabinet's watchdog.

The ROM deliberately defers scheduled callback dispatch while a callback is
active ($C28–$C32). Continuing IRQ delivery preserves that rule; it does not
make sound-sequence callbacks reentrant. AY playback and hardware-register
updates continue, but physical sound/animation calibration remains open.

## Evidence

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

## Controls and correctness checks

Normal SDL play enables pacing. `--no-shuffle-vblank` disables it;
`--shuffle-vblank` explicitly enables it in research mode, which defaults off.
Normal bounded-clock Amiga play enables it; a `native-no-shuffle-vblank` marker
in the launch directory disables it. Legacy/corrected clock comparison modes
leave it off. Diagnostic replay executes the original RTS with its original
timing; an explicitly requested live continuation installs the hook at handoff.

The ROM identity, RTS and caller metadata are checked against the user's local
ROM. Generated guards and captures remain ignored; no ROM data is committed.
Synthetic board tests check that wait time advances peripheral clocks while
preserving watchdog age and that normal expiry resumes afterward. Host harness,
platform/native, short-hook and feed-hook checks pass. Native builds pass the
68000 arithmetic audit. Diagnostic RAM/VRAM/frame/AY comparisons remain required
on both chipsets. The final AGA/ECS builds pass exact equality for 262,144 RAM
bytes, 524,288 VRAM bytes, 172,064 cropped pixels and 30 AY writes. Evidence:
`tmp/shuffle-final-{aga,ecs}-compare.log`. Normal marker-free AGA startup and
SDL startup-cache reuse with either pacing setting also pass.
