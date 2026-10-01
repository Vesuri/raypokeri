# Amiga architecture notes — RAY Pokeri

## Display: takeover (not OS-friendly)

Same approach as Rescue on Fractalus (its `amiga/ARCH.md`):

- `LoadView(NULL)` + `WaitTOF()` × 2 to suspend the OS display.
- Own copper list pointed at by `cop1lc` directly.  Built at RUNTIME in chip RAM
  (`CopperList::allocate`): a `__chip` static initialiser is silently discarded, because
  `.MEMF_CHIP` is a BSS hunk.
- `Pokeri.cpp` builds only a black-screen fallback. `AmigaScreen::prepare` builds
  two full lists with fixed bitplane pointers, one per interleaved display buffer.
  Completed lists publish COP1LC in safe main-thread scanlines 8..299 without
  a COPJMP1 strobe. The next VBI retires buffer ownership after the automatic
  Copper reload; late delivery never restarts the list mid-screen.
- DMA: master + copper + blitter on; bitplane DMA once there is something to show.
- On exit: `RemIntServer`, drain the blitter queue, restore the OS copper list and DMA,
  `LoadView(savedView)`, `WaitTOF()` × 2, close libraries.

## VBI: exec `AddIntServer`

`Pokeri.cpp` keeps its VERTB server on Exec's chain. Level-2/3/4/6 wrappers
chain Exec and pause the guest CIA timer. Physical user-mode returns can redirect
to the validated Line-A service stub, providing a safe guest boundary without
using the trace vector. A software-requested level-2 interrupt requests service
when needed. No original game handler runs inside the Amiga interrupt. Device
services admit Amiga interrupts; guest transition/save/restore sections remain
masked. All vectors and CIA resource ownership are restored on exit. WHDLoad
keeps its own VBR and forwards supported exceptions; normal play needs neither
NoVBRMove nor NoWriteCache.

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

## Native validation and layout

See [architecture](../docs/architecture.md) for private service stack, virtual
SR/IPL/stacks, calibrated bounded clock, compact windows and cleanup ownership.
See [testing](../docs/testing.md) for exact replay and live qualification.
Musashi remains host-only. Normal startup reads original ROMs, checks sizes and
patch guards, and runs approved fast initialization; no replay or SHA work occurs.

`AmigaSurface` owns interleaved bitplanes; `AmigaScreen` composes the 608×283
viewport into 640-pixel padded rows. Wide fetches require detected AGA hardware;
ECS uses its own fetch window and FMODE=0. A500+ is supported by the binary but
performance tuning there is deferred. Virtual exception frames remain six-byte
68000 frames, while physical frame handling follows the detected CPU.

Live device-guard checks inspect 1 KiB per serviced frame across 64 KiB; the
4 KiB RAM canary is checked on wrap. Replay and exit check all guard bytes.
Normal return, error and left-mouse exit share vector/hardware restoration.
A fatal service-stack escape also releases tracked temporary allocations after
ordinary owners have relinquished them. No IRQ handler allocates memory.
