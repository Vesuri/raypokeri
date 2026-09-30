# WHDLoad without NoVBRMove and NoWriteCache

**Status 2026-09-30: W2 investigation complete; W1/W3–W5 remain open; release options unchanged.** Release 0.1 requires both
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
  observed guest workload has now been audited under W2 below; unseen paths
  and full CPU compatibility remain unproved.
- A launcher or global configuration that doesn't pass the icon's tooltypes
  fails outright.

### NoWriteCache: exit-time hang

**MEASURED ([release.md](release.md)):** with the default write cache, the test
hung inside WHDLoad on exit. NoWriteCache fixed both cold and warm runs. The
current cache-creation investigation and its remaining decision are recorded
under W1 below.

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

### W2 executed-opcode inventory (2026-09-30)

**MEASURED:** the host-only `--opcode-audit` observer records the actual first
word at each instruction entry, including RAM code, rather than combining a
coverage bitmap with memory read after execution. Four successful
runs observe:

| Workload | Instruction entries |
| --- | ---: |
| Fast cold startup + 1 s after Ready | 6,129,137 |
| Retained warm startup, 1 s absolute endpoint | 757,890 |
| Research cold boot/play to 65.5 s | 65,876,069 |
| Research cold boot/service to 39 s | 37,931,343 |
| Total | 110,694,439 |

Fast startup runs have no reset. Each hardware-test play/service run has its
one expected startup watchdog reset in the documented `$20DC–$20E2` loop
(at 19,840,226 cycles); there are no gameplay resets. The reproducer checks
that distinction rather than accepting arbitrary resets.

The union contains 17,966 PCs (107 in RAM) and 1,620 distinct opcode words.
There are **zero MOVEP entries and zero words invalid for the 68000 ISA**.
The play run enters the documented Double callback `$18176` and the subsequent
choice transition `$18380`; it is not merely a timed key request. Eight PCs
have differing observed words across fast/hardware-test boot policies; none
changes its word within an individual capture. No captured path is silently
classified from its final memory contents.

