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
| `amigaInputApply` (50 Hz) | ≈380 µs: copies/clears 3×128 volatile bytes under Disable() |
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
| T4 | `amigaInputApply` 380 µs at 50 Hz | latch/clear only the 17 keys read | −1.5% gameplay |
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
