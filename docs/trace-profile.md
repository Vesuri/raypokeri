# Instruction-trace profile and remaining performance plan

2026-09-29, normal code of `c0396c4` (the traced layout differs only as described
below). This is an analysis and proposed plan. The task list remains
[remaining-work.md](remaining-work.md). Nothing here changes a timing contract or
device model, and the decision points below are open.

## Summary

Startup and audio timing are both limited by the per-access cost of servicing
original device instructions. Drawing is the main cost only in a few bursts.

- **Warm start, 9.2 s** (every launch after the first): boot takes 8.1 s for 0.5
  board-seconds of original time. The FIFO-feed episodes take 58%, of which
  HD63484 drawing is 2.6 s (32%). The FIFO-empty interrupt handler's other hooks
  take another 31%. Original code executes for only about 3% of the time.
- **Cold start, 22.5 s** (first launch only): the same boot, plus about 12 s to
  supply the 100 reserve coins. Of those 12 s, 40% is the startup delay-loop hook
  dispatched at 1 ms quanta and 25% is serial-interrupt register access that the
  short paths do not admit.
- **Audio.** Sound is written by the original 100 Hz tick. When services use more
  than about 75% of the CPU for longer than the three-frame credit window, board
  time stalls, so notes are late and stretched. In the normal build the Double
  transition delays sound by about **540 ms**, and each face-up card reveal by
  about **100 ms**. Each AY register write itself costs **1.53 ms**, so writing one
  14–15-register note takes about 22 ms.
- **Largest fixable items.** Several frequent original operand forms still take the
  generic 400–500 µs dispatcher only because the short paths do not admit them:
  - the card-window move callback: 23 sites, **7.1 ms per call**
  - the AY strobe writes from D3
  - the serial ISR's A1/A2-based ACIA accesses
  - the FIFO-empty interrupt re-arm, which is also the next interrupt's delivery point

  Together with the generic dispatcher's own fixed cost, these are the main plan
  items.

Realistic targets are in [Expected outcome](#expected-outcome). Complete elimination
of the Double stretch is not among them on a stock A1200 without one of the
decisions listed at the end.

## Method

**Instrument.** `~/.local/fs-uae/fs-uae` contains the Barto gdb stub. Its `monitor
profile N` records every executed instruction for N PAL fields (at most 100),
with the instruction's cycle cost and its pre-execution registers. DMA contention
and wait states are folded into the instruction that waited. This observer is
outside the Amiga: the traced executable has no timers, counters or scopes,
unlike the TIME_LEDGER build, whose observer inflates the cold start by about 45%.

**Layout.** FS-UAE records PCs only inside the executable's first code hunk and
Kickstart. `TRACE_CODE=1` therefore places the Board storage (the 68008 ROM/RAM
image) in zero-filled storage in `.text`, so original instructions keep their PCs.
Nothing else changes, and normal builds are unaffected. After the TRACE_CODE,
DOUBLE_SCENARIO and ledger builds, the rebuilt normal executable's allocated
sections are byte-identical to the frozen validated normal release
(`tmp/perf/Pokeri-dispatch-outline-normal.elf`).

**Attribution.** `host/trace_reduce.cpp` (a C++ pass, about 1 GB/s) with
`host/native_trace.py` (configuration and report):
- Every cycle is classified as original code, a service entered from original code
  (by handler: Line-A, trace, TRAP, VBI, audio, CIA), a nested Amiga interrupt
  inside a service, or Kickstart. Nesting is rebuilt from vector entry addresses
  and RTEs; an original-code PC resynchronizes it.
- Native call stacks are rebuilt from `jsr`/`bsr` plus SP, giving self and
  inclusive time.
- Service episodes are keyed by the original instruction that entered them, which
  gives each hook site's own cost, excluding nested IRQs.
- Per field, the analyzer counts executions of the ROM's `irq_system_tick` (`$0C06`)
  as 10 ms board ticks. That gives board time in every field without touching the
  guest. It also counts AY writes (`PaulaAy::write`), card-cache begin/hit
  counters, presentations and VBIs.

Every field's traced cycles equal its elapsed emulated cycles (284,204 per PAL
field on the A1200 preset).

**Workloads.**
- Cold and warm startup, captured in consecutive 100-field traces from the first
  original instruction to Ready.
- A keyboard-only gameplay scenario (`DOUBLE_SCENARIO=1`): coin, deal, hold a pair
  (four of a suit, else the highest card), draw, and press D only once the ROM's
  Double-ready byte `$4112F` is set. The ROM does not accept Draw with nothing
  held, so the earlier blind variant stalled after one round.
- Deal, draw and the accepted Double (round 5) were traced: 18 s in total.
- A normal-build run of the same scenario, with read-only breakpoints on AY writes,
  keys and card counters, gives PAL frame and beam times of every AY write. It was
  accepted in round 3.

**Limits.**
- These are FS-UAE A1200 figures (68EC020 at 14.19 MHz, 1 MB Chip, 8 MB Fast), not
  physical-machine timings. CPU-bound costs scale down on faster CPUs.
- Different live hands differ.
- The traced build's times agree with the normal-build observations: cached
  landing backs take 38–40 ms in both, and warm Ready is about 8.9 s from launch
  against 9.22 s measured by TOD.

## Startup

| Phase (A1200) | Wall | Board | What dominates |
|---|---:|---:|---|
| Native preparation | 0.8 s | – | not traced (ROM load, card data, allocation) |
| Boot: first instruction → door (warm and cold) | 8.1 s | 0.50 s | FIFO feed + drawing 58%, FIFO-handler hooks 31% |
| Door, status, collect (cold) | ≈1–2 s | 0.2 s | same service mix |
| Reserve refill, 100 coins (cold) | ≈12 s | 5.3 s | delay-loop hook 40%, serial ISR 25% |
| Close, confirm, Ready (cold) | ≈0.5 s | 0.15 s | – |

**MEASURED boot (warm trace, fields 0–404, 8.1 s):** 1,722 HD63484 commands,
23,877 FIFO words, 4,113 virtual IRQs and 5,350 full C dispatches.

| Cost | Share | Time |
|---|---:|---:|
| FIFO feed episodes (`$2E58`), including drawing | 57.6% | 4.66 s |
| ↳ HD63484 command execution (`Hd63484::execute`) | 31.7% | 2.57 s |
| ↳ ↳ PAINT 1.08 s, pattern tiles 0.62 s, 106 curves at 4.9 ms each 0.52 s, lines 0.46 s, rectangles 0.37 s | | |
| ↳ per-word feed/model overhead (≈88 µs per word) | 25.9% | 2.09 s |
| FIFO-handler address write `$2E82`, 61% full dispatch | 12.9% | 1.04 s |
| FIFO-control re-arm `$2EB2`, always full dispatch | 8.5% | 0.69 s |
| Status/empty/tail hooks `$2E30`, `$2E70`, `$2EBC` | 9.6% | 0.78 s |
| Original code | ≈3% | 0.24 s |

