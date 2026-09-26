# Normal-game startup

The user approved bypassing the coin-op startup hardware diagnostics on
2026-09-25/26, then requested the fastest acknowledged cabinet setup in both
SDL and Amiga. Normal launches use this policy automatically. Original game
logic, module loading, device initialization and graphics construction still
execute. The runtime watchdog remains active.

## Diagnostic bypasses

`src/native/BootPolicy.h` contains offsets and newly assembled patch operations;
original bytes remain only in the ignored native patch guards. Native loading
checks every affected instruction span. The host verifies the original chips.

| Location | Normal-game behavior |
|---|---|
| $10AE, $110C | Existing module-checksum bypass; keep loader/header checks |
| $121E | Keep original RAM clear, return success without destructive patterns |
| $1F2E | Pass PIA, AY, timer and watchdog startup tests |
| $25A4 | Return known read-only ROM mapping, also for later probes |
| $5B9C | Select the configured 512 KB display memory |
| $10F2C | Pass graphics readback checksum, omit its FIFO drain |
| $16E1C | Pass video-memory/external-board tests |

The unresolved FIFO model is unchanged. A newly reached low ROM marker write
is explicitly ignored only at its exact PC/address/width/value; it never writes
the Amiga's vectors. Details and evidence tags are in `rom-set.md`.

## Cabinet setup

`src/Startup.h` is shared by SDL and Amiga. It observes the original main loop,
door/refill/attention flags, serial state and accounting. It only supplies
external pin changes and existing protocol packets:

1. Wait for initialization to reach the main loop, then open the cabinet.
2. Wait for the door action and link to complete; exchange peripheral status.
3. Press Collect until the original program enters refill, then release it.
4. Supply 100 reserve coins. Each waits for its accounting increment and both
   sides of the serial transaction to finish, including queued meter traffic.
5. Close the cabinet, wait for that action, exchange status, and allow the
   close-door callback to finish before enabling play.

There are no fixed multi-second waits. The controller is checked at 100 Hz;
the existing serial peer retains its inferred 1 ms byte pacing. A stage with no
progress for 30 board-seconds fails loudly. Ready requires zero player credits,
100 reserve coins, enabled input, cleared attention and exited refill. No
accounting value, CPU state, outcome or framebuffer is injected by setup.

## Measured results

- SDL fresh startup: **8.12 board-seconds**, **2.90 host seconds** on this Mac
  with dummy video/audio; cached startup **0.11 seconds**. Earlier intermediate
  measurements of 5.31 board-seconds omitted complete outgoing-link/close-door
  completion and are superseded. The ready image shows credits 0, bet 1 and
  winnings 0; a real coin input increments credits through the ROM.
- A1200, K=1.5, PAL, 1 MB Chip/8 MB Fast, opt-in VBI sampling: ready at **11.87
  board-seconds / about 136 PAL seconds**. The prior diagnostic boot plus fixed
  setup took about 365 PAL seconds. First presented frame is at 0.60 PAL seconds;
  this is the initial display, not a complete ready image.
- The live run reaches 75 board-seconds with all 24 scripted key transitions,
  zero watchdog resets, no native/device error, intact guard and restored
  vectors. Ready RAM has credits 0/reserve 100; final RAM has credits 1/reserve
  102. The first 60 seconds of play still take **199.26 sampled PAL seconds**.
  Different startup timing changes the live workload; this is not a speedup
  claim over the earlier 179.80-second hand. Performance remains open.
- ECS replay with the same boot patches: **7,008,979 instructions, 64,000,002
  cycles, 7,831 IRQs**, every one of **262,144 RAM bytes identical** at matching
  addresses. It also passes the independent planar blitter stress test and
  restores vectors. Its recorded external inputs predate the final controller's
  extra link-idle checks; the comparison reuses those exact recorded inputs.

