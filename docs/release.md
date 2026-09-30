# Amiga release: Pokeri 0.3

The user requested an initial release on 2026-09-29, keeping the current game,
coin/credit and operator behavior. Performance and physical-fidelity work remains
in [remaining-work.md](remaining-work.md); this package does not claim 50 FPS.

## Contents and installation

`make release` builds the ordinary Amiga executable and WHDLoad slave, packages
`dist/Pokeri-0.3.lha`, then audits it with independent Lhasa decompression and
header/payload checksums. The nine drawer contents are Pokeri, Pokeri.slave,
Pokeri.inf, Install, Install.info, ReadMe, ReadMe.info, EmptyNVRAM and
FreshAccounting, plus a drawer icon. The two save templates are authored empty
slots; they contain no played-game state. There are no ROMs, Kickstart images,
diagnostic markers or replay files. The local compressor is LHa for UNIX (`lha-compress`, or the `LHA`
environment override); the independent checker uses Lhasa's `lha` command.
The version strings in both executables must match VERSION and the release date.

Installer 43 asks for a destination parent and one directory containing the four
unpacked chips. It validates new source file sizes before copying. An existing
Pokeri drawer offers Remove/Keep, defaulting to Keep; Keep updates binaries and
preserves saves. If all four chips already exist under data, no ROM-source
question is asked. Remove explicitly includes saved credits/accounting. No ZIP
unpacker or ROM download is involved.

The native loader searches data/, the current drawer, then legacy rom/, for each
chip. A found file with the wrong size is an error, not a reason to fall through
to another copy. Existing relocation/patch-site checks remain; startup SHA
calculation has not been reintroduced. Saves are relative to the current drawer.
See [the distributed ReadMe](../release/ReadMe) for controls and manual layout.

## WHDLoad compatibility

The kick31/kickfs slave reserves 1 MB Chip and 4 MB Fast plus the 512 KB Kickstart
image. Initial WHDLoad support requires PAL and 68020+, with no AGA requirement.
The tested configuration is A1200, 2 MB Chip and 8 MB Fast, JIT disabled, Kickstart
3.1 and WHDLoad 19.2 build 6941. The loader supplies a 16 KB application stack
using Exec StackSwap and preserves the DOS program directory and arguments.

A retained, validated `POK!SAVE` descriptor selects WHDLoad startup behavior;
it changes no original ROM instruction or gameplay timing. Live interrupt
returns enter the shared scheduler through the validated Line-A redirect stub,
including the approved software-requested level-2 service interrupt. The
runner does not replace WHDLoad's VBR. NoVBRMove and NoWriteCache are optional;
neither is installed or required. Diagnostic replay and deliberately selected
trace-based research modes still require NoVBRMove and refuse a moved VBR.

Esc or left mouse exits normally and saves. Help is WHDLoad's emergency exit
without saving; physical key and persistence checks pass. If a user explicitly
enables NoVBRMove, WHDLoad cannot provide its Help emergency exit.

**DERIVED from kickfs.s:** ACTION_RENAME_OBJECT is unsupported. The WHDLoad save
path therefore copies the previous complete image to `.bak`, then writes the
new `.bin`, using supported Open/Read/Write/Close operations. It checks exact
lengths and errors, refusing to replace a malformed existing save. Normal
AmigaDOS retains its `.new`/Rename path. Both `nvram.bin` and `accounting.bin`
are supported; backups are not automatically restored over a damaged primary.

**MEASURED:** the former cached new-file creation hang is avoided by the
approved installer-created blank save slots. All four primary/backup files
exist before PRELOAD; the loader validates them before takeover. Cached and
uncached cold/warm tests, PRELOAD on/off and supported-version tests pass with
exact backups and normal return. NoWriteCache is now optional and is no longer
set by the installer. Precise exit timing is not a release gate, by the user's
2026-09-30 decision. Original cold initialization is retained.

The user approved and required trace-free default activation on 2026-09-30.
The differing-hand worst-case AY comparison remains a documented performance
limitation, not a release blocker; see [WHDLoad compatibility](whdload-compatibility.md).

## Verification

- Actual Installer 43, isolated FS-UAE fixtures: fresh, Keep and Remove pass.
  Checks cover binaries, all ROMs, required tooltypes, skipped ROM-source prompt
  on Keep, save preservation/deletion and an unrelated drawer remaining intact.
- Production game/slave: finite native-live budget, cold then warm launch;
  normal return, 32,768-byte NVRAM and retained accounting written. On the warm
  exit, both backups equal the previous files byte for byte. No test slave or
  native-live marker is packaged.
- Standalone data/current-directory lookup and saves pass in the same
  release runner. Debug emulator output is muted; normal launches retain audio.
- The archive checker verifies the allowlisted members against their build
  inputs, both version strings, Amiga HUNK/icon headers and all checksums.

Reproduce local tests after sourcing amiga/env.sh:

```
python3 tools/test_release_installer.py
python3 tools/test_whdload.py --rom /path/kick40063.A600 \
  --rtb /path/kick40063.A600.RTB --seconds 150 --repeat 2
python3 tools/test_whdload.py --standalone data --seconds 150
python3 tools/test_whdload.py --standalone current --seconds 150
make release
```

These tests use the user's local Workbench/Installer/Kickstart files, keep their
fixtures in ignored tmp/, and terminate only the emulator process they started.
They are release/persistence checks, not a new whole-game performance benchmark.

## 0.2 package verification (2026-09-30)

`make release` completed with a normal, profiler-free build. Both $VER strings
read 0.2 (30.09.2026). The 206,301-byte LH5 archive contains ten allowlisted
members including the drawer icon; independent Lhasa decompression, header and
payload checksums, build-input identity and save-template checks pass.
`dist/Pokeri-current/Pokeri` matches the packaged executable. Both compatibility tooltypes are optional; the normal executable now uses
the approved trace-free live service path. Prior correctness/performance qualifications remain unchanged.

## 0.3 cabinet keyboard layout (2026-09-30)

The user-supplied cabinet photograph and button labels define the two gameplay
rows: F1–F5 hold cards; F6–F10 Double/Low/High/Bet/Collect; Space Deal. Enter
inserts a coin. Previous number/letter/arrow gameplay aliases and C are removed.
Delete/O/L replace the former service function keys; Escape still saves and
quits. The slave's emergency key is Help (raw $5F), avoiding F10 Collect.
The archive checker verifies the actual ws_keyexit header byte. Earlier W4
physical F10 tests validate the prior release; they are not a physical Help
keypress test. The persistence policy and handler implementation are unchanged.

**MEASURED:** release 0.3 builds and audits successfully (206,415 bytes).
Its native live24 run uses the remapped keys and finishes all events without
error/reset, with restored vectors. The standalone download is refreshed from
the packaged executable. Evidence: `tmp/release-0.3-build.log` and
`tmp/function-keys-live24-summary.txt`.