**DERIVED:** the handler's non-feed hooks cost about 1.24 ms per FIFO-empty
interrupt. The ROM feeds only the words present at the interrupt's producer
snapshot and then re-enables WFE. The model's FIFO is always empty, so the next
interrupt arrives at the following boundary. The full dispatch at `$2EB2`/`$2E82`
is where that next interrupt is delivered (`pushException`).

**MEASURED cold refill (fields 500–1099, 12 s, 5.31 board-s):**

| Site | Entries | Full | µs/entry | Share |
|---|---:|---:|---:|---:|
| `$2442` delay loop (startup batch hook) | 14,853 | 13,097 | 325 | 40.2% |
| Serial ISR `$16B4–$1720`: `btst #n,(a1)`, `move.b 1(a1),d2`, `move.b (a2)+,1(a1)` and similar | ≈9,000 | ≈90% | 300–440 | ≈24.5% |
| Meter/credit drawing (FIFO feed and handler) | – | – | – | ≈13.8% |

**DERIVED:** fast-forward advances in 1 ms quanta. Each quantum costs about
2.8 dispatches of about 0.33 ms, about 0.9 ms of native time. The serial ISR's
ACIA accesses use A1/A2 bases, but the short peripheral path admits only A3-based
forms (`Native.cpp`: `port.reg==3`), so every one takes the generic dispatcher.

The ledger recipe reproduces this split with inflation: 31.96 profiled seconds
for the cold start, of which boot→door is 11.16 s and refill→close 18.74 s
(`amiga/.run/pa-ledger`).

## Gameplay and audio timing

**How sound timing works here.**
- The ROM's sound sequencer runs from the 100 Hz board tick and writes 14–15 AY
  registers per sound record through `sound_register_write` (`$0D58`): six hooked
  PIA writes per register.
- Board time follows PAL time, but never faster than 4× the measured guest time,
  with a three-frame credit window.
- A burst in which services leave the original code less than about 25% of the CPU
  stops board ticks, so the next sound write is late. Lateness beyond the window
  is not recovered.
- Paula plays in real time; envelopes are already on PAL time.

**MEASURED normal-build AY-write lateness** (wall minus board time between
successive AY-write batches, `amiga/.run/pa-double-release`):
- Double entry: 263 ms, then 276 ms (846 ms wall for 570 board-ms, with the
  accepted Double inside).
- Win and draw sequences: 180–190 ms.
- Each of the five face-up reveals: 95–110 ms.
- Applying one batch of AY writes (first to last write) takes a median 21.7 ms wall.
- 47 of 143 inter-batch gaps exceed 50 ms of lateness.

**MEASURED steady state (quiet hold screen):** original code runs 67% of the time,
63% of it in its own delay loop. Services take 33%. Headroom is ample; only
bursts matter.

**MEASURED 18 traced gameplay seconds** (deal, draw, Double): board/wall 0.851.
Original code 36% (31.7% delay loop). Services:

| Group | Share | Notes |
|---|---:|---|
| FIFO feed episodes, including drawing | 10.3% | card backs, layout, procedural backs |
| Card-window move callback (`move_card_window_tick`, 23 sites) | 8.1% | **7.1 ms per call**; absolute `$F6000/$F6002` forms, all full dispatch |
| AY register write sequence (`$0D5A–$0D7C`) | 5.3% | **1.53 ms per register**; `$0D68/$0D7C move.b d3,22(a3)` full at ≈435 µs each |
| System-tick RTE (`$0C3E`) | 5.1% | 731 µs when full: presentation, Board tick, input apply |
| FIFO-empty handler non-feed hooks | 4.9% | ≈1.26 ms per interrupt |
| Trace exceptions delivering ticks inside the delay loop | 4.2% | 565 µs each |
| Main-loop, TRAP, callback-list and SR hooks | ≈15% | 50–120 µs each, short paths |

By code: assembly entry/short paths 25% (CIA clock stop/restart/read alone 3.6%),
C scheduler/clock 13.7%, board devices 6%, prepared-hook execution 5.3%, drawing
and blitting about 7%.

**MEASURED burst anatomy (traced):**

| Burst | Wall | Board advance | Composition |
|---|---:|---:|---|
| Double entry | 420 ms at 97% services | 110 ms | 5 cached backs ≈40 ms each; 180 ms layout: 444 words through **128** FIFO interrupts, drawing only 17% |
| Cached landing back | 38–40 ms | ≈0 | 260 words; ≈26 more FIFO-empty interrupts than a 17 ms light back |
| Guard-refused back (2 per deal) | 100–120 ms | ≈0 | procedural: 8 curves at 3.5 ms, PAINT 39 ms, lines 17 ms, rectangles 11 ms |
| Five face-up reveals | 820 ms | 430 ms | full dispatches 38%, AY writes, card-window callbacks, `copy180` 4 ms each |

Fixed per-event costs on the A1200 preset:

| Event | Cost |
|---|---:|
| Full C dispatch, typical | 250–450 µs (`nativeDispatch` self ≈100 µs, `nativeClockPause` 30–75 µs, 2–3 `Board::irq` at ≈17 µs) |
| Short assembly hook | 40–150 µs |
| FIFO-empty interrupt service, excluding words | ≈1.26 ms |
| Per fed command word | ≈40–90 µs |
| AY register write | 1.53 ms |
| `move_card_window_tick` call | 7.1 ms |
| Board tick (`Board::tick`) | 150–190 µs |
| `amigaInputApply` (50 Hz) | ≈380 µs: copies/clears 2×128 volatile event bytes under Disable() |
| 180° rank/suit copy (`copy180`, CPU reversed words) | 3.6–4.1 ms |

## Candidate changes

The actionable items, their order, gates and status are **T1–T14 in
[remaining-work.md](remaining-work.md)**; T11–T14 follow the decisions below. This table keeps the measured basis for
each item. Estimates are sums of measured removable cost at a target per-event
cost; the gains are not additive with certainty.

