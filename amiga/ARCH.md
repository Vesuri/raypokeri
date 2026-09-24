# Amiga architecture notes — Pokeri

## Display: takeover (not OS-friendly)

Same approach as Rescue on Fractalus (its `amiga/ARCH.md`):

- `LoadView(NULL)` + `WaitTOF()` × 2 to suspend the OS display.
- Own copper list pointed at by `cop1lc` directly.  Built at RUNTIME in chip RAM
  (`CopperList::allocate`): a `__chip` static initialiser is silently discarded, because
  `.MEMF_CHIP` is a BSS hunk.
- DMA: master + copper + blitter on; bitplane DMA once there is something to show.
- On exit: `RemIntServer`, drain the blitter queue, restore the OS copper list and DMA,
  `LoadView(savedView)`, `WaitTOF()` × 2, close libraries.

## VBI: exec `AddIntServer`

`Pokeri.cpp` keeps its VERTB server on Exec's chain. During native game execution,
a small level-3 entry shim chains Exec's original handler and arms a single trace
when returning to physical user mode. The trace provides an eligible game
boundary for deferred VBI time and virtual IRQ delivery, including hook-free
loops. The original vector is restored on exit. This is a 68000 bring-up path;
WHDLoad integration is later work.  Rescue on Fractalus later
replaced the whole VERTB `IntVector`: that won back ~4% of the frame from the OS servers
ahead of it.  Adopt that only if a measurement shows it's needed here.

## Layers

| Layer | What it is |
|-------|------------|
| **Hardware** | the dA JoRMaS template framework classes (`AmigaHardware`, `Bitmap`, `CopperList`, `Sprite`, `Palette`, `Util`) in `src/platform/amiga/framework/`; their `*Assembler.s` are kept pristine and bridged for vasm at build time (`amiga/Makefile`) |
| **App** | `Pokeri` (`src/platform/amiga/Pokeri.cpp`): takeover, VBI, main loop.  `Native.cpp` and `NativeEntry.s` execute the original 68008 code and call the shared HD63484 / AY-3-8912 board services (`docs/porting-approach.md`) |

The template's demo layer (`Part`, `Script`, `ProductionRunner`, `ModulePlayer`, the
TrackerPacker replay) is deliberately not vendored.

## Build

`make` from `amiga/` with the shared toolchain on PATH (`. env.sh`, same shell command).
ASSEMBLER is on by default; `make NO_ASSEMBLER=1` builds the portable C++ bodies.  The
build fails if a 32-bit software mul/div (`__mulsi3` & co.) is linked (the `audit` target).

## Native validation

See [Phase 4](../docs/phase4-preflight.md) for the private service stack, virtual
guest SR/IPL and stacks, original-ROM hash checks, replay schedule and full-RAM
comparison procedure. Musashi remains host-only. The Amiga path uses integer
math and a freestanding container subset; it opens no OS math libraries.

Normal completion, a loud device/replay stop and a left-mouse exit use the same
vector-restoration path. Fatal allocation errors escape the service stack; an
allocation ledger reclaims any temporary containers skipped by that escape
after normal application destruction and OS/hardware restoration.

Phase 4 is complete under the approved diagnostic scope. Phase 5 uses replay
boot followed by live VBI timing. `AmigaSurface` stores authoritative bitplanes
and accelerates fills/copies with Agnus; `AmigaScreen` composes and flips the
576×283 viewport. `PaulaAy` replaces reference PCM synthesis with hardware loops.
Raw CIA keyboard ownership and audio.device allocation are restored on exit.
Live guard checks inspect 1 KB per serviced frame; diagnostic and exit checks
inspect the full 512 KB. See [Phase 5](../docs/phase5-amiga.md) for measured gates,
shortcuts, controls and local test procedures.
