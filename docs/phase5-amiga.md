# Phase 5: native Amiga devices

The original program executes on the 68000. Musashi remains a host reference;
no emulator, floating-point math or OS math libraries enter the Amiga build.
Phase 5 remains open: paired boot output and device persistence pass, but the
direct boot and a full coin/deal/hold/draw sequence now pass with ECS and AGA
fetches on A1200. Play remains far slower than real time; 50 FPS and physical
display/audio validation remain open.

## Planar video

`Surface` separates HD63484 semantics from storage. Host VRAM remains packed.
The Amiga allocates 512 KB of authoritative Chip RAM as four contiguous planes.
Packed RD/WT/MOD operations scatter/gather four pixels on the bus boundary.
Drawing, patterns and pixel reads work directly on the planes. There is no
chunky shadow or full-frame chunky-to-planar conversion.

Agnus accelerates clears, solid rectangles, horizontal/vertical lines and
shifted disjoint copies, including replace/OR/AND/XOR and edge masks.
Disjointness uses actual row intervals, allowing side-by-side card rectangles
whose enclosing address spans overlap. Solid-pattern detection considers only
its active pattern window, so unrelated artwork in pattern RAM cannot force a
per-pixel fill. Tall clears are split into bounded blits; overlapping-row
replace clears become contiguous masked fills.

Unzoomed PTN tiles up to 16×16 use a 64-entry planar cache (20 KB Chip RAM,
allocated once). Each tile has four colour planes and a mask plane. Four queued
A/B/C/D blits apply the mask and replace/OR/AND/XOR directly to VRAM. Cache keys
include pattern contents, colours, selected window, pointer, dimensions,
transparency mode and alignment; hits reuse the expanded data. Eviction drains
outstanding DMA before overwriting a tile. The ROM uploads patterns at runtime,
so expansion happens on first use, rather than speculatively decoding ROM data
at startup. No cache or pixel buffers are allocated per draw. The native ECS
self-test covers all alignments, colour modes and logical operations, repeated
hits and eviction with queued DMA.

Other patterned and curved drawing, unsupported scan directions and overlapping copies retain
the shared command algorithms against planar storage. Declining a fast path
preserves ACRTC overlap order. The explicit replay startup blitter test covers masks/minterms
before the program touches VRAM. Drawing and display copies submit ordered
register records to the framework queue. Its bounded producer waits when full;
BLIT completion interrupts drain it during both original execution and device services. CPU
VRAM reads/writes and software drawing synchronize before accessing pending
results; queued display reads also protect their source from CPU mutation.
The VBI publishes a pending frame only after its queued blits finish and only
in scanlines 0..7. A late VBI must not restart the Copper mid-picture. Overlay
CPU writes, forced diagnostic captures and teardown synchronize explicitly.
The application owns the OS blitter and restores its previous interrupt handler.
Live BLIT interrupts pause the guest clock and arm a return trace, as do VBI and CIA interrupts; the original OS handlers remain chained.

The display blits the ACRTC's upper/base/lower screens and window into two
interleaved 576×283 four-plane buffers; the VBI flips the copper list. Each
list is built by AmigaScreen::prepare with fixed pointers to its own buffer.
Pokeri.cpp also allocates a tiny black-screen fallback list. Unaligned moving
windows use queued masked blitter shifts, with a bounded CPU edge fallback. Source
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

## Temporary A1200 bring-up

User decision (2026-09-25): use `AMIGA_MODEL=A1200` until native gameplay works,
then return to A500 performance. All three launchers now default to A1200;
`AMIGA_MODEL=A500+` selects the original performance target. Debug audio remains
muted and normal `run.sh` retains sound. The binary still targets 68000 instructions.
Physical exception frames adapt to Exec CPU flags: six bytes on 68000;
eight-byte format 0 and twelve-byte trace format 2 on 68020. On 68010+, a
private Fast RAM copy of the complete vector table avoids Chip RAM vector-fetch
contention. The original VBR is restored, and relocated code is cache-flushed.
The original game retains its virtual six-byte 68000 frames. An early A1200
replay matches every RAM byte at 87,899 instructions. Direct ECS-fetch and
AGA-fetch A1200 runs both complete 76.5 virtual seconds including coin, Deal,
Hold and Draw, with no native fault and restored vectors.
Three audited short boot-poll intervals now use their exact unhooked instruction
cost when consecutive loop counters prove the path; their complete instruction
bytes are checked before running. This avoids sub-CIA-tick rounding without
skipping tests. Wider DMA fetches are conditional on actual AGA identification:
public Alice/Lisa flags or matching hardware IDs. AGA uses FMODE=3, DIW
$1D81/$38A1 and DDF $38/$B8; OCS/ECS retains the original 16-bit fetch layout.
The measured A1200 Kickstart reports only $13 in ChipRevBits0, so the hardware
ID fallback is necessary. CPU type alone never enables AGA.

