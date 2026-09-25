# Native sampling and batch measurements

`native-measure` now selects counters and a VBI PC sampler. The former
per-access/per-command `ReadEClock` scopes have been removed. Normal launches
allocate no sampling storage. An explicit profile allocates 768 KB of sample
records and 16 KB of hook counters in Fast RAM. Samples are 12-byte big-endian
records: interrupted PC, virtual board cycles and nested scope context. There
are 65,536 slots; overflow is counted, never silently overwritten.

The level-3 wrapper checks both VERTB request and enable. A BLIT-only interrupt
does not sample. The wrapper preserves caller-clobbered registers before calling
the collector, including when interrupting a C++ service. Diagnostic replay can
also enable sampling. Its full-RAM comparison checks register preservation.

Stage the ROM link and a `native-measure` marker in an isolated run directory,
then use `POKERI_REPLAY=0 POKERI_RUN_DIR=.run/your-profile` with
`GDBSCRIPT=profile.gdb` and the usual `diag_run.sh`. Debug launchers mute host
audio. The script writes captures in `tmp/`; save each under its own prefix
before another run. A full-hand script can break at `nativePlayReady` and
`nativeReturned` to separate startup and play.

After sourcing `amiga/env.sh`, attribute samples with:

```
python3 host/native_profile.py --samples tmp/native-profile.bin \
  --elf amiga/.run/your-profile/Pokeri.elf \
  --log amiga/.run/your-profile/gdb-out.log
```

Use `--cycle-start`/`--cycle-end` to select a phase. Use the ELF copied into the
run directory, not an executable rebuilt afterwards. PC symbol relocation comes
from the `BASE` line. The PC rows are flat samples; scope context is nested and
must not be added to them. Masked sections, higher-priority interrupts and code
phase-locked to VBI are under-sampled. Counts/50 approximate PAL time; they do
not replace the elapsed-time counter or prove 50 FPS.

Milestones identify first presentation swap (not a verified nonblank image),
last checksum data read, drain completion, ready and finish. Their time resolution
is one VBI; the final read marker precedes the checksum function's return by a
few instructions. Live `nativeInstructions` remains the existing ABI name but
all new output labels it `dispatches`; replay retains actual instruction counts.
The clock ledger records existing units unchanged: measured guest intervals,
charged intervals after subtraction, hooked-instruction charges and exact boot
poll-loop charges. It does not silently fix the pending clock-unit defect.

`native-benchmark` selects a separate synthetic diagnostic and requires
`POKERI_REPLAY=0`. It enters the native service environment, runs six batches of
512 read-only status operations, restores vectors and exits without executing
the game. `nativeBenchTicks` contains whole-batch E-clock ticks for: full C
nativeDispatch; executeHook+checked Bus; checked Bus read; Board read; HD63484
status read; register-reset control. The first two and the control reset the
synthetic CPU context per iteration. The others do not. These batches isolate
C layers; they do not include a Line-A exception per operation. Only batch
boundaries read timer.device, and Amiga interrupts remain enabled so its overflow
accounting runs. An earlier masked batch produced a backwards timestamp and
was discarded. Keep that failed result out of performance comparisons.

## Results, 2026-09-25

- Active sampling during ECS replay: all 262,144 RAM bytes match at 876,360
  instructions / 8,000,002 cycles; blitter tests pass and vectors restore.
  `tmp/sample-replay-comparison.log`.
- Paired early direct boots of the same binary to 8,000,000 virtual cycles:
  sampling off 3,537 VBIs / 136,944 dispatches; on 3,637 VBIs / 134,669 dispatches.
  About 2.8% more PAL time, with 1.7% fewer dispatches because live timing is
  sensitive to measurement and placement. This bounds this early workload's
  observer impact, not the whole hand. `tmp/sample-off-driver.log` and
  `tmp/sample-on-driver.log`.
- Full profiled A1200 direct boot/play: 612,000,000 cycles, 1,170,897 dispatches,
  50,583 samples, no drops, 371 submitted/swapped frames, null error, vectors
  restored and one expected startup watchdog reset. E-clock duration 1,012.89 s.
  `amiga/.run/pc-sample/gdb-out.log`. Do not directly subtract this from a
  differently compiled older build and call the difference profiler overhead.
- Startup below 22 million virtual cycles: 33.50% of PC samples in nativeDispatch,
  16.61% Bus::access, 10.45% executeHook, 4.71% canonical address conversion,
  4.11% Board::irq. `tmp/pc-sample-startup.txt`.