| Item | Measured now | Change | Estimate |
|---|---|---|---|
| T1 | `$0D68`/`$0D7C` `move.b d3,22(a3)` full, ≈435 µs each; 1.53 ms per AY register | admit D3–D7 peripheral write sources (today D0–D2) | −0.55 ms/register, −8 ms/note, −3% gameplay |
| T2 | 23 absolute `$F6000/$F6002` sites in `move_card_window_tick`, all full; 7.1 ms per call, 8% of gameplay | short absolute-address ACRTC forms; reads via the unchanged shared endpoint | ≈1.5 ms per call, −6% gameplay |
| T3 | serial ISR `$16B4–$1720` A1/A2 forms full, 300–440 µs; 25% of refill | admit A1/A2 ACIA bases | −2.3 s cold |
| T4 | `amigaInputApply` 380 µs at 50 Hz | latch/clear only the consumed keys (15 event keys; Escape is a level) | −1.5% gameplay |
| T5 | FIFO-empty re-arm `$2EB2`/`$2E82` full ≈380 µs; 1.26 ms handler overhead per interrupt | assembly completion of admitted promotions; then trim `$2E30`/`$2E70`/`$2EBC` | −0.25 then −0.2 ms per interrupt: −0.5 s boot, −35 ms Double, −7 ms per landing |
| T6 | full dispatch 250–450 µs (`nativeDispatch` self ≈100 µs, `nativeClockPause` 30–75 µs, repeated `Board::irq`) | one grant per pause, cached IRQ level, one pending-work gate | −30%: −5% gameplay, −0.6 s boot, −1.5 s refill |
| T7 | tick RTE 731 µs; `Board::tick` 150–190 µs; trace tick delivery 565 µs (4.2%) | cheaper tick model and delivery | −0.3 ms per tick, −3% steady |
| T8 | two guard-refused backs per deal ≈110 ms; `copy180` 3.6–4.1 ms; boot curves 4.9 ms, PAINT 1.08 s, tiles 0.62 s | proved admission case; table reversal; primitive work | −130–160 ms/deal, −40–60 ms/reveal set, −0.8 s boot |
| T9 | `$2442` startup delay hook 14,853 entries, 40% of refill | assembly batch path; advance to next due edge while serial idle (watchdog deadlines included) | −2.5 to −4 s cold |
| T10 | 0.8 s native preparation not traced | trace from `nativePrepareInner` | – |

### Expected outcome

These are estimates, sums of measured removable costs at target per-event costs.
They must be re-measured with the same tooling. FS-UAE A1200:

| Metric | Now | After T1–T9 | Notes |
|---|---:|---:|---|
| Warm Ready | 9.2 s | ≈6.5 s | ≈4.5 s if T12 leads to implementation |
| Cold Ready | 22.5 s | ≈13 s | 100 reserve coins kept (decision B) |
| Cached landing back | 38–40 ms | ≈25 ms | ≈20 ms with T13 |
| Face-up reveal (wall per card) | ≈150 ms | ≈80 ms | |
| Double entry busy interval | 420 ms | ≈250 ms | |
| Audible Double stretch | ≈540 ms | ≈250–300 ms | not eliminated |
| Deal board/wall (traced) | 0.85 | ≈0.93 | |

**DERIVED floor:**
- A cached back still delivers 260 hooked words and about 26 interrupts. At an
  optimistic 40 µs per word and 120 µs per interrupt that is about 14 ms before the
  blit and presentation.
- The Double entry's 1,346 hook entries and 133 interrupts cost at least about
  100 ms, well past the 60 ms credit window.

Some drawing-related sound stretch therefore remains on a stock 14 MHz 020 even
with T13 and T14. Only an ACRTC timing model could remove it, and T11 researches
that without adopting it.

## Decisions (2026-09-29)

The user decided: A — research first (T11), no model change; B — keep 100
reserve coins; C — a design study only (T12), with implementation after a
separate go/no-go; D — wider fusion authorized for the FIFO-empty handler (T13)
and `sound_register_write` (T14) only. The background at decision time follows.

- **A. ACRTC drawing/FIFO timing model.** On the board the ACRTC draws
  asynchronously and the 100 Hz tick keeps time. The model's always-empty FIFO
  makes every WFE interrupt immediate and draws in zero board time. Modelling
  command duration would restore both behaviors, but it changes device timing
  and needs hardware evidence (see [rom-set.md](rom-set.md) FIFO questions).
- **B. Fewer automatic reserve coins.** The refill scales at about 0.12 s per
  coin. It affects only the first launch, since saved accounting makes later
  launches warm.
- **C. Build-time boot artwork.** Exact command-stream recognition, like the
  card-back cache, would replace about 2.6 s of drawing and part of the feed
  cost, while writing authoritative VRAM.
- **D. Wider fused sequences.** Execute the original FIFO handler or the
  command-feed loop as one guarded assembly block with every original boundary
  preserved. This extends the reserve option in native-performance-plan.md
  beyond the approved bounded sequences.

## Reproduction

```
cd amiga
./trace.sh startup trace-cold                  # ≈11 × 1 GB captures
./trace.sh startup trace-warm .run/<saved-drive>/dh1
./trace.sh play trace-play                     # deal, draw, accepted Double
python3 ../host/native_trace.py --run .run/trace-play --prefix double \
    --fields 2 22 --tree 1.0 --timeline        # one window, call tree, per field
```

`--fields` selects a window of the concatenated captures. The report prints:
- the category split
- subsystem and function self time
- inclusive call counts
- a call tree merged per callee
- original-code routines by `disasm/symbols.csv`
- hook sites with their own and full-dispatch cost
- a per-field timeline of guest time, services, ticks, AY writes, card hits and
  presentations

Normal-code AY lateness uses a `DOUBLE_SCENARIO=1` build (no `TRACE_CODE`) with
`GDBSCRIPT=release-double.gdb`, `native-test-inputs` and `POKERI_REPLAY=0`. Then
run `python3 host/release_timing.py --scenario double LOG`, which reports the
batch application span and batch-to-batch lateness. Use `diag_run.sh` with a
generous delay, because every AY write is a breakpoint. For cold/warm Ready
elapsed time, keep using `STARTUP_PROFILE=1` (native-profile-build.md): trace
time starts at the first original instruction and excludes preparation.

## Evidence

Local, ignored:
- `amiga/.run/pa-startup-{cold,warm}`, `pa-play-trace2` (the traced runs and
  reductions)
- `pa-play-trace` (the first deal trace; its draw capture is the stalled no-hold
  round)
- `pa-double-release`, `pa-ledger`
- reports in `tmp/perf-analysis/*.txt`
- scenario/diff backups in `tmp/perf-analysis/`


## T1: AY strobe register admission (2026-09-29)