## Boot and live timing

Normal startup runs directly from reset without SHA hashing or replay files.
It now skips the coin-op hardware diagnostics under the user-approved
[startup policy](startup-policy.md), retaining initialization. SDL and Amiga
share acknowledgement-driven cabinet setup; the fixed 40.5-second schedule
has been removed.
The planar/blitter stress test remains diagnostic-only. The earlier blank-screen
reset loop was measured before the current clock changes: direct boot has since
passed the ROM watchdog test before the new bypass policy, completed cold setup
and accepted coin/Deal.
The corrected moving-window path now completes a hand and accepts later inputs.

A reserved CIA-A timer A brackets original execution; native device services
and Amiga level-2/3/6 interrupt handlers are excluded from measured guest time.
The approved option-C policy supplements this with wall time bounded by recent
guest throughput; see [native-clock.md](native-clock.md). Device services permit
Amiga interrupts while the guest clock is paused; only exception-state
transitions remain masked. Original handlers never run inside an Amiga ISR. An independent NOP/Line-A
calibration measures transition cost. Original hooked-opcode cycle costs are
host-generated metadata from Musashi; no CPU emulator is linked natively.
Bitplane DMA starts only when the first real frame is ready. A second calibration
accounts for display-enabled bus contention. This is an estimated native guest
clock, not a measurement of the original board oscillator or a 50 FPS claim.

Normal cold startup supplies SDL's external door/Collect/refill/status sequence:
100 reserve coin events and no player-credit events. The original ROM does all
accounting. Controls become available after that sequence; Escape still exits
during setup. Both live sequences confirm zero player credits and 100 reserve coins at ready.

Replay-paced boot remains available explicitly for diagnostic comparisons:
`POKERI_REPLAY=1` in the launcher creates `native-replay`. With `native-live`
also present it switches from replay to live execution at the recorded endpoint.
Without `native-live`, explicit replay exits at its endpoint.

Guest execution accumulates 10 ms board steps; PAL VBI drives inputs, Paula and display flips. An injected game handler completes
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

Build and stage the four ROMs under `amiga/.run/dh1/rom/`, then:

```sh
cd amiga
. ./env.sh
make
./run.sh
```

Normal `run.sh` removes the `native-replay` marker, keeps audio enabled and needs
no `replay.bin`. It performs file-size and patch-site comparisons, but no SHA-256
calculation. ROM hashes are verified on the host by `make roms-check` and native
table generation. The ROM is loaded directly into the aligned Board allocation;
there is no second 256 KB ROM image/copy or pre-clear of ROM storage. Twelve
original vector bytes are retained for the audited low-vector sentinel reads.
The packed video buffer was already absent in freestanding builds. RAM, device
guard and visible bitplanes still receive their required initialization.

`debug.sh` and `diag_run.sh` select replay explicitly by default and remain
silent; use `POKERI_REPLAY=0` to debug direct startup without a replay file.
`POKERI_REPLAY=1 ./run.sh` explicitly enables replay in the normal launcher.
The existing `native-live` marker requests continuation after diagnostic replay;
its optional four-byte budget retains the previous semantics. `native-display`
enables graphics for replay-only diagnostics. The slow single-step boot is
confined to explicit replay. Neither mode is claimed to achieve 50 FPS gameplay.

## Validation

`make harness-platform-check` tests planar packed-word readback and 100,000
logical pixel operations, plus all AY envelope shapes and period rewrites.
`make harness-check harness-native-check` retains device and instruction tests.
The Amiga build audits forbidden FPU/32-bit software arithmetic dependencies.

