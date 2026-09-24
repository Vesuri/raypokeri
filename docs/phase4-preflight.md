# Phase 4 preflight — pending timing decision

Phase 4 is not implemented or signed off. No Amiga game execution has been
claimed. The following checks precede the loader and interrupt implementation.

## Shared-model build requirement

The installed m68k-amiga-elf GCC 15.1.0 cannot compile `src/board/Board.cpp`:
`<array>` is unavailable. Its library lookup also finds no `libstdc++.a` or
`libgcc.a`. The existing Amiga build is freestanding, disables exceptions, and
rejects software 32-bit multiply/divide helpers.

The shared board currently uses STL containers, exceptions in state handling,
64-bit timing products/divisions, and floating-point drawing/audio math. Linking
it unchanged is therefore not a build-system-only change. A prerequisite is a
freestanding implementation of the required storage/math/runtime facilities,
with the same shared device semantics and host regression tests. Do not clone
or replace the board with a second, guessed Amiga device implementation. Do not
remove the arithmetic audit or introduce Musashi into the Amiga build.

Reproduce the initial compile probe after sourcing `amiga/env.sh`:

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

The plan does not specify how these two schedules are made identical for its
byte-for-byte RAM gate. A comparison that ignores timer, stack or RNG bytes
would weaken the agreed gate and must not be silently substituted.

### Proposed decision: separate diagnostic scheduling from live pacing

Keep the original per-instruction host reference and VBI-paced native operation.
Add a diagnostic-only native run that delivers inputs, watchdog resets and
virtual interrupts at the host-recorded original-instruction boundaries and
advances the shared board to the corresponding reference cycle boundaries.
The 68000 still executes the original game instructions; diagnostic tracing
must not interpret them or import game results from a capture. Reference event
records and dumps remain in `tmp/`. Compare complete RAM at an agreed boundary,
normalizing only independently proven allocation deltas as in Phase 3.

This would prove native execution, hook semantics and shared-device behavior
under an identical schedule. Separately verify live VBI operation reaches idle,
deferred interrupts cannot reenter services, guard memory remains intact, and
all owned vectors/OS state are restored on exit. The diagnostic replay must
not be presented as proof of cycle-accurate live VBI timing.

This scheduling addition needs the user's decision before implementation. The
alternative is to require the live native run itself to reproduce the host's
instruction/cycle schedule, which is a substantially larger timing design than
the current VBI-based Phase 4 description.