Local evidence: `tmp/fast-setup-sdl-normal.log`, `tmp/fast-setup-sdl-check.log`,
`tmp/fast-start-ready.png`, `amiga/.run/fast-start-final/gdb-out.log`,
`tmp/fast-start-play-profile.txt`, `tmp/fast-start-boot-profile.txt`, and
`tmp/fast-boot-replay-comparison.log`. Debug emulator audio is muted; normal
launch audio remains enabled.

## Research controls and checks

- SDL `--hardware-tests`: original hardware tests, no automatic startup cache.
- Research harness: tests remain the default. Use `--skip-hardware-tests` to
  select the fast policy and `--auto-setup` for automatic cabinet preparation.
- Amiga `native-hardware-tests` marker: retain the original startup tests.
  Normal launch needs neither this marker nor any replay file.
- Replay configuration records the chosen boot policy; historical replays keep
  their original path. `host/native_check.py --skip-hardware-tests` selects the
  matching comparison. Input scripts accept fractional milliseconds for exact
  recorded-cycle comparisons; `--auto-setup` can reproduce current setup.
- `host/sdl_play_check.py` verifies skipped test PCs, required initialization,
  clean/cached state equality, zero credits, coin input, unlimited running,
  arbitrary working directory and opt-in-only captures. Host device and native
  hook/clock/runtime tests pass. The 68000 build passes its arithmetic audit.


## Live cabinet messages use the same idle boundary

`src/CabinetInput.h` shares the evidenced outgoing-link-idle predicate with
startup. The SDL and native input frontends retain coin/status requests in order
until both peer and ROM are idle, then issue one application packet. Ordinary
buttons and the cabinet-door pin still update through their normal input path.
No accounting, CPU state or reply is supplied by the queue.

This fixes the same partial-transmit overlap found during rapid refill when it
occurs later in normal play. Before the fix, an instrumented default-clock run
and a faster-clock diagnostic stopped with `serial transmit checksum` around
service-door actions. Afterward both schedules complete 600,000,000 cycles with
all 24 key transitions, empty retained input queues, no error, zero watchdog
resets and restored vectors. Synthetic checks retain and order every packet
across all eleven observed busy/disabled-link conditions. SDL rebuild and a
silent fresh-start smoke run also pass. Source traces are recorded in
[rom-set.md](rom-set.md); native performance remains a separate gate.

With the current assembly handlers and graphics optimizations, K=1.5 reaches
ready at 96,800,000 cycles and completes the following 60 game-seconds in
**108.96 sampled PAL seconds**. The experimental K=37/16 run takes **101.54**;
it is neither a calibrated replacement default nor real-time acceptance.


The corrected counter-instrumented run also completes (63.14 board-seconds
after ready; 24 transitions; no transport error/reset; empty external queue).
However, the final frame still displays the ROM's `P2 87` attention code after
service actions. That behavior is a separate unresolved service-mode gate;
“no error” above refers to the harness/device error channel, not proof that the
ROM has returned to a playable screen. See `rom-set.md` for the identified
callback and the limits of that evidence.


### Live door acknowledgement correction

The P2 87 failure above is now reproduced and corrected, rather than dismissed
as native timing. Immediate status replies in the host play/door sequence reach
`$14E48` at 63.16 seconds; deferring the same replies avoids it through 80 seconds.
Live native and SDL controls now queue each door edge and wait for a main-loop
pass with the corresponding observed ROM door mode before sending status.
Transport-idle checks still apply to each packet; no fixed delay or game RAM
write is introduced. Synthetic tests cover old/new door flags, an unfinished
callback, unrelated PCs, busy links and rapid queued close/reopen actions.

`amiga/.run/door-ack-live/gdb-out.log` completes 85 board seconds, including all
24 scripted key transitions, with empty requests, zero resets/errors and restored
vectors. Its final capture shows the normal poker/pay-table display with one
remaining credit, replacing P2 87. ECS replay again matches all RAM/VRAM/pixels
and AY writes (`tmp/door-ack-replay-comparison.log`). The first 60 game-seconds
still cost 101.04 sampled PAL seconds; performance remains open.