**MEASURED:** extending peripheral byte writes from D0–D2 to D0–D7
admits the two D3 strobe sites without changing the shared PIA endpoint,
read forms, clock contract or interrupt-promotion rules. The linked assembly
matrix checks 188,416 peripheral cases, including the exact endpoint byte,
all CCR/byte values and preservation of D2–D7 across C ABI clobbers.

The cycle-exact gameplay trace compares `pa-play-trace2` with `t1-ay-play`:

| AY site | Calls in each capture | Before, µs/call | T1, µs/call | Full dispatches before → T1 |
|---|---:|---:|---:|---:|
| `$0D68` | 630 | 443.87 | 135.98 | 630 → 2 |
| `$0D7C` | 630 | 426.94 | 138.78 | 630 → 7 |

The two sites save 596 µs per register update in these captures. Occasional
promotion remains possible; the shared endpoint and original instruction
boundaries are retained. Hands and interrupt phases differ, so this is a
per-site measurement, not a controlled total-session speedup. The candidate
trace accepted Double in round 1 and exited with status 4, error/reset zero.

**MEASURED normal-code Double:** accepted in round 3, 36 key transitions,
status 4, no error/reset and vectors restored. Median AY batch application is
12.6 ms (baseline 21.7 ms). The two largest consecutive-write excesses across
Double entry are 323.3/358.2 ms; this hand does not demonstrate an improvement
over the baseline 262.9/276.4 ms gaps. Ready-to-finish is 71.34 board seconds /
76.48 PAL seconds (0.9328). Faster register writes do not close the graphics
stall or audio-lateness targets. Normal cold/warm A1200 live24 also pass,
with batch medians 12.4/12.2 ms and ratios 0.9580/0.9633.

The restored default executable's allocated sections match the frozen T1
normal binary. Headless suites and short/feed oracles pass. Cold/warm ECS live24 also pass (ratios 0.2570/0.2605: compatibility, not
real-time acceptance). ECS and AGA replay both match all 262,144 RAM bytes,
524,288 VRAM bytes, 172,064 displayed pixels and 60 AY writes at 7,904,133
instructions / 64,000,000 cycles / 8,685 IRQs. T1 is complete.
The current replay fixture requires `native_check.py --live-boot
--skip-hardware-tests --auto-setup`; the older fixed `fast-setup-replay.inputs`
from the historical handoff does not describe this fixture.
Local evidence: `.run/t1-ay-play`, `.run/t1-ay-double`, `tmp/t1-ay-*-report.txt`.


## T2: absolute video-register accesses (2026-09-29)

**MEASURED:** admit checked absolute-long ACRTC byte
reads into D0–D7, byte/word writes from D0–D7 and immediate byte/word writes.
The descriptor checks the relocated extension before accessing the unchanged
shared read/write endpoint, and uses the existing per-instruction promotion
rule. No callback fusion or RD/FIFO model change is involved.

The linked independent-instruction matrix passes 432,640 cases on 68000/68020:
all registers and CCR values, byte and word boundaries, C ABI clobbers,
unchanged registers and rejection of changed absolute addresses. Existing
short/feed matrices and headless suites also pass.

**MEASURED:** `t1-ay-play` → `t2-absolute-play` gives 2,680/2,680 → 18/3,164
full dispatches at `$1E4E4–$1E57C`. Typical byte-site means drop from 380–500 µs
to 110–155 µs; the word-write site `$1E502` drops from 447.9 to 173.2 µs.
Summing each of the 20 sites' mean once gives 8.183 → 2.514 ms. This is an
explicit per-site comparison: the helper is also invoked independently, so
summing all recorded costs and dividing by callback entries would inflate the
callback result. It is not a worst-case deadline bound or the proposed 1.5 ms
estimate. The candidate trace accepted Double in round 3 and exited cleanly.

The first cold A1200 live24 passes, with deal/draw ratios 0.9629/0.9616
(T1 0.9282/0.9436). Hands and interrupt phases can differ. Cold/warm A1200
session ratios are 0.9657/0.9714; ECS 0.2609/0.2644. All four runs finish 24
inputs with no error/reset and restored vectors.

**MEASURED normal-code Double:** accepted in round 10, 88 key transitions,
clean exit. Ready-to-finish 226.10 board / 234.76 PAL seconds (0.9631).
Median AY batch application is 12.7 ms. The largest consecutive-write excess is
267.3 ms (T1 358.2 ms); this is a different hand and is not a controlled
worst-case latency improvement. The audio/card deadlines remain open.

ECS/AGA replay both match all RAM, VRAM, 172,064 pixels and 60 AY writes at
7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs. The restored default
build matches the frozen candidate's allocated sections. T2 is complete.
Evidence: `.run/t2-absolute-{play,double,aga,ecs,live-aga,live-ecs}` and
`tmp/t2-absolute-*-report.txt`.


## T3: serial interrupt operands (2026-09-29)

**MEASURED:** the serial routines need A1 port accesses,
`BTST #n,(A1)` and `MOVE.B (A2)+,d16(A1)`, not merely another MOVE base register.
The new guard checks the device address and the source byte's owned ROM/RAM
range before reading or changing A2. Byte sources may be odd or the final owned
byte; wrapping and out-of-range sources decline unchanged. Original bit tests
change only Z. All forms retain shared peripheral endpoints and the existing
per-instruction promotion rule. The existing A3 path gains no extra branches.

The independent 68000/68020 oracle passes 524,308 serial cases, covering all
byte/CCR values, source/port bounds, postincrement, bit tests, saved registers and
C ABI clobbers. Existing peripheral/video matrices also pass.

**MEASURED:** cold-start trace `pa-startup-cold` → `t3-serial-startup`:

| Site | Before µs | T3 µs | T3 full dispatches / calls |
|---|---:|---:|---:|
| `$16B4` | 338.13 | 133.21 | 5 / 630 |
| `$16CE` | 439.65 | 145.30 | 5 / 630 |
| `$16DA` | 362.98 | 170.84 | 110 / 630 |
| `$16EA` | 336.72 | 136.96 | 54 / 2,007 |
| `$16F0` | 334.37 | 131.91 | 18 / 2,007 |
| `$16F6` | 333.14 | 129.63 | 2 / 741 |
| `$170E` | 435.96 | 142.33 | 4 / 741 |
| `$1718` | 356.51 | 135.54 | 3 / 631 |
| `$1720` | 368.61 | 141.91 | 9 / 1,267 |

