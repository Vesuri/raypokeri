# WHDLoad without NoVBRMove and NoWriteCache

**Status 2026-09-29: plan, not implemented.** Release 0.1 requires both
tooltypes ([release.md](release.md)). The goal is to run and save correctly with
WHDLoad's default options on every supported system (PAL, 68020 or better,
WHDLoad 17+), with no significant performance cost. Both tooltypes then become
optional and are removed from the icon, installer and ReadMe. The open items are
W1–W5 in [remaining-work.md](remaining-work.md).

## Why each option is needed today

### NoVBRMove: trace exceptions

**DERIVED from the code:**
- `nativeInstallVectors` (`Native.cpp`) writes the low vector table under
  WHDLoad. It installs handlers for bus and address error, illegal, zero divide,
  CHK, TRAPV, privilege violation, trace (`$24`), Line-A, Line-F, TRAP #0–15
  and autovectors 2, 3, 4 and 6.
- Live execution uses trace in two places:
  1. **Interrupt return.** The level 2/3/4/6 wrappers (`nativeLevel2`…`6` in
     `NativeEntry.s`) set T in an interrupted user-mode frame before chaining to
     Exec. One guest instruction later the trace exception enters the
     dispatcher. The dispatcher then delivers VBI time, board ticks, virtual
     IRQs and input at a guest instruction boundary. CIA-A timer A (the guest
     clock's budget expiry) uses the same path.
  2. **Pending-tick resume.** The dispatcher resumes with T set while a board
     tick is pending but cannot be delivered (`liveTicks && !liveIrqActive`,
     `Native.cpp`). Short paths copy that SR into their return frame
     (`nativePhysicalResume`).
- Diagnostic and replay modes also single-step with trace (`nativeFastBoundary`).

**DERIVED from the WHDLoad 19.2 documentation** (`whdload.i`, autodoc
`ws_Flags` and the ws_GameLoader conventions, `opt.html` NoVBRMove):
- With its default moved VBR, WHDLoad forwards autovector interrupts through the
  low vectors.
- It forwards Line-A, TRAP, privilege violation, illegal, zero divide, CHK,
  TRAPV and Line-F only when the matching `WHDLF_Emul*` slave flag is set.
- No flag exists for trace, bus error or address error.
- The VBR is private to WHDLoad and must not be changed.

**MEASURED ([release.md](release.md)):** without NoVBRMove, the trial stopped
with a WHDLoad Trace exception.

The earlier attribution to trace exceptions is therefore correct. The slave also
lacks EmulIllegal, EmulDivZero, EmulChk, EmulTrapV and EmulLineF. With a moved
VBR, each of those installed vectors would stop WHDLoad as well; NoVBRMove
currently hides this.

Costs of requiring NoVBRMove, from the WHDLoad documentation:
- No QuitKey, DebugKey or freezer checks by WHDLoad.
- No Enforcer/VMM protection of the vector table.
- No WHDLoad emulation of 68060 unimplemented integer instructions.
  **INFERRED:** this matters only if such an instruction executes natively. The
  native code is compiled for the 68000 and the assembly files contain none. The
  guest ROM's executed opcodes have not been audited.
- A launcher or global configuration that doesn't pass the icon's tooltypes
  fails outright.

### NoWriteCache: exit-time hang

**MEASURED ([release.md](release.md)):** with the default write cache, the test
hung inside WHDLoad on exit. NoWriteCache fixed both cold and warm runs. The
cause has not been investigated.

**DERIVED from `kickfs.s` and `NvramFile.cpp`:** the exit save handles
`nvram.bin` and `accounting.bin` the same way:
1. It reads the old image.
2. It creates `.bak`: kickfs opens MODE_NEWFILE as a 0-byte `resload_SaveFile`,
   then writes with `resload_SaveFileOffset`.
3. It creates `.bin` the same way.

That is at least two WHDLoad write calls per file, always on a new or truncated
file. With the write cache, WHDLoad keeps these in memory and writes them when
WHDLoad exits. With NoWriteCache, each call is a physical write with a switch to
the OS.

**DERIVED from `opt.html`, cost INFERRED:** WriteDelay (default 150, i.e. 3 s)
follows each physical write. NoWriteCache may therefore add several seconds at
exit on real hardware. The FS-UAE tests run in warp mode and did not measure
exit time.

**ws_DontCache.** `kick31.s` hard-codes `ws_DontCache` to 0 in the slave header.
Using it needs a local copy of that header code (kick31.s is public domain).
Excluded files would also be written physically, with the same WriteDelay cost.
It is a fallback, not the first choice.

## Plan

### W1 — root-cause the exit hang (first, small)

**2026-09-30 tooling:** `tools/test_whdload.py` now exposes
`--write-cache enabled|disabled` and `--vbr moved|fixed`, plus `--file-log` and
`--write-delay`. Defaults retain the release's NOWRITECACHE/NOVBRMOVE.
`--prepare-only` writes an inspectable fixture without launching an emulator;
`command.txt` records the exact command. Each repeated attempt retains its own
emulator output, result, register dump and `.whdl_log` when produced. These are
test options only; the slave, installer and release defaults are unchanged.

**MEASURED fixture check:** authored smoke fixtures produce the expected
unchanged default command and the requested moved-VBR/cached-write command
without PRELOAD, with FILELOG/WRITEDELAY=0. This verifies option construction,
not WHDLoad execution or the exit hang. The cold/warm matrix and emulated exit
duration below remain open. Example (supply the existing local ROM/RTB paths):

```
python3 tools/test_whdload.py --rom /path/kick40068.A1200 --rtb /path/kick40068.A1200.RTB --write-cache enabled --file-log --repeat 3
```


**MEASURED smoke execution:** two runs of the authored `Smoke.slave` with
PRELOAD, FILELOG, moved VBR and default write caching both save `PASS` and return
normally (`tmp/whdload-test-r0309u_7`, `/tmp/pokeri-w1-smoke-cache.log`). This
proves simple cached SaveFile/Abort works in the local environment; it does not
exercise Pokeri's DOS save sequence. An initial full-game fixture accidentally
used zero-byte installer-test Kickstart placeholders and failed before loading
the game; it is excluded from the matrix. The launcher now rejects incomplete
Kickstart images and empty RTB files before creating a fixture or emulator.
The full-game cached-write test uses a verified 512 KiB image and 5,000-byte RTB.

**Read-only wait capture:** `--debug-port PORT` reserves no other process's
port and rejects an occupied one. GDB runs asynchronously, ignoring WHDLoad's
intentional CPU-probe traps while the test executes. At return/timeout it
restores trap stopping, interrupts and records registers, current instructions,
stack/vector words and a 640-byte PC window under the isolated fixture. There
are no target-memory writes. A smoke run with default cache/moved VBR and
WRITEDELAY=0 returns normally and produces the complete capture
(`tmp/whdload-test-ebbtkdty`). The full Pokeri FILELOG run with the default write
delay timed out while still loading optional startup markers, without a save
attempt; this is not yet evidence of the reported exit hang. The next run uses
WRITEDELAY=0 to remove logging's documented delay, with CPU capture enabled.

**MEASURED controlled reproduction (2026-09-30):** with PRELOAD, NOVBRMOVE,
FILELOG and WRITEDELAY=0, the default write-cache run does not return within
240 host seconds. Its read-only capture is at `$2282C2`, whose instruction
sequence uniquely matches offset `$B59E` in the fixture's WHDLoad executable.
The surrounding routine traverses a linked structure and tests bit 0 at node
byte offset 232; the branch returns to the same node when set. The following
allocation routine initializes 240-byte nodes with a file-cache tag. Identifying
this as cache cleanup is **INFERRED**, not a WHDLoad source-symbol match; the
first capture did not include the node bytes and does not yet prove the reason
for the flag or its failure to change. Evidence:
`tmp/whdload-test-guom8va1/cpu-1.log` and
`tmp/whdload-test-guom8va1/cpu-window-1.bin`, `/tmp/pokeri-w1-game-cache-debug.log`.

The matched NOWRITECACHE control, with every other option unchanged, returns
normally on both cold and warm attempts, writes NVRAM/accounting, and retains
exact previous-image backups. Both read-only CPU/node captures complete.
Evidence: `tmp/whdload-test-k31nbu0c`,
`/tmp/pokeri-w1-game-no-cache-debug.log`. This isolates cache behavior in the
local PRELOAD configuration, but does not close the PRELOAD-off matrix, saved
state/cleanup bisection or emulated exit-duration gate. A repeated cached run
includes the node, list root and read-only DMA/interrupt-enable registers.

**MEASURED node/bisection:** the second cached capture identifies the node as
`nvram.bin`, with bit 0 at byte 232 set and no child node. The previously
visible FILELOG tail was buffered and cannot establish that execution remained
in startup: the save already exists in WHDLoad's cache. Evidence:
`tmp/whdload-test-fuyl3t_b/a1-window-1.bin` and `cpu-1.log`.

The authored `whdload/SaveSmoke.s` reproduces this same wait without the native
runner, original ROM execution or hardware takeover. It saves a synthetic
32 KB NVRAM and 32-byte accounting image via DOS, including previous-image
backups. Cached run `tmp/whdload-test-g11nwmjm` stops at `$2282B8` in the same
node loop; uncached `tmp/whdload-test-g1zndr69` passes cold/warm files/backups.
`UPDATE_SAVE=1` changes Open to MODE_READWRITE and still reproduces the cached
wait (`tmp/whdload-test-55y8gl2l`). Thus the native runner's cleanup is not a
necessary cause, and non-truncating Open alone does not solve this case.

Direct resload controls in `SmokeSlave.s` pass both a four-byte zero-create/
offset-write and a 32 KB version (`tmp/whdload-test-2xt6a7mw`,
`tmp/whdload-test-5pswt8hv`). This excludes those API calls alone as a sufficient
cause in that configuration. The next control adds the documented Examine
operation used by kickfs after creating the empty file. These smoke variants
and the `--slave`/`--smoke-size` test switches are diagnostic-only; no release
save code or slave option has changed.

1. Add switches to `tools/test_whdload.py` that omit NOWRITECACHE and choose
   whether NOVBRMOVE is passed. Reproduce cold and warm, with PRELOAD on and
   off, three repeats each. Record:
   - whether the saves reached disk;
   - the emulated exit duration;
   - WHDLoad's FileLog (`.whdl_log`).
2. On a hang, read the CPU state read-only through the FS-UAE gdb stub. Locate
   the PC in WHDLoad, kickfs, Kickstart or the executable (match the memory
   against the `C/WHDLoad` hunks) and find what it is waiting for.
3. Bisect with test-only switches, none of them packaged:
   - no exit save;
   - `.bin` only, without `.bak`;
   - one file only;
   - an in-place update of the existing fixed-size file (MODE_READWRITE, no
     truncation);
   - a Smoke-slave program making the same DOS calls without the native runner;
   - WriteDelay=0.
4. Compare the machine state the runner hands back after `nativeRun` with the
   state before it started:
   - INTENA and DMACON;
   - CIA-A/B ICR masks and control registers: the guest timer stopped and its
     ICR vector removed, the keyboard ICR vector restored;
   - audio and blitter idle.

The outcome selects the fix:
- **The port's leftover state:** fix the cleanup. The write cache then works
  unchanged.
- **A save pattern WHDLoad's cache mishandles:** change the WHDLoad save path to
  one that works while keeping the `.bak` guarantee. Candidates are in-place
  fixed-size updates, or a direct `resload_SaveFile` through a resload pointer
  the slave publishes in the `POK!SAVE` block.
- **An unavoidable WHDLoad defect:** set `ws_DontCache` to `#?.(bin|bak)`
  through a local kick31 header copy, and report it upstream (with the user's
  approval before sending anything).

Gate: `test_whdload.py` cold and warm, twice each, with the default write cache
and with NoWriteCache, PRELOAD on and off. Backups must equal the previous
images, exit must complete, and the exit duration is recorded.

### W2 — trace inventory and WHDLoad forwarding cost (before code)

- Count trace entries by what armed them (interrupt-return wrapper, pending-tick
  resume, other) in cold/warm startup and the Double scenario. Use `trace.sh`,
  with the reducer classifying each trace by its preceding event.
- Audit the short paths that lower the virtual IPL, return from guest interrupts
  or clear device IRQs. Does each decline, or re-check pending delivery, while
  `nativeShortPending` is set?
- Measure WHDLoad's per-exception forwarding cost with a CIA-timed benchmark
  build under WHDLoad, comparing a moved VBR with NoVBRMove. Measure:
  - a Line-A round trip;
  - TRAP;
  - privilege violation;
  - level-2/3 interrupt entry.

  Write the results to a file in the data drawer. **INFERRED:** the trace
  profiler probably cannot attribute cycles under WHDLoad, because kickemu loads
  the program outside the hunks the stub records. Verify before relying on it.
- On the host harness, audit the guest's executed opcodes for 68060
  unimplemented instructions (MOVEP and others).

### W3 — trace-free live service entry

**Interrupt return by frame-PC redirection.**
- When a wrapper interrupts user mode, it saves the frame PC in a single slot
  and substitutes a native stub, unless the frame already points at the stub.
- The stub's only instruction is a reserved Line-A opcode. Exec returns to it,
  and the Line-A arrives through the forwarded `$28` vector.
- One exact-PC descriptor in the existing short-path table restores the saved
  PC and continues exactly as the trace entry does now. Other Line-A sites pay
  nothing.
- The same stub serves VBI, CIA-A guest-clock expiry, audio and CIA-B.
- The service is entered before the next guest instruction instead of after it.
  Both are boundaries an interrupt could hit, and no guest instruction is
  skipped or repeated.
- A second interrupt before the stub runs finds the stub PC and leaves it alone.
- Live paths no longer need the 68000 traced-TRAP case or format-2 trace frames.

**Pending-tick resume without T.** The approach depends on W2:
- If the audit confirms it, rely on the privileged SR/RTE instructions that
  already trap, plus full-dispatcher fallbacks in short paths while
  `nativeShortPending` is set.
- If a real case remains, arm CIA-A timer A to expire immediately; it then
  enters through the stub. Live timing is not instruction-exact, so a few guest
  instructions may run first.

**Other runner and slave changes.**
- Diagnostic/replay stepping keeps trace. Under WHDLoad with a non-zero VBR
  (read-only MOVEC), diagnostic modes refuse with a clear message.
- Standalone builds keep their private Fast RAM vector table.
- Add EmulIllegal, EmulDivZero, EmulChk, EmulTrapV and EmulLineF to the slave,
  so every vector the runner installs behaves the same with a moved VBR.
- Bus and address errors cannot be forwarded. They become WHDLoad's own stop
  with a register dump, which is still a loud stop.
- Leave WHDLF_NoKbd unset. **DERIVED from the autodoc:** WHDLoad leaves
  keyboard acknowledgement to the program when `$68` is initialized, and the
  runner's keyboard ICR vector is reached through `$68`.
- Use one live path on all platforms if the standalone build shows no
  regression; otherwise select the path at startup.

Gates:
- CPU tests of the redirect: an interrupt at the stub, nested and back-to-back
  IRQs, an IRQ during a service, and 68020/030/040/060 frames.
- Headless suites.
- Exact ECS/AGA replay. The diagnostic path is unchanged, but it must still pass.
- Cold/warm live24 on A1200 and ECS.
- `trace.sh` re-measurement.
- `release_timing.py --scenario double`.
- The W5 WHDLoad matrix.

### W4 — QuitKey (decision needed)

With a moved VBR, WHDLoad checks QuitKey on every level 1–3 interrupt and exits
at once, skipping the game's exit save. `slv_keyexit` is 0 today. Verify which
key that means under WHDLoad (rawkey `$00` or WHDLoad's F10 default). Then set
it explicitly; F10 (`$59`) is recommended, because the game doesn't use it.

Options:
- **a.** Accept that QuitKey exits without saving; the ReadMe tells players to
  quit with Esc.
- **b.** Add checkpoint saves under WHDLoad so QuitKey loses only recent
  changes. With the write cache these are memory copies. They would, however,
  be a new behavior of the running game, needing safe points and a separate
  decision.
- **c.** Keep NoVBRMove as an optional setting for players who want no QuitKey.

Recommendation: a for this work, with b as a separate decision.

### W5 — packaging, test matrix and performance gate

- Remove NoVBRMove and NoWriteCache from:
  - the icon and the installer (`release/Install`, `tools/test_release_installer.py`);
  - the ReadMe;
  - `slv_info`;
  - the test defaults.

  Both remain valid options, because users may set them globally; test both.
- FS-UAE matrix, JIT off:
  - CPUs: A1200 68020 (baseline), 68030 with MMU, 68040, 68060.
  - Options: the defaults, then NoVBRMove and NoWriteCache one at a time.
  - PRELOAD on and off, with cold and warm saves.

  Record any configuration that cannot be tested locally, for example because
  the 68040/68060 libraries are missing.
- Proposed performance gate, to be confirmed:
  - Under WHDLoad, the defaults against NoVBRMove: warm and cold Ready within 2%,
    and the Double scenario's AY lateness no worse than the run-to-run spread.
  - Standalone normal build: no regression from W3 (`trace.sh`,
    `release_timing.py`).
- Expected cost, **INFERRED:** forwarding costs a few WHDLoad instructions plus
  a Chip RAM vector fetch per exception. The gameplay entry rate is **MEASURED**
  from the deal/reveal trace: 2,627 Line-A and 43 trace entries in 0.82 s, about
  3,300 per second. At 1–3 µs per entry that is 0.3–1% of the CPU. Startup
  rates still need measuring.

  If the measured cost exceeds the gate, NoVBRMove stays documented as an
  optional speed setting; the game still works without it.
- A version bump and new package are a separate release decision.

### Latest isolated controls (2026-09-30)

**MEASURED:** `ExamineSaveSmoke.slave` adds `resload_Examine` of the newly
created empty file before the 32 KB offset write, with the required
`WHDLF_Examine` flag. It also returns normally and writes the exact authored
payload (`tmp/whdload-test-b4h7debx`). Therefore the documented API sequence
alone, even with that lookup and full NVRAM size, is not sufficient to reproduce
the DOS/slave-context failure. The runner-free DOS reproducer remains the
smallest demonstrated failing case. Production saves remain unchanged until a
candidate passes in that same context; a direct whole-file resload save through
a slave-provided callback is the next planned save-path candidate.

### Whole-file callback in the DOS context (2026-09-30)

**MEASURED:** diagnostic `SaveCallbackSmoke` / `SaveCallback.slave` retains
DOS loading and old-file reads but performs each complete output/backup write
through `resload_SaveFile`. It uses a separate, bounded 16-byte `POK!CB01`
descriptor (version 1, explicit size, initially null callback); it cannot patch
the production 12-byte save descriptor. The callback preserves the DOS library
base and all caller registers except the documented Boolean result.

With PRELOAD, FILELOG, WRITEDELAY=0 and the default write cache, the cold run
still fails to return within 240 host seconds. Read-only capture stops at
`$2282C2`, testing bit 0 at offset 232 of the same `nvram.bin` cache node:
child pointer zero, flag byte 1, metadata tag `WHFC`. Thus replacing DOS
truncate/Write/Close with whole-file resload writes is **not sufficient** in
this context. It is not a production save fix and W1 remains open. Evidence:
`tmp/whdload-test-28sj9r8w`, `/tmp/pokeri-w1-save-callback.log`.

The ordinary release slave rebuild remains byte-identical to its pre-experiment
binary. Diagnostic make targets are excluded from normal `all`. The separate
`SaveCallbackNoReadSmoke` variant omits even the failed DOS old-file lookup for
a cold-only bisection; it intentionally does not preserve backups and must
never be used as a release save path.


**MEASURED bisection:** the cold no-read callback variant also times out (120
host seconds), at `$2282B8` with the same NVRAM node, zero child, flag 1 and
`WHFC` tag (`tmp/whdload-test-wu0w5xm4`). No DOS Open/Read/Write/Close of save
files is needed to reproduce the cache hang. The matched full callback variant
with NoWriteCache passes cold and warm, including byte-exact 32 KB NVRAM,
32-byte accounting and both previous-image backups
(`tmp/whdload-test-h81a6j92`). Therefore the callback/descriptor works, but it
cannot remove the release option. Further bisection must target the DOS/slave
context or cache resource behavior, rather than adopting a different file-write
API. No production memory/file/cache rules have changed.

### PRELOAD, VBR, version and resource controls (2026-09-30)

**MEASURED:** the full callback reproducer passes cold/warm with PRELOAD off
and no NoWriteCache (`tmp/whdload-test-io1raywu`). The normal T14 game likewise
passes three launches, preserving both saves and previous-image backups
(`tmp/whdload-test-_gs95ni3`). This does **not** prove newly created files were
cached: WHDLoad's [18.7 history](https://www.whdload.de/docs/History.html)
explicitly conditions new/growing-file caching on PRELOAD and enough memory to
preload all files. Disabling PRELOAD is a diagnostic control, not the planned
release fix.

The callback still hangs with moved VBR and PRELOAD: `$2282C8`, same NVRAM
cache node (`tmp/whdload-test-7lfu_624`). Thus NoVBRMove is not required for
this failure. An isolated official **WHDLoad 20.0 build 7051** executable also
hangs at the corresponding bit test `$228684`, with `nvram.bin`, zero child,
flag 1 and tag `WHFC` (`tmp/whdload-test-u2h7o5_7`). The shared 19.2 build 6941
installation was not replaced. Source archive:
[official user package](https://www.whdload.de/whdload/WHDLoad_usr_small.lha),
extracted only under `tmp/whdload-current-control`.

The following authored direct-SaveFile/Abort controls each pass cold/warm:

| Fixture | Added control |
| --- | --- |
| `tmp/whdload-test-nih5j6az` | `data` current directory, 32 KB write |
| `tmp/whdload-test-8z6l3b1b` | Same, 1 MB Chip / 4 MB Fast reservation |
| `tmp/whdload-test-i0c3fbq6` | Directory plus Examine metadata flag/API |
| `tmp/whdload-test-3l6kacfm` | Directory plus a second 32-byte file |

These controls do not execute DOS or game code. Their cold data directories
start empty, which can change PRELOAD/cache allocation. A seeded input is needed
before concluding that the failing context requires DOS. The resource control
reserves 4 MB Fast; kickemu additionally reserves 512 KB for its Kickstart, so
it is not a byte-exact match to the whole slave allocation. All diagnostic
variants remain outside normal `all` and the release archive.


**MEASURED seeded control:** `tmp/whdload-test-1rawq4b1` starts with an
independent authored input in `data`, then writes the 32 KB and 32-byte files.
Both cold/warm runs pass, and both saved payloads are byte-exact. Thus empty
cold PRELOAD input is not sufficient to explain the earlier minimal-test pass.
`tools/test_whdload.py --smoke-preload-seed` makes that distinction explicit;
`--smoke-data-dir` checks the file under the slave's declared directory.

**MEASURED observer control:** without FILELOG, the callback still stops at
`$2282C2` on the NVRAM node (`tmp/whdload-test-q7d4czxx`). Thus diagnostic file
logging is not required to reproduce it. The `SaveEarlyAbort.slave` diagnostic
calls `resload_Abort` immediately after the authored 32-byte accounting write,
before returning through DOS close/unload; it is a teardown bisection only.
It deliberately bypasses normal cleanup and is never packaged or selected by
normal `all`.


**MEASURED teardown bisection:** the early-abort variant also times out at
`$2282C8` on the same NVRAM node (`tmp/whdload-test-c2iw0l8p`). Normal DOS
close/unload after both writes is therefore not required for this failure;
this does not prove that the abort itself was reached. A proposed 16 MB Zorro
II control was rejected by FS-UAE as unsupported and stopped via its owned
Python process, allowing normal fixture cleanup. `tmp/whdload-test-5khxhn7d`
is excluded from evidence. The test runner now rejects Fast RAM requests
above 8 MB rather than running a silently different configuration. Testing
additional memory requires a separately verified Zorro III configuration.
