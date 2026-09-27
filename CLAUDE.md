# Pokeri — RAY video poker (68008) → Amiga port

Porting the Finnish RAY video poker machine *Pokeri* to the Amiga from its four EPROM dumps
alone — no schematic, manual or MAME driver.  The board is a **68008 + HD63484 ACRTC + AY-3-8912**.
The original 68008 instructions run **natively** on the Amiga's 68000, as in the Vette port
(`~/Documents/Vette`).  The port supplies relocation plus Amiga implementations of the video
(bitplanes/blitter), sound (Paula) and I/O the code talks to.  **Research stage**: the memory
map is largely known. The host renders game/service screens, plays and doubles a hand,
and replays full snapshots deterministically. Phase 3 relocation is verified at two
placements through strict access tables. Phase 4 is complete under the approved
diagnostic scope: native boot and full-RAM equality pass. Phase 5 planar
video and Paula backends are implemented: paired RAM/VRAM/frame/AY checks pass,
and explicit replay boot continues into live VBI timing. Normal runs now skip coin-op hardware diagnostics, retaining initialization and using acknowledgement-driven cabinet setup shared with SDL. They boot directly without replay or SHA hashing; direct boot now completes cold setup and accepts coin/Deal. The moving-window fix now completes coin/deal/hold/draw with ECS and AGA fetches on A1200; real-time performance remains open (recent 60 game-seconds: 75.96–89.68 sampled PAL seconds). Guarded assembly covers common CPU controls, TRAPs, PIA/ACIA moves and common video writes; normal cabinet messages wait for the same ROM link-idle boundary as startup. Moving-window frames now retain each buffer’s background (about 80% cheaper isolated A1200 composition with raster DMA), and prepared planar point drawing reduces curve costs; see native-clock.md for measured limits. The approved bounded wall/guest clock supplements guest execution with conservatively capped wall time (K=1.5 boot, calibrated K=4 after ready, subject to CPU probes); services permit Amiga IRQs while measured guest time is paused; completed Copper lists publish without a strobe and retire at the next VBI. AGA fetches require chipset detection and retain the ECS fallback. Active-window solid fills, tall clears and opaque PAINT spans of at least 16 pixels use blits; small PTN tiles are expanded once per cached pattern/colour/alignment and drawn with planar masks. See Phase 5 notes before continuing. Physical
palette/clock/audio validation remains qualified in the plan.

## Reference docs — READ ON DEMAND (this file stays small on purpose)

| Doc | Read it when |
|---|---|
| `docs/bringup-plan.md` | **Start here.** Status, pending user decisions, and the phased plan: Musashi harness (done) → boot to idle (done on the 512 KB video path) → **reference output (implemented; calibration open)** → relocation/hook tables derived by running → the original code on the Amiga, gated by RAM-state equality with the harness |
| `docs/native-performance-plan.md` | Staged Phase 5 performance recovery: evidence and budget model, **approved timing option C; FIFO semantics unresolved**, observer-free measurement, fast access hooks, **verified command-feed fusion enabled by default (10% pair saving; real-time gate still open)**, scheduling, shifted blits, startup policy and acceptance gates |
| `docs/native-burst-plan.md` | **Current gameplay performance plan**: time-ledger profile of the card deal (per-command costs, stalls, hook residual), staged drawing/presentation/hook/scheduling plan and its pending decisions; **A1–A6 accepted: drawing/front-end changes measured with exact ECS/AGA replay; B2 accepted; full-frame B1 rejected pending B3; C1/C2/D1/D2 experiments authorized** |
| `docs/native-dispatch-profile.md` | Measured startup/gameplay dispatcher call distribution and ranked assembly targets |
| `docs/startup-policy.md` | Approved hardware-test bypass, shared acknowledgement-driven operator setup, zero-credit startup and research overrides |
| `docs/native-clock.md` | Approved bounded live clock, paired calibration, assembly status boundaries and validation limits |
| `docs/phase5-amiga.md` | Native planar storage/blitter, offline AY noise/mixed loops with live envelopes, hybrid boot, controls, persistence and validation |
| `docs/phase4-preflight.md` | Approved native execution design, full-RAM validation results, diagnostic run procedure, and the live-pacing gate deferred to Phase 5 |
| `docs/phase3-relocation.md` | Relocation/hook tables, the authorized temporary checksum bypass, strict address guards, two-base verification, and coverage limits |
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
./run.sh            # FS-UAE A1200 / KS 3.1 during bring-up; left mouse button quits
./debug.sh          # FS-UAE gdb stub + m68k-amiga-elf-gdb
./diag_run.sh [s]   # headless: run s seconds, then gdb runs the read-only prints in diag.gdb
```

- The toolchain (`~/.local`) and Ghidra (`tools/ghidra` → `~/.local/share/ghidra`) are SHARED
  with the other Amiga projects.  Don't install per-repo copies.
- Debug launchers (`debug.sh`, `diag_run.sh`) use `SDL_AUDIODRIVER=dummy`
  to silence host output while retaining emulated Paula. Normal `run.sh` keeps audio.
- **Never `pkill fs-uae` / `pkill gdb`.**  The scripts source
  `~/.local/share/amiga/fsuae_common.sh`, which kills only this directory's recorded pid and
  gives each project its own gdb-stub port.
- The FS-UAE gdb stub serves memory reads but silently drops writes.  Inject test inputs from C
  (a `-D` flag plus a VBI-count window), and keep `.gdb` scripts read-only.  A `.gdb` script
  aborts at the first unknown symbol.
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
- 68000 target: **never emit a 32-bit software mul/div** (the `audit` target fails the build).
  RAM is uniformly slow on an A500, so optimise by reducing memory accesses.
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
  Add a vector/TRAP/IRQ/jump-table target to `entrypoints.csv` the moment it's identified,
  because Ghidra cannot reach those by flow.  Mark evidence (MEASURED/DERIVED/INFERRED) rather
  than treating a plausible name as fact.
- Keep this file small.  New hard-won detail goes in the matching `docs/` file (add an index row).
- Ask the user at genuine decision points (they're an experienced retro-porter and want to
  steer architecture/scope choices).