These sites total 3.313 → 1.290 seconds (9,243 → 9,284 calls). In the normal
cold live24 run, Ready is at frame 967/line 120 versus T2 frame 1069/line 283:
2.050 seconds earlier from the same VBI-counter origin, excluding earlier
loading/preparation. This agrees with the per-site saving but is not a paired
physical-hardware startup bound.

Normal cold/warm A1200 live24 pass (ratios 0.9702/0.9650). Accepted-Double
passes in round 1 with 12 key transitions, clean exit and ratio 0.9511; median
AY batch 12.3 ms, largest consecutive-write excess 352.2 ms. Different hands
prevent treating that as a controlled comparison with T2's 267.3 ms; T3 fixes
serial access cost, not the graphics-induced sound-write deadline. The normal
build's allocated sections match the frozen candidate after measurement builds.
Headless and linked short/feed matrices pass. Cold/warm ECS live24 pass,
ratios 0.2625/0.2343 (compatibility, not real-time acceptance). Full ECS/AGA
replay matches all 262,144 RAM bytes, 524,288 VRAM bytes, 172,064 pixels and
60 AY writes at 7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs.
T3 is complete. Evidence: `.run/t3-serial-{startup,double,aga,ecs,live-aga,live-ecs}`,
`tmp/t3-serial-*-report.txt`.


## T10 — preparation attribution (2026-09-29)

**Completed measurement/tooling scope.** `./trace.sh prepare NAME` starts the
instruction trace at `nativePrepareInner`, before file loading, allocations and
model construction. It records at most five 100-field blocks, stopping after
vector installation has occurred. The analyzer marks `nativeInstallVectors`
(`prepared`); its containing field gives a 20 ms interval for the preparation
boundary. The final block can contain original execution and must not all be
reported as preparation. Select fields ending before that marker for attribution.
This mode adds no target counters, timers, or production behavior.

**MEASURED, A1200:** `t10-prepare` / `t10-prepare-entry` reach vector installation
in fields 35/36, respectively: approximately 0.70–0.74 s from capture start.
The latter's fields 0–35 contain no original instructions, Line-A dispatches,
board ticks or AY writes. Of their 720 ms:

| Self time | Share | Approximate time |
|---|---:|---:|
| `memset` | 51.18% | 368.5 ms |
| Kickstart | 35.40% | 254.9 ms |
| `nativePrepareInner` | 4.84% | 34.8 ms |
| `CardBackCache::installPrepared` | 2.31% | 16.6 ms |
| `PaulaAy::prepare` | 2.07% | 14.9 ms |
| `prepareHook` | 1.79% | 12.9 ms |
| `shortDescriptor` | 0.76% | 5.5 ms |

Inclusive constructor time overlaps these rows and must not be added to them:
`Board::Board` costs about 133 ms, including its required RAM initialization.
The shared toolchain support `memset` is a byte-store/compare/branch loop (**DERIVED**
from compiled library code). A wide, aligned fill is therefore a concrete next
candidate; this trace does not justify removing required memory initialization.
Kickstart time still combines allocation, file I/O and other OS calls; it has
not been attributed to individual OS operations.

Reproduction:
```
cd amiga
./trace.sh prepare preparation
python3 ../host/native_trace.py --run .run/preparation --prefix prepare --timeline
# Choose the field range before ev_prepared in reduced-prepare/fields.tsv.
python3 ../host/native_trace.py --run .run/preparation --prefix prepare --fields 0 35 --top 50
```
The example field range is specific to the measured run; inspect the event in a
new run. Local evidence: `.run/t10-prepare{,-entry}` and
`/tmp/pokeri-t10-prepare-entry-only.log`. The launcher restores the normal build;
this measurement used the committed clock batching with IRQ caching disabled.


### Preparation follow-up: exact wide fill (default, 2026-09-30)

`FAST_MEMSET=1` wraps external `memset` calls with local 68000 assembly. It
handles byte/word alignment heads, aligned longword blocks and a byte tail;
it does not skip any initialization. The shared toolchain support source is
unchanged. The validated option is enabled by default; `FAST_MEMSET=0`
retains the comparison implementation.

**MEASURED:** paired A1200 preparation traces reach vector installation at
740–760 ms without the wrapper and 420–440 ms with it, a 300–340 ms saving.
In preparation-only windows, fill self-time falls from about 369 to 73 ms.
These numbers concern preparation, not the original game's whole startup.
Both captures use IRQ caching and leave dispatcher work gating disabled.

`make harness-memset-check` (after sourcing `amiga/env.sh`) executes 49,586
synthetic cases on 68000 and 68020, checking every byte store, exact bounds,
alignment, values, zero-length calls, lengths through 512 KB, return value and
callee-saved registers. It passes. `host/native_memset_check.py --elf ...`
also proves the measured executable contains the same tested assembly and
no remaining bytewise `memset` symbol. The combined `FAST_MEMSET=1` /
`DISPATCH_WORK=1` candidate passes exact ECS/AGA replay: 262,144 RAM bytes,
524,288 VRAM bytes, 172,064 pixels and 60 AY writes, at 7,904,133 instructions /
64,000,000 cycles / 8,685 IRQs. Cold/warm live24 on both machines finish with
24 inputs, zero errors/resets and restored vectors. A1200 board/PAL ratios
are 0.9737/0.9785; ECS ratios are 0.2772/0.2799.

The normal-code Double scenario accepts Double and finishes cleanly; AY batch
median is 11.6 ms and maximum excess batch delay 255.9 ms. This differing-hand
observation is not an audio speedup claim. The gameplay trace completes nine
captures. The VBI probe's only late samples are at lines 41/48 during startup/
display calibration, both with calibration active; that remains T7 work.
The restored default build is checked against the frozen validated candidate.

Local evidence: `.run/t10-memset-{before,after}`,
`/tmp/pokeri-t10-memset-{before,after}-only.log`, and
`/tmp/pokeri-t10-memset-cpu2.log`.


## T7 tick arithmetic (default, 2026-09-30)

`TICK_PRODUCT=1` evaluates the board's system/input phase products and serial
millisecond product with native 16-bit multiplies. It preserves the complete
32-by-32-to-64 result, including full-width rates and carries; phase addition,
threshold loops, edge order and IRQ semantics are unchanged. Host/reference
builds retain ordinary wide arithmetic.

**MEASURED:** against the T6 work-gating trace, `Board::tick` falls from
192.4 to 156.5 microseconds per call (1,710/1,719 calls). General `__muldi3`
calls fall from 3,941 to 493; their inclusive share falls from 0.57% to 0.03%.
The full tick-handler RTE mean falls from 607.7 to 587.3 microseconds.
The serial mean is 53.8/56.8 microseconds, so this trace does not demonstrate
a separate serial saving. The hands differ: these are per-call observations,
not a controlled whole-session speedup or the complete T7 target.

