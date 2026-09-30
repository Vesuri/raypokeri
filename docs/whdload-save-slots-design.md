# Proposed first-install save slots for WHDLoad

2026-09-30. **Approved by the user; implementation and qualification in progress.** This is a candidate
for W1, not a claim that the cache root cause is completely understood.

## Evidence and problem

In the local kickemu configuration, WHDLoad 19.2 and 20.0 can return success
from both save calls and then loop indefinitely on a newly created cached file
at exit. The authored reproducer's retained marker proves it reached stage 60
(after both saves and CloseLibrary). NoResInt, moved VBR, whole-file SaveFile,
skipping old-file reads and bypassing DOS teardown do not remove the failure.
Save-only and all-file ws_DontCache patterns also fail in this configuration.

**MEASURED:** the same callback with all four files already present passes two
runs, including exact backups. The normal-game repeated-run gate is recorded
in whdload-compatibility.md. This supports testing an overwrite-only strategy;
it does not establish a general WHDLoad defect or prove placeholder behavior.

## Approved change

The installer prepares four fixed-size slots before WHDLoad starts:

- `data/nvram.bin` and `data/nvram.bak`: 32,768 zero bytes, exactly matching the
  board model's existing no-file initial NVRAM contents.
- `data/accounting.bin` and `data/accounting.bak`: the existing accounting image
  size, with a new explicitly authored fresh-state marker and zero padding.

No original ROM bytes, saved gameplay state or derived artwork are distributed.
Templates are generated during packaging, not committed as NVRAM images. An
existing save is never silently replaced. The installer retains its current
explicit target-replacement confirmation; a repair/initialization helper, if
needed for existing installations, creates missing slots only.

The loader accepts exactly the complete fresh accounting marker/padding as
"no retained accounting loaded". It leaves game RAM untouched and runs the
same original initialization/reserve accounting as an absent file. Other bad
images remain errors. Ordinary valid accounting format remains unchanged.
The first successful save replaces the fresh slot with ordinary encoded
accounting; the backup remains the complete preceding image. Restoring a fresh
backup has the explicit meaning of a fresh start.

The WHDLoad save path retains supported DOS operations and the complete-image
backup guarantee, with constant lengths. Missing slots must be diagnosed before
creating new cached files: do not silently reintroduce the known hang. Standalone
AmigaDOS and SDL must retain their normal missing-file behavior. The user should
not need to understand cache options or binary save formats.

## Gates before adoption

1. Host tests for exact fresh marker recognition, damaged/truncated/oversized
   images, unchanged valid-image decoding, and unchanged RAM on fresh input.
2. Prove absent-file versus fresh-slot cold startup equivalence: original
   initialization executes, Ready credits/bet/reserve match, and retained state
   at the same board checkpoint matches. No default filled-accounting image.
3. WHDLoad PRELOAD/cache on: truly fresh slots, warm saves, existing saves and
   backups; at least three cold fixtures and three warm repeats. Verify exact
   previous-image backups and normal return. Include the supported-version
   matrix; 19.2/20.0 results alone cannot prove WHDLoad 17+ compatibility.
4. Normal game cold/warm live, cleanup and save/load checks, plus startup-time
   comparison. Game/graphics/clock behavior remains unchanged.
5. Installer/release tests: generate all slots, preserve existing data in the
   approved installation flows, reject malformed slots without overwrite,
   and verify ROM lookup and ordinary standalone operation.
6. Only after these pass consider removing NOWRITECACHE from the installer,
   icon and end-user ReadMe. NOVBRMOVE remains separate W2/W3 work.

Alternative: retain the documented NOWRITECACHE requirement while pursuing the
external cache issue. That relaxes the current goal of default-option operation
and therefore also requires the user's decision. No external report is sent
without explicit authorization.

## Fresh-marker loader qualification (2026-09-30)

**Implemented:** the shared fixed-size accounting image recognizes exactly
`PKAF0001` followed by 932 zero bytes as an authored fresh slot. Host and Amiga
loaders require the exact file length first, leave RAM untouched and keep the
retained-accounting flag false. Ordinary `PKAC0001` saves/CRC checks are unchanged.
This is loader groundwork; installer generation and missing-slot protection are
not yet implemented and NOWRITECACHE remains the release default.

