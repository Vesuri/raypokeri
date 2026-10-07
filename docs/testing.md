# Testing RAY Pokeri

Commands below run at the repository root unless stated otherwise. Use clang and
Apple GNU Make 3.81 for host checks; source `amiga/env.sh` in the same shell as
cross-toolchain commands. ROMs, replay/state files, screenshots and traces stay
in ignored `rom/`, `tmp/` or `amiga/.run/`. Never commit their bytes.

## Suites and build ownership

```sh
make harness-unit                 # synthetic CPU/device/platform/memory tests
make harness-rom                  # original-ROM scenarios, relocation and artwork
. amiga/env.sh
make -C amiga clean
make -C amiga -j8                  # RELEASE=0, normal development implementation
make harness-elf                  # actual linked assembly against CPU oracles
```

`harness-elf` checks both 68000 and 68020 where applicable: flags, stacks, allowed
stores, guards, IRQ promotion, clock charges and fallback paths. A CPU oracle
passing does not establish DMA-era timing. Drawing changes also need native
replay and representative live checks. Sanitizer memory tests exercise ownership,
partial initialization, teardown and the small freestanding containers.
Do not run the SDL window-memory launcher as routine automation on this host;
it produced the unwanted library/crash dialogs reported by the user.

There is one owner of `amiga/out` at a time. Clean after changing flags. Freeze
both RAYPokeri and RAYPokeri.elf before starting a long validation, and use
POKERI_EXE/POKERI_ELF to point at them. Do not rebuild that ELF under a running
oracle. Do not edit a launcher while it runs. Never pkill fs-uae or gdb: launchers
track their own process IDs and ports. Only one run may own a run directory;
platform-diag also uses shared dump filenames, so replay captures run serially.

## Exact native replay

`tmp/layout.replay` is the compact-window fixture. It is recorded on demand
rather than retained; the command below reproduced it byte for byte on
2026-10-06. Its endpoint is 7,903,177 instructions, 64,000,006 cycles and
8,693 IRQs. Build the host and run:

```sh
build/pokeri-host --devices --serial-peer --system-hz 100 --input-hz 50 \
  --watchdog-ms 400 --watchdog-reset-us 50000 --ay-clock 1000000 \
  --palette-rom 0 --stall-instructions 100000000 --bypass-module-checksums \
  --skip-hardware-tests --auto-setup --instructions 7903177 \
  --record-replay tmp/layout.replay --out tmp/layout-reference
```

Verify the endpoint rather than assuming future model changes preserve it.
Stage four chips in a fresh `amiga/.run/<name>/dh1/rom/`, copy the replay to
`dh1/replay.bin`, and write 64,000,006 as four big-endian bytes to `dh1/native-live`
to stop at the replay-to-live boundary. From `amiga/`, with env.sh sourced:

```sh
POKERI_RUN_DIR=.run/<name> POKERI_REPLAY=1 AMIGA_MODEL=A1200 \
  GDBSCRIPT=platform-diag.gdb ./diag_run.sh 1500
```

Repeat separately with A500+. Diagnostic launchers mute host audio; normal
run.sh remains audible. Copy the shared `tmp/native-platform-boot-{ram,vram,screen}.bin`
to a unique prefix after each run and before starting the next replay.
Then, at the root:

```sh
python3 host/native_check.py --live-boot --skip-hardware-tests --auto-setup \
  --log amiga/.run/<name>/gdb-out.log --ram tmp/<name>-ram.bin \
  --out tmp/<name>-reference
python3 host/planar_capture_check.py --native tmp/<name> \
  --host tmp/<name>-reference --log amiga/.run/<name>/gdb-out.log
```

All 65,536 native RAM-window bytes, 524,288 VRAM bytes, 172,064 cropped pixels
and 60 AY writes must match. Require restored vectors and no native error.
Older 256 KiB RAM/576-pixel fixture figures are obsolete. Equality uses shared
models and must not be presented as validation against the physical machine.

## Live gameplay and timing

For cold live24, create a fresh run drive with ROMs, empty `native-test-inputs`
and a four-byte big-endian `native-live` budget of 480,000,000. There must be no
saves. For warm validation, copy the cold run's `accounting.bin` and `nvram.bin`
into a separate drive; preserve the original fixture.

```sh
# from amiga/, env.sh sourced
POKERI_REPLAY=0 POKERI_RUN_DIR=.run/<name> AMIGA_MODEL=A1200 \
  GDBSCRIPT=release-timing.gdb ./diag_run.sh 900
# from the repository root
python3 host/release_timing.py amiga/.run/<name>/gdb-out.log
```

Require status 4, all 24 input transitions, no error/reset and vectors restored.
The 2026-10-01 qualification results are recorded in [release.md](release.md).
Run cold/warm on A1200 and A500+; allow longer for ECS. Report board/PAL ratio,
Ready frame, full card intervals and AY excess gaps. ECS performance is much
slower and is not judged against the A1200 ratio. Normal PAL timing comes from
emulated frame/beam positions, not host elapsed time; warp is useful for correctness.