`make harness-product-check` runs the cross-compiled helper on independent
68000/68020 interpreters against host 64-bit multiplication: 200,450 edge and
random cases pass, including full-width rates, carries, ABI and stack bounds.
The Amiga audit passes. Exact ECS/AGA replay matches every RAM/VRAM byte,
pixel and AY write at the standard 7,904,133-instruction fixture. All four
cold/warm live24 cases finish with zero error/reset and restored vectors;
A1200 board/PAL ratios are 0.9747/0.9802, ECS 0.2788/0.2828. Normal-code
Double is accepted in round three (38 inputs), with 11.5 ms median AY batch
span and 240.1 ms maximum excess batch delay. These differing-hand timings
do not establish an audio speedup. The combined bounded-calibration A1200
probe also has no late VBI samples (startup/play maximum 7/10).
The exact tick-product option is now default; `TICK_PRODUCT=0` remains the
comparison path. T7's RTE and bounded-calibration work are separate.
Local evidence: `tmp/t7-product-play`, `.run/t7-product-play`,
`/tmp/pokeri-t7-product-{cpu,play-report}.log`; baseline `.run/t6-work-play`.

### T7 tick-return attribution (2026-09-30)

`host/native_trace.py --run amiga/.run/t7-product-play --site 0xc3e`
attributes native calls only to outer Line-A service at that original PC;
nested Amiga interrupts and other guest sites are excluded. Existing global
tables are unchanged. A synthetic trace checks exact own/inclusive costs, call
counts and both exclusions; the 900-field real capture's selected total also
matches the pre-existing site total exactly.

**MEASURED:** tick-return services consume 808.4 ms across this capture. Their
1,340 full dispatcher calls average 515.0 microseconds inside C, plus entry/
exit cost. The 1,229 generic instruction-executor calls average 90.7 microseconds
(including nominal guest-cycle charging); clock pause averages 69.2. Required
presentation averages 170.5 microseconds when called (749 calls); 525 Board
ticks average 156.7 microseconds. Inclusive figures overlap and must not be
added as independent costs.

The outer tick RTE deliberately refuses the short path because it must release
the presentation request and enter the scheduler. A useful next experiment is
a dedicated verified RTE endpoint that completes its register/stack effects and
then immediately promotes to the same scheduler. Simply letting it return to
the guest would lose required work. The measured executor cost bounds the
possible saving; it cannot establish the entire 0.3 ms T7 estimate by itself.
Local report: `/tmp/pokeri-t7-tick-site-report.log`; synthetic check:
`python3 host/native_trace_test.py`.

### Dedicated outer tick RTE (2026-09-30, default)

`TICK_RETURN=1` admits only the verified `$0C3E` RTE to a dedicated short
endpoint. The shared privilege, trace and stack guards still apply. It pops
the original frame, restores virtual SR/stack state, releases the tracked
outer tick and requests composition, then immediately promotes as an already
completed instruction to the existing dispatcher. It does not resume guest
execution before scheduling, skip an IRQ boundary or add a presentation clock.
Nested/unrelated returns retain the ordinary endpoint; diagnostic replay does
not install the new descriptor. `TICK_RETURN=1` is now the default; zero retains
the comparison path.

**MEASURED:** compared with the combined tick-product/chunked-calibration
trace, the full `$0C3E` service mean falls 589.2 → 516.9 microseconds
(1,337/1,316 calls), about 12.3%. Overall observed dispatch mean is
399.6 → 365.8 microseconds. The generic executor disappears from the selected
tick-return attribution; presentation, board ticking and IRQ scheduling remain.
Different hands/Double rounds prevent a controlled whole-session speedup claim.
The candidate trace accepts Double in round seven and exits without error/reset.

The linked CPU matrix passes 262,144 dedicated outer-return cases across
every saved SR, 68000/68020 and both stack-switch settings, checking registers,
CCR, frame/stack state, exact composition request and immediate promotion.
Existing short forms pass too, including tracked-frame rejection through the
generic endpoint. FIFO tests pass 502,272 fused and 3,755,520 whole-feed cases;
handler exit passes 2,752,512 cases preserving every original event boundary.
The larger linked fixture region needs a 4 KB extraction buffer; the independent
synthetic guest data remains separate.

**MEASURED release gates:** exact ECS and AGA replay matches all 262,144 RAM
bytes, 524,288 VRAM bytes, 172,064 cropped pixels and 60 AY writes at 7,904,133
instructions / 64,000,000 cycles / 8,685 IRQs. Cold/warm live24 passes on both
machines with clean cleanup, no error and no watchdog reset. Observed A1200
ratios are 0.9311 cold / 0.9806 warm; the cold hand accepts Double and is not
comparable with the previous different hand. ECS ratios are 0.2816 / 0.2853.
The separate normal Double scenario accepts in round 12, with an 11.5 ms median
AY batch and 241.3 ms maximum excess batch delay. The A1200 VBI probe completes
24 inputs with no late samples (maximum startup/play scanline 6/10).
These pass correctness and scoped regression gates, not the overall card/audio
or steady-time targets. The proposed 0.3 ms per-tick saving is not demonstrated.
Headless suites pass. The default build matches every allocated ELF section
(address, size and initialized bytes) of frozen `tmp/t7-return-candidate`.

Evidence: `tmp/t7-return-{candidate,play}`, `.run/t7-return-play`,
`/tmp/pokeri-t7-return-{cpu2,boundary-check,play-report}.log`; comparison
`.run/t7-combined-play`.

## T8 rotated-copy geometry (2026-09-30, default)

The renderer already has a compile-time 256-entry bit-reversal table.
`COPY180_WORD_PLANES=1` instead moves row/word geometry outside the plane
loop: source/destination mapping, masks and shifts are computed once for all
four planes. Each plane retains the same reversal and ROP, and overlapping
pixel rectangles retain the scalar fallback. No asset bytes or clock rules
change.

**MEASURED gameplay trace:** `PlanarSurface::copy180` mean is 3,668.4 →
2,201.0 microseconds (38/19 calls), about 40% lower. Including the surface's
blitter synchronization, the means are 4,004.8 → 3,255.4 microseconds. The
hands and call counts differ, so do not multiply this into a claimed controlled
whole-session saving. The candidate accepts Double and exits cleanly.

