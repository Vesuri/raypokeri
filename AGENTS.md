# RAY Pokeri — RAY video poker (68008) → Amiga port

RAY Pokeri runs the original RAY 68008 program natively on the Amiga, with portable
board models and planar/blitter video plus Paula audio. Musashi is host-only.
The current release is RAY Pokeri 0.91 (11.10.2026). Current implementation and
validation procedures are consolidated below; older experiments live in Git history.

**Current work:** [docs/remaining-work.md](docs/remaining-work.md) is the only work
queue. Completed experiments are not instructions to repeat them.

Normal startup skips approved coin-op diagnostics, retains original initialization
and accounting, and uses startup-only fast-forward. Saved accounting survives
ordinary native launches. Gameplay keeps the approved bounded clock and original
interrupts. Normal run.sh has audio; debug runs are muted. The display is 608×292
on SDL and 608×283 on Amiga, cropping five top/four bottom rows. AGA fetches require
chipset detection; ECS compatibility is retained.

## Reference docs

| Doc | Read it for |
| --- | --- |
| `docs/remaining-work.md` | Active gates, remaining outcomes and declined/deferred scope |
| `docs/architecture.md` | Native hooks/interrupts, clock, graphics/audio, input, saves and ownership |
| `docs/performance.md` | Current measurements, alignment rule and every experiment's disposition |
| `docs/testing.md` | Host/linked suites, exact replay, live timing, installer and release procedures |
| `docs/release.md` | Current archive, requirements, WHDLoad memory/save/quit policy and qualification |
| `docs/hardware.md` | Physical-board evidence tagged PHOTO/HV/KH |
| `docs/rom-set.md` | ROM/hardware findings tagged MEASURED/DERIVED/INFERRED |
| `docs/visual-reference.md` | Real-machine footage and variant differences |
| `docs/porting-approach.md` | Design constraints and prior art |
| `amiga/ARCH.md` | Platform takeover, Copper/DMA and Exec interrupt chain |
| `src/platform/amiga/framework/GCC-PORT.md` | Shared cross-toolchain/framework notes |

## Shared environment and knowledge

The toolchain, emulators, Kickstarts, WHDLoad and the Python environment come from AmigaXDev
(`make setup` there); `amiga/env.sh` sources `~/.local/share/amiga/env.sh`. Lessons that hold
for any port live in AmigaXDev's docs: add or correct them there, and keep only what is
specific to this game in `docs/`.

| AmigaXDev doc | For |
| --- | --- |
| `docs/environment.md` | the shared variables and installed files |
| `docs/playbooks/new-port-playbook.md` | the phases of a port and their gates |
| `docs/reference/amiga-hardware.md` | Chip bus, takeover, interrupts, copper, blitter, Paula, AGA, the dA JoRMaS framework |
| `docs/reference/m68k-optimisation.md` | 68000/68020 cost model and code generation |
| `docs/reference/measurement-method.md` | profiling instruments, A/B discipline, pricing a change |
| `docs/reference/validation-harness.md` | faithfulness to the original: references, differentials, sabotage |
| `docs/reference/headless-fsuae.md` | the shared launcher, FS-UAE builds, gdb stub, diag runs |
| `docs/reference/port-toolchain.md` | flags, linking, link audits, WHDLoad slaves, release archives |
| `docs/native-port-validation.md` | crash dumps as regressions, installer updates, release archive checks |

Cross-session in-progress notes live in the auto-memory (`MEMORY.md` is its index).  Stable facts
live here or in `docs/`; memory holds what is still moving.

## Build / run / debug

```
make roms [SRC=…]   # verify + unpack the user's ROM dump into rom/ (repo root)
make program-image  # the three program chips -> disasm/program.bin ($00000-$2FFFF)
cd amiga && . ./env.sh && make   # -> out/RAYPokeri   (source env.sh in the SAME shell command)
./run.sh            # FS-UAE A1200 / KS 3.1 during bring-up; left mouse button quits
./debug.sh          # FS-UAE gdb stub + m68k-amiga-elf-gdb
./diag_run.sh [s]   # headless: run s seconds, then gdb runs the read-only prints in diag.gdb
```

