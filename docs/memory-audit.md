# Memory ownership audit — 2026-09-28

Scope: allocations reached by Pokeri's normal Amiga/SDL startup, gameplay,
diagnostic preparation and shutdown, plus the framework graphics factories.
This is an ownership audit with targeted failure tests, not a proof against
all possible out-of-bounds writes or every possible allocation failure.

## Findings and fixes

- **MEASURED:** the preceding Guru fix (`3603020`) releases the static cabinet
  input queue before the emergency heap sweep. Its CRT destructor runs later.
  See [the reproduced exit fault](phase5-amiga.md#exit-guru-static-input-queue-lifetime-2026-09-28).
- **DERIVED; failure-injection tested:** CopperList, Bitmap and Sprite allocated
  Chip storage first, then returned `new Owner(...)`. If the second allocation
  failed, the raw Chip storage leaked. All three now free it on that path.
  CopperList also rejects zero/overflowing lengths.
- **DERIVED; sanitizer tested:** Bitmap allocated `dataWidth / 8` bytes per row
  while its drawing stride rounds up to a 16-pixel word. The allocator and
  destructor now use the padded stride; invalid dimensions and overflowing
  16-bit strides are rejected. Mask generation handles allocation failure and
  includes the padded final word. Pokeri's current screen uses AmigaScreen /
  AmigaSurface directly, so this was a latent framework defect.
- **DERIVED; SDL failure-injection tested:** Window previously called SDL_Quit
  only if startup reached the `enabled` flag. A failure creating the window or
  renderer left initialized SDL subsystems alive. Initialization ownership is
  now recorded immediately after successful SDL_Init, independently of readiness.

## Ownership and ordering

| Owner | Allocation / matching release | Lifetime check |
|---|---|---|
| Native board and guard | tracked uninitialized allocation / delete[]; aligned Board uses placement construction and explicit destruction | original unaligned pointer retained; incomplete construction is reclaimed by emergency heap sweep |
| Replay | tracked byte array + reader / delete[] + delete | reader destroyed before backing bytes; absent in normal startup |
| Vector copy | optional 1,024-byte Fast allocation / same-size FreeMem | vectors restored before release |
| AmigaSurface | planar words × 2, pattern cache × patternWords × 2 (320 or 512 bytes/entry), copy masks 16 × 66 × 2 / matching FreeMem | partial prepare calls release; pending blits drained before freeing |
| AmigaScreen | two `Bytes + 8` allocations and two owned Copper lists | frees original pointer (`buffer - 4` words), after Copper/OS view restored and blits drained |
| Card cache | Chip bitmap/mask storage, tracked cache object and temporary canvas | cache detached and blits drained before freeing; temporary canvas deleted on ordinary failure paths |
| Paula | 32-byte waveform, five shared noise DMA buffers (40,960 bytes), message port, IO request and audio.device | stop audio DMA / remove servers; close device; delete request and port; free waves |
| Input | static cabinet queue | remove keyboard handler, release retained queue before emergency sweep |
| Timing diagnostics | samples, hook counters, marks/frames/commands/events, timer request | inactive before free; fixed allocation sizes match release; timer ownership released |
| Freestanding containers | tracked new/delete | no heap allocations in physical VBI, keyboard or audio IRQ handlers; service-stack faults sweep abandoned temporaries after normal owners are destroyed |
| SDL Window | SDL window/renderer/texture/audio device; SDL subsystem ownership | close audio, destroy texture/renderer/window, then SDL_Quit, including partial startup |

Amiga `-fcheck-new` is required and present: its freestanding operator new returns
null on allocation failure. The heap adds its own stored allocation size and
checks header-size overflow. It removes each block from its list before FreeMem.
A fatal service-stack escape intentionally cannot unwind local C++ objects;
`pokeriReleaseHeap` is the final recovery sweep, after hardware and normal owners
have been released. Static owners must not retain swept pointers.

## Validation

**MEASURED:** `make harness-memory-check` compiles the actual allocation-only
source sections with a checked Exec mock and ASan/UBSan. It fails both allocations
of each graphics factory in turn; checks zero outstanding blocks, exact FreeMem
sizes, non-word-aligned bitmap/mask bounds, invalid dimensions, heap unlinking,
null/failed/overflowing allocation and abandoned-allocation sweeps. The test pads
the tracking header for the host's 16-byte new alignment; Amiga ABI and DMA
lifetimes are checked separately. It does not emulate Exec fragmentation.

**MEASURED:** `make harness-window-memory-check` forces renderer creation to
fail after SDL and window creation, then checks SDL_WasInit is zero after
teardown. Normal dummy audio/window teardown also passes. This test runs without
ASan: the installed SDL compatibility library aborts in its loader before main
when linked with ASan. The allocation/container tests retain ASan/UBSan.

**MEASURED:** native runs reach the verified CRT epilogue *after static
destructors*, with heapHead zero and no breakpoint at Exec's actual Alert
implementation:

- A1200 normal live run: 160,000,000 cycles, four input transitions, zero resets.
- A1200 missing first ROM: preparation failure after board/guard allocation.
- A1200 malformed replay: preparation failure, no guest execution.
- A500+ with 512 KB Chip RAM: preparation allocation failure, no guest execution.

Local evidence: `amiga/.run/memory-{normal,no-rom,bad-replay,lowchip}/gdb-out.log`.
The earlier Guru regression additionally covers 24-input A1200 and 10-input ECS
normal exits. Host board/drawing/platform/native tests and the Amiga arithmetic
audit pass. These runs do not exhaustively inject failure at every native OS
allocation, nor validate every unused utility in the imported framework.


## Startup status through finalization (2026-09-30)

**MEASURED:** W3's negative startup test initially returned success even though
preparation refused diagnostic trace under WHDLoad's moved VBR. The shared GCC
support `_start` is void, ignores main's return and leaves finalizer-clobbered
D0 as the OS result. Pokeri's local `RuntimeStart.cpp` now preserves the result
while retaining preinit/init order and reverse finalization. The shared
installation is untouched; its entry is renamed at compile time and discarded.
A linked CPU test checks order, stack, callee-saved registers and seven full-width
return codes on 000/020/030/040. WHDLoad now reports the intended replay refusal;
normal launch/save/exit still passes. This fixes error reporting, not an
allocation or ownership change. Reproducer: `make build/runtime-start-test`,
then `python3 host/runtime_start_check.py --elf amiga/out/Pokeri.elf`.

## Release memory budget (2026-09-30)

**MEASURED:** stripped runtime-noise release, standalone A1200 with 1 MB Chip
and 8 MB Fast. A read-only Exec MemList inspection at main and nativePlayReady
finds 776,624 additional Chip bytes and 1,114,840 additional Fast bytes. The
loaded HUNK allocations total 321,416 bytes (including BSS, excluding debugger
symbols); these are already present at main. Thus attributed game storage at
Ready is approximately **758.4 KiB Chip + 1,402.6 KiB other**, about 2.11 MiB in
total. This includes small library/runtime changes between those samples; it is
not an exhaustive gameplay peak or a minimum-machine proof. Evidence:
`amiga/.run/memory-budget/gdb-out.log`, release `amiga/out/Pokeri` HUNK header.

**DERIVED:** the principal Chip allocations are 524,400 bytes planar VRAM,
181,136 bytes display buffers, 32,768 bytes pattern cache, 11,200 bytes card
cache, 2,112 bytes copy masks, 384 bytes Copper data and 24,608 bytes audio.
The tracked C++ heap at Ready is 1,112,693 bytes, including the 560,618-byte
Board and 524,288-byte guard; do not add this heap again to the measured totals.

The initial measurement used `CHIPMEMSIZE=$100000` and `FASTMEMSIZE=$400000`
(superseded by the reduced reservation below). kick31.s adds
its $80000-byte Kickstart image: the actual header requests **1 MiB base and
4.5 MiB expansion memory**. This covers the measured release, with conservative
headroom for runtime allocations and emulated OS services. It is not a claim
that the executable consumes 5.5 MiB. WHDLoad, the host OS and PRELOAD need
additional memory outside that reservation. This initial reservation was subsequently reduced and tested as described below.


**DERIVED (subsequent noise-detail correction, same date):** two additional DMA
loops add 16,384 Chip bytes to the measured baseline above, making attributed
Chip storage approximately 774.4 KiB. The 1 MiB slave reservation is unchanged.
The 758.4 KiB figure remains the measured pre-correction release snapshot.

## Reduced WHDLoad reservation (2026-09-30)

**DECISION:** user requested 2 MiB OtherMem. The production slave now sets
`FASTMEMSIZE=$180000`; kick31 adds $80000, giving exactly **2 MiB total OtherMem**.
Chip remains 1 MiB. The release header audit asserts both actual header values.
This supersedes the conservative 4.5 MiB reservation above.

**MEASURED:** the reduced production slave passes two consecutive cold/warm
96,000,000-cycle runs, including save writes, backup preservation and normal
exit, plus a 480,000,000-cycle automatic gameplay run with normal save/exit.
Tests use the larger development executable to supply a finite cycle budget
and scripted inputs; the packaged release omits those controls and is smaller.
WHDLoad uses its default moved VBR, write cache and PRELOAD. Physical emulator
RAM remains 2 MiB Chip / 8 MiB Fast for the host OS/WHDLoad; the game runs within
the slave's 1 MiB Chip / 2 MiB OtherMem reservation, not that physical total.
Evidence: `tmp/whd-2mb-test.log`, `tmp/whd-2mb-gameplay.log`, fixtures
`tmp/whdload-test-dgluc1m8` and `tmp/whdload-test-t8geeisr`.

The release archive/header checks pass. Host OS, WHDLoad and PRELOAD still need
memory outside the reserved game region; 2 MiB OtherMem is not a claim that a
machine with only 2 MiB total Fast RAM can load the whole WHDLoad installation.
