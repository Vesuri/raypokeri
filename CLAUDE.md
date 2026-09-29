# Pokeri — RAY video poker (68008) → Amiga port

Pokeri runs the original RAY 68008 program natively on the Amiga, with portable
board models and planar/blitter video plus Paula audio. Musashi is host-only.
Phases 0–4 are complete within their documented scopes. Phase 5 devices, direct
boot, persistence and scripted gameplay work; startup speed, card/audio deadlines,
sustained real-time performance and physical calibration remain open. The initial Phase 6 release is packaged; broader game-scope changes are deferred.

**Current work:** [docs/remaining-work.md](docs/remaining-work.md) is the single
current task list, with measurement dates, completion criteria and deferred work.
Read it before treating an older plan's “next” or “pending” paragraph as a task.
Performance documents preserve historical evidence; their old totals are not
current-release measurements. Update the work list when status changes.

Normal startup skips approved coin-op diagnostics, retains original initialization
and accounting, and uses startup-only fast-forward. Saved accounting survives
ordinary native launches. Gameplay keeps the approved bounded clock and original
interrupts. Normal run.sh has audio; debug runs are muted. The display is 608×292
on SDL and 608×283 on Amiga, cropping five top/four bottom rows. AGA fetches require
chipset detection; ECS compatibility is retained.

## Reference docs — READ ON DEMAND (this file stays small on purpose)

| Doc | Read it when |
|---|---|
| `docs/release.md` | Initial 0.1 archive, single-directory ROM installer, WHDLoad vector/save compatibility and release checks. |
| `docs/whdload-compatibility.md` | **Plan (W1–W5):** why NoVBRMove (trace exceptions, missing Emul flags) and NoWriteCache (unexplained exit hang) are needed, trace-free service entry, QuitKey decision, test matrix and performance gate |
| `docs/remaining-work.md` | **Start here:** current open work, completed items, measurement limits and deferred scope. |
| `docs/bringup-plan.md` | Phase scope and status, architecture, fidelity qualifications and later release decisions. Current task order is in remaining-work.md. |
| `docs/native-performance-plan.md` | Performance constraints, approved timing option C, budget model and acceptance gates; dated execution history is not the current task queue. |
| `docs/native-burst-plan.md` | Completed burst experiments and their retained/rejected/opt-in dispositions. Historical timings; current outstanding goals are in remaining-work.md. |
| `docs/card-back-blit-design.md` | **Implemented/default by explicit approval; ECS/AGA exact replay and live24 pass, complete-feed/audio latency targets remain unmet:** build-prepared card-back cache and shared guarded white-card prefix, exact command-sequence recognition, interleaved backing VRAM and one masked four-plane blit; prefix observations/fallback and audio-latency acceptance are explicit |
| `docs/card-cache-preparation.md` | Default build-time card preparation with exact recipe/data proof; paired A1200 cold/warm Ready 24.32/10.46 s including preparation, exact ECS/AGA replay and cold/warm live cleanup pass; original initialization and gameplay deadlines remain open |
| `docs/native-fifo-control-plan.md` | Approved bounded three-write FIFO-control fusion: preserves every original IRQ boundary; 23% isolated saving, CPU/live and exact ECS/AGA replay gates pass; enabled by default; dedicated CCR-low endpoint saves a further 14.4% in the triplet batch with CPU/live, ECS/AGA replay and VBI gates passing; landing/audio deadline remains open |
| `docs/native-video-handler-plan.md` | Default bounded entry/exit fusion: CPU proofs, exact ECS/AGA replay and live cleanup pass; paired cost savings and startup VBI qualification. |
| `docs/native-video-irq-fast-path.md` | Default T5 assembly admission and bounded clock: measured service savings, exact ECS/AGA replay and live/VBI gates pass; entry/tail and whole-card goals remain open. Older C/frame prototypes remain comparison-only. |
| `docs/trace-profile.md` | **Current attribution and ranked plan:** FS-UAE cycle-exact instruction traces (`TRACE_CODE=1`, `amiga/trace.sh`, `host/native_trace.py`) of warm/cold startup, deal/draw/Double and normal-build AY lateness; per-site costs and the 2026-09-29 decisions (T11–T14) |
| `docs/native-dispatch-profile.md` | Measured startup/gameplay dispatcher call distribution and ranked assembly targets |
| `docs/native-rendering-followup.md` | Chronological evidence for startup, artwork, scrolling, persistence, shuffle sound and subsequent optimizations. Current tasks are consolidated in remaining-work.md. |
| `docs/pattern-interleaved-blit.md` | Default single-blit four-plane PTN tiles; 22.5% lower synthetic miss cost, 12 KB extra Chip cache; exact ECS/AGA replay and cold live24/cleanup pass, gameplay deadlines remain open |
| `docs/memory-audit.md` | Allocation ownership, partial-startup/failure cleanup, static-owner Guru fix and regression coverage |
| `docs/rendering-path-audit.md` | Rendering fast/fallback routes, tile-seam fix, blitter setup cache, clear and scrolling measurements |
| `docs/live-envelope-clock-experiment.md` | Approved live PAL-clock envelopes; measured Double decay, comparison marker, replay isolation and separately outstanding sound-write delays |
| `docs/double-transition-performance.md` | Double-entry card redraws and held-note attribution; separately proved all-white-border cache case and before/after latency evidence |
| `docs/input-response.md` | Confirmed short-press loss before ROM sampling, hold-ready gate and measured idle-loop workload; read-acknowledged transition queues implemented and validated |
| `docs/native-dispatch-separation.md` | Measured instruction-executor separation, common dispatcher cost and normal-game/replay validation. |
| `docs/native-profile-build.md` | Normal builds omit dormant profiling branches; explicit profiling builds, controlled feeder and normal-game timing evidence. |
| `docs/startup-interrupt-latency.md` | Calibration masking explains the late first VBI; bounded between-sample interrupt window and read-only entry/service measurements. |
| `docs/native-presentation.md` | Original tick-completion refresh requests, graphics-drain/card boundaries and VBI publication; replaces independent cycle/card-hit presentation timers |
| `docs/shuffle-pacing.md` | Default consumer-paced shuffle with original sound scheduling, bounded marker queue, exact ECS/AGA frames/replay, historical producer-wait comparison and remaining physical calibration |
| `docs/startup-fast-forward-design.md` | Approved/default startup-only fast-forward, its timing contract and validation history; current elapsed-time qualification is in remaining-work.md. |
| `docs/startup-policy.md` | Approved diagnostic bypass, acknowledged cabinet setup, zero-credit cold startup and research overrides; dated startup measurements. |
| `docs/native-clock.md` | Approved bounded live clock, paired calibration, assembly status boundaries and validation limits |
| `docs/phase5-amiga.md` | Native planar storage/blitter, Paula loops/envelopes, direct boot, explicit replay handoff, controls, persistence and validation |
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
- Normal builds omit profiling support. Use `PROFILE_SUPPORT=1` with `native-measure`;
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