`make harness-word180-check` checks both separate and interleaved planes
against the independent per-pixel oracle: all source/destination alignments,
ROPs, partial words, nonaligned strides, native 608-pixel rows, disjoint pixel
rectangles sharing a storage word, and unchanged overlap/bounds refusals.
Both layouts pass. Combined release gates below pass; this is now the default. Evidence:
`.run/t8-word180-play`, `tmp/t8-word180-play`,
`/tmp/pokeri-t8-word180-{play-report,host2,interleaved}.log`; comparison
`.run/t7-combined-play`.

### Background-refused cards: concrete admission candidate

**MEASURED from the eleven saved preparation backgrounds:** the two rejected
cases have 17 ordinary black guard pixels in each left corner and 17 white
guard pixels in each right corner. The current all-ordinary/all-white test
necessarily rejects this mixture. The archived samples identify a candidate;
current live runs must still demonstrate the same case and its savings.

A local shared-renderer proof checks the left-eligible/right-white predicate
with fourteen eligible left colours, 128 randomized backgrounds and those two
recorded cases. All 144 match the canonical cached bitmap over their original
background in all 8,800 pixels and the separately rendered mixed case's final
parameters. A follow-up repeats all 144 cases with both scalar and rectangle
semantics (288 checks): every command's complete parameter array and diagnostic
work count matches the separately rendered mixed-background reference, and
both the white-card prefix and final image match canonical masked composition
over the original background. The two captured backgrounds pass both routes.
Prepared per-command progress, guarded admission and full model/CPU/replay/live
validation are still required before adoption.
Evidence: `tmp/t8-right-white-proof.cpp` and
`/tmp/pokeri-t8-right-white-proof.log` and
`/tmp/pokeri-t8-right-white-stages-proof.log`; no cache admission has changed.


### Mixed-background cache implementation (2026-09-30, default)

`CARD_RIGHT_WHITE=1` admits only the separately proved left-eligible/right-white
case. The existing all-ordinary and all-white paths remain. Each of the 68 guards
stores its side classification at preparation; runtime admission does no new
coordinate division. A third 79-command progress table preserves the different
corner PAINT positions and work counts. Prepared descriptor version 3 checks
that table and the side metadata before writing storage. The existing bitmap
and mask are reused; there is no second artwork allocation.

**MEASURED tests:** 5,369 differential cases pass, including every observation
cut for both planar layouts with scalar/rectangle semantics, the complete card
and white prefix, and mutation of each of the 68 mixed guards back into the
rejected class. Disabling the experiment preserves the old fallback. Seventeen
malformed prepared descriptors fail before changing output storage. Borrowed
FIFO tests pass 221,185 granted completions, 2,088 batch cuts / 237,120 words,
and 510 partial re-admissions / 100,440 continued words.

The opt-in Amiga build passes the arithmetic audit. A 900-field gameplay trace
completes an accepted Double in round one without errors or watchdog resets.
It also includes `COPY180_WORD_PLANES=1` and the now-default tick return, so it
is not an isolated whole-session comparison. After repairing the moved trace
probes, the saved capture has 38 starts, 25 complete hits and zero background
guard refusals. Starts also include white-prefix sequences: their difference
is not a count of failed cards. This is scoped live coverage, not a controlled
per-card gain. The combined release gates below pass; this admission is now
enabled by default, with broader timing deadlines retained.

Local evidence: `tmp/t8-card-{play,candidate}`, `.run/t8-card-play`,
`/tmp/pokeri-t8-card-{tests,negative,play-report}.log`. The normal build enables this admission; `CARD_RIGHT_WHITE=0` retains the
comparison path.


### Counter probe maintenance (2026-09-30)

The extra cache metadata moved the compiled start increment four bytes. Both
normal-code timing scripts correctly refused to use their old address, so those
attempts supply no gameplay evidence. `host/release_probe.py` now resolves their
markers before launch: the exact frozen ELF supplies DWARF member offsets;
disassembly proves the preserved first-argument register and uniquely identifies
the corresponding field increments. The generated GDB script still verifies the
complete instructions in loaded memory before setting read-only breakpoints.
No game instructions or profiling counters were added.

The instruction-trace analyzer uses the same discovery and optionally counts
background guard refusals. Unsupported compiler shapes omit that optional count
with an explicit note, rather than reporting a guessed zero. Synthetic tests
cover changed offsets/registers, frame-pointer/stack prologues, duplicate or
missing increments, wrong argument loads and pointer reassignment. The old
frozen tick-return executable resolves to its original offsets; the new cache
build resolves the moved start and changed field offsets correctly. Both normal
timing workloads were restarted only after their original handles had terminated.


### T8c current startup attribution and colour experiment (2026-09-30)

**MEASURED:** five 100-field captures from original execution through warm Ready
(and the remainder of the last capture) total ten PAL seconds. The frozen
mixed-card/word-copy candidate, using copied saved accounting, spends 1.151 s
inclusive in PAINT, 0.537 s in curves and 0.236 s in pattern expansion. The
PAINT symbol also includes its compiler-generated local call entries; its 5,768
trace calls must not be reported as 5,768 guest PAINT commands. The final
capture extends past Ready; ten seconds is not a precise Ready measurement.
Evidence: `.run/t8-card-warm`, `/tmp/pokeri-t8-card-warm-report.log`.

Opt-in `SOLID_COLOR_PLANES=1` constructs uniform-nibble colour plane masks
directly before falling back to the original general conversion. PAINT and
pattern tiles share the helper; with the flag off they retain the original
code. There is no new bitmap, lookup table or drawing approximation.
`make harness-solid-color-check` checks every 16-bit colour word and output
bounds against a pixel-bit oracle, plus 55,552 tiles including every uniform
colour pair and alignment. All pass, and the native arithmetic audit passes.
**MEASURED matched-accounting capture:** pattern-expansion mean is
571.1 → 526.5 microseconds (413/414 calls, about 7.8% lower); inclusive PAINT
is 1.151 → 1.140 s. Both captures extend to the end of the first 100-field
block that reaches Ready, so their different post-Ready work prevents a precise
startup-time saving claim. This remains opt-in; controlled attribution and full
release gates remain if retained.

**Combined card/copy release-gate progress:** headless suites and exact AGA
replay pass. Normal Double accepts in round six (60 key transitions), exits
cleanly, has an 11.6 ms median AY batch and 222.4 ms largest excess batch delay.
Its overall board/PAL ratio is 0.9758; differing hands prevent comparison with
previous sessions as a controlled speedup. The VBI probe passes 24 inputs with
no error/reset and no late samples (startup/play maxima 6/10). Exact ECS replay also passes all 262,144 RAM bytes, 524,288 VRAM bytes,
172,064 pixels and 60 AY writes at 7,904,133 instructions / 64,000,000 cycles /
8,685 IRQs. Its launcher encountered a shell read-offset error after its script
was edited while running; GDB had completed, restored vectors and written all
dumps. The recorded emulator was cleaned up and the independent comparators
passed those dumps. This was not a replay failure or a restarted guest run.

