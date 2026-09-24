# Pokeri — RAY video poker (68008) → Amiga port

Porting the Finnish RAY video poker machine *Pokeri* to the Amiga from its four EPROM dumps
alone — no schematic, manual or MAME driver.  The board is a **68008 + HD63484 ACRTC + AY-3-8912**.
The original 68008 instructions run **natively** on the Amiga's 68000, as in the Vette port
(`~/Documents/Vette`).  The port supplies relocation plus Amiga implementations of the video
(bitplanes/blitter), sound (Paula) and I/O the code talks to.  **Research stage**: the memory
map is largely known. The host renders game/service screens, plays and doubles a hand,
and replays full snapshots deterministically (Phase 2 implementation complete; palette,
clock and audio-listening validation remain qualified in the plan).

## Reference docs — READ ON DEMAND (this file stays small on purpose)

| Doc | Read it when |
|---|---|
| `docs/bringup-plan.md` | **Start here.** Status, pending user decisions, and the phased plan: Musashi harness (done) → boot to idle (done on the 512 KB video path) → **reference output (implemented; calibration open)** → relocation/hook tables derived by running → the original code on the Amiga, gated by RAM-state equality with the harness |
| `docs/hardware.md` | The physical machine: the processor board (PCB 5003-2, read off a photo), the video and sound boards, EPROM sockets/labels, controls, game rules from articles, people.  Source-tagged PHOTO/HV/KH |
| `docs/rom-set.md` | Anything about the ROMs: chip roles, the vector table, the `romgame`/`g200para` modules, the open questions.  Claims are tagged MEASURED / DERIVED / INFERRED — keep them tagged |
| `docs/visual-reference.md` | Judging rendered output: what the real-machine footage shows (frames in git-ignored `ref/footage/`). The Finnish video (`2BI-eUaPCOc`) matches our ROM set; the English screenshot is a different variant with different pay rules |
| `docs/porting-approach.md` | Designing relocation, the HD63484/AY-3-8912 services or interrupts; the prior-art index into the Vette and Rescue on Fractalus docs |
| `amiga/ARCH.md` | Display takeover, the VBI, the framework split |
| `src/platform/amiga/framework/GCC-PORT.md` | The toolchain install and how the SAS/C framework builds under GCC |

Cross-session in-progress notes live in the auto-memory (`MEMORY.md` is its index).  Stable facts
live here or in `docs/`; memory holds what is still moving.

## Build / run / debug

```
make roms [SRC=…]   # verify + unpack the user's ROM dump into rom/ (repo root)
make program-image  # the three program chips -> disasm/program.bin ($00000-$2FFFF)
cd amiga && . ./env.sh && make   # -> out/Pokeri   (source env.sh in the SAME shell command)
./run.sh            # FS-UAE A500+ / KS 3.1; left mouse button quits
./debug.sh          # FS-UAE gdb stub + m68k-amiga-elf-gdb
./diag_run.sh [s]   # headless: run s seconds, then gdb runs the read-only prints in diag.gdb
```

- The toolchain (`~/.local`) and Ghidra (`tools/ghidra` → `~/.local/share/ghidra`) are SHARED
  with the other Amiga projects.  Don't install per-repo copies.
- **Never `pkill fs-uae` / `pkill gdb`.**  The scripts source
  `~/.local/share/amiga/fsuae_common.sh`, which kills only this directory's recorded pid and
  gives each project its own gdb-stub port.
- The FS-UAE gdb stub serves memory reads but silently drops writes.  Inject test inputs from C
  (a `-D` flag plus a VBI-count window), and keep `.gdb` scripts read-only.  A `.gdb` script
  aborts at the first unknown symbol.
- `make clean` after editing a widely included header or changing build flags: the Makefile
  tracks neither, and a stale object gives a working-but-wrong binary.

## Hard rules

- **Never commit original data or anything derived from it byte for byte**: ROMs, disassembly
  output, screenshots, audio captures, emulator state.  `tmp/`, `rom/`, `ref/` and `disasm/`
  (except `symbols.csv`) are local-only.  The chips have NO file extension, so check
  `git status --ignored` before any `git add` that touches new files.
- **Keep the original 68008 instructions.**  Implement the services they call; do not replace
  game decisions with guesses.  Unknown device accesses stay loud stops.
- Preserve every live register and condition code at binary hooks, and check the original bytes
  before patching.  Never map the original memory over Amiga vectors/Exec state.
- Do not call original code from inside an Amiga interrupt.  The VBI updates time/input/Paula,
  and the game's handlers run at safe points.
- 68000 target: **never emit a 32-bit software mul/div** (the `audit` target fails the build).
  RAM is uniformly slow on an A500, so optimise by reducing memory accesses.
- A `__chip` static initialiser is silently discarded (`.MEMF_CHIP` is a BSS hunk).  Fill chip
  data at runtime.

## Working conventions

- **Commit directly to `main`, one cohesive whole per commit.**  Identity is the repo-local
  `Vesa Halttunen <vesuri@jormas.com>`.  No commit signing, no git hooks (`core.hooksPath` is
  pinned to `.git/hooks` locally to bypass the global ones), no co-author lines, and nothing
  employer-specific in this repository.
- Names live in `disasm/symbols.csv` (the source of truth) and `ghidra_scripts/entrypoints.csv`.
  Add a vector/TRAP/IRQ/jump-table target to `entrypoints.csv` the moment it's identified,
  because Ghidra cannot reach those by flow.  Mark evidence (MEASURED/DERIVED/INFERRED) rather
  than treating a plausible name as fact.
- Keep this file small.  New hard-won detail goes in the matching `docs/` file (add an index row).
- Ask the user at genuine decision points (they're an experienced retro-porter and want to
  steer architecture/scope choices).