- The toolchain and Ghidra (`tools/ghidra` → `~/.local/share/ghidra`) are shared; don't install
  per-repo copies.
- The launchers take their FS-UAE options from the shared `fsuae_common.sh` (`fsuae_options`):
  debug launchers (`debug.sh`, `diag_run.sh`) and `WARP=1` runs are silent (emulated Paula keeps
  running) with the window behind the others; normal `run.sh` keeps audio.  `SOUND` and `WINDOW`
  (`front`, `back`, `none`) override that.
- **Never `pkill fs-uae` / `pkill gdb`.**  The scripts source the shared `fsuae_common.sh`,
  which kills only this directory's emulator and gives each project its own gdb-stub port
  (AmigaXDev `docs/reference/headless-fsuae.md`).
- The FS-UAE gdb stub serves memory reads but silently drops writes.  Inject test inputs from C
  (a `-D` flag plus a VBI-count window), and keep `.gdb` scripts read-only.  A `.gdb` script
  aborts at the first unknown symbol.
- Do not edit a shell launcher while it is running: the shell can resume reading
  at stale file offsets. Wait for it to terminate, or run a frozen copy.
- `make release` uses `RELEASE=1`: no diagnostic replay, self-tests,
  automatic inputs or `native-*` research markers. Development builds default
  to `RELEASE=0`; clean when switching. Keep the ELF for debugging.
- Development builds omit profiling support unless explicitly enabled. Use `PROFILE_SUPPORT=1` with `native-measure`;
  `DISPATCH_PROFILE=1` and `TIME_LEDGER=1` enable it automatically. Read-only
  `amiga/release-timing.gdb` measures the normal executable without a profiler.
- `make clean` after changing build flags. The Makefile now includes generated header
  dependencies, but a clean rebuild is still appropriate after changing platform layouts.

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
- Even-sized global data objects of four or more bytes must be `alignas(4)`;
  the ELF audit rejects misalignment. Removing unrelated globals must not regress
  68020 bus costs.
- 68000 target: **never emit a 32-bit software mul/div** (the `audit` target fails the build).
  Cost rules: AmigaXDev `docs/reference/m68k-optimisation.md`.
- A `__chip` static initialiser is silently discarded (`.MEMF_CHIP` is a BSS hunk).  Fill chip
  data at runtime.

## Working conventions

- **Commit directly to `main`, one cohesive whole per commit.**  Identity is the repo-local
  `Vesa Halttunen <vesuri@jormas.com>`. No commit or tag signing, no git hooks
  (`core.hooksPath=/dev/null`), no commit template, no co-author/co-created-by
  lines, and nothing employer-specific in this repository.
- **Personal GitHub only:** origin is `git@github.com:Vesuri/raypokeri.git`.
  Repository-local URL routing uses HTTPS with a credential helper explicitly
  selecting the saved **Vesuri** login, independently of the global active CLI
  account. SSH fallback is disabled. Preserve this isolation; never use the
  global CLI account implicitly for repository operations.
- Names live in `disasm/symbols.csv` (the source of truth) and `ghidra_scripts/entrypoints.csv`.
  Ghidra runs take `-scriptPath "ghidra_scripts;$AMIGA_GHIDRA_SCRIPTS"`: `MarkEntries` and
  `ApplyNames` are Pokeri's own, `DumpCallGraph` and `ExportListing` are AmigaXDev's shared scripts.
  Add a vector/TRAP/IRQ/jump-table target to `entrypoints.csv` the moment it's identified,
  because Ghidra cannot reach those by flow.  Mark evidence (MEASURED/DERIVED/INFERRED) rather
  than treating a plausible name as fact.
- Keep this file small.  New hard-won detail goes in the matching `docs/` file (add an index row).
- Ask the user at genuine decision points (they're an experienced retro-porter and want to
  steer architecture/scope choices).
