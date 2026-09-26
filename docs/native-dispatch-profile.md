# Native dispatcher call distribution

Measured on 2026-09-25 with the selected K=1.5 clock, an A1200 direct cold boot,
and 60 board-seconds of scripted input after the 40.5-second operator setup.
This reports **call counts**, separately from the sampled time profile. A
routine called frequently is an assembly candidate, but frequency alone does
not measure its cost or establish a speedup.

The later approved fast-start policy removes startup diagnostics and the repeated
ROM/RAM probe. These counts describe the earlier workload and remain useful
for ranking the other gameplay handlers; they are not post-bypass measurements.

## Completed counter run after input pacing correction

The next counter run (`amiga/.run/cabinet-distribution/gdb-out.log`) reaches its
600,000,000-cycle budget without a transport error or watchdog reset. It
starts play at 94,880,000 cycles, so the measured interval is **63.14 board
seconds**, not 60. All 24 key transitions are delivered and the external-message
queue empties. Final frame inspection still shows ROM attention `P2 87` after
service actions; this is not full gameplay/service acceptance.

There are 276,065 gameplay entries: **234,937 (85.10%)** take the short assembly
path. Of 46,973 full dispatcher calls, 5,845 promote an already-counted short
access. Leading forms are short SR logic (10.40%), short RTE (10.30%), the four
short PIA forms (26.86% combined), and short TRAP #8 (6.94%). Remaining full
video command-word writes are 10,242 (3.71%), and video address-register writes
6,874 (2.49%). Startup has 239,934 entries, including 153,609 short operations;
4,852 of 91,177 full calls are promotions. All counts reconcile.

Reproduce with `tmp/cabinet-distribution-{ready,end}` prefixes,
`--play-seconds 63.14` and the default 32-byte descriptor layout. The matching
report is `tmp/cabinet-distribution-report.md`. Counters remain separate from
normal-build time measurements and do not establish a latency reduction.

## Updated reduced-save coverage (2026-09-26)

The latest instrumented capture uses 32-byte descriptors and separate TRAP
counters. It reconciles 239,841 startup entries: 153,451 short accesses (63.98%)
and 91,206 full dispatcher calls, of which 4,816 promote a short access already
counted. The 31.06-board-second gameplay prefix has 156,956 entries, including
126,858 short accesses (80.82%); 2,838 of 32,936 dispatcher calls are promotions.
This capture stops on a serial checksum error around service-door input, so it
is a partial call-frequency sample, **not** a completed acceptance workload or
a replacement for the historical full 60-second counts below. Normal cabinet
input pacing is being validated separately.

The largest remaining C access families are video command-word writes (8,851;
5.64% of all gameplay-prefix entries) and video address-register writes (5,724;
3.65%). At startup these two families account for 34,636 and 25,997 accesses,
respectively. Ordinary short paths now cover SR/RTE, PIA/ACIA byte moves,
bounded-memory CMP/TST and original TRAP frames, as well as video status.
This supports prioritizing command feeding next, subject to the performance
plan's explicit FIFO decision gate. It does not prove an execution-time gain
from call counts alone.

Evidence: `amiga/.run/short-distribution/gdb-out.log` and
`tmp/short-distribution-{ready,end}-*.bin`. Use `--play-seconds 31.06` for this
partial capture. The reporter subtracts promotions when counting original
entries and adds separate short TRAP counts; legacy 16-byte descriptor captures
require `--descriptor-bytes 16`. Normal builds exclude these extra site counters.

The sections below retain the earlier baseline and its exact scope.

## Method and scope

`DISPATCH_PROFILE=1` adds counters at direct `nativeDispatch` call sites and
its virtual CPU-control branches. It also uses the spare fourth longword in
each guarded short-status descriptor to count the assembly path by site. The
existing counters classify full dispatcher entry kinds and Line-A indices.
`native-measure` must also be present. No per-call OS timers or logs are used.

The extra instrumentation is compiled out of normal builds. Disassembled
normal instructions match the previously validated executable exactly; the
explicit diagnostic build also matches the measured instrumented executable.
Normal audio remains enabled; this emulator capture was muted.