The native boot passed all 262,144 RAM bytes, 524,288 VRAM bytes, 163,008 cropped
pixels and 840 AY writes against the host. An initial live continuation passed
9.5 virtual seconds with an intact guard, no native error and restored vectors,
but its saved emulator log shows maximum CPU speed and disabled CPU/memory/
blitter cycle accuracy. It is not an A500-speed performance pass.
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
that diagnostic blind spot. The first expiry context is now captured in the isolated controls below. The previous live pass used
CPU/chipset acceleration overrides absent from this failing run; this confounds
the comparison with input/wrap integration. Current controls keep default A500+
CPU/chipset timing and accelerate only playback with warp mode.

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
requested measurements before choosing, with performance improvements first. The isolated controls below now reproduce the first expiry without test inputs
or forced counter wrap, including an unprofiled baseline.
The alternative is to retain wall-clock timing and reduce service costs until
it leaves sufficient execution time for the original program.


The queued backend passes the early full-RAM/mask/minterm/queue-saturation/
interrupt-cleanup gate and full boot comparisons; the live continuation fails
at the first watchdog expiry as detailed below.
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
boot comparisons now pass; the live timing gate still fails as detailed below.


The full queued run exposed a framework Boolean ABI defect before completing
boot: the assembly busy helper returns `$FF`, while GCC `bool` requires `1`.
An out-of-line idle test could consequently allow a Copper restart during a
blit. The wrappers now normalize raw bytes. A dedicated busy-with-empty-queue
hardware check rejects the old wrapper and passes the fix; strict early RAM
comparison still passes. Full corrected boot comparisons also pass; the live
continuation still expires, as detailed below.
Debug launchers now discard host audio through SDL's dummy driver; emulated
Paula stays active, and normal `run.sh` retains audio output.


### Default-speed watchdog controls

