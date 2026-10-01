> RAY Pokeri integration: this is retained shared-template/toolchain background,
> not the game's directory layout or a claim that its demo/audio layers are vendored.
> Build from `amiga/` with `. ./env.sh && make`; output is `out/RAYPokeri`.
> The game uses only the hardware framework subset, its own runtime/audio, and
> integer math. Use [the current testing guide](../../../../docs/testing.md)
> for release/diagnostic commands. Global even-sized data of at least four bytes
> needs `alignas(4)` and must pass the ELF alignment and software-mul/div audits.

# Building the dA JoRMaS C++ template with m68k-amiga-elf-gcc

This is the **dual-build** starting point for new C++ productions: one set of
source files that compiles **both** under **SAS/C 6.x** on Amiga (the `smakefile`)
and, intact, under the modern **GCC** Amiga cross-toolchain on a host PC
(macOS/Linux/Windows), run/debugged in FS-UAE. This document is the canonical,
self-contained guide — prerequisites, the build/run/debug procedure, and exactly
how the two builds stay in sync from one source tree. It is the reference the
per-production ports (e.g. `Productions/JRm-hC74/GCC-PORT.md`) are derived from.

The framework here (`AmigaHardware`, `Bitmap`, `CopperList`, `Palette`, `Sprite`,
`Part`, `Script`, `ProductionRunner`, `Util`, `ModulePlayer`, the TrackerPacker
replay) is the most complete, cleanest version — it merges the feature sets of
JRm-hC73, JRm-hC74, JRm-bS75 and DanceDiverse3. Start a production by copying this
directory and replacing `ExampleProduction`/`ExamplePart` + the `module`/`sample`
assets with your own.

### Layout

```
Template/C++/
  GCC-PORT.md            <- this file
  smakefile              SAS/C build (sc + Bin2Hunk)
  *.cpp *.h *.s          the framework + ExampleProduction/ExamplePart (build under both)
  module.Sng sample.Smp  raw assets (Bin2Hunk / .incbin inputs; swap in your own)
  Makefile               GCC build (m68k-amiga-elf-gcc)
  SASCCompat.h           SAS/C keyword shims, force-included on GCC only
  GCCRuntime.cpp         -nostdlib C++ runtime + level-3 interrupt trampoline
  incbin.s               GCC asset embedding (.incbin == Bin2Hunk equivalent)
  compat-include/        minimal stdlib.h/limits.h/math.h for -nostdlib
  run.sh / debug.sh      launch / source-level-debug in FS-UAE (OCS A500)
```

All commands below run from this directory. SAS/C: `smake`. GCC: `make`.

---

## 1. Prerequisites (GCC build)

macOS (Apple Silicon or Intel). You need the cross toolchain, it on `PATH`, and a
Kickstart ROM.

### 1.1 Install the toolchain
The toolchain is **BartmanAbyss `vscode-amiga-debug` v1.8.2** — its gcc / gdb /
elf2hunk / FS-UAE are mutually matched (DWARF ↔ gdb ↔ stub), which is what makes
source-level debugging work. It ships prebuilt macOS binaries (native arm64; only
FS-UAE is x86_64 via Rosetta 2):

| Tool | Purpose |
|------|---------|
| `m68k-amiga-elf-gcc` 15.1.0 + binutils + gdb | C/C++ compiler, assembler (`as`), linker, objdump, gdb |
| `elf2hunk` | ELF → Amiga HUNK executable (`.exe`, magic `0x000003F3`) |
| `vasmm68k_mot` | Motorola-syntax assembler for the hand-written `.s` files |
| `fs-uae` | emulator (CPU-exception breakpoint + GDB stub) |