- Play at/after 324 million cycles: 13.03% nativeDispatch, 10.89% planar plot4,
  10.76% pixelAddress, 10.58% HD plot, 6.59% HD pixel, 4.68% planar pixel4,
  plus their wrappers. Generic per-pixel drawing is the dominant play target.
  `tmp/pc-sample-play.txt`.
- Batch microbenchmark: 406.54, 239.77, 91.47, 24.59, 13.31 and 49.93 microseconds
  per iteration respectively. Full C dispatch minus context-reset control is
  approximately 356.61 microseconds; this is a synthetic startup status access,
  not the mean of all game dispatches. `tmp/ablation-driver.log`.

The evidence supports specialized C access paths first for startup and shifted
blitter copies for gameplay. It does not select the throughput cap K, establish
FIFO glue behavior, or validate changing production timing.

The opt-in host `--pc-histogram tmp/path.csv` records instruction counts per
original PC and reference board-second. It does not allocate a histogram during
normal play. The 69.5-second strict reference run counts 71,664,173 instructions,
matching the sum of its CSV (`tmp/pc-headroom.csv`). The two instructions at
`$2442/$2444` account for 98.2% of instructions in seconds 8–18 (idle), 91.5%
in 25–35 and 94.3% in 55–65 (play). These are instruction shares, not cycle
shares or proof of a safe throughput multiplier. Option C was explicitly
approved after this measurement; calibration and validation remain required.

## Prepared access checkpoint

The loader guards every admitted instruction's full extension bytes before
relocation, then captures immutable operands and relocated device addresses.
Concrete prepared buses retain exact address/width/direction checks. Video
accesses call the existing shared device methods; other devices and uncommon
operands retain the checked fallback. `native-generic-hooks` forces the old
executor. No FIFO semantics change is made.

Both executors pass 7,040 Musashi comparisons including all registers/CCR,
postincrement aliasing, A7 byte increments, displacement/absolute/indexed
operands, bus order and matching partial state on injected data faults. The
ECS prepared replay passes all 262,144 RAM bytes after 6,083,063 instructions,
64,000,000 cycles and 4,307 interrupts (`tmp/prepared-replay-comparison.log`).

The same A1200 status batch measures 208.49 microseconds for prepared C dispatch
after subtracting its register-reset control, versus 356.61 microseconds before
(42% lower). This excludes exception entry and still misses the short-access
budget. No per-access OS timing calls were added. Evidence:
`tmp/prepared-ablation-driver.log`; the 512-operation batch ticks are 92,427
for full C dispatch and 16,703 for the context-reset control at 709,379 Hz.


## Assembly status and bounded-clock checkpoint

The admitted immediate BTST status path saves only D0/A0/A1 and executes in
assembly, using an exact status snapshot published by the shared model after
full services. Address/site guards and the original CCR result are retained.
General accesses, device mutations and scheduling still use C++. Actual Amiga
IRQ wrappers also call the small clock-accounting routines; they do not execute
the original game's interrupt handlers. See [native-clock.md](native-clock.md).

The synthetic 512-exception batch costs 38.65 us per Line-A/RTE after its
matched loop control (14,172 minus 136 E-ticks at 709,379 Hz). This includes
CIA boundaries and still exceeds the 25 us target. It is not comparable to the
208.49 us general C batch as a whole-game speedup.

The K=2 experiment's 60-second gameplay interval has 8,124 VBI samples
(162.48 PAL seconds): 31.45% nativeDispatch, 21.33% guest ROM $02400..,
5.11% prepared execution, 4.76% clock accounting, 4.43% output overlay drawing,
4.33% wide integer multiply, 4.04% blitter wait and 3.13% prepared bus access.
These are flat sampled-PC shares, not inclusive costs. The earlier per-pixel
bottleneck has shrunk; the general dispatcher remains the first target.
The run completes at 804,000,000 cycles with one expected reset, all scripted
inputs and restored vectors. Its K=2 remains experimental: the tighter paired
drain budget selects a lower default, detailed in native-clock.md.
Evidence: `amiga/.run/clock-final/gdb-out.log`, `tmp/clock-final-samples.bin`;
use that run's saved ELF when attributing its samples.


Exact direct-call and hook-form counts, split at the ready boundary, are now in
[native-dispatch-profile.md](native-dispatch-profile.md). The additional counters
require `DISPATCH_PROFILE=1`; they are compiled out of the normal executable.
The gameplay ranking differs from startup: CPU control, PIA operand forms,
low-vector sentinel checks and ignored ROM probes dominate entry count.