Both controls pass the entire boot RAM/VRAM/frame/AY comparison, then expire at
450 ms of live time. The baseline has no profiler; the corrected queued build
has timing observations. No live test inputs or forced wrap are enabled. Both
advance the main-loop delay counter just once before expiry; stopped PCs are
inside original user-mode callback execution. Guard and vector restoration pass.
No graphics command count changes and there are no measured blitter waits.
The AY tick/VBI observations total about 23 ms of the 452 ms instrumented
interval including observation overhead; per-call service sampling is too
sparse for a reliable aggregate. Full evidence and limitations are in
[ROM findings](rom-set.md#first-live-watchdog-expiry-default-speed-controls-2026-09-25).

Historical decision point (superseded by the service-excluded clock above): retain PAL VBI for display,
keyboard and Paula, but either (a) accumulate the original code's execution
intervals between native service calls for board time, excluding the service
bodies, or (b) retain wall-clock board time and optimize the CPU/hook path until
the original callbacks/main loop make sufficient progress. Option (a) retains
original game IRQ execution, watchdog checks and original instructions; it does
not inject CPU/RAM results. It can make game time slower than wall time when
service work is expensive. That investigation now uses the service-excluded CIA clock described above.
Phase 5 remains open; Phase 6 has not started.

### Current performance qualification

Interrupt-enabled services keep the VBI, Paula and blitter queue running while
original board time is paused. They do not make expensive device operations
instantaneous. Before the final side-by-side copy optimization, the instrumented
ECS-fetch A1200 sequence spends approximately 58% of its PAL machine time in
video bus services; Paula tick/VBI work is under 1%. The slowest video service
lasts about nine seconds. Startup also executes 78,204 RD commands through
hardware-access hooks. The AGA run is faster but still far from real-time play.
See `docs/rom-set.md` for counts, test scope and local evidence files. Normal
runs leave measurement disabled and retain audio; debug launchers remain muted.

### Pattern/fill optimization validation (2026-09-25)

A command-level profile identified solid RFRCT and the initial CLR as major
software fallbacks. Before these fixes, the first 41 RFRCT commands consumed
27,623,138 E-clock ticks (about 39 seconds), and one CLR consumed 5,299,536 ticks
(7.47 seconds). Native measurement can now report `NativeTiming::videoCommands`
by opcode group; it remains opt-in through `native-measure`.

`make harness-check` and `make harness-platform-check` pass. Synthetic PTN tests
compare the cached planar expansion to independent per-pixel expectations,
including wrapped pattern windows and nonuniform colour words. The deterministic
40.5-second host scenario remains byte-identical in CPU, RAM, VRAM, palette
indices, board state and NVRAM after the scanline-visited PAINT optimization.
The normal 68000 build passes the no-software-mul/div audit. An experimental
LTO build was tested but is not enabled in the Makefile or delivered binary.

The ECS replay passes the real blitter self-test (4,305 submissions), restores
vectors, and matches all 262,144 RAM bytes at 876,360 instructions / 8,000,002
cycles. Evidence: `tmp/cache-regression-driver.log`,
`tmp/cache-regression-comparison.log`. These correctness gates do not establish
50 FPS; curved drawing, packed bus access and native hook overhead remain.

The unprofiled A1200 full-hand run also completes: 612,000,000 virtual cycles,
all 24 scripted input transitions, zero credits at ready, later credits 1 /
reserve 102, 376 presented frames, no native error, and restored vectors.
There is only the expected startup watchdog reset. Its 41,787 PAL VBIs compare
with 51,429 in the preceding active-window-fill/scanline-PAINT build (about 19%
less emulated machine time after adding the tall-clear and tile-cache paths).
That is still about 836 seconds of PAL time for 76.5 seconds of board time,
not real-time play. Evidence: `amiga/.run/cache-live/gdb-out.log`,
`tmp/cache-live-driver.log`, captures under `tmp/cache-live-*`.

### Startup cost clarification

The current-build startup profile recorded in `docs/rom-set.md` reaches only
2.62 virtual seconds in about 326 PAL seconds. Its completed drawing commands
account for about 17.4 seconds, while inclusive video-bus services account for
75.7 seconds and Paula for 1.7 seconds. Thus drawing/audio alone do not explain
startup. The general exception/access-hook path and its service-excluded clock
need attention before further small graphics optimizations. In live mode the
`nativeInstructions` label is misleading: it counts service dispatches (592,990
in this sample), not all original CPU instructions. Normal native boot also
runs the 40.5-second cold-setup schedule unconditionally, including with loaded
NVRAM. Warm-start handling remains open; shifted blitter copies are implemented below.

Review correction: that profile made about 982,000 `ReadEClock` calls. Roughly
60 of its 325.5 seconds and about a third of each timed video access are
observer cost. The sampled service record attributes about 291 s to the C
dispatch interior (≈330 µs per dispatch without the observer). The checksum
plus its FIFO drain at `$11030` account for 64% of the dispatches. Guest board
time also runs at 88.7% of measured time because E-ticks are scaled ×10
(7.09 MHz) against an 8 MHz board clock. Under this service-excluded clock,
real-time play is unattainable at any non-trivial service cost; the timing
contract was subsequently revised under approved option C in `docs/native-performance-plan.md`.

Current low-overhead measurements and reproduction instructions are in
[native-profile.md](native-profile.md). Old per-access ReadEClock scopes have
been removed. FIFO hypotheses are isolated host experiments; production
FIFO semantics remain unchanged. The live clock was subsequently revised under
approved option C; historical timing results below retain their stated policies.

### Shifted blitter copies (2026-09-25)

The native copy backend now supports every relative horizontal alignment for
non-overlapping forward rectangles. A masked leading column preloads the B
shifter for left shifts. A shared 2,112-byte Chip RAM mask table supplies the
first real word's mask; first/last masks preserve surrounding pixels. Prefetches
are bounded by both allocations. Screen composition uses the same queued path;
each screen allocation has eight leading bytes to retain AGA pointer alignment
and provide a safe preserved prefetch word. OCS/ECS support is unchanged.

ACRTC sequential overlaps, unsupported directions, oversized rectangles and
VRAM-edge prefetches retain their correct fallback. `copyRejectedBounds`,
`copyRejectedOverlap`, `shiftedCopies` and `displayBlits` expose those decisions.
Diagnostic tests cover all 256 alignment pairs, eight widths across word
boundaries, four logical operations, independent packed-pixel expectations,
interleaved display pitch, blanking, preserved edges and queued dependencies.
The ECS blitter passes; the early ECS replay still matches all 262,144 RAM bytes
(`tmp/shift-replay-comparison.log`).

A subsequent A1200 option-C test with conservative K=1 completes the 76.5-second
boot/play script, all 24 input transitions and 379 frame swaps, with one expected
watchdog reset, no error and restored vectors. It submits 808 surface copies
(576 shifted) and 1,337 display-region blits; no admitted copy is rejected for
bounds or overlap. This excludes directions rejected before the Surface call.
It takes 584.47 PAL E-clock seconds and remains far from real time; the clock
and access-path changes in that run prevent attributing the overall time change
to the blitter alone. Local evidence: `amiga/.run/clock-c/gdb-out.log`.


### Bounded clock and assembly status checkpoint

Normal live timing now uses approved option C with corrected E-clock units and
bounded recent guest/wall-time credit. Paired boot phases select K=1.5 rather
than the functionally passing but over-budget K=2. The real Line-A status path
is assembly-only and preserves the exact shared-model result. The ECS replay
matches all RAM through checksum and FIFO drain, and the default completes
cold setup plus 60 board-seconds of scripted play with one expected reset,
all 24 inputs, no native error and restored vectors. That play interval still
takes 179.80 PAL seconds; general C dispatch remains costly. Startup policy and
real-time acceptance remain open. See [native-clock.md](native-clock.md) for
the calibration, regression evidence and retained comparison modes.

## Guarded compare/test assembly (2026-09-26)

Five frequent comparison/test sites now read owned RAM/ROM or their admitted
null-vector values in assembly, with aligned whole-longword bounds checks and
preserved registers/CCR. They make no C++ call until a scheduling boundary.
The earlier 15.18% call share includes real RAM reads; optimizing only null
vectors did not help. The actual bounded-memory implementation raises short
accesses from 52,576 to 98,935 in the live test and reduces its first 60 game
seconds from 199.26 to 179.06 sampled PAL seconds. Startup remains about 135
PAL seconds. This is not real-time gameplay and schedules differ slightly.

All 24 input transitions complete with zero resets, no errors, intact guard and
restored vectors. ECS replay matches all 262,144 RAM bytes at 7,008,979
instructions; 8,192 assembled flag cases and 112 address-guard cases also pass.
See [native-clock.md](native-clock.md) for exact scope and evidence. Dispatcher,
clock and virtual CPU-control/PIA work remain the next performance targets.

## Exact curve and lamp-panel reductions (2026-09-26)

The updated gameplay profile identified wide curve products and the optional
lamp panel as measurable costs. Curve cross/dot products now use native signed
16×16 multiplication when both operands fit, retaining a 64-bit result and an
exact wide fallback for larger coordinate differences. No curve ordering,
clipping, rounding or pixels change. The lamp panel now writes masked planar
words and short spans instead of performing four Chip RAM read/modify/writes
per pixel; it allocates no extra buffer.

The pixel oracle checks 512 panel configurations, every latch byte, all palette
pairs, and untouched edge/full-buffer contents. The native ECS paired capture
also matches all 262,144 RAM bytes, 524,288 VRAM bytes, 163,008 cropped display
pixels and 30 AY writes after 7,008,979 original instructions. Build arithmetic
checks and host drawing tests pass. Evidence:
`tmp/graphics-hot-check.log`, `tmp/hotpaths-replay-comparison.log` and
`amiga/.run/hotpaths-replay/gdb-out.log`.

Together with the contemporaneous interruptible CPU-control/PIA short paths,
the first 60 game-seconds take 118.02 sampled PAL seconds (previous PIA-path
measurement: 132.62). That combined comparison does not isolate graphics alone
and is still not real-time acceptance.


### Native handler progress, 2026-09-26

The additional guarded assembly SR/RTE, PIA/ACIA and TRAP paths reduce the
measured 60-board-second play interval from 179.06 to 108.58 sampled PAL seconds
at the unchanged K=1.5. Ready takes 87.06 sampled PAL seconds. The shared models
remain in use and every exceptional operand/privilege/stack case has a checked
fallback. Actual assembled instruction tests and the ECS full RAM/VRAM/frame/AY
replay pass; see [native-clock.md](native-clock.md) for counts and capture paths.

This does not meet the real-time or animation deadline gates. A separate
counter-instrumented live run exposed a serial packet collision during cabinet
input; zero watchdog resets alone is not enough to accept the scenario. Normal
input pacing and the next command-write decision are tracked in the performance
plan. The default clock has not been raised and normal audio is still enabled.
