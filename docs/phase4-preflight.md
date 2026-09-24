# Phase 4 — approved design and verified native diagnostic execution

Phase 4 is complete under the user-approved diagnostic scope (2026-09-25).
Native boot and the strict full-RAM gate have passed. Live VBI-paced boot has
not passed; the user approved moving that gate to Phase 5 with the Amiga device
backends. Initial preflight findings, resolved decisions and results follow.

## Initial shared-model build findings (resolved)

The initial build with m68k-amiga-elf GCC 15.1.0 could not compile `src/board/Board.cpp`:
`<array>` is unavailable. Its library lookup also finds no `libstdc++.a` or
`libgcc.a`. The existing Amiga build is freestanding, disables exceptions, and
rejects software 32-bit multiply/divide helpers.

At preflight, the shared board used STL containers, exceptions in state handling,
64-bit timing products/divisions, and floating-point drawing/audio math. Linking
it unchanged required more than build-system changes. The implementation added
freestanding storage/math/runtime facilities,
with the same shared device semantics and host regression tests. Do not clone
or replace the board with a second, guessed Amiga device implementation. Do not
remove the arithmetic audit or introduce Musashi into the Amiga build.

The original compile probe omitted the compatibility headers now supplied by
the Amiga Makefile. For comparison, after sourcing `amiga/env.sh`:

```
m68k-amiga-elf-gcc -m68000 -std=gnu++17 -fno-exceptions -fno-rtti \
  -fsyntax-only src/board/Board.cpp
```

The full diagnostic is local in `tmp/phase4-board-compile.log`.

## Timing and the RAM-equality gate

The host advances Board after each original instruction by Musashi's cycle
count, and tests its pending IRQ before executing the next instruction. Phase 4
instead describes physical VBI timing and interrupt delivery at eligible game
PCs or the next hook exit. Native device-service overhead changes when a VBI
occurs relative to game instructions. Matching the number of delivered
interrupts alone does not fix the interrupted PCs, elapsed virtual cycles,
stack contents, or timer/RNG work performed between interrupts.

The original plan did not specify how these schedules would be identical for its
byte-for-byte RAM gate. A comparison that ignores timer, stack or RNG bytes
would weaken the agreed gate and must not be silently substituted.

### Approved decision: separate diagnostic scheduling from live pacing

Keep the original per-instruction host reference and VBI-paced native operation.
Add a diagnostic-only native run that delivers inputs, watchdog resets and
virtual interrupts at the host-recorded original-instruction boundaries and
advances the shared board to the corresponding reference cycle boundaries.
The 68000 still executes the original game instructions; diagnostic tracing
must not interpret them or import game results from a capture. Reference event
records and dumps remain in `tmp/`. Compare complete RAM at an agreed boundary,
using identical native/host allocation bases so no normalization is needed.

This proves native execution, hook semantics and shared-device behavior
under an identical schedule. Phase 5 must separately verify live VBI operation
reaches idle,
deferred interrupts cannot reenter services, guard memory remains intact, and
all owned vectors/OS state are restored on exit. The diagnostic replay must
not be presented as proof of cycle-accurate live VBI timing.

The user approved this scheduling addition. The
alternative is to require the live native run itself to reproduce the host's
instruction/cycle schedule, which is a substantially larger timing design than
the current VBI-based Phase 4 description.

## Approved physical-user-mode execution

The user additionally approved running the original instructions in physical
user mode with virtual guest SR/IPL, SSP and USP. Native Line-A/trace exception
frames then land on a private service stack, rather than overwriting bytes
below the game stack and invalidating the full-RAM gate. CPU-control hooks
preserve the original program's virtual supervisor/user transitions. Native
TRAPs and injected IRQs build the guest exception frames in its RAM explicitly.
This supersedes Phase 4's original physical-supervisor-mode requirement.

## Validated native implementation

The Amiga executable now links the shared Board with a small freestanding C++
container subset. It contains neither Musashi nor floating-point/OS math-library
calls. HD63484 curves use an integer implicit ellipse and cross-product angular
ordering; AY volume uses the same fixed DAC approximation. The 40.5-second host
reference remains byte-identical in CPU state, complete RAM, device state,
coverage and final frame after the integer conversion. Synthetic hook execution
is compared against Musashi for every implemented addressing form and CCR bit.

The loader verifies SHA-256 of all original chips before relocation or patching.
`host/tables/cpu-control-hooks.csv` records the additional covered SR/USP/RTE
sites as instruction locations and operation names, without ROM bytes. Generated
native tables live in ignored `amiga/generated/` and no longer depend on a local
disassembly file. Original operands are read from the verified image at runtime.

Exec's Supervisor transition must run with its original privilege vector. Owned
vectors are installed only after that transition, and restored before returning
to Exec. The trace handler also handles the extra supervisor trace observed at
68000 TRAP entry without counting it as an original instruction. A level-3 shim
chains Exec's existing handler and arms one trace on return to physical user
mode; this lets the live path deliver deferred virtual IRQs even in a game loop
that has no device hooks. Services run with interrupts masked and cannot reenter.

Two native modes currently use a local `replay.bin` in the executable directory:

