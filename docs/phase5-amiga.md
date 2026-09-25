# Phase 5: native Amiga devices

The original program executes on the 68000. Musashi remains a host reference;
no emulator, floating-point math or OS math libraries enter the Amiga build.
Phase 5 remains open: paired boot output and device persistence pass, but the
extended live continuation falls into a watchdog reset loop. Live controls
cannot yet be called validated.

## Planar video

`Surface` separates HD63484 semantics from storage. Host VRAM remains packed.
The Amiga allocates 512 KB of authoritative Chip RAM as four contiguous planes.
Packed RD/WT/MOD operations scatter/gather four pixels on the bus boundary.
Drawing, patterns and pixel reads work directly on the planes. There is no
chunky shadow or full-frame chunky-to-planar conversion.

Agnus accelerates clears, solid rectangles, horizontal/vertical lines and
aligned disjoint copies, including replace/OR/AND/XOR and edge masks. Patterned
and curved drawing, differently aligned copies and overlapping copies retain
the shared command algorithms against planar storage. Declining a fast path
preserves ACRTC overlap order. The startup blitter test covers masks/minterms
before the program touches VRAM. Drawing and display copies submit ordered
register records to the framework queue. Its bounded producer waits when full;
BLIT completion interrupts drain it while original instructions execute. CPU
VRAM reads/writes and software drawing synchronize before accessing pending
results; queued display reads also protect their source from CPU mutation.
The VBI publishes a pending frame only after its queued blits finish. Overlay
CPU writes, forced diagnostic captures and teardown synchronize explicitly.
The application owns the OS blitter and restores its previous interrupt handler.
BLIT-only interrupts do not force an extra native game trace.

The display blits the ACRTC's upper/base/lower screens and window into two
interleaved 576×283 four-plane buffers; the VBI flips the copper list. Source
rows 5 through 287 are visible. PAL DIW starts at `$1D91`, stops at `$38B1`,
DIWHIGH is `$2100`, and fetch spans `$44` through `$CC`. Geometry changes outside
the implemented format stop loudly. Palette candidate zero is reduced from six
to four bits per component; the physical board palette remains unconfirmed.

## Paula audio

Three Paula channels play short square loops at AY tone pitches. A fourth
plays shared noise; the noise buffer is refreshed in bounded batches. Envelope
control advances in batches using the original register writes and virtual
time, then updates Paula volumes at VBI. No oscillator is synthesized sample
by sample. All four channels are allocated through `audio.device` and released
on exit, following the hardware-loop approach used by Rescue on Fractalus.

Deliberate approximations: shared additive noise replaces the AY's bitwise
mixer, envelopes and register output are VBI-quantized, volume levels are an
approximation, and very high pitches clamp to Paula's safe minimum period.
These change sound fidelity, not CPU-visible AY registers. Audio verification
compares the entire ordered masked register stream; it is not an analogue or
by-ear hardware calibration.

## Boot and live timing

The user approved replay-paced boot because wall-clock MMIO service overhead
breaks the ROM's watchdog polling self-test. Boot still executes every original
instruction, memory test and drawing command. The replay supplies only timing,
external inputs and IRQ boundaries. At the recorded idle boundary, native
execution switches to VBI timing and live controls.

Each PAL VBI schedules two 10 ms board steps. An injected game handler completes
before the next step can overwrite its pending source. IRQ handlers execute at
safe points, never recursively inside an Amiga interrupt. Unsigned low-counter
deltas plus a 64-bit live elapsed counter allow cycle wrap. Live guard checks
cover 1 KB per serviced frame; diagnostic and exit checks cover all 512 KB.

## Controls and persistence

| Control | Action |
|---|---|
| Space / joystick fire | Deal / draw |
| Return / joystick down | Collect |
| B / joystick up | Bet |
| 1–5 | Hold cards |
| D | Double |
| Left / Right | Big / Small |
| C | Coin packet through the inferred external peer |
| F1 | Toggle service door and peer door-status messages |
| F2 | Service input |
| F3 | Toggle output-latch panel |
| Escape / left mouse button | Exit and restore the OS |

The provided FS-UAE launcher leaves joystick ports unassigned; configure an
emulated controller when using one.

The raw keyboard uses `ciaa.resource` while task switching is forbidden; its
previous handler is restored on exit. Joystick is the second physical port.
The optional panel labels latch rows 0–7, with bits 0–7 left to right. Physical
lamp assignments are still unidentified; the UI does not invent names.

`nvram.bin` contains exactly the 32 KB device mapped at `$D0000`. It loads before
takeover and saves after clean live exit through `nvram.new` and `nvram.bak`.
Malformed files stop. Diagnostic replay does not load or save it. This device
is unused by the covered game path: credits/books reside in work RAM. Persisting
those across new allocations is not implemented; this is not a promise of
credit retention. The coin/accounting/release policy remains Phase 6.

## Running