`amiga/dispatch-profile.gdb` captures counters at `nativePlayReady` and
`nativeReturned`. Gameplay is the difference, avoiding startup's checksum and
refill traffic. The ready breakpoint falls inside one dispatcher invocation;
that invocation's remaining helper calls belong to the gameplay delta. Counts
are exact for this instrumented schedule, not necessarily the counts of an
unprofiled run with different real-time IRQ ordering.

The direct-work counters include inlined call sites and CPU-control branches,
not every descendant called by a device service. Clock-accounting calls made
inside `nativeClockPause` or Amiga IRQ wrappers are not direct dispatcher calls
and are excluded from its direct-charge row. Rows overlap: each dispatcher
entry calls several helpers. Neither those counts nor entry percentages are
inclusive CPU-time percentages.

The report verifies that full hook counts sum to full Line-A entries, that all
Line-A forms are classified, and that snapshot counters never go backwards.
The captured short total also reconciles with the independent global counter.
The GDB captures, program images and generated tables remain ignored locally;
this document contains counts, names and addresses only.

## Reproduction

Build from the root with the shared toolchain loaded:

```
. amiga/env.sh
make -C amiga clean
make -C amiga -j4 DISPATCH_PROFILE=1
```

Use an isolated debug run with `native-measure`, `native-test-inputs`, and a
four-byte big-endian `native-live` limit of 804000000. Select
`GDBSCRIPT=dispatch-profile.gdb`, `POKERI_REPLAY=0` and `AMIGA_MODEL=A1200`.
The script writes `tmp/dispatch-ready-*.bin` and `tmp/dispatch-end-*.bin`.

```
python3 host/native_dispatch_profile.py \
  --ready tmp/dispatch-ready --end tmp/dispatch-end --play-seconds 60
```

For sampled time, use the matching ELF saved in the capture directory:

```
python3 host/native_profile.py \
  --samples tmp/dispatch-samples.bin \
  --elf amiga/.run/dispatch-distribution/Pokeri.elf \
  --log amiga/.run/dispatch-distribution/gdb-out.log \
  --cycle-start 324000000
```

After collecting, clean and rebuild without `DISPATCH_PROFILE=1`. The default
`amiga/out/Pokeri` has been restored to that normal build.


## Results and assembly priorities

The capture ends at 804,000,000 cycles with no native error, restored vectors,
one expected startup watchdog reset and all 24 scripted input transitions.
Startup has 868,001 entries; gameplay has 271,346 (4,522 per board-second).
Both reconcile exactly with the independent dispatch/short global counters.
The extra counters extend the play sample from 179.80 to 188.96 PAL seconds
on these schedules, about 5.1%. They are suitable for call frequency; use the
non-counter profile for cost estimates. The normal executable has been restored.

For gameplay, the four repeated PIA forms at $FB01E/$FB01C total 65,252 calls
(24.05% of all entries); CPU controls total 59,888 (22.07%), dominated by
30,751 SR-logic operations and 29,093 RTEs. Five comparison/test sites that also admit null-vector sentinels
($616A/$6170/$6186/$61CA/$61E2) total 41,199 (15.18%). The ignored ROM-write
probe at $25AA adds 16,320 (6.01%). Together these are 67.31% of gameplay
entries. Most reads at those five sites use ordinary RAM pointers; these are
not counts of null-vector reads alone. Their tight instruction handlers and common scheduling boundary are
higher gameplay priorities than display word feeding (9,783, 3.61%).

The reduced assembly display-status path covers 22.97% of startup entries but
only 4.21% of gameplay. The later normal-game startup policy bypasses the
checksum/drain, so it is no longer a time-to-ready target. Any research-path
specialization of those accesses still awaits the FIFO model decision. That decision does not prevent work on the CPU-only guards,
virtual CPU-control handling or PIA dispatch boundaries.

Order the next work by the measured workload:

