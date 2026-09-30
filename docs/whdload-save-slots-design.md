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
