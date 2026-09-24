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

The skeleton hangs its VERTB server on exec's chain (`Pokeri.cpp`).  This keeps exec's
level-3 handler, needs no VBR plumbing, and is WHDLoad-safe.  Rescue on Fractalus later
replaced the whole VERTB `IntVector`: that won back ~4% of the frame from the OS servers
ahead of it.  Adopt that only if a measurement shows it's needed here.

## Layers

| Layer | What it is |
|-------|------------|
| **Hardware** | the dA JoRMaS template framework classes (`AmigaHardware`, `Bitmap`, `CopperList`, `Sprite`, `Palette`, `Util`) in `src/platform/amiga/framework/`; their `*Assembler.s` are kept pristine and bridged for vasm at build time (`amiga/Makefile`) |
| **App** | `Pokeri` (`src/platform/amiga/Pokeri.cpp`): takeover, VBI, main loop.  The original 68008 code and the HD63484 / AY-3-8912 service implementations will hang off it (`docs/porting-approach.md`) |

The template's demo layer (`Part`, `Script`, `ProductionRunner`, `ModulePlayer`, the
TrackerPacker replay) is deliberately not vendored.

## Build

`make` from `amiga/` with the shared toolchain on PATH (`. env.sh`, same shell command).
ASSEMBLER is on by default; `make NO_ASSEMBLER=1` builds the portable C++ bodies.  The
build fails if a 32-bit software mul/div (`__mulsi3` & co.) is linked (the `audit` target).