First build and stage the ROMs and 40.5-second boot replay using the procedure
in [Phase 4](phase4-preflight.md#reproducible-diagnostic-procedure). Then:

```sh
# Repository root; ignored marker selects replay boot followed by live play.
touch amiga/.run/dh1/native-live
cd amiga
. ./env.sh
make clean
make
./run.sh
```

Live mode is currently experimental because of the reset failure below. The
prepared normal run directory is left in diagnostic mode.

An empty `native-live` marker means no time limit. A four-byte big-endian value
sets a total virtual-cycle budget, including boot. Removing the marker selects
diagnostic replay, which exits at its recorded endpoint. Optional
`native-display` enables graphics in diagnostic-only mode. Target validated so
far: FS-UAE A500+, 68000, PAL, 1 MB Chip and 8 MB Fast RAM, Kickstart 3.1.
Boot single-steps roughly 40 million original instructions and is consequently
slow; replay optimization and reduced release memory requirements are open.

## Validation

`make harness-platform-check` tests planar packed-word readback and 100,000
logical pixel operations, plus all AY envelope shapes and period rewrites.
`make harness-check harness-native-check` retains device and instruction tests.
The Amiga build audits forbidden FPU/32-bit software arithmetic dependencies.

The native boot passed all 262,144 RAM bytes, 524,288 VRAM bytes, 163,008 cropped
pixels and 840 AY writes against the host. An initial live continuation passed
9.5 virtual seconds with an intact guard, no native error and restored vectors.
The real Agnus mask/minterm self-test also passed. See tagged measurements in
[ROM findings](rom-set.md#phase-5-native-timing-and-display-findings).

`host/native_check.py --live-boot --log LOG --ram CAPTURE` checks a hybrid boot
capture after clean exit. Add `--watchdog-stop` only for an intentional first-
expiry diagnostic: it accepts exactly one reset with `live watchdog expired`,
prints an explicit boot-only qualification, and still checks every RAM byte.
It never treats that stop as a live pass. `host/planar_capture_check.py --native PREFIX --host
PREFIX --log LOG` compares the paired VRAM, cropped frame and AY stream.
Use `GDBSCRIPT=platform-diag.gdb ./diag_run.sh 1800` from `amiga/` to
capture `tmp/native-platform-boot-*` and a final live screen. The script reads
state only. Capture native VRAM as 262144 big-endian words and the chosen screen buffer as
40752 words at `nativeBootReady`; do not compare unrelated live endpoints.

For opt-in local diagnostics, `native-test-inputs` sends a fixed deal/hold/draw/
double/coin/service/lamp sequence through the normal keyboard path after boot.
`native-test-wrap` moves only the service counter near its 32-bit wrap at the
handoff; it changes no CPU or game RAM results. Neither marker belongs in a
normal play directory. Add `native-stop-on-watchdog` for the next diagnostic:
it stops at the first live expiry and preserves the CPU context. The debugger
also prints the live reset count, first reset PC and elapsed cycles.

### Extended run: live gate failed

The final combined run repeats the exact boot RAM, VRAM, frame and AY results
at ROM `$2C6E00`, RAM `$27E77C`, guard `$306EB4`. It consumes all 22 scripted key
events, advances to 640,000,006 total cycles and 8,000 system edges, and ends with
low counter 315,934,464 after the forced wrap. However, it has only 44 additional
virtual IRQs, no additional HD63484 commands, and ends back in the original RAM
test at `$1224`, SR `$2704`. This is a failed live-play gate despite status 4,
a null native error, intact guard and restored vectors. The old bounded-exit
status did not expose reset loops; the new reset counter/stop option addresses
that diagnostic blind spot. The first expiry context has not yet been captured.

The 32,768-byte NVRAM fixture, with all byte values represented, survives load,
save and backup unchanged. An invalid eight-byte file is rejected before native
vector installation and remains unchanged. Paula allocation leaves its audio
interrupt bits disabled (`INTENA=$602C`). Evidence:
`tmp/phase5-final-comparison.log`, `tmp/phase5-final-native.log`,
`tmp/phase5-invalid-native.log`, `tmp/phase5-audio-irqs.log`.

### Timing investigation

The proposed change keeps PAL VBI for display, input and Paula updates, but
excludes time spent executing native device hooks from the board clock. Sample
elapsed time at service entry, restart its measurement at service exit, and
accumulate the original-code execution intervals with integer clock conversion.
Original game IRQ instructions still run and contribute elapsed time; no
watchdog bypass, CPU results or RAM results are injected. Replay boot stays as
validated. This would replace the current VBI catch-up timing policy. The user
requested measurements before choosing, with performance improvements first. The first-reset context and
an isolated live test still need to distinguish the precise failure trigger.
The alternative is to retain wall-clock timing and reduce service costs until
it leaves sufficient execution time for the original program.


The queued backend passes the early full-RAM/mask/minterm/queue-saturation/
interrupt-cleanup gate; full replay-to-live comparisons are in progress.
The control run omits forced counter wrap and scripted keys to isolate the
first watchdog expiry. The queued run uses the same inputs and clock policy.

`native-measure` opens `timer.device` before takeover and uses integer
`ReadEClock` observations only during live continuation. It never drives the
board clock. `platform-diag.gdb` prints the clock frequency, paired-read overhead,
elapsed ticks and seven inclusive categories: native service, board tick,
presentation, guard scan, AY envelope tick, AY VBI, and blitter wait. Native
services sample one call in 64; the remaining categories observe every call.
Nested totals overlap and must not be added. Subtracting paired-read overhead
is only an estimate; instrumentation itself has a cost. The low 32-bit elapsed
counter is intended for bounded runs shorter than one E-clock wrap (about
101 minutes in PAL). The timer resource is released on every cleanup path.


The first measured housekeeping improvements preserve the existing clock and
validation policy: exact short-operand integer multiplication, a 68000 word
scan for the guard, and display invalidation at control-register writes instead
of polling 256 bytes on every presentation check. In the short startup probe,
guard and presentation checks fall to approximately 0.96 ms and 0.26 ms after
subtracting the paired-read floor; these are not idle measurements. The optional
`native-test-guard` marker checks deliberate corruption at the scan boundaries
before installing native vectors. Full diagnostic and exit guard checks remain.

For comparisons against a saved build, `diag_run.sh` accepts `POKERI_EXE` and
`POKERI_ELF` overrides; supply the matching executable and symbols together.
Use a separate run directory, debugger port and capture prefix for each run.
An early full-RAM comparison also passes with `EXTRA_ARGS=--warp_mode=1`, which
removes emulator throttling while retaining emulated CPU/display timing. Full
boot checks for the optimized build are still pending.