Cold/warm live24 passes on both machines with no error/reset and restored
vectors. A1200 ratios are 0.9762/0.9376, ECS 0.2838/0.2870. Different hands
and doubling activity prevent a controlled speedup comparison. These results
do not close the complete-card, sound-write or sustained-time deadlines.
The validated pair is enabled by default; `SOLID_COLOR_PLANES` stays opt-in.

The default rebuild matches every allocated ELF section of frozen
`tmp/t8-card-candidate` (addresses, sizes and initialized bytes). The opt-in
colour experiment compiles away completely from this release build.


### T8c short fills and bounded curve stamps (2026-09-30, default)

`SMALL_FILL_WORD_PLANES=1` computes each short fill/span's word address and mask
once before visiting its four planes. Rectangle limits, nibble phase, ROPs,
row-crossing mapping and refusal conditions are unchanged. It allocates no
extra storage. `make harness-small-fill-check` passes the independent pixel
oracle in both layouts, now including native 608-pixel pitch and interleaved row
crossings, with all tested alignments, logical operations and untouched bits.

**MEASURED:** against the colour-only experiment with the same saved accounting,
`PlanarSurface::smallFill4` mean is 343.3 → 149.5 microseconds (659/668 calls),
about 56% lower. PAINT's inclusive captured cost is 1.140 → 1.032 s. The faster
run reaches Ready in four rather than five 100-field blocks; that coarse capture
boundary is not a two-second startup saving. Different final-block work prevents
a precise Ready-time comparison. Native arithmetic audit passes. Evidence:
`.run/t8-fill-warm`, `/tmp/pokeri-t8-fill-warm-report.log`.

`DENSE_CURVE_STAMPS=1` uses at most 256 scratch words (512 stack bytes) to combine
small cached outlines directly into masks ordered by logical row/word. Larger
bounding boxes retain the original sort. It changes neither ellipse arithmetic,
point traversal, logical-row alias ordering nor drawing-work counts. The packed
renderer remains the oracle. `make harness-dense-curve-check` passes both plane
layouts, every tested alignment/ROP/COL, wrap and aliased row pitch, including
new radius-24/27 cases around the dense/fallback boundary.

**MEASURED:** adding dense stamps to the fill/colour candidate reduces observed
stamp mean 2,128.4 → 828.3 microseconds and whole curve mean 4,883.3 → 3,538.5
microseconds (104/106 calls). Both startup captures use the same retained
accounting and extend to a 100-field boundary after Ready; these are local
service measurements, not an exact startup-time or whole-game speedup claim.
Evidence: `.run/t8-dense-warm`, `/tmp/pokeri-t8-dense-warm-report.log`.

The three startup flags together pass packed/planar drawing comparisons in both
layouts and the native arithmetic audit. Frozen `tmp/t8-startup-candidate`,
`tmp/t8-startup-double` and `tmp/t8-startup-vbi` supply full release validation;
all gates pass. Exact ECS/AGA replay matches 262,144 RAM bytes, 524,288 VRAM
bytes, 172,064 pixels and 60 AY writes at 7,904,133 instructions / 64,000,000
cycles / 8,685 IRQs. Cold/warm live24 is clean on both machines (A1200 ratios
0.9755/0.9388; ECS 0.2840/0.2870). Double reaches round 7 / 66 keys, median
AY batch 11.5 ms and maximum excess 195.7 ms. VBI startup/play maxima are
6/10 with no late sample. Full headless suites pass. All three flags are now
default; every allocated ELF section matches the tested candidate, including
addresses, sizes and initialized bytes. The T9 one-quantum overload preserves
the exact old default code instead of introducing an unnecessary multiply.

**MEASURED:** normal cold Ready moves from frame 917 / beam 289 to 899 / 21
(18.358496 → 17.981344 s), warm from 406 / 93 to 388 / 72
(8.125952 → 7.764608 s), using `frame*20 ms + beam*64 µs`. These exclude
loading/preparation before the shared VBI origin. Ready board cycles remain
47,040,000 cold / 4,640,000 warm. The 0.377/0.361 s improvements do not prove
the original 0.8 s estimate or close startup/gameplay/audio deadlines.
Evidence: `.run/t8-startup-{aga,ecs,live-aga,live-ecs,double,vbi}`,
`/tmp/pokeri-t8-startup-*-{check,report}.log`.


### T9 quiet startup batching prototype (2026-09-30, opt-in)

`STARTUP_QUIET_BATCH=1` keeps the existing 1 ms time grid but allows an original
delay loop to advance up to the next relevant boundary. A quiet serial peer may
be grouped only until the first system/input edge, watchdog warning/reset, or
10 ms cabinet observation. Watchdog age is relative to the latest kick. The
Board supplies a read-only timing snapshot and refuses stale threshold caches.
Unsupported clock/rate profiles, an explicit input event file, masked/active
handlers, serial activity or an error retain one quantum. A limited live run
also stops on its original first due quantum. Gameplay and replay are unchanged.

This is the C scheduling prototype, not T9's planned assembly admission. Both
that fast path and measured release validation remain. The native arithmetic
audit passes; no default is changed. `harness-startup-quiet-check` passes
16,102,001 horizon cases. `harness-startup-board-tick-check` compares 300 complete
serialized Board states (229 grouped cases) with repeated 1 ms ticks, including
AY phase and serial state, and checks that no intermediate hardware edge was
skipped. It also checks stale watchdog threshold refusal. All pass.
A fresh native cold/live24 measurement passes from `tmp/t9-quiet-candidate`,
which also contains the three T8c drawing flags for comparison with that
candidate. Evidence: `/tmp/pokeri-t9-{quiet-test,board-tick-test}.log`.

**MEASURED T9 prototype:** Ready remains at 47,040,000 board cycles, with 100
setup coins and 6,751 delay-hook calls (37,284,244 batched delay cycles). It
occurs at frame 807 / beam 105, 16.146720 s from the VBI origin: 1.834624 s
earlier than T8c cold. The 480 M-cycle run completes all 24 inputs, status 4,
zero errors/resets and restored vectors; board/wall ratio is 0.9767. This is
one cold prototype run, not the full release gate or a whole-startup measurement.
Evidence: `.run/t9-quiet-cold`, `/tmp/pokeri-t9-quiet-cold-report.log`.
