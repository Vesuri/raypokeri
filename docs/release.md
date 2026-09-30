# Initial Amiga release: Pokeri 0.1

The user requested an initial release on 2026-09-29, keeping the current game,
coin/credit and operator behavior. Performance and physical-fidelity work remains
in [remaining-work.md](remaining-work.md); this package does not claim 50 FPS.

## Contents and installation

`make release` builds the ordinary Amiga executable and WHDLoad slave, packages
`dist/Pokeri-0.1.lha`, then audits it with independent Lhasa decompression and
header/payload checksums. The seven drawer contents are Pokeri, Pokeri.slave,
Pokeri.inf, Install, Install.info, ReadMe and ReadMe.info, plus a drawer icon.
There are no ROMs, Kickstart images, saved state, diagnostic markers or replay
files. The local compressor is LHa for UNIX (`lha-compress`, or the `LHA`
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
it changes no original ROM instruction or gameplay timing. Under WHDLoad the
native runner uses low vectors and does not replace VBR. The installed
**NoVBRMove** tooltype is necessary for its trace exceptions. Without it, the
trial stopped with a WHDLoad Trace exception. Esc or left mouse exits through
the game and saves. The slave explicitly selects F10 as an emergency exit,
which does not save current progress and is unavailable with NoVBRMove.
Actual F10/Esc keypress and persistence checks pass on the trace-free moved-VBR
candidate (W4); NoVBRMove remains required pending its performance gate.

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

Removing NoVBRMove remains subject to trace-free live service performance
checks in [WHDLoad compatibility](whdload-compatibility.md).

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