1. Reduce shared clock/IRQ bookkeeping on the hot instruction paths. Full
   gameplay dispatch repeats IRQ queries 265,934 times, clock pause 259,933,
   video-status publication 259,934, and direct cycle accounting 256,707 times.
   Preserve every real IRQ/device boundary; counts alone do not permit dropping
   required work. The preceding normal-build PC sample puts 30.21% in the
   dispatcher, 5.21% in cycle accounting and 4.83% in prepared execution.
2. Give virtual SR/RTE and the frequent PIA operand forms short guarded paths.
   Keep virtual privilege/stack/IRQ behavior exact and the shared device model.
3. Give the comparison/test sites guarded assembly handlers for owned memory
   and null-vector sentinels. The approved startup policy now removes the ROM
   probe. Neither change alters FIFO semantics.
4. Shorten command feeding and checksum/drain accesses after the FIFO decision;
   retain actual shared-model effects, rather than inventing success flags.

These counts establish priorities, not an achieved instruction-cost budget.
The normal build remains about three times slower than PAL during this test.
Detailed counts follow; helper counts overlap and cannot be summed as time.

## Startup through operator setup

868,001 entries: 668,630 full C dispatches and 199,371 short assembly accesses.

### Entry types

| Entry | Count | Share |
|---|---:|---:|
| full Line-A | 652,871 | 75.22% |
| TRAP #8 | 9,974 | 1.15% |
| trace / scheduler | 3,376 | 0.39% |
| TRAP #2 | 974 | 0.11% |
| TRAP #5 | 969 | 0.11% |
| TRAP #7 | 200 | 0.02% |
| TRAP #1 | 155 | 0.02% |
| TRAP #11 | 100 | 0.01% |
| TRAP #3 | 6 | 0.00% |
| TRAP #9 | 3 | 0.00% |
| TRAP #12 | 2 | 0.00% |
| short status assembly | 199,371 | 22.97% |

### Hook families (share of all entries)

| Handler / operand form | Calls | Share |
|---|---:|---:|
| ASM: bit_test.8 immediate → indirect(0) [$F6000] | 199,371 | 22.97% |
| C: move.16 immediate → displacement(0) [$F6002] | 94,702 | 10.91% |
| C: move.8 displacement(0) → data(0) [$F6002] | 76,155 | 8.77% |
| C: test.8 none → displacement(0) [$F6002] | 76,152 | 8.77% |
| C: bit_test.8 immediate → displacement(3) [$FB01F] | 63,844 | 7.36% |
| C: move.8 displacement(6) → displacement(3) [$FB01E] | 54,300 | 6.26% |
| C: move.32 immediate → indirect(0) [ROM/RAM] | 50,881 | 5.86% |
| C: move.8 data(4) → displacement(3) [$FB01E] | 50,867 | 5.86% |
| virtual CPU control | 37,822 | 4.36% |
| C: move.16 postincrement(1) → displacement(0) [$F6002] | 36,334 | 4.19% |
| C: move.8 immediate → indirect(0) [$F6000] | 29,546 | 3.40% |
| C: move.16 displacement(0) → data(2) [$F6002] | 16,400 | 1.89% |
| C: move.8 immediate → displacement(0) [$F6002] | 6,902 | 0.80% |
| C: move.8 indexed(4) → displacement(3) [$FB01E] | 6,538 | 0.75% |
| C: move.16 data(3) → displacement(0) [$F6002] | 6,148 | 0.71% |
| C: move.16 data(2) → displacement(0) [$F6002] | 6,148 | 0.71% |
| C: move.8 displacement(3) → data(2) [$FB01C] | 5,787 | 0.67% |
| C: bit_test.8 immediate → indirect(1) [$FB002] | 5,469 | 0.63% |
| Other hook forms | 28,876 | 3.33% |

### Direct dispatcher work

Counts overlap: one dispatch calls several helpers. They are not time percentages.