**MEASURED:** the unit matrix rejects every single-bit mutation of both valid
accounting and fresh markers before changing RAM. Original-code host boots with
fresh slots versus absent files have identical full RAM, VRAM, NVRAM and saved
accounting, reach Ready with zero credits and the original 100-coin reserve.
Truncated/oversized/damaged marker files fail without overwrite. Existing warm,
relocation and in-progress-hand recovery checks pass. The Amiga build and
software-mul/div audit pass. Evidence: `/tmp/pokeri-w1-fresh-check.log`,
`/tmp/pokeri-w1-fresh-native-build.log`, `host/accounting_check.py`.

**MEASURED native cached-write control:** WHDLoad 19.2 with PRELOAD, NOVBRMOVE
and default write caching passes one truly fresh-slot launch followed by two
warm launches. Each returns normally and both backups equal the exact preceding
images, including the authored marker on the first save. Frozen executable:
`tmp/w1-fresh-native`; fixture `tmp/whdload-test-pdun_4b8`;
log `/tmp/pokeri-w1-fresh-native-check.log`. This is one cold fixture, not the
three-cold-fixture/version/option matrix required before adoption.

## Installer and startup protection (2026-09-30)

**Implemented:** packaging generates `EmptyNVRAM` (32,768 authored zero bytes)
and `FreshAccounting` (940-byte authored marker). The installer validates all
existing slot sizes before initializing missing files. It never overwrites a
present save/backup; a missing backup is copied from its current save. A new
installation gets four complete slots. A malformed existing file is preserved
and reported. The ReadMe explains the Keep repair path.

WHDLoad live startup checks all four slots before board/display allocation:
NVRAM lengths, and full accounting marker/CRC validity. Failure returns code 22
through the slave with an explicit repair message; no cached file is created.
Standalone missing-file behavior and diagnostic replay remain unchanged. Test
fixtures now model the installer, with separate missing/invalid negative modes.

**MEASURED:** real Installer 43 fresh, Keep, Remove and malformed-size flows
pass. Existing NVRAM and its distinct backup survive Keep, while absent backups
are initialized. Cached WHDLoad rejects missing NVRAM backup and corrupt
accounting backup without changing any save. One fresh plus two warm cached
launches pass all saves/exact backups/normal exits with the preflight enabled.
The Amiga build and integer audit pass. An independently decoded development
archive has exactly ten allowlisted members, including only authored templates;
all payloads/checksums match. No ROM or retained gameplay image is packaged.

Evidence: `/tmp/pokeri-w1-installer-slots-final.log`,
`/tmp/pokeri-w1-slot-{missing,invalid}.log`,
`/tmp/pokeri-w1-slots-cache.log` (fixture `tmp/whdload-test-mdvt1s7y`),
`/tmp/pokeri-w1-slots-build.log`, development archive `tmp/w1-package/Pokeri-0.1.lha`.
NOWRITECACHE/NOVBRMOVE remain release defaults until the outstanding matrix gates
pass; the development archive is not a new published release.

## Version and PRELOAD matrix (2026-09-30)

**MEASURED:** with the current preflight and installer-style fresh slots,
NOVBRMOVE and default write caching, WHDLoad 19.2 (build 6941) and 20.0
(build 7051) each pass three independent fresh installations and one warm
repeat per installation: six clean returns and six exact-backup checks each.
WHDLoad 17.0 (build 5139) passes one fresh installation and two warm repeats.
All runs use the finite 96M-cycle normal game; this is persistence/cleanup
coverage, not an accepted-Double or final moved-VBR performance measurement.

17.0 was obtained from the [official old-version archive](https://www.whdload.de/whdload/old-whdload/WHDLoad_17.0_usr.lzx)
and extracted only under `tmp/whdload-17-control`; the shared installation is
unchanged. Test executable SHA-256 values:

- 17.0: `bede997a689dff5a142f810da9db2cf0567b227496e252c83c84a16142835625`
- 19.2: `a31ad5d1ffae918311a0e8901877299d84b63e4c657f05c897801f37f14b8cec`
- 20.0: `5e66d140e12b09769ba0bba87dc5a6f683261fd40eb43b94c390d1c62db1adb5`

PRELOAD-off 19.2 passes one fresh and two warm launches with default caching,
and a separate fresh plus two warm launches with NOWRITECACHE. All six return
normally with exact backups.
Evidence: `/tmp/pokeri-w1-cache-matrix-{17,19,20}.log` and
`/tmp/pokeri-w1-no-preload-matrix.log`; every log retains the fixture paths.

**Still open:** complete the repeated uncached/PRELOAD control counts and
measure emulated exit duration through WHDLoad's final cache flush. Host warp
elapsed time is not that measurement; timing only the game's save calls would
miss the final flush. Release options therefore remain unchanged. The final
W5 CPU/moved-VBR and performance gates are separate.
