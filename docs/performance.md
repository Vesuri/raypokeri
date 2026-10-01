# Performance and experiment register

Updated 2026-10-01. Current correctness gates and reproduction commands are in
[testing.md](testing.md). The port is playable, but complete-card and sound-write
deadlines remain unmet; aggregate timing near real time is not a 50 FPS claim.

## Measurement definitions

Board/PAL ratio is elapsed board seconds divided by elapsed PAL seconds; 1.0
means real time. K is a throughput cap, not that ratio: the calibrated clock
requests K=1.5 in boot and K=4 in play, bounded by the detected CPU ceiling.
Native services earn no guest-execution time. PAL time limits spendable credit.
The bounded clock, startup fast-forward, and PAL-time envelopes are approved
policies; further clock or interrupt changes need a separate decision.

Complete-card interval means recognition begin to cache hit, including original
command feeding; it excludes later display publication. It is not the isolated
DMA blit time. AY batch span is the time to apply original register writes;
excess gap is PAL minus board time between batches. A real-time envelope can
finish even though a later register write is delayed. Report both separately.

Use normal-code read-only debugger probes for acceptance. Full ledger observers
add about 6–7% service overhead and historically inflated cold startup by about
45%; VBI PC sampling misses masked code. Do not compare those totals directly.
Hands can differ with timing: compare paired hands or state the sample mismatch.
ECS runs establish compatibility, not A1200 performance.

## Current evidence

Cleanup qualification passes exact ECS/AGA replay, both machines' cold/warm
live24, A1200 Double and the A1200 startup VBI deadline. ECS Double reached its
12-hand no-win diagnostic limit; the user accepted AGA Double coverage on
2026-10-01 instead of another ECS sample. These are workload observations, not promises for
all game states or physical machines.

| Scope | Measurement | Qualification |
| --- | --- | --- |
| Cleanup A1200 cold live24, 2026-10-01 | Ready frame 793; 54.070 board s / 55.288 PAL s = 0.9780 | 24 inputs, zero errors/resets, vectors restored |
| Cleanup A1200 warm live24 | Ready frame 379; 59.370 / 60.378 s = 0.9833 | same completion gates; no accepted Double in live24 |
| Cleanup A1200 complete cards | cold median 37.568 ms (16.768–62.432); warm 34.368 (16.704–42.688) | 15/13 cards; different hands, not a paired cleanup comparison |
| Cleanup A1200 Double | accepted round 5; 116.090 board / 119.733 PAL s = 0.9696 | 46 transitions, no reset/error, vectors restored |
| Same Double sound/cards | AY batch median 9.2 ms; largest excess 272.5 ms; card median 34.336 ms | 219 AY batches, 110 complete cards; late writes still remain |
| Cleanup ECS cold/warm live24 | ratios 0.2837 / 0.2856; Ready frames 3773 / 1889 | all 24 inputs each, no reset/error, vectors restored |
| Cleanup A1200 startup VBI | 793 samples; entry ≤1, service ≤5, done ≤6 | no sample at line 29 or later |
| T13 paired A1200 cold cards, 2026-10-01 | median 39.10 → 35.20 ms; total −5.1% | 15 matching cards, 14 faster; median saving 1.15 ms |
| T13 paired warm cards | 37.70 → 36.54 ms; total −1.7% | 13 matching cards; median saving 0.90 ms |
| Pre-T13 Double, 2026-09-30 | median 34.912 ms, range 16.704–57.312 | different workload; not a paired cleanup comparison |
| Same Double AY writes | median batch 9.2 ms; maximum excess gap 225.6 ms | envelope decay already runs on PAL time |
| T13 Double | shuffle-gap excess 276.2 → 256.7 ms | unpaired hands; not a universal latency bound |
| WHDLoad startup, 2026-09-30 | cold 17.22 s; warm 8.36–8.40 s | includes preparation/init, excludes loading and early CRT |
| T13 A1200 VBI-origin Ready | cold frames 807/808; warm 389/384 | excludes roughly 0.42–0.44 s preparation |
| T13 A1200 live24 board/PAL | cold 0.9761; warm 0.9807 | workload totals hide drawing stalls |
| T13 traced drawing bursts | 0.935 | trace capture scope, not a whole-session guarantee |
| T13 A500+/ECS live24 | about 0.28 | approximately 3.5× slower than real time |

