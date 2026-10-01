# RAY Pokeri 0.90 release

Version **0.90 (01.10.2026)** is the initial public release. Earlier version
numbers were internal development packages. The distributed [ReadMe](../release/ReadMe)
is the end-user WHDLoad installer document; this page records engineering policy
and qualification. Outstanding performance goals are in
[remaining-work.md](remaining-work.md), not release promises.

## Package and requirements

`make release` clean-builds the release executable and production slave, packages
`dist/RAYPokeri-0.90.lha`, and independently verifies it. The archive contains the
installer drawer icon plus nine files: RAYPokeri, RAYPokeri.slave, RAYPokeri.inf,
Install, Install.info, ReadMe, ReadMe.info, EmptyNVRAM and FreshAccounting.
No ROMs, Kickstart, WHDLoad binary, captured saves, replay or diagnostic markers
are distributed. EmptyNVRAM and FreshAccounting are authored blank save slots.
The standalone download is `dist/RAYPokeri-current/RAYPokeri`.

The end-user requirements specify PAL, 68020 or better, AGA recommended,
WHDLoad 17+, Kickstart 3.1 with its matching RTB, and Installer 43+. Supported
Kickstart pairs are A600 40.063, A1200 40.068 and A4000 40.068. Allow OS/PRELOAD
headroom beyond the slave reservation. The binary and slave also permit a
68000/ECS machine and have passed emulated compatibility tests; those much
slower runs do not change the recommended end-user configuration.