| Routine or control operation | Calls | Calls / full dispatch |
|---|---:|---:|
| guest-cycle charge | 730,213 | 1.092 |
| Board::irq | 672,679 | 1.006 |
| clock pause | 668,630 | 1.000 |
| publish video status | 668,629 | 1.000 |
| prepared hook executor | 615,030 | 0.920 |
| push virtual exception | 25,086 | 0.038 |
| virtual RTE | 24,117 | 0.036 |
| guard check | 17,871 | 0.027 |
| virtual SR logic | 13,353 | 0.020 |
| advanceClock / Board::tick | 4,050 | 0.006 |
| liveInputs | 4,050 | 0.006 |
| coldSetupStep | 4,050 | 0.006 |
| screen presentation | 2,024 | 0.003 |
| virtual SR read | 350 | 0.001 |
| board reset | 18 | 0.000 |
| virtual USP transfer | 2 | 0.000 |
| clock calibration | 1 | 0.000 |

### Most frequent original access sites

| PC | Path | Calls | Operation |
|---|---|---:|---|
| $1103C | C | 76,152 | test.8 none → displacement(0) [$F6002] |
| $10FCC | C | 76,152 | move.8 displacement(0) → data(0) [$F6002] |
| $10FC0 | C | 76,152 | move.16 immediate → displacement(0) [$F6002] |
| $11040 | ASM | 76,040 | bit_test.8 immediate → indirect(0) [$F6000] |
| $10FC6 | ASM | 76,027 | bit_test.8 immediate → indirect(0) [$F6000] |
| $020BE | C | 63,844 | bit_test.8 immediate → displacement(3) [$FB01F] |
| $025AA | C | 50,879 | move.32 immediate → indirect(0) [ROM/RAM] |
| $013E6 | C | 50,865 | move.8 displacement(6) → displacement(3) [$FB01E] |
| $013E2 | C | 50,865 | move.8 data(4) → displacement(3) [$FB01E] |
| $02E5E | C | 36,334 | move.16 postincrement(1) → displacement(0) [$F6002] |
| $02E58 | ASM | 36,334 | bit_test.8 immediate → indirect(0) [$F6000] |
| $01DD2 | C | 16,400 | move.16 displacement(0) → data(2) [$F6002] |
| $02E82 | C | 6,864 | move.8 immediate → indirect(0) [$F6000] |
| $02E36 | C | 6,864 | move.8 immediate → indirect(0) [$F6000] |
| $02E30 | ASM | 6,864 | bit_test.8 immediate → indirect(0) [$F6000] |
| $02472 | C | 6,538 | move.8 indexed(4) → displacement(3) [$FB01E] |
| $01E5C | C | 6,147 | move.16 data(2) → displacement(0) [$F6002] |
| $01E56 | C | 6,147 | move.16 immediate → displacement(0) [$F6002] |

## Gameplay after ready

271,346 entries: 259,933 full C dispatches and 11,413 short assembly accesses.
4,522.4 entries per board-second.

### Entry types

| Entry | Count | Share |
|---|---:|---:|
| full Line-A | 230,656 | 85.00% |
| TRAP #8 | 16,313 | 6.01% |
| TRAP #2 | 4,779 | 1.76% |
| TRAP #5 | 4,716 | 1.74% |
| trace / scheduler | 3,226 | 1.19% |
| TRAP #7 | 190 | 0.07% |
| TRAP #1 | 26 | 0.01% |
| TRAP #13 | 16 | 0.01% |
| TRAP #3 | 6 | 0.00% |
| TRAP #9 | 2 | 0.00% |
| TRAP #11 | 2 | 0.00% |
| TRAP #12 | 1 | 0.00% |
| short status assembly | 11,413 | 4.21% |

### Hook families (share of all entries)