**DERIVED:** of the unimplemented integer families in Motorola/NXP
[MC68060UM, section C.2](https://www.nxp.com/docs/en/data-sheet/MC68060UM.pdf),
MOVEP is the one available in the 68000 ISA. CHK2/CMP2, CAS/CAS2 and the listed
long multiply/divide forms are later-ISA instructions. Thus these observed
68000 workloads require none of that integer emulation. This is not proof
about unvisited game paths, unimplemented effective addresses, native/OS code,
exception forwarding or a real 68060 run. The subsequent sections complete W2's trace inventory, short-path pending
audit and WHDLoad forwarding-cost benchmark.

**Verification:** the MOVEP classifier agrees with the independent Musashi
disassembler over all 65,536 first words, including all 256 MOVEP encodings.
Tests retain distinct words at a changed RAM PC and reject malformed/ambiguous
CSV evidence. A control play run with observation disabled is byte-identical
in full CPU, RAM, board state, VRAM, displayed indices, coverage and event
stream. The host regression suite passes. `make harness-opcode-scenarios`
reproduces the inventory and equivalence gate; captures and the report stay
in ignored `tmp/w2-opcode-*`. No Amiga code or WHDLoad flag changes.

### W2 trace-origin measurements (2026-09-30)

**MEASURED:** the offline trace reducer now identifies the executed
`ORI.W #$8000,(SP)` instruction separately in each level-2/3/4/6 wrapper.
It attributes an ensuing trace only when the observed arm (or dispatcher
resume) is followed by one guest instruction. A missing arm, another guest
instruction or intervening synchronous exception invalidates attribution;
those entries are reported as unresolved. Capture-file boundaries reset this
provenance. Counts must sum to the independently counted trace entries.
This adds no runtime counters, instructions or OS calls.

| Capture window | PAL seconds captured | Dispatcher resume | Level 2 | Level 3 | Level 4 | Level 6 | Unresolved | Total |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Current cold startup | 16 | 7 | 9 | 118 | 0 | 0 | 0 | 134 |
| Current retained warm startup | 8 | 6 | 10 | 52 | 0 | 0 | 0 | 68 |
| T14 deal | 6 | 21 | 38 | 358 | 199 | 0 | 0 | 616 |
| T14 draw | 4 | 7 | 18 | 237 | 95 | 0 | 0 | 357 |
| T14 accepted Double | 8 | 19 | 44 | 489 | 230 | 0 | 0 | 782 |

Level 3 includes blitter and VBI interrupts; do not label this column as VBI
alone. Level 4 is Paula audio. The three gameplay windows are the validated
T14 trace captures, not a newly paired performance comparison. Fresh startup
captures use the current default code; capture loops stop in 100-field chunks
after Ready (cold ends at frame 807 / 47.12M cycles, warm at frame 403 / 6.72M).
The seconds column is therefore capture duration, **not exact startup latency**.
Both finish Ready with status 1 and no fatal stop. Preparation before the first
original instruction is outside these windows.

**DERIVED:** 763/782 Double traces (97.6%) follow interrupt-wrapper arms;
19/782 follow pending-tick dispatcher resumes. Both arming paths must be dealt
with in W3. Removing trace does not by itself remove the required scheduler
work or prove a performance gain; WHDLoad forwarding-cost measurement remains.

Synthetic tests cover each proven arm, pending resume, missing provenance,
multiple guest instructions, synchronous exceptions and replacement of an arm.
The existing linked short-path matrices also pass after this audit, including
262,144 tick-return cases with immediate scheduler promotion and 32,798 TRAP
cases. No live trace-service behavior is changed.

Evidence: `amiga/.run/w2-trace-{cold,warm}`, `tmp/w2-trace-{deal,draw,double}`,
`/tmp/pokeri-w2-trace-tests.log`, `/tmp/pokeri-w2-short-audit-check.log`.
Reproduce analysis with `host/native_trace.py --run RUN --prefix PREFIX`; the
new `trace-origins.tsv` sits beside the existing reduced tables.

### W2 pending-service path audit (2026-09-30)

**DERIVED from the current default code** (`shortDescriptor`,
`nativeShortLengthDone`, the control guards and shared I/O endpoints):

| Route | Pending-service behavior |
| --- | --- |
| Virtual SR logic and ordinary RTE | Descriptor promotion mask 3: both queued clock/IRQ work (bit 0) and urgent work (bit 1) promote after publishing the new virtual SR/stack/PC. Frame changes always promote. Privilege, bounds and virtual trace transitions decline to the checked full path. |
| Outer tick RTE | `nativeShortTickRteRead` restores the original frame, records tick completion and unconditionally promotes. Nested/unrelated RTEs retain the ordinary control route. |
| Guest TRAP | Descriptor mask 3 after publishing the original virtual exception frame. It cannot silently bypass pending work. |
| PIA/ACIA reads and writes, including IRQ-clearing reads | Mask 2; `shortIoCompleted` refreshes IRQ priority, pending bit 0 and urgent bit 1 against the current virtual IPL after the device operation. Model faults force bit 1. |
| Bank-2 PIA shortcuts | Mask 2; these pins do not feed the board IRQ encoder. They retain pending bit 0 and refresh frame/quit work. |
| Video FIFO/control writes | Mask 2 and fresh shared-device status/IRQ checks. The CCR-low specialized endpoint recomputes the same source priority. Sequence boundaries check urgent work and frame changes. |
| Side-effect-free status/sentinel reads | Mask 0 is intentional: they do not lower IPL or change IRQ sources. They still check frame changes. They do not themselves guarantee draining a tick backlog. |
| Pure video address selection | It changes neither IRQ source nor virtual IPL; the normal video completion mask is retained. |

No audited default SR-lowering/return path discards the queued-work bit. Existing
bounded entry/exit/control/sound fusions retain those shared guards or their
proved boundary checks; T13's larger setup prototypes remain disabled.

**Important W3 constraint:** pending-tick resume is not just a workaround for
masked IRQs. `nativeDispatch` advances at most one eligible tick quantum per
iteration (or the already-approved startup quiet batch), then sets physical T
when `liveTicks && !liveIrqActive`. That condition does **not** require a high
virtual IPL. Tick backlog can remain with interrupts already enabled and no
currently asserted board IRQ, so waiting exclusively for a later SR-lowering
instruction/RTE would miss service opportunities. W3 needs an explicit service
request for that case, such as the planned immediate CIA-A expiry, preserving
quantum order and existing boundaries. This is an implementation requirement,
not authorization to batch arbitrary ticks or change the clock contract.

This source audit does not replace the W3 CPU/scheduler/replay gates. No
trace-arming or dispatch behavior has been changed by the offline observer.

### W2 exception-forwarding benchmark (2026-09-30)

**MEASURED:** `whdload/ExceptionBenchmark.s` is a standalone authored diagnostic,
not a game/replay build. It executes in physical user mode with a private
supervisor stack and handlers in reserved BaseMem at `$1000`. Four trials of
128 exceptions each cover Line-A, TRAP0, privilege (`ORI.W #0,SR`), software
PORTS level 2 and software BLIT level 3. No blitter operation is started;
VBI/DMA and CIA interrupt sources are disabled, so natural video interrupts do
not contaminate these counts. Each handler preserves guest registers and
returns with RTE; fault handlers advance the saved PC by the instruction size.

CIA-A timer B measures E-clock ticks at 709,379 Hz. Each trial subtracts a
matching indirect-call/return loop. Wrong delivery counts and timer underflow
are fatal, not accepted samples. Timing excludes file writes and WHDLoad exit.
The report is `data/exception-timing.bin`; `host/exception_benchmark.py` rejects
truncated, unsupported, overflowed and miscounted records. Its malformed-record
test passes. Three independent launches per VBR policy provide 12 samples per
exception type. All six launches return normally with exactly 128 deliveries
per batch.

The environment is the launcher's FS-UAE A1200/68020, 2 MiB Chip + 8 MiB Fast,
JIT disabled, WHDLoad 19.2, PRELOAD and NOWRITECACHE. FS-UAE's effective log
confirms `cpu_speed=real`, `cpu_compatible=true`, `cpu_cycle_exact=true` and
`blitter_cycle_exact=true`. CPU caches retain the same WHDLoad default policy
in both runs. These are isolated default-policy costs, not a measured native
service-path speedup or a hardware calibration. IRQ figures include software
request/acknowledgement and polling; the paired **difference** isolates the
changed VBR forwarding policy more closely than either absolute total.

Median microseconds per exception, loop baseline subtracted:

| Exception | Fixed VBR | Moved VBR | Added forwarding cost |
|---|---:|---:|---:|
| Line-A | 9.956 | 15.749 | 5.793 |
| TRAP0 | 7.379 | 19.168 | 11.790 |
| Privilege | 13.007 | 58.331 | 45.325 |
| Level 2 | 12.357 | 46.101 | 33.744 |
| Level 3 | 12.214 | 36.195 | 23.981 |

Across the 12 samples, each policy/type range is at most 0.15 µs. An earlier
identical-instruction build with different code alignment changed absolute means
by roughly 1 µs but retained forwarding deltas within 0.2 µs. Do not extrapolate
the absolute synthetic means to the much larger real handlers.

Final local fixtures: fixed `k_tkpo5p`, `kmknpblu`, `2eb_2q09`; moved
`h66ldr_4`, `lrl4r60v`, `rnn94yx5`, all under `tmp/whdload-test-*`.
Launcher logs: `/tmp/pokeri-w2-exception-data-{fixed,moved}-{1,2,3}.log`.

**MEASURED setup limitation:** installing the privilege handler inside the
slave caused WHDLoad to stop on the fault despite EmulPriv. Installing it in
reserved BaseMem made the complete batch pass. The earlier MOVE-SR trial also
failed its delivery count; it is not a forwarding timing sample. The final
benchmark uses the explicit privileged ORI stimulus and identical BaseMem
handler placement for both policies. W3/W5 must validate the real runner's
allocated handler locations rather than infer forwarding support from flags
alone.

Reproduce (debug audio is muted by the launcher):

```sh
make -C whdload exception-benchmark
. amiga/env.sh
python3 tools/test_whdload.py --mode smoke --slave build/whdload/ExceptionBenchmark.slave --vbr fixed --seconds 90 --debug-port 3188
python3 tools/test_whdload.py --mode smoke --slave build/whdload/ExceptionBenchmark.slave --vbr moved --seconds 90 --debug-port 3188
python3 host/exception_benchmark.py --fixed FIXED/game/data/exception-timing.bin --moved MOVED/game/data/exception-timing.bin
python3 host/exception_benchmark_test.py
```

Use each launch's printed fixture path for FIXED/MOVED; repeat with distinct
fixtures and pass multiple files to each analyzer option. The diagnostic target
is not included in `all` or release packaging. The normal game, slave and
release options are unchanged. W2 is complete within its measured scope;
trace-free implementation and whole-game compatibility/performance remain W3/W5.

### W3 — trace-free live service entry

**Opt-in IRQ integration (2026-09-30), not default:**
`SERVICE_REDIRECT=1` links `NativeServiceRedirect.s`. The level-2/3/4/6 wrappers
pause/account guest time as before, then redirect physical-user return PCs into
an authored Line-A opcode. That opcode indexes one appended exact-PC descriptor
in the ordinary short table, so other Line-A instructions pay no new lookup
branch. The redirect clears any outstanding artificial pending-tick T bit, preventing
a traced Line-A at the stub. Its guard restores the saved PC and dispatches kind 11: service at a
boundary without counting an invented guest instruction. Conflicting/orphan
slots take the existing loud native-fault path. Diagnostic/generic hooks and
calibration retain the previous trace behavior.

**MEASURED primitive gates:** the linked candidate's redirect/consume bytes
exactly match the assembled object. 524,288 68000/68020 cases cover every saved
SR, all armed/stub combinations, repeat/consume, unchanged extension bytes and
registers. A further 452 cases inject a real level-7 interrupt at every helper
instruction boundary. An independently authored wrapper passes the actual
CPU-created supervisor frame to the helper and returns with RTE; it preserves
the outer operation's registers, condition flags, frame and pending slot,
including interruptions between publication stores. These are not 030/040/060
execution or whole-Exec tests.

**MEASURED linked wrapper/descriptor gates:**
`host/native_service_entry_check.py --elf tmp/w3-redirect-irq/Pokeri.elf`
passes 1,573,184 additional 68000/68020 cases using the actual linked
level-2/3/4/6 wrappers and ordinary Line-A lookup. Every saved SR is checked
under live redirect, disabled redirect and calibration policies. The tests
check the artificial T-bit clearing, exact original frame/extension bytes,
register and stack restoration, and clock call ordering/counts. Clock C
endpoints are explicit ABI-clobbering test doubles; these cases do not claim
to validate the clock implementation itself. Lookup tests cover successful
kind-11 entry, missing/disabled slot loud faults, and refusal to consume the
slot for a matching opcode at another PC or a wrong opcode at the stub.
Build the test with `make build/native-service-entry-test` after sourcing the
Amiga toolchain environment. The local log is `/tmp/pokeri-w3-entry-check.log`.

**MEASURED four-CPU extension:** separate `service-m68kcpu`/`service-m68kops`
objects now enable 68000/020/030/040 without changing the original-game harness
or other native tests. The redirect matrix passes 1,048,576 cases and 904 real
nested level-7 injections/returns across those four models. The linked wrapper
and lookup matrix passes 3,146,368 cases. The vendored public CPU-type getter
omits 68030, so a test-only C probe checks the internal selected type explicitly;
no vendor source was changed. Logs: `/tmp/pokeri-w3-four-cpu-build.log` and
`/tmp/pokeri-w3-four-cpu-entry.log`. This is instruction/frame correctness under
Musashi with translation disabled, not cycle timing, MMU/cache coverage, a
68060 model, or proof of whole-game WHDLoad compatibility. The 68060 and W5
runtime matrix remain open.

**MEASURED preliminary live gate:** frozen `tmp/w3-redirect-irq/Pokeri[.elf]`,
fixture `amiga/.run/w3-redirect-warm`, and log
`/tmp/pokeri-w3-redirect-live.log` complete 24 inputs at 480,000,000 cycles with
status 4, zero resets/errors and restored vectors. Board/PAL ratio is 0.9822.
No accepted Double callback occurred; cached-card maximum is 44.736 ms and AY
batch median 9.2 ms. These differing-hand observations are not a paired speedup
or a Double deadline pass. The debug run was muted. Normal build restored;
all allocated ELF code/data match validated T14 exactly.

**MEASURED ECS/AGA diagnostic replay gate:** the same frozen opt-in candidate
passes all 262,144 RAM bytes, 524,288 VRAM bytes, 172,064 cropped pixels and
60 AY writes at 7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs, with
status 4, zero native error and restored vectors on both chipsets. Fixtures:
`amiga/.run/w3-redirect-replay-{aga,ecs}`; dumps/reference:
`tmp/w3-redirect-replay-{aga,ecs}-{replay,reference}-*`. Both runs and host
comparisons completed. This validates diagnostic isolation for the current
hybrid candidate; final pending-service changes still require their own gates.

**Implemented diagnostic moved-VBR refusal (2026-09-30):** preparation checks
`native-replay` before allocating the board or taking over the display. Under
WHDLoad only, an Exec Supervisor callback reads VBR without modifying it. A
nonzero VBR returns exit code 21; the slave reports:
"Diagnostic native-replay requires NOVBRMOVE. Remove native-replay for normal play."
This does not enable moved-VBR live play. NoVBRMove/NoWriteCache release defaults
and the opt-in live redirection setting are unchanged.

**MEASURED:** the first negative test exposed an existing startup-runtime bug:
the shared support `_start` returns void and discards main's status across
finalizers. A failed startup therefore returned success. Pokeri now supplies
`RuntimeStart.cpp`, preserving constructor/finalizer order and main's result;
the shared toolchain source is not edited. The old entry is renamed only when
compiling Pokeri's support object and discarded by linker garbage collection.
`host/runtime_start_check.py --elf amiga/out/Pokeri.elf` passes 28 linked
000/020/030/040 cases with deliberately clobbering callbacks and return values
0, 1, 20, 21, INT_MAX, INT_MIN and -1. Callee-saved registers, stack and callback
order also match. Build its oracle with `make build/runtime-start-test`.

The moved-VBR negative fixture `tmp/whdload-test-scvbn4cz` reports the exact
message, returns failure, and creates no save files. The ordinary fixed-VBR
fixture `tmp/whdload-test-onope0r4` starts and quits successfully with both save
files present. Logs: `/tmp/pokeri-w3-vbr-guard-{run,positive}.log`. The new
`tools/test_whdload.py --expect-replay-vbr-refusal` option reproduces the negative
test (`--mode quit --vbr moved` plus the existing ROM/RTB arguments). The startup
fix changes normal error reporting, not live scheduling. Full WHDLoad replay
and the final moved-VBR gameplay matrix remain W5 gates.

**Pending-work request implemented opt-in (see qualification below):** the user
approved software PORTS, leaving CIA-A guest accounting intact. Calibration's wrapper fallback has now been removed as well (see below);
the candidate remains opt-in until full runtime/performance qualification. Reprogramming CIA-A is not part of the approved implementation.
Remaining gates include 68060 frame execution, the 030/040/060 runtime matrix,
full headless/replay/cold/warm matrices, accepted Double, tracing and WHDLoad.

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

**Pending-tick resume without T.** W2 found backlog even when IPL is already
low. Privileged SR/RTE traps and full-dispatcher short-path fallbacks remain;
the approved software-requested level-2 PORTS interrupt supplies the missing
pending-work boundary. The request is made at IPL7 immediately before physical
user return. CIA-A expiry is not reprogrammed. See the implemented request and
linked-entry proof above; concurrent real source stress remains open.

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

### W4 — QuitKey (policy approved; keypress test pending)

With a moved VBR, WHDLoad checks QuitKey on every level 1–3 interrupt and exits
at once, skipping the game's exit save. The slave now explicitly sets
`slv_keyexit` to `$59` (F10).
**MEASURED (2026-09-30):** WHDLoad 19.2 replaces that zero with `$59` (F10)
before calling the slave. An explicit `$59` remains `$59`, and a `QuitKey=69`
override replaces zero with `$45`. Thus zero does not disable the quit key in
this environment. The approved policy now makes F10 explicit.

The diagnostic-only `make -C whdload quitkey-probe` builds
`QuitKeyZero.slave` and `QuitKeyF10.slave` from the smoke test. Each saves the
actual loader-supplied `ws_keyexit` byte to `keyexit-value`, then exits normally.
Run with `tools/test_whdload.py --mode smoke --slave PATH --vbr moved`; the
optional `--quit-key 69` tests the override. These commands use fresh fixtures,
no game code and muted audio. All three runs returned normally. Fixtures:
`tmp/whdload-test-g42fyrbc`, `warpaowz`, `rh2lgcgo`; logs:
`/tmp/pokeri-w4-{keyzero,keyf10,keyoverride}.log`. This measures loaded key
selection, not actual keypress handling or persistence on emergency exit.
The slave header now selects F10; compatibility tooltypes remain unchanged. The user selected option a
below on 2026-09-30: explicit F10 emergency exit, Esc for normal saving.

Options:
- **a.** Accept that QuitKey exits without saving; the ReadMe tells players to
  quit with Esc.
- **b.** Add checkpoint saves under WHDLoad so QuitKey loses only recent
  changes. With the write cache these are memory copies. They would, however,
  be a new behavior of the running game, needing safe points and a separate
  decision.
- **c.** Keep NoVBRMove as an optional setting for players who want no QuitKey.

Decision: a. Do not add checkpoint saves.

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

### Save completion markers and pre-existing files (2026-09-30)

**MEASURED:** the instrumented authored callback reaches stage 60 (both saves
returned and DOS library closed), phase 12, two calls, last size 32, before the
same cache hang (`tmp/whdload-test-uz2kl91u`). The read-only 8 MB snapshot has
one active marker and separate zero-valued copies from the executable/cache.
`NORESINT` still hangs after both calls (`tmp/whdload-test-tf5g2_1_`). This
locates the failure after successful save calls; it is not a blocked write API.

**MEASURED:** seeding all four complete save/backup files before PRELOAD makes
the callback pass twice with byte-exact previous-image backups
(`tmp/whdload-test-oh7yi31n`). This distinguishes overwrite of pre-existing
files from creation of new cached files in the tested context. It does not
prove a universal WHDLoad defect. The game-level repeat is separately tracked.

The SDK's documented `ws_DontCache` pattern was tested through a local generated
include; the shared SDK remains untouched and normal slave output remains
byte-identical. Both the four exact save names (`tmp/whdload-test-45hukb_c`) and
`#?` for all files (`tmp/whdload-test-0as3dyl9`) still hang after the DOS reproducer
reaches stage 60. Header-relative pattern pointers were checked in the binaries.
Neither pattern is enabled in production or accepted as a fix. The adapter
rejects missing/duplicate SDK header sites instead of guessing.

A [first-install save-slot proposal](whdload-save-slots-design.md) is now concrete
for a user decision. It would preserve original cold initialization while
avoiding creation of cached save files during the WHDLoad session. It is not
implemented and its cold-placeholder/version gates remain open.


**MEASURED normal-game confirmation:** with genuine pre-existing NVRAM,
accounting and both backups, the unchanged production slave and T14 executable
pass three PRELOAD/default-write-cache launches, retaining exact previous-image
backups (`tmp/whdload-test-ytmlekhs`). This is warm-save evidence, not proof of
the proposed fresh placeholders. NoWriteCache remains required for the current
installer's missing-save first launch. The save-slot implementation decision is
pending; W2/T13 work can continue independently.

### Decisions and current-option CPU controls (2026-09-30)

The user approves W1 installer-created blank save slots and the T7/W3
software-requested level-2 interrupt experiment. The CIA clock must remain intact.
W4 is decided: F10 remains an emergency exit, while Esc/left mouse uses normal
save-and-quit. Periodic saves are not authorized or required by this decision.

**DERIVED from the supplied WHDLoad API documentation:** `ws_keyexit` permits a
slave-owned keyboard handler, but WHDLoad independently checks the same key when
VBR is moved. No pre-quit callback is documented in `resload_Control` or the slave
header. `WHDLTAG_CBSWITCH_SET` runs on OS-to-game switches with no stack and DMA/
interrupts disabled; it is not a termination hook. `ExecuteCleanup` is an OS
command run on exit, not a game-state save callback. Sources:
`Autodoc/whdload.doc` (`ws_keyexit`, `WHDLTAG_CBSWITCH_SET`),
`Include/whdload.i` control tags, `Docs/en/opt.html` QuitKey/ExecuteCleanup.

**MEASURED:** the validated current T13-tail normal executable passes cold and
warm finite-budget save/exit runs on emulated 68040 and 68060 with JIT off,
PRELOAD, NOVBRMOVE and NOWRITECACHE. Both saves are written and warm backups
exactly equal the preceding files. Fixtures `tmp/whdload-test-hnr5kx8i` (040)
and `tmp/whdload-test-s4vpij3j` (060), logs `/tmp/pokeri-w5-current-{040,060}.log`.
These are current-option controls, not moved-VBR/default-cache qualification,
full gameplay or physical-CPU compatibility proofs. The W5 final matrix remains.

### W3 pending-work PORTS request (2026-09-30, opt-in)

`SERVICE_REDIRECT=1` now records pending live ticks without setting artificial
trace in the physical resume SR. After constructing the real user return frame,
with interrupts masked and calibration finished, `nativeServiceRequest` clears
the software flag and writes SETCLR|PORTS to INTREQ. Deferring until this boundary
prevents a supervisor-mode IRQ from consuming the request before user return.
It does not read CIA ICR or acknowledge hardware sources; the unchanged chained
Exec handler owns those operations. Disabled PORTS is a loud stop, not a lost
request. Calibration retains the pending flag; diagnostic/disabled operation
keeps its existing trace policy.

**MEASURED CPU gates:** 3,150,592 linked 000/020/030/040 cases pass, including
all existing wrapper/descriptor cases, request/defer/disabled-PORTS paths and
128 actual level-2 delivery/acknowledgement/RTE/Line-A cycles. Guest frame/CCR
and all saved registers survive, exactly one request and acknowledgement occur,
and the service kind does not count an invented guest instruction. The authored
old-handler double acknowledges PORTS only: this is not a proof of the complete
Exec CIA/keyboard implementation. The independent 1,048,576 redirect primitive
and 904 nested-IRQ tests still pass.

**MEASURED live gate:** one warm A1200 normal-code run reaches 480M cycles,
completes all 24 inputs and accepts a Double, with status 4, zero resets/errors
and restored vectors. It advances 59.420 board seconds in 63.542 PAL seconds
(ratio 0.9351). AY batch median is 9.1 ms; largest excess batch delay is 500.0 ms.
Different hands prevent paired attribution; this is not a performance win or
an audio-deadline pass. No live storm occurred in this workload, but simultaneous
CIA/keyboard stress and the full runtime matrix remain required.

Evidence: frozen `tmp/w3-ports/Pokeri[.elf]`,
`/tmp/pokeri-w3-ports-{build,cpu-build,entry}.log`,
`amiga/.run/w3-ports-warm/gdb-out.log`. **Keep SERVICE_REDIRECT=0 by default.**
Remaining work includes calibration trace removal, explicit concurrent-source
coverage, full cold/warm/replay/Double/trace gates, 68060 and moved-VBR WHDLoad.

### W3 calibration and first default-option WHDLoad runs (2026-09-30)

The enabled redirect path now preserves calibration frames without adding T.
Each private timing sample already ends in explicit Line-A; interrupt windows
between samples run in supervisor mode. No sample needs an invented trace or
redirection into game service. Disabled/diagnostic operation keeps its original
policy. The slave now requests EmulIllegal, EmulDivZero, EmulChk, EmulTrapV and
EmulLineF as well as its existing forwarding flags. F10 is explicit per the
approved emergency-quit policy; actual keypress/persistence coverage remains W4.

**MEASURED:** all 3,150,592 linked CPU cases pass with the new calibration policy,
and the headless device/platform/native suites pass. Frozen candidate
`tmp/w3-notrace/Pokeri[.elf]` and `Pokeri.slave` completes cold/warm normal-game
finite-budget launches with neither NOVBRMOVE nor NOWRITECACHE on WHDLoad 19.2:
all saves, exact previous-image backups and clean returns pass. The same test
passes with CPU 68030 and requested `uae_mmu_model=68030`, JIT off; the test
launcher now exposes `--mmu` and rejects incompatible CPU selections. This is
configured-emulator evidence, not physical MMU/cache validation or a full game
performance comparison.

Evidence: `/tmp/pokeri-w3-notrace-entry.log`,
`/tmp/pokeri-w3-notrace-host-check.log`,
`/tmp/pokeri-w3-notrace-moved.log` (`tmp/whdload-test-eh_zdclo`),
`/tmp/pokeri-w3-notrace-030mmu.log` (`tmp/whdload-test-lh1vfx37`).
The normal SERVICE_REDIRECT=0 build is restored and audited. Full ECS/AGA
replays, concurrent-source stress, full live/Double/VBI/trace gates and the rest
of the WHDLoad CPU/option/performance matrix remain. The first AGA replay hit its
240-second safety limit during self-tests; it is incomplete, not a pass. A
separate longer fixture retains this evidence and continues qualification.

### W3 replay, live and CPU qualification (2026-09-30)

**MEASURED:** the frozen trace-free candidate matches every diagnostic reference
byte on ECS and AGA: 262,144 RAM bytes, 524,288 VRAM bytes, 172,064 cropped pixels
and 60 AY writes at 7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs.
The successful longer AGA run replaces the incomplete 240-second attempt as
qualification evidence; both logs are retained. Replay validates diagnostic
isolation; the linked matrices and live runs exercise the redirect path.

Cold and saved-state warm live24 runs finish with zero errors/resets and restored
vectors on A1200 and A500+. Board/PAL ratios are A1200 0.9291/0.9811 and ECS
0.2836/0.2864. ECS remains correctness evidence only. The cold A1200 run accepts
a Double; the warm run does not. Different hands preclude a paired speed claim.

The dedicated normal-code Double scenario accepts in round 4, completes 46 input
transitions and returns cleanly. Its 93.270 board seconds take 95.658 PAL seconds
(ratio 0.9750). Across 165 AY batches the median application span is 9.2 ms;
maximum excess batch delay is 249.6 ms. Cached-card intervals reach 65.312 ms.
Clock minimum/maximum/overhead are 33/45/40 cycles, CPU limit 80 sixteenths and
active ratio 64. These measurements do not close card/audio deadlines.

The A1200 VBI probe verifies its loaded instruction and reports 816 startup
samples (maximum scanline 6), 2,769 gameplay samples (maximum 10), zero samples
at/after line 29 and no BLITHOG samples. All 24 inputs and cleanup pass.

Default-option WHDLoad 19.2 (no NOVBRMOVE/NOWRITECACHE) also passes cold/warm
save, exact backup and return checks on configured 68040 and 68060. Together
with the previous 020/030 runs this covers the first CPU pass, not the complete
option/PRELOAD/performance matrix or real-hardware cache/MMU behavior.

Evidence: `/tmp/pokeri-w3-notrace-{ecs,aga}-compare.log`,
`amiga/.run/w3-notrace-{cold,warm}-{aga,ecs}/gdb-out.log`,
`tmp/w3-notrace-{cold,warm}-{aga,ecs}-summary.txt`,
`amiga/.run/w3-notrace-double/gdb-out.log`, `tmp/w3-notrace-double-summary.txt`,
`/tmp/pokeri-w3-notrace-vbi.log`, `/tmp/pokeri-w3-notrace-040-060.log`.
The normal non-experimental build is restored. Instruction capture/reduction is
in progress; concurrent-source/actual QuitKey tests and final comparative gates
remain. SERVICE_REDIRECT remains disabled by default.

### W3 gameplay instruction capture (2026-09-30)

**MEASURED:** the frozen `SERVICE_REDIRECT=1 TRACE_CODE=1 DOUBLE_SCENARIO=1`
candidate completes deal, draw and accepted Double (round 1), with zero error
or reset and normal return. Nine captures cover 900 PAL fields / 18.000 s:
66,253 Line-A entries, **zero trace entries**, 5,313 full dispatches and
2,213 virtual IRQs. The sampled intervals execute 16.76 board seconds
(ratio 0.931), with 615 AY writes and 25 cache hits from 39 sequence starts.
This establishes trace-free service in these gameplay windows, not unobserved
startup/mode coverage or a paired speed improvement.

The analyzer now verifies the redirect wrapper's diagnostic fallback arm
(`ORI.W #$8000,16(SP)`) as well as the ordinary wrapper's `(SP)` form, selected
by the linked service-request symbol. Wrong offsets, missing and duplicate arms
are rejected by synthetic tests. The first reduction failed at that verification
step; the game capture itself completed and was reused without rerunning it.
Evidence: `amiga/.run/w3-notrace-trace`,
`/tmp/pokeri-w3-notrace-trace-report.log`, `/tmp/pokeri-w3-parser-test.log`.
Concurrent-source stress and comparative/WHDLoad gates remain open.

### W3 trace-dependent launch modes (2026-09-30)

**DERIVED:** replay is not the only mode requiring trace forwarding. Disabling
short hooks, generic-hook research and the benchmark disable the redirect stub;
a build without SERVICE_REDIRECT also always needs the fixed VBR. Previously
only replay had an early guard, so these modes could reach an unforwarded trace
under WHDLoad defaults.

The startup guard now derives eligibility from the same flag used by the
wrappers, before board allocation/display takeover. Trace-dependent modes with
a moved WHDLoad VBR return code 23 and a specific slave message asking for
NOVBRMOVE. Replay keeps its existing code 21/message. Normal trace-free play
and standalone launches retain their previous policy. No scheduling default
is changed; SERVICE_REDIRECT remains opt-in.

**MEASURED:** the no-short-hooks, generic-hooks and benchmark candidate launches
and the ordinary non-redirect build all refuse moved VBR, with all four save
files byte-identical. The candidate still completes cached moved-VBR cold/warm
launches, saving both files and exact backups. The replay-specific refusal also
passes without creating saves. The normal release build also starts, saves and
returns with fixed VBR (`tmp/whdload-test-1jz5ay6r`). Linked runtime startup tests pass 28 cases on
000/020/030/040. Both build variants pass their normal build audits.

Reproduce refusals with `tools/test_whdload.py --vbr moved
--expect-trace-vbr-refusal MODE` plus the usual executable/slave/ROM/RTB options;
MODE is `normal`, `no-short-hooks`, `generic-hooks` or `benchmark`. The normal
case expects a non-redirect build. Evidence: `/tmp/pokeri-w3-mode-*.log`, frozen
candidate `tmp/w3-mode`, positive fixture `tmp/whdload-test-aokoedy3`.

### W1 control repeat-count completion (2026-09-30)

**MEASURED:** three additional independent fresh/warm pairs pass on WHDLoad
19.2 with the current normal build and fixed VBR:

| PRELOAD | Write cache | Fixture | Result |
|---|---|---|---|
| off | enabled | `tmp/whdload-test-8fzczwz3` | cold/warm return, both saves and exact backups |
| off | disabled | `tmp/whdload-test-86n2mnnl` | cold/warm return, both saves and exact backups |
| on | disabled | `tmp/whdload-test-k0ksgi6u` | cold/warm return, both saves and exact backups |

Logs: `/tmp/pokeri-w1-final-control-{off-cached,off-uncached,on-uncached}.log`.
The PRELOAD-off pairs supplement the earlier independent fixtures
`nqjccldg` and `b1shoi84` (each fresh plus two warm runs). PRELOAD-on uncached
also has the current-build fresh return `1jz5ay6r` and the previously recorded
cold/warm uncached controls. PRELOAD-on cached has three independent fresh/warm
fixtures in each of the 19.2/20.0 matrices. Thus the required cold/warm repeat
counts across all four combinations are covered; the emulated full exit-time
measurement remains open, so this does not close W1 or remove NOWRITECACHE.

### W3 real Exec CIA-source stress (2026-09-30)

The diagnostic-only `CIA_STRESS=1` build causes the keyboard source owned by
Pokeri using `cia.resource/SetICR(SETCLR|SP)` once per PAL VBI, after display
publication. This is the documented way to request an owned CIA source
([CIA resource](https://wiki.amigaos.net/wiki/Cia.resource)). Exec dispatches the
real registered keyboard handler, including its serial read/acknowledgement.
The test counts the callback and discards its synthetic, unspecified serial
byte; it does not inject a key into the game. At most one request is outstanding.
The ordinary CIA guest-clock source and native software PORTS service remain
active. This is a source-routing stress test, not a physical-key or QuitKey test.
Do not use this diagnostic for interactive keyboard testing or performance.

**MEASURED:** warm standalone A1200 live24 completes 480,000,000 board cycles
with 3,429 requests and 3,429 deliveries, zero delayed/cancelled/pending requests,
all 24 scripted inputs, status 4, zero error/reset and restored vectors. Under
WHDLoad defaults (moved VBR, PRELOAD, write cache), cold and warm finite-budget
launches deliver 1,165/1,165 and 980/980 respectively, again with zero
delayed/cancelled/pending requests. Both return, save and preserve exact backups.
These counts prove that the requested serial-source work is not lost while
the tested service mechanism is active; they do not enumerate every possible
hardware phase or validate the keyboard's physical serial-bit transport.

The test records final counters in an authored `POK!CIA!STRESS01` marker.
`tools/test_whdload.py --capture-fast --expect-cia-stress --debug-port PORT`
validates the unique nonzero record using a read-only post-return RAM capture.
Missing/conflicting records, undelivered requests and delayed delivery fail;
owned emulator cleanup precedes those assertions. No game-side file I/O or
new timer programming is added. Normal `.text/.rodata/.data/.bss` are byte-exact
against the pre-test normal executable after disabling CIA_STRESS again.

Evidence: `amiga/.run/w3-cia-stress-aga/gdb-out.log`,
`/tmp/pokeri-w3-cia-whdload.log`, `tmp/whdload-test-tdq_dyrl/fast-{1,2}.bin`,
`tmp/w3-cia-stress`, `/tmp/pokeri-w3-cia-{stress,record}-{build,restore}.log`.
The standalone run directory retains its original executable/ELF; the frozen
`tmp/w3-cia-stress` pair includes the subsequent final-record-only addition.
Physical key/F10 validation and comparative performance gates remain open.