- Default diagnostic mode traces original instruction boundaries, reproduces the
  host's external input/interrupt schedule and stops at its recorded endpoint.
- An empty `native-live` marker selects VBI pacing. It consumes only the external
  input records at their emulated times, ignores reference bus/IRQ scheduling,
  and continues until the left mouse button is pressed or a guard stops it.
  A four-byte big-endian cycle budget in that marker instead enables a bounded
  live diagnostic (status 4), distinct from successful replay completion (2).

There is still **no native display or Paula output**; those are Phase 5. A blank
Amiga screen in this phase does not demonstrate game boot. Use counters, the
completed replay marker, and the full-RAM gate below. Both the early 100 ms checkpoint and the full 40.5-second attract checkpoint
match every RAM byte and restore the owned vectors. The full run executes
40,477,629 original instructions, advances 324,000,006 reference cycles and
delivers 21,267 virtual IRQs, ending at original PC `$2442`, SR `$2000`.
A replay with a deliberately incorrect final PC stops loudly and also restores
the vectors. Live-mode validation is deferred to Phase 5. The full 512 KB scan
starved live execution during RAM initialization. The user
approved incremental live scanning: 1 KB per serviced frame, wrapping after
512 frames (10.24 seconds at 50 Hz without service backlog), with full scans
retained for diagnostic replay and exit. The incremental scan does not resolve
the live starvation: repeated samples now stop in AY reference synthesis,
with zero virtual IRQs and repeated guest startup resets. The user approved
moving the live gate to Phase 5 rather than requiring reference-model
optimization to close Phase 4. No audio state or watchdog behavior is bypassed.
The bounded live test exits normally at 400,000,000 virtual cycles (50 seconds), status 4,
2,503 service/trace boundaries and zero virtual IRQs, at original PC `$21D8`.
The full exit guard check passes, native error is null and owned vectors are
restored. Evidence: `tmp/native-live-incremental.log` and
`amiga/.run/native-incremental/gdb-out.log`.

### Reproducible diagnostic procedure

From the repository root, with verified chips in `rom/`:

```
make harness harness-native-check
build/pokeri-host --devices --serial-peer --system-hz 100 --input-hz 50 \
  --watchdog-ms 400 --watchdog-reset-us 50000 --ay-clock 1000000 \
  --palette-rom 0 --stall-instructions 100000000 --bypass-module-checksums \
  --io-table host/tables/io-accesses.csv \
  --inputs host/scenarios/relocation-play.inputs --ms 40500 \
  --record-replay tmp/native.replay --out tmp/native-reference
mkdir -p amiga/.run/dh1/rom
cp rom/77POK30 rom/77POK38 rom/77POK34 rom/PARA200J amiga/.run/dh1/rom/
cp tmp/native.replay amiga/.run/dh1/replay.bin
```

Ensure there is no `amiga/.run/dh1/native-live` marker for this diagnostic. Then:

```
cd amiga
. ./env.sh
make clean
make
DEBUG_PORT=42377 FSUAE_RUN=.run/native-process \
  GDBSCRIPT=native-diag.gdb PROGRESS_INTERVAL=30 \
  EXTRA_ARGS='--uae_cpu_speed=max --warp_mode=1 --uae_blitter_cycle_exact=false --uae_cpu_memory_cycle_exact=false --uae_cpu_cycle_exact=false' \
  ./diag_run.sh 3600
cd ..
python3 host/native_check.py
```

The comparison tool requires successful native completion and reruns the host
at the **actual native allocation addresses**. Thus every RAM byte, including
saved pointers and residual stack fragments, is compared directly without any
mask or normalization. Reference timing records contain instruction counts,
PCs, cycles, external inputs and interrupt identities, never game register or
memory values to inject. All captures and original images remain ignored/local.

The trace fast path still checks every executed PC and counts every original
instruction. It preserves the hardware trace frame and enters the full C
handler at recorded boundaries, hooks and TRAPs. It neither predicts instructions
nor skips their native execution. Each diagnostic snapshots its executable
symbols and GDB script before launch, so later source edits cannot change a
running capture.

## Verified result (2026-09-25)

The FS-UAE A500+ diagnostic used a 68000, 1 MB Chip RAM and 8 MB Fast RAM.
At ROM `$4C1100`, RAM `$278A54` and guard `$501194`, the host rerun matches
**all 262,144 native work-RAM bytes**, without ignored ranges or relocation
normalization. Native error is null, the guard is intact and vector restoration
is verified. Local evidence: `tmp/native-probe-final.log`,
`tmp/native-amiga-ram.bin`, `tmp/native-full-comparison.log`.

The cross-build arithmetic audit passes. The linked image has no Musashi core,
floating-point runtime helpers, OS math-library references or 32-bit software
multiply/divide helpers. Host device/drawing/display/state tests, 2,560 synthetic
hook comparisons, container wrap/growth/copy tests, replay malformed-input tests,
wide-integer arithmetic checks and SHA-256 comparisons pass. This verifies the
covered native diagnostic schedule, not physical-board timing or live pacing.

Live mode is currently a bring-up path, not a playable Amiga frontend. Its
32-bit cycle budget also bounds runs to less than one counter wrap; long-running
live timing and the Amiga video/audio backends remain future work.