## Alignment regression and rule

**MEASURED:** removing unused 16-bit switches moved 101 hot longword-sized data
objects onto addresses congruent to 2 modulo 4. A paired idle-host comparison
showed cold Ready moving from frame 808 to 874 (about 8% slower) and gameplay
about 0.65% slower. The original build itself had 45 such objects.

**DERIVED:** m68k GCC permits two-byte alignment for these globals; on the 68020
a crossing longword access requires an extra bus cycle. Explicit alignas(4) in
Native, NativeTiming, GCCRuntime, Pokeri, AmigaInput and AmigaHardware removes
the layout dependency. The initial fixed measurement reaches frame 793, about
1.7% faster than the pre-cleanup frame 807/808 baseline.

The Makefile audit rejects an even-sized .data/.bss object of at least four bytes
whose address is not divisible by four. This is a build invariant, not a compiler
optimization flag. Keep it when adding fields or removing globals. The separate
software-mul/div audit remains in force. Linked CPU oracles test semantics;
normal gameplay and VBI probes test the timing consequence. The retained ledger/
dispatch/startup/VBI diagnostic configuration also passes the audit after aligning
its own data; normal allocated ELF sections remain identical to the validated
frozen build. The audit caught those diagnostic objects during this check.

## Rendering and remaining cost

