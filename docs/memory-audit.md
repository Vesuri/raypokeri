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
| Paula | 32-byte waveform, offline bank, message port, IO request and audio.device | stop audio DMA / remove servers; close device; delete request and port; free waves |
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