1. Download the **v1.8.2** `.vsix` (VS Code Marketplace, publisher *BartmanAbyss*,
   "Amiga C/C++ Compile, Debug & Profile", or the GitHub releases page
   <https://github.com/BartmanAbyss/vscode-amiga-debug/releases>). A `.vsix` is a zip.
2. Unzip it and copy the macOS tool tree to a **space-free** path, plus the
   freestanding CRT support files (they ship in the vsix template, not bin/):
   ```sh
   unzip -q amiga-debug-1.8.2.vsix -d amiga-debug
   cp -R amiga-debug/extension/bin/darwin       "$HOME/.local"          # opt/bin, elf2hunk, vasmm68k_mot, fs-uae
   cp -R amiga-debug/extension/template/support "$HOME/.local/support"  # gcc8_{a,c}_support
   xattr -dr com.apple.quarantine "$HOME/.local"   # clear Gatekeeper quarantine
   ```
   The Makefile finds `support/` relative to gcc as `<gcc>/../../support`, which
   resolves to `$HOME/.local/support` for this layout.

> **No spaces anywhere on the build path.** GCC's LTO link encodes the install
> path into `COLLECT_LTO_WRAPPER` and `posix_spawnp`s it; a space breaks the spawn.
> Keep both the toolchain and this checkout under space-free paths.

### 1.2 Put the tools on PATH
```sh
export TC="$HOME/.local"
export PATH="$TC/opt/bin:$TC:$TC/fs-uae:$PATH"
```
Verify: `m68k-amiga-elf-gcc --version` → 15.1.0; `which elf2hunk vasmm68k_mot fs-uae m68k-amiga-elf-gdb`.

### 1.3 Kickstart ROM
FS-UAE needs a Kickstart ROM, which is licensed and **not** included — supply your
own **KS 3.1** (`kick40063.A600`/`kick40068.A1200`; KS 3.1 auto-boots directory
hard-drives, KS 1.3 stalls). Point the run/debug scripts at it via `$KICKSTART`
(or pass it as their first argument):
```sh
export KICKSTART="$HOME/Amiga/kick31.rom"
```

---

## 2. Build, run, debug

With §1 done (toolchain on `PATH`, `$KICKSTART` exported), from this directory:
```sh
make                 # compile + link -> out/JoRMaS.exe (HUNK; assets embedded)
make NO_ASSEMBLER=1  # same, but the portable C++ bodies instead of the asm
./run.sh             # boot it in FS-UAE as an OCS A500. Left mouse button quits.
./debug.sh           # same, attached to m68k-amiga-elf-gdb (source-level) — see 2.4.
make clean           # remove obj/ + out/
```
`run.sh`/`debug.sh` also accept the ROM path as `$1`.

### 2.1 The core strategy: the `ASSEMBLER` switch
Every framework `.cpp` provides a portable C++ implementation under
`#ifndef ASSEMBLER`, with the hand-tuned SAS/C register-convention routine under
`#ifdef ASSEMBLER` (+ a matching `*Assembler.s`). `ASSEMBLER` selects between them
and **defaults on for both SAS/C and GCC** (set in `Util.h`); `make NO_ASSEMBLER=1`
(GCC) / `-DNO_ASSEMBLER` (SAS/C) forces the C++ bodies.

SAS/C+ASSEMBLER links the `*Assembler.s` directly (its `__asm` register-parameter
declarations match the asm's calling convention). GCC has no register-parameter
syntax, so GCC+ASSEMBLER reaches the *same* `.s` through thin register-marshalling
wrappers (§4) — `this` into `a0`, each argument into its SAS/C-annotated register,
then `jsr` the raw SAS/C symbol. The `.s` are assembled by `vasm`, symbol-bridged
from the pristine SAS/C source at build time (§4). The `NO_ASSEMBLER` GCC build
compiles the C++ bodies and needs none of the framework `.s`.

### 2.2 Compiler flags (Makefile)
- **`-m68000`** (not `-m68020`). The demo is a 68000 program. `-m68020` makes GCC
  emit 020-only constructs — scaled-index addressing, 32-bit `mulu.l`/`divu.l`,
  bit-field ops (`bfextu`) — which **illegal-instruction-trap on a real 68000**
  (e.g. an A500). 68000 code also runs on the A1200's 68EC020, so it is universal.
- **`-msoft-float`** — no FPU on the 68000 (the toolchain defaults to *hard* float
  for `-m68020`, which would emit trapping line-F instructions).
- `-nostdlib -fomit-frame-pointer -ffunction-sections -fdata-sections`
  `-fno-rtti -fno-exceptions -fno-use-cxa-atexit -std=gnu++17 -fpermissive`
  (`-fpermissive` + `-Wno-write-strings`/`-Wno-pointer-arith` absorb SAS/C-era
  `UBYTE*`↔`const char*` drift against the modern NDK).
- Link: `-Wl,--gc-sections,-Ttext=0,--emit-relocs`, then `elf2hunk`. `--gc-sections`
  drops framework routines a given production doesn't call (e.g. the example doesn't
  use `combineWithMask`, so it isn't in the final exe).

### 2.3 Run as an OCS A500
`run.sh` launches `fs-uae --amiga_model=A500` with `--chip_memory=1024
--fast_memory=8192`. A500/OCS is the lowest-common-denominator target; the extra
chip + fast RAM leaves room for the program load plus a large `MEMF_CHIP` bitmap
working set (stock 512 KB chip can be too tight). FS-UAE is driven with two
directory hard-drives: dh0 boots and `cd dh1:` + runs the program; dh1 holds
`out/JoRMaS.exe` (copied in under the trigger name). Always pass
`--window_width`/`--window_height`, or FS-UAE stalls with "Not a valid drawable
size for glViewport" when launched from a non-GUI shell — run from a GUI session
to see the render loop.

> Productions that detect/require a specific chipset (e.g. an OCS-only HAM trick,
> or an AGA path gated on `GFXF_AA_LISA`) may need a particular `--amiga_model`
> and/or `SetPatch` — see `Productions/JRm-hC74/GCC-PORT.md` for a worked example.

### 2.4 Source-level debugging (`./debug.sh`)
`debug.sh` boots the same A500 with FS-UAE's built-in GDB stub and attaches
`m68k-amiga-elf-gdb` to `out/JoRMaS.elf` (full source symbols; load offset 0, so
runtime PCs map straight to symbols). Build first, then `./debug.sh` (or
`./debug.sh "$KICKSTART" my.gdb` to script it). At the prompt: `continue` runs;
**Ctrl-C** breaks in; then `break <file>:<line>` / `backtrace` / `info registers` /
`x/8i $pc` / `stepi`. The patched FS-UAE **breaks on CPU exceptions** (an
address/illegal-instruction fault stops with a usable backtrace) and supports
**hardware watchpoints** (`watch <symbol>` catches the write that corrupts a
variable). The recipe is fiddly and `debug.sh` encodes it: gdb needs
`HOME`/`XDG_CACHE_HOME` set, you must connect to `127.0.0.1` (not `localhost` →
IPv6), and you must **not** probe the port first (the stub accepts one client).

---

## 3. How the two builds stay in sync (one source tree)

The C++ logic is identical for both compilers; the deltas are compiler/ABI plumbing.

1. **Compiler detection.** SAS/C defines `__SASC`; this gcc defines `__GNUC__`.
2. **`SASCCompat.h`** (force-included by the GCC Makefile via `-include`; the
   `smakefile` does NOT include it, and its whole body is `#ifndef __SASC`, so it is
   inert under SAS/C). It maps SAS/C storage/keyword extensions to GCC equivalents
   or no-ops:
   - `__chip` → `__attribute__((section(".MEMF_CHIP")))` (linker maps to a chip hunk)
   - `__far`, `__saveds`, `__stdargs`, `__regargs`, `__aligned`, `register __d0…__a6` → no-ops
   - `__inline` → *nothing* (the framework declares accessors `__inline` in headers
     but **defines them out-of-line** in `.cpp`; GCC's `inline` would not emit them,
     breaking cross-TU calls — so they must become ordinary functions)
   - fixed-width typedefs `int8_t…uint32_t` (the framework only typedefs these for
     `__cplusplus < 201103L`; the GCC build is gnu++17)
   - **`__asm` is deliberately NOT redefined** — the NDK library-call macros expand
     `__asm("d0")` register bindings at the *call site*; neutralising `__asm` would
     break every `OpenLibrary`/`AllocMem`/`LoadView`/…
3. **`__asm` function declarations** sit in `#if defined(ASSEMBLER) && defined(__SASC)`
   / `#else` pairs. SAS/C+ASSEMBLER takes the `__asm` branch (links the `.s`
   directly); everything else takes the plain branch. GCC+ASSEMBLER compiles (plain
   decls) and reaches the asm via wrappers (§4). Covers `Util::sqrt`/`ungzip` +
   `Rect`, `AmigaHardware`'s `getVBR`/`isLongFrame`/`isBlitterBusy`/`blitterWait` +
   the blitter ops, `Bitmap`, `CopperList`, and `ProductionRunner`'s `AutoVector`
   typedef + `installLevel3Interrupt`.
4. **Correctness niceties that GCC `-O2` needs (also valid/beneficial under SAS/C):**
   `volatile bool hasQueuedBlits` and a `volatile` store in `processBlitterQueue`
   (the blit IRQ writes them; without `volatile`, `-O2` can spin forever or duplicate
   the `bltsize` trigger), a `volatile` sweep of the hardware-register pointer macros,
   loop variables scoped to their loop, and `GfxBase` set after `OpenLibrary`
   (`#ifndef __SASC`). Assembly `Scc` return bytes must enter GCC through an
   integer byte and be normalized with `!= 0`, never bound directly to `bool`.
   `$FF` violates GCC's 0/1 representation: an optimized `!value` can otherwise
   XOR it with 1 and produce nonzero `$FE`. This affected `isBlitterBusy` and
   the screen-flip idle check; both Boolean wrappers now normalize their output.
5. **Runtime shims (GCC only):** `GCCRuntime.cpp` supplies `operator new`/`delete`
   via `AllocMem`/`FreeMem`, `__cxa_pure_virtual`, tiny `qsort`/`abs`/`labs`, the
   `SysBase`/`GfxBase` globals, and the **level-3 VERTB/BLIT interrupt trampoline**
   (replacing `ProductionRunnerAssembler.s`, which is therefore the one framework
   `.s` not assembled for GCC — its trampoline `jsr`s back into C++ with the SAS/C
   `this`-in-`a0` ABI that GCC doesn't use). `compat-include/` provides minimal
   `stdlib.h`/`limits.h`/`math.h`; the toolchain's `support/gcc8_{a,c}_support`
   provide the freestanding CRT (program entry, `memset`, …).
6. **Assets — same raw files both sides (§3.1).**

### 3.1 Assets — built from raw files on both sides
Both builds convert the same raw files into linkable objects with a platform tool,
rather than committing prebuilt `.o`:
- **SAS/C** (`smakefile`): `Bin2Hunk module.Sng module.o NAME _module HUNK data`
  and `Bin2Hunk sample.Smp sample.o NAME _sample CHIP`.
- **GCC** (`incbin.s` + `m68k-amiga-elf-as`): `.global <sym>` + `.incbin "<file>"`,
  with `sample` in a `.MEMF_CHIP` section. The assembler *is* the Bin2Hunk
  equivalent — and unlike `objcopy` it gives the exact symbol names + chip/data
  section control.

`ExampleProduction` expects symbols `module` (the ProTracker song) and `sample`
(its chip-RAM sample data). SAS/C prefixes them `_` (Bin2Hunk `NAME _module`); GCC's
ELF names have no prefix. **For a real production:** drop in your own `module.Sng`/
`sample.Smp`, add image/font raws, and mirror each asset in both `incbin.s` (one
`.global`+`.incbin`, chip vs data per how the hardware reads it) and the smakefile's
Bin2Hunk rules.

---

## 4. Calling the SAS/C register-argument assembler from GCC (ASSEMBLER-on path)

GCC does **not** accept `__asm("d1")` on function *parameters* (compile error), but
it **does** accept `register T v __asm("d1")` *locals* — the mechanism the NDK `LPx`
inline macros use. So each asm routine gets a thin C++ wrapper with GCC's normal
signature that marshals into register locals and `jsr`s the raw SAS/C symbol:

```cpp
// Util.cpp, under #if defined(ASSEMBLER) && !defined(__SASC):
uint32_t Util::sqrt(uint32_t x) {
    register uint32_t arg __asm("d1") = x;   // SAS/C: arg in d1
    register uint32_t ret __asm("d0");       // SAS/C: result in d0
    __asm volatile("jsr _sqrt__4UtilFUl" : "=r"(ret), "+r"(arg) : : "cc", "memory");
    return ret;
}
```

The wrapper carries gcc's mangled name (call sites link unchanged) and the `jsr`
targets the raw SAS/C symbol. Member functions also marshal `this` into `a0`; the
clobber list is `"cc", "memory"` plus the Amiga scratch registers (`d0/d1/a0/a1`)
not already bound (SAS/C callee-saves `d2-d7/a2-a6`). Wrapped routines: `Util::sqrt`/
`ungzip` + `Rect::update`/`unite`, the `AmigaHardware` blitter/VBR/`isLongFrame`
routines, `Bitmap::clear`/`copy`/`copyWithMask`/`line`/`fill`, and
`CopperList::showSprite`/`showBitmap`.

The matching `*Assembler.s` are assembled by `vasmm68k_mot -m68000 -Felf -no-opt`
(`-no-opt` keeps branch sizes as written, so PC-relative jump tables don't shift)
and linked. A Makefile pattern rule `sed`-bridges each from the pristine SAS/C
source at build time:
- `section code`/`__MERGED` → ELF `.text`/`.data`/`.bss`.
- C++ **static-data** symbols the asm imports (`AmigaHardware::hasQueuedBlits`/
  `octants`/the blitter queue, `Util::sin`/`cos`) SAS/C-mangled → GCC Itanium-mangled,
  so the asm resolves against the C++ definitions. The xdef'd **routine entry points
  keep their SAS/C names** — that is what the wrappers `jsr`.
- the sin/cos tables' SAS/C small-data (`a4`) base → absolute (GCC's code model).
- cross-object `bsr _blitter*` (Bitmap→AmigaHardware) → `jsr`: a PC-relative branch
  to another object is `R_68K_PC16`, which `elf2hunk` can't represent; absolute `jsr`
  is `R_68K_32` (which it can). Intra-object `bsr` stay PC-relative.
- `getVBR`'s 68010+ `movec vbr,d0` → its encoding (`dc.w $4e7a,$0801`) so vasm
  `-m68000` accepts it (getVBR keeps its C++ body for `NO_ASSEMBLER`).
- the unused SAS/C 32-bit math-helper imports (`__CXM33`/`__CXD33`) dropped.
- SAS/C dot-in-middle local labels (`u.foo`, `bL.bar`) → underscores (vasm splits
  them); `.b/.w/.l/.s` size suffixes are one char and left alone.

The TrackerPacker replay (`TrackerPackerReplayV3.1.s`) is bridged the same way
(`@tp_init__Fv`/`@tp_end__Fv`→`tp_init`/`tp_end`, the header declares them
`extern "C"` for GCC; `_tp_*` data symbols drop their SAS/C `_`; `pt1.1`→`pt1_1`).

---

## 5. Verification status

Both **GCC** builds are verified on macOS (toolchain at `~/.local`): `make` (ASSEMBLER
on) and `make NO_ASSEMBLER=1` both compile every TU, `vasm`-assemble all bridged
`.s`, link with no undefined symbols, and `elf2hunk` to a valid HUNK `.exe`. Running
the render loop visually requires a GUI session (the non-GUI "Not a valid drawable
size" stall is FS-UAE, not the program). The **SAS/C** build is correct-by-construction
(the guards leave SAS/C seeing essentially its original source — `__asm` decls and
`*Assembler.s` unchanged, `SASCCompat.h` inert) but has **not** been re-verified on an
Amiga/SAS-C install — do that before relying on it; likely touch-points are the
`Util.h` `ASSEMBLER` default and the `smakefile` Bin2Hunk rules.