The card body/back is cached, but its original command stream still runs. Resident
font/rank/suit/face artwork is copied; simple clears/bars already use blitter fills.
[Architecture](architecture.md#planar-graphics) lists fast routes and guarded
fallbacks. Drawing plus feed/interrupt processing still exceeds 20 ms for many
cards. The joined T13 handler saves about 1 ms/card; it did not meet its estimated
20 ms landing or 150 ms Double-entry targets, and the latter was not remeasured
as a whole-block bound. A new drawing-side proposal is needed, not another
unqualified claim that the blit itself is fast.

Do not revive the rejected compact-Board-dispatch experiment based on its isolated
5.48 µs admission saving: it showed no card gain. The later compact **guest address
windows** are a separate retained memory-saving change. T12 startup-artwork
caching was explicitly declined. T11 timed-ACRTC research does not authorize
changing production FIFO/busy/IRQ semantics. A500 tuning and hardware calibration
remain deferred. See [remaining-work.md](remaining-work.md).

## Experiment register

One-line records preserve the outcome of the former plan/experiment documents.
Figures below are historical before/after samples at their recorded scope; they
are not additive and are not current release benchmarks. Settled alternatives,
opt-in experiments and their benchmark drivers have been removed from the working
tree; Git history preserves them. “Retained” means the implementation remains,
not that its old compile-time flag is supported.

| Experiment | Disposition | Result / reason |
| --- | --- | --- |
| A1 word-parallel PAINT | retained | deal PAINT 8.026→3.414 ms/call |
| A2 curve stamps | retained | deal CRCL 4.541→1.591 ms, ELPS 3.025→0.780 ms |
| A3 planar lines | retained | RPLL deal mean 16.84→6.54 ms (estimated 3–5 ms not met) |
| A4 flipped AGCPY | retained | 17×17 `$E300` 53.2→2.99 ms |
| A5 front end / small fills | retained | synthetic face 221.7→210.4 ms; unconditional-drain 64-word fill variant rejected (RFRCT 0.92→1.27 ms) |
| A6 direct plane view | retained | synthetic face →205.1/171.9 ms cold/warm |
| B1 wall-cadence presentation | rejected | deal 13.08→15.46 s for 8 board-s |
| B2 COP1LC reload, ownership | retained | zero late-swap deferrals |
| B3 damage rects (+B1 retry) | rejected; historical patch | deal 13.01→14.04 s (15.90 s with B1) |
| B4 zero-copy ECS display | deferred (later ECS) | not implemented |
| C1 delay-loop idle hook | research removed | deal guest 3.47→0.21 s but elapsed only 13.14→12.97 s |
| C2 whole feed loop | retained | 211.7→185.8 µs/word (−12.2%) |
| C3 FIFO word path | retained | 512-word feed 95.1→88.9 ms (−6.5%) |
| C4 dispatcher prologue | retained | 116→100 µs per deal call |
| D1 deferred ACRTC commands | rejected | 0 of 13,066 commands ran at idle (barriers) |
| D2 credit window 1–3 frames | retained = 3 | post-ready 64.59→60.33 s (−6.6%) |
| Blitter-line mode, rotated-glyph cache | not added | not prerequisites |
| Card-back result cache | retained | DMA 4.0–5.6 ms per back |
| White-prefix reuse | retained | 36.63→13.93 ms service |
| Inline intermediate params | retained | WPTN batch 82.8→51.5 ms (−37.8%) |
| Header bridge | retained | WPR feed −18.6% |
| Feeder counters out of normal build | retained | −4.5% / −7.6% |
| Address-selector asm | retained | −54% per op; cold/warm Ready −1.70/−1.26 s |
| Register-resident feeder | retained | WPR/WPTN −17.6/−29.0% |
| Virtual stack switch | retained | 133.59→33.28 ms/512 (conservative) |
| Virtual user TRAP | retained | 135.38→35.42 ms/512 |
| Inline feeder boundaries | retained | −5.0/−7.6% |
| Read-only display DMA | retained | −7.8% |
| C cached-completion shortcut | rejected | 14.3% slower |
| Assembly raster completion | retained | −4.5% |
| Cached register/move | retained | −14.0% vs raster-only |
| Bounded card repair / completed-card presentation | retained | repair −48.85% elapsed per iteration |
| Translated cached AMOVE | retained | 27.888→26.005 ms/card |
| Short-command copy | rejected | 0.37% slower |
| White-prefix reconstruction | not retained | — |
| Temporary drain priority | rejected | VBI deadline regression |
| Packed drawing-position update | retained | ~0.2–0.3% |
| Joined masked branch check | retained | 23.260→22.414 ms/card |
| Source-span guard reuse | retained | 22.414→21.510 ms/card |
| Startup-only fast-forward | retained | cold 34.78→26.88 s at that time |
| Enabled-source IRQ query | rejected | 0.12% |
| Exception-frame word ops | retained | −6.86% |
| Clock compiler inlining | retained | −11.5% (grant-only alone 6.1% slower) |
| Packed cached-position arithmetic | not retained | 0.63% |
| Specialized CCR-low byte write (1st try) | not retained | 1.24% (superseded by FAST_FIFO_VALUE) |
| Live instruction counts off | retained | back −1.9% |
| Compact C++ Board member layout | rejected (distinct from compact guest windows) | ~5.48 µs/admission, no card-feed gain |
| Within-service status reuse | rejected | 11.27 µs/admission, cards unchanged |
| Build-time card preparation | retained | host-prepared image; removes native raster preparation |
| Interleaved PTN tile blits | retained | 2.223→1.722 ms/tile, +12 KB Chip |
| Video IRQ C prototype | historical; now retained component | mean admission 333→334 µs (no gain alone) |
| Early scheduler rejection | rejected | mean 492 µs (outlier), no net gain |
| Single status sample in admission | rejected | 40.800 vs 40.672 ms total |
| Assembly exception frame | retained component | 68000 cycles 6,532→5,602 |
| Dispatcher/executor separation | retained | full dispatch batches −6.94/−5.96%  |
| Target-only `-O3` Native.cpp | not retained | mixed gameplay result |
| Profiling compiled out | retained | whole back 22.64→21.06 ms  |
| Local parameter-store duplication in feeder | rejected | <0.4% |
| K=2 clock | experimental, not selected | comparison only |
| Bounded FIFO-control fusion | retained | 230.28→177.36 µs/triplet (−23.0%) |
| Dedicated CCR-low endpoint | retained | fused triplet a further −14.4% (~26 µs) |
| Handler exit fusion | retained | 130.62→115.37 µs/exit (−11.7%) |
| Handler entry fusion | retained | entry −13.8% AGA / −13.5% ECS |
| Wider register-save/queue-load fusion (2026-09-29) | not pursued | removes no extra exceptions |
| Cache batch prototype 1 | rejected; removed | synthetic 22.63→18.29 ms/card, live landings 42.3→47.5 ms |
| Prototype 2 (precomputed bounds) | rejected; removed | synthetic 17.30 ms/card, landing 26 still 46.8 vs 42.2 ms |
| Inactive-call guard | rejected | landings 47.4–48.5 ms |
| Partial-command re-entry | research removed | 10-word backs −0.7%, 1-word 84→127 ms (regress) |
| T1 AY strobe D3–D7 | retained | `$0D68/$0D7C` 444/427→136/139 µs; AY batch median 21.7→12.6 ms |
| T2 absolute ACRTC forms | retained | 20-site sum 8.183→2.514 ms (estimated 1.5 ms) |
| T3 serial A1/A2 forms | retained | 9 sites 3.313→1.290 s; cold Ready −2.05 s |
| T4 compact input snapshot | retained | 438.6→123.0 µs/call |
| T5 assembly admission + clock | retained | `$2E82`/`$2EB2` 278/325→247/266 µs |
| T5 boundary inline + paired phases | retained | entry/exit/triplet −2.9/−6.2/−4.9% |
| T6 ordered clock batch | retained | clock pause 57.5→45.8 µs; first loop version rejected |
| T6 IRQ-source cache | retained | IRQ query 20.4→11.5 µs; non-directional version rejected |
| T6 dispatch work gating | retained | 170.9→151.8 µs/dispatch (−11.2%); est. −30% not shown |
| T7 calibration chunks | retained | no late A1200 VBI; "unmask bookkeeping only" rejected |
| T7 tick product | retained | `Board::tick` 192.4→156.5 µs |
| T7 tick return | retained | `$0C3E` 589.2→516.9 µs (estimated −0.3 ms/tick unproved) |
| T8a mixed-background card | retained | zero guard refusals in trace |
| T8b rotated copy | retained | copy180 3.67→2.20 ms |
| T8c solid colour planes | retained | tile 571→527 µs |
| T8c small fills | retained | 343→150 µs |
| T8c dense curve stamps | retained | curve 4.88→3.54 ms; cold/warm Ready −0.377/−0.361 s |
| T9 quiet startup batch | retained | cold −1.835 s (estimated 2.5–4 s) |
| T9 short delay return | retained | −19.7…−23.3 ms |
| T10 preparation attribution | done (tooling) | prep 0.70–0.74 s, 51% bytewise memset |
| T10 follow-up wide memset | retained | prep 740–760→420–440 ms |
| T11 ACRTC timing research | research only, production unchanged | 3M rate: startup video IRQs 5,870→3,422 |
| T12 boot-artwork cache | **declined** (2026-09-30) | study: ~450 KB, est. 1–1.8 s saving |
| T13 setup bridge | research removed | regression 113.8→200.0 µs (nonempty) |
| T13 register-resident setup | research removed | 138.4 vs 112.5 µs ordinary |
| T13 deferred metadata | research removed | 129.6 vs 112.5 µs |
| T13 whole-handler reference | proof completed; redundant fixture removed | 63,040 cases, 13,056 IRQ insertions |
| T13 consumer-store/return | retained | 176.4→158.6 µs (−10.1%) |
| T13 joined handler | retained (2026-10-01) | setup 111.4→96.0 µs; paired live cards ~1 ms faster |
| T14 sound-write fusion | retained | 840.0→668.7 µs/register (estimated 200 µs not met) |
| W2 exception/CPU matrix | qualified on earlier builds | moved VBR added 5.8 µs/Line-A; observed program needs no 68060 integer emulation |
| W4 emergency quit | Help retained, Esc saves | no pre-quit callback; no checkpoint saves; physical test used earlier F10 mapping |
| W5 WHDLoad options/CPUs | historical 24 cold/warm pairs passed | differing-hand worst AY gap remains qualified, not a no-regression proof |
| W3 trace-free service | retained (user required) | no aggregate regression; 0.9698 vs 0.9697 Double ratio |
| Service-excluded clock / units-only fix | rejected as production policies | services caused unavoidable slowdown; units-only clock still reset watchdog |
| Plain wall clock | rejected | insufficient guest progress caused watchdog expiry |
| Bounded clock with calibrated K | retained | preserves guest throughput floor and limits stored time credit |
| Legacy producer shuffle waits | superseded | delayed original sound scheduling; consumer-paced markers retained |
| Independent presentation timer | removed | could expose partial commands/cards; original tick completion now requests refresh |
| PAL-time AY envelopes | retained by user decision | audible decay no longer stretched by drawing; late writes remain separate |
| Offline mixed-waveform bank | replaced | about 162 KiB embedded data removed in favor of runtime shared buffers |
| Higher-rate fixed tone/noise loops | retained | five 8 KiB DMA buffers; four fixed gating rates, no per-note synthesis |
| Startup SHA and diagnostic allocations | removed from normal boot | size/patch guards retained; no replay file in normal startup |
| Coin hardware test bypass / acknowledged setup | retained by user decision | original initialization/accounting kept; zero-credit start, no fixed setup script |
| Pattern seam splitting | retained | sampled scalar PTN fallbacks eliminated |
| Curve closure / ellipse initial error | corrected shared renderer | missing 0/Q/6 pixels fixed; no coin-specific artwork patch |
| Read-acknowledged key queues | retained | short presses survive slow guest polling |
| Early calibration unmask-only variant | rejected | bookkeeping window too short; chunked probes retain IRQ windows |
| WHDLoad new-file/cache experiments | installer slots retained | low-memory new-file creation hung; overwriting preloaded files passed |
| Backup/rename save path | removed by user decision | two in-place saves, each one operation; WHDLoad uses resload_SaveFile |
| Compact guest RAM/device windows | retained | 64 KiB each plus RAM canary; full replay requalified |
| HUNK stripping / release-only diagnostics | retained | symbols and research code excluded; separate ELF retained |
| Compiler loop unrolling | disabled explicitly | avoid executable growth; not needed for selected paths |
| Four-byte global-data alignment | retained, audited | initial cold Ready 874 regressed → 793; old baseline 808 |

## Useful tools

- `release-timing.gdb` / `release-double.gdb` with `host/release_timing.py`: normal
  code, Ready/keys/card/AY/end counters and PAL timing.
- `trace.sh` with `host/native_trace.py`: per-instruction cost attribution; large
  local trace files should be reduced and deleted after use.
- `STARTUP_PROFILE=1`, `startup-elapsed.gdb`: preparation/init boundaries.
- `VBI_LATENCY=1`, `startup-vbi.gdb`: post-service scanline distribution; reject
  A1200 samples at line 29 or later for the current qualification.
- `PROFILE_SUPPORT=1`, `DISPATCH_PROFILE=1`, `TIME_LEDGER=1`: explicitly instrumented
  diagnostic builds. Never ship them or use their elapsed totals as release timing.
- `make harness-elf`: linked instruction/CCR/stack/IRQ oracles. Build and freeze the
  exact ELF being qualified; do not overwrite it while checks are running.
