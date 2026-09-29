# Separate instruction execution from the native scheduler

2026-09-29. Baseline: `dfcb756`. This is a function-layout optimization, not
another guest hook or a change to interrupt delivery or timing.

The general Line-A executor formerly lived inside `nativeDispatch`, sharing
compiler register allocation and code layout with the common scheduler. Moving
that block into a non-inlined helper leaves the common path smaller. A normalized
source comparison verifies that its instruction-effects body is unchanged.
Its failure return still exits through the dispatcher's existing interrupt and
measurement scopes.

## Controlled measurements

**MEASURED:** A1200, existing isolated benchmark, 512 iterations, 709,379 E-clock
ticks/second. One clock pair surrounds each batch; no per-operation observer.

| Batch | Before ticks | After ticks |
|---|---:|---:|
| Full C interrupt dispatch | 86,174 | 80,194 |
| Exception-frame construction | 31,789 | 31,459 |
| Board IRQ/vector query | 29,472 | 29,469 |
| IRQ context/control | 16,370 | 16,374 |
| Full C status-hook dispatch | 122,300 | 115,010 |
| Generic hook only | 76,207 | 76,401 |
| Hook context/control | 16,697 | 16,480 |

The full dispatch batches improve by 6.94% and 5.96% respectively. These exclude
assembly exception entry/return and original interrupt-handler execution.
The isolated cached-card feeder is essentially unchanged (59,691 versus 59,690
ticks for four cards); that batch does not execute the original interrupts.

`nativeDispatch` shrinks from 15,020 to 8,970 bytes and the new helper occupies
6,662 bytes. This is not a claim of lower total code size or fewer saved
registers: the new dispatch prologue actually saves one more register. The
measured total service cost establishes the benefit, not an assumed compiler
explanation.

Local evidence: `amiga/.run/dispatch-outline-{before,after}` and frozen
`tmp/perf/Pokeri-dispatch-outline-{before,after}(.elf)`.

## Normal gameplay

**MEASURED:** the candidate completes the cold 24-input A1200 scenario with
status 4, no error/reset and restored vectors. Post-Ready 54.11 board seconds
occupy 56.771 PAL seconds (ratio 0.9531). Deal/draw ratios are 0.9091/0.9436.
The preceding baseline was 0.9527 overall, 0.9090/0.9338 deal/draw. Different
live hands prevent treating this small whole-session difference as a controlled
speedup.

Complete landing backs 24–27 take 38.208, 38.336, 37.856 and 39.424 ms, versus
38.784, 39.936, 39.872 and 38.592 ms in the baseline. Light backs 21/23 take
17.088/17.696 ms. These are observed command-feed intervals, not final display
publication times or worst-case bounds. The 20 ms complete-card target remains
open. The hand does not accept Double; it supplies no new Double audio claim.
Local evidence: `.run/dispatch-outline-live`, `tmp/dispatch-outline-live.txt`.

## Validation

The default flag-free allocated code/data sections match the frozen normal
candidate. Headless model/platform/native tests and linked short/feed CPU
checks pass, including 3,755,520 whole-feed instruction-boundary cases.
Both ECS and AGA replay match 262,144 RAM bytes, 524,288 VRAM bytes, 172,064 pixels and
60 AY writes at 7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs.
Cold ECS live also completes all 24 inputs with error/reset zero and restored
vectors. The default implementation is validated; the release archive has not
been repackaged by this change.

The read-only startup VBI probe records 1,063 samples. One arrives at scanline
36 and completes at 40 during the intentionally masked CPU-speed sample, with
board cycles zero and the display inactive. This does not extend the earlier
calibration result into a universal startup latency guarantee. After board time
starts, all 1,059 samples enter by line 1 and complete by line 6. Calibration
minimum/maximum remains 33/45, overhead 40, gameplay ceiling 80.
Evidence: `tmp/dispatch-outline-headless.log`,
`.run/dispatch-outline-{aga,ecs,live,live-ecs,vbi}` and the corresponding
`tmp/dispatch-outline-*-check.log` files.


## Broader compiler setting experiment (not retained)

A separate target-only `-O3` build of Native.cpp, with all other compilation
unchanged, reduces the 512-iteration IRQ/hook batches further to 77,246/108,745
ticks (context controls 16,061/15,577). Generic-hook/frame batches fall to
61,819/25,279 ticks. The four-card feed remains effectively unchanged at
59,843 ticks. All isolated batches end with zero board cycles and no error.

The cold A1200 24-input run passes without error/reset and restores vectors.
Its post-Ready ratio is 0.9553, deal/draw 0.9071/0.9414; complete landing backs
are 38.208/39.104/37.280/38.272 ms. Different live hands and mixed interval
results do not establish a useful consistent gameplay gain from the broader
compiler change. It is not retained and no full replay acceptance is claimed
for that variant. No tracked build switch or optimization override remains.
Local evidence: `.run/native-o3-{bench,live}`, `tmp/native-o3.mk` and frozen
`tmp/perf/Pokeri-native-o3{,-normal}(.elf)`.