The slave reserves **864 KiB Chip and 1,472 KiB OtherMem**, including the
512 KiB Kickstart image (960 KiB remains for game/OS allocations). It gives the
program a separate 16 KiB stack. The compact guest windows are described in
[architecture.md](architecture.md#native-placement-and-hook-contract).

## Installation, lookup and saving

Installer 43 defaults to Intermediate experience. It asks for a destination
parent and a single source drawer containing the four unpacked 65,536-byte chips:
77POK30, 77POK38, 77POK34 and PARA200J. It checks presence/size before copying;
it does not unpack ZIPs or download data.

An existing RAYPokeri drawer offers Remove/Keep, default Keep. Remove deletes the
whole drawer including saves. Keep updates program files and preserves saves.
When all four ROMs already exist in data/, Reinstall / Use existing defaults to
Use existing and skips the source question. Missing save slots are created;
wrong-sized existing saves cause refusal without overwriting them.

The installed slave and icon live in RAYPokeri/, while the executable, ROMs and
saves live in data/. Standalone ROM lookup checks data/, the current drawer and
legacy rom/, in that order. WHDLoad sets the current drawer to data/ and reads
there directly. A found wrong-sized file is an error. Runtime relocation and
patch-site byte checks remain; startup does not calculate ROM SHA hashes.

There are exactly two current save files: **nvram.bin (32,768 bytes)** and
**accounting.bin (940 bytes)**. Valid accounting uses PKAC0001 plus a CRC;
FreshAccounting uses PKAF0001 and requests original cold initialization.
Esc/left mouse saves and quits. Each file is rewritten in place once, using
one DOS Write standalone or one resload_SaveFile under WHDLoad. There are no
backup files, temporary files or renames; old .bak files are ignored.

**MEASURED:** creating new files through the WHDLoad write cache hung during
exit in low-free-memory fixtures, including a runner-free reproducer. Overwriting
files present before PRELOAD succeeded. The internal WHDLoad cause remains
unidentified. Installer-created fixed-size slots avoid that path, and the
program checks both before takeover. Missing/invalid slots are a loud startup
refusal, not permission to create them. Precise save-and-exit timing is not a
release gate (user decision 2026-09-30).

## WHDLoad interrupt and quit policy

The live service path is unconditionally trace-free, as requested by the user.
Interrupt returns use the checked Line-A service redirect and the approved
software-requested level-2 interrupt. The runner does not change WHDLoad's VBR;
its Emul flags forward original exceptions to the game's low vectors. Bus/address
errors remain loud WHDLoad stops. The POK!SAVE descriptor supplies the save callback
and startup mode; its presence and structure are checked by the slave.

The normal icon sets PRELOAD. **NoVBRMove and NoWriteCache are neither required
nor installed.** Diagnostic replay requires NoVBRMove and refuses a moved VBR
before takeover; normal release builds contain no replay support. Removed
trace-based research modes are not supported options.

**Help is emergency quit without saving; Esc/left mouse saves.** No periodic
checkpoints are implied. With NoVBRMove, WHDLoad cannot supply the Help QuitKey.
The earlier human keyboard test passed with the former F10/C layout. Help's
current header byte is verified automatically; that is not a claim of a new
physical Help keypress test. F10 now means Collect and Enter inserts a coin.

## Qualification and reproducibility

Use [testing.md](testing.md) for current commands. Finite-budget automatic
save/exit tests require a development executable; a release runs until the player
quits and rejects the test runner's automatic-quit setup. Diagnostic builds are
frozen separately so the final amiga/out remains the release build.

**Clean build (2026-10-01):** `RAYPokeri` is 241,952 bytes and the LH5 archive
is **132,197 bytes**. Two clean release builds produced the same archive.
SHA-256: `2861cec31c8839f303ca3ee7c8a7e1696b1148caa71fb7e1d89f2afe311bb2e2`.
The standalone copy matches the packaged executable. Both version strings and
all ten archive members pass the independent audit.

| Current cleanup check | Result |
| --- | --- |
| Host unit/ROM suites | PASS, including relocated scenarios and artwork comparisons |
| Linked CPU oracles | PASS on the aligned build; final raster oracle covers 1,249,920 cases |
| ECS and AGA exact replay | PASS: all 65,536 RAM bytes, 524,288 VRAM bytes, 172,064 pixels and 60 AY writes at 7,903,177 instructions / 64,000,006 cycles / 8,693 IRQs |
| Cold/warm live24, both machines | PASS: status 4, all 24 inputs, zero resets/errors, restored vectors |
| A1200 Double | PASS: accepted round 5, 46 input transitions, clean completion |
| ECS Double | 12 hands without a win; diagnostic limit, zero resets, restored vectors. User accepted AGA coverage and waived another ECS attempt (2026-10-01) |
| A1200 startup VBI | PASS: 793 samples, maximum post-service line 6; required <29 |
| Retained profiling build | PASS: ledger/dispatch/startup/VBI options and alignment audit; normal allocated sections unchanged |
| Installer 43 | PASS: fresh, Keep/Use existing, Keep/Reinstall, Remove, malformed-save refusal |
| WHDLoad cold/warm saves | PASS: production slave, development image, PRELOAD/moved VBR/write cache; both files valid and normal return twice |
| Standalone ROM lookup/saves | PASS: data/ and current-directory ROMs, both files valid and normal return |

The cleanup had also removed the test runner's WHDLoad/slave fixture copies.
Restoring those copies fixed a pre-launch test failure; the production game and
archive did not change. All save checks above were then rerun successfully.

Current live24 ratios are 0.9780 cold / 0.9833 warm on A1200 and 0.2837 /
0.2856 on ECS. A1200 Double is 0.9696 over its longer five-hand workload.
Complete cards and late AY writes still miss their goals; detailed numbers and
qualifications are in [performance.md](performance.md).

Historical scope, not a claim of rerunning every matrix cell on this binary:

- WHDLoad 19.2 was the main emulator configuration. Earlier slot tests also
  covered 17.0 and 20.0; the current slave is not yet requalified on every version.
- The prior CPU/option matrix passed 24 cold/warm pairs across 68020,
  68030+MMU, 68040 and 68060, with PRELOAD on/off and optional NoVBRMove or
  NoWriteCache. Current 68000/ECS coverage is tracked separately.
- Original cash payout and resumed play were validated before this cleanup;
  no coin model changes are part of the cleanup.
- Aggregate near-real-time A1200 timing does not establish constant 50 FPS,
  sub-20 ms card completion or physical-board fidelity.

The release code audit rejects diagnostic entry points/markers and symbol/debug
hunks. elf2hunk strips symbols; the separate ELF/map keeps them for development.
Loop unrolling is explicitly disabled. There is no embedded precomputed audio
bank: runtime Paula buffers replace it.

The archive has deterministic sorted LH5 members and fixed date headers.
Independent Lhasa decompression checks every payload CRC, exact member allowlist,
HUNK/icon headers, slave memory/Help/68000 flags, version strings and identity
with build inputs. Both executable versions, VERSION, Install, ReadMe and the
checker/date metadata must be updated together for a future release.