| Handler / operand form | Calls | Share |
|---|---:|---:|
| virtual CPU control | 59,888 | 22.07% |
| C: compare.32 displacement(2) → data(0) [ROM/RAM] | 18,210 | 6.71% |
| C: move.32 immediate → indirect(0) [ROM/RAM] | 16,320 | 6.01% |
| C: move.8 data(4) → displacement(3) [$FB01E] | 16,313 | 6.01% |
| C: move.8 displacement(6) → displacement(3) [$FB01E] | 16,313 | 6.01% |
| C: move.8 indexed(4) → displacement(3) [$FB01E] | 16,313 | 6.01% |
| C: move.8 displacement(3) → data(2) [$FB01C] | 16,313 | 6.01% |
| C: test.32 none → indirect(2) [ROM/RAM] | 13,431 | 4.95% |
| ASM: bit_test.8 immediate → indirect(0) [$F6000] | 11,413 | 4.21% |
| C: move.16 postincrement(1) → displacement(0) [$F6002] | 9,783 | 3.61% |
| C: move.8 data(1) → displacement(3) [$FB016] | 6,544 | 2.41% |
| C: move.8 immediate → indirect(0) [$F6000] | 6,538 | 2.41% |
| C: move.8 displacement(3) → data(0) [$FB017] | 6,000 | 2.21% |
| C: move.8 displacement(3) → data(0) [$FB016] | 6,000 | 2.21% |
| C: test.32 none → indirect(0) [ROM/RAM] | 4,779 | 1.76% |
| C: compare.32 displacement(0) → data(4) [ROM/RAM] | 4,779 | 1.76% |
| C: move.8 data(0) → displacement(3) [$FB014] | 1,846 | 0.68% |
| C: move.8 data(2) → displacement(3) [$FB014] | 1,786 | 0.66% |
| Other hook forms | 9,500 | 3.50% |

### Direct dispatcher work

Counts overlap: one dispatch calls several helpers. They are not time percentages.

| Routine or control operation | Calls | Calls / full dispatch |
|---|---:|---:|
| Board::irq | 265,934 | 1.023 |
| publish video status | 259,934 | 1.000 |
| clock pause | 259,933 | 1.000 |
| guest-cycle charge | 256,707 | 0.988 |
| prepared hook executor | 170,768 | 0.657 |
| push virtual exception | 33,810 | 0.130 |
| virtual SR logic | 30,751 | 0.118 |
| virtual RTE | 29,093 | 0.112 |
| guard check | 8,345 | 0.032 |
| advanceClock / Board::tick | 6,000 | 0.023 |
| liveInputs | 6,000 | 0.023 |
| coldSetupStep | 6,000 | 0.023 |
| screen presentation | 3,001 | 0.012 |
| virtual SR read | 44 | 0.000 |

### Most frequent original access sites

| PC | Path | Calls | Operation |
|---|---|---:|---|
| $025AA | C | 16,320 | move.32 immediate → indirect(0) [ROM/RAM] |
| $0B3E2 | C | 16,313 | move.8 displacement(3) → data(2) [$FB01C] |
| $02472 | C | 16,313 | move.8 indexed(4) → displacement(3) [$FB01E] |
| $013E6 | C | 16,313 | move.8 displacement(6) → displacement(3) [$FB01E] |
| $013E2 | C | 16,313 | move.8 data(4) → displacement(3) [$FB01E] |
| $06170 | C | 13,431 | test.32 none → indirect(2) [ROM/RAM] |
| $0616A | C | 13,431 | compare.32 displacement(2) → data(0) [ROM/RAM] |
| $02E5E | C | 9,783 | move.16 postincrement(1) → displacement(0) [$F6002] |
| $02E58 | ASM | 9,783 | bit_test.8 immediate → indirect(0) [$F6000] |
| $00C48 | C | 6,000 | move.8 displacement(3) → data(0) [$FB016] |
| $00C40 | C | 6,000 | move.8 displacement(3) → data(0) [$FB017] |
| $061E2 | C | 4,779 | compare.32 displacement(0) → data(4) [ROM/RAM] |
| $061CA | C | 4,779 | test.32 none → indirect(0) [ROM/RAM] |
| $06186 | C | 4,779 | compare.32 displacement(2) → data(0) [ROM/RAM] |
| $02E82 | C | 1,630 | move.8 immediate → indirect(0) [$F6000] |
| $02E36 | C | 1,630 | move.8 immediate → indirect(0) [$F6000] |
| $02E30 | ASM | 1,630 | bit_test.8 immediate → indirect(0) [$F6000] |
| $00D2A | C | 1,396 | move.8 data(1) → displacement(3) [$FB016] |