For an accepted Double, clean-build `DOUBLE_SCENARIO=1`, freeze the exe/ELF,
use `native-test-inputs`, omit the finite cycle budget, and run `release-double.gdb`.
Reduce with `host/release_timing.py --scenario double LOG`; require `done=true`,
`failed=false`, an accepted Double and clean return. Hands may differ. The driver
stops after 12 hands without a win; that is a sampling limit, not evidence that
the Double transition failed. For the 2026-10-01 ECS qualification the user
explicitly accepted AGA Double coverage instead of another sample.
Payout uses `PAYOUT_SCENARIO=1`, `payout.gdb` and a 3,000,000,000-cycle ceiling;
it checks win → Collect → payout → coin → next deal without injecting balances.

Build `VBI_LATENCY=1` for `startup-vbi.gdb`. On the A1200 qualification, no
post-service sample may reach line 29. `STARTUP_PROFILE=1` and
`startup-elapsed.gdb` measure preparation/init separately. Keep these diagnostic
builds out of the release. Other retained options are PROFILE_SUPPORT,
DISPATCH_PROFILE, TIME_LEDGER, TRACE_CODE and WHD_DEBUG_MAP; removed experimental
switches and benchmark markers are not supported. `native-measure` requires
profiling support. Research clock overrides are not approved release defaults.

## Installer and persistence

The installer test drives actual Installer 43, not an imitation. It covers fresh,
Keep/Use existing, Keep/Reinstall, Remove and malformed-save refusal. Reuse must
skip the ROM-source question; Keep must preserve saves and unrelated files.
Noninteractive viewer stand-ins exercise MultiView success and More fallback
through actual Installer run calls, and the fixture checks WHDLInstPath reuse
and persistence. Installer 43 must skip the newer drawer-opening command.

With a development executable and cross tools on PATH. Common Amiga files come
from `~/.local/share/amiga`: `KICKSTART` (default `Kickstarts/kick40063.A600`),
`WORKBENCH_ADF` (default `Workbenchv2.04rev37.67Workbench.adf`), `INSTALLER43`
(default `Installer43/Installer`) and WHDLoad (`WHDLoad/C/WHDLoad`); set the
variables to use other copies.

```sh
make -C whdload
python3 tools/test_release_installer.py
python3 tools/test_whdload.py --rom /path/kick40063.A600 \
  --rtb /path/kick40063.A600.RTB --seconds 150 --repeat 2
python3 tools/test_whdload.py --standalone data --seconds 150
python3 tools/test_whdload.py --standalone current --seconds 150
```

WHDLoad defaults are PRELOAD, moved VBR and write cache. Test slots must exist
before PRELOAD. Successful save/exit creates valid 32,768-byte NVRAM and
940-byte accounting; there are no new backups. Negative missing/malformed-slot
tests must leave files unchanged. `--gameplay`, `--fast 4096`, `--cpu 68000
--model A500+`, optional NoVBRMove/NoWriteCache/PRELOAD-off and supported CPUs
extend the matrix as appropriate. The historical matrix is not a claim that every
option was rerun for each documentation-only change. Help emergency quit requires
human verification; Esc/left mouse save normally. Automated input playback has
previously truncated key sequences, so do not mislabel a timeout as a key test.

## Release build

```sh
make release
make release-check
```

This cleans the Amiga build, builds RELEASE=1, assembles the production slave,
packages the archive, then independently decompresses/audits the exact allowlist,
CRCs, versions, HUNK/icon headers and build-input identity. The executable audit
rejects diagnostic markers and symbol/debug hunks.
Release images ignore research markers and cannot run finite-budget automatic
quit tests; do those with a development image, then restore the release build.
Refresh `dist/RAYPokeri-current/RAYPokeri` from `amiga/out/RAYPokeri` manually.
Record archive size/SHA and qualification scope in [release.md](release.md).

## Cleanup policy

Reduce logs before deleting them and preserve conclusions in versioned docs.
The 2026-10-01 cleanup removed 596 requested paths (1.45 GiB), after reducing
their validation logs. Hash checks confirmed the retained fixtures were unchanged.
`tmp/` holds only reproducible scratch, so it can be emptied whenever no run
is active. Each producer recreates its own inputs: `tools/native_tables.py`
re-exports `m68000-cycles.bin`, `harness-face-up-check` rewrites
`faceup-catalog.words`, `tools/card_back.py` reruns its catalog pass, and the
command above records `layout.replay`. The user's ROM dump lives in
`ref/pokeri-rom.zip` (git-ignored), the default source for `make roms`.
Do not remove active run directories or another task's files/processes.
