# ACRTC command timing study (T11)

2026-09-30. Research only. No production timing, FIFO, replay or default has
changed. The bounded T11 study is complete; its proposal and limitations are at the end.
No physical oscillator rate or production timing change has been established.

## Manual evidence

**DERIVED:** Hitachi HD63484 ACRTC User's Manual, printed pp. 14, 17,
61-62, 126 and 173; local source `ref/manuals/hd63484-acrtc-users-manual.pdf`.
The command table on PDF page 184 (printed 173) was inspected visually because
OCR loses multiplication symbols and misreads its operation-mode footnote.

- Read and write FIFOs are separate, each 16 bytes / eight words.
- WFE describes the write FIFO, not the command processor. A command may be
  executing after its parameters have been removed from that FIFO. CED is a
  separate status bit.
- The documented WFE interrupt service sends one to eight words. The next
  command is fetched as the previous command completes.
- With an initially empty FIFO, register-access commands execute faster than
  the host can supply them; that exception does not establish zero drawing
  duration or an unlimited queue.
- In single access, one memory operation uses two 2CLK periods. In interleaved
  access, display and drawing alternate over four 2CLK periods. The ROM's
  programmed OMR selects interleaved access; see `rom-set.md` display decoding.

**DERIVED:** the table gives the following approximate execution-cycle counts.
`P=4` for operation modes 0-3 and `P=6` for modes 4-7. `A,B` count drawing dots
along the main/sub directions; `x,y` count physical words. `L` counts line dots,
and `d` curve dots. These are not native renderer instruction counts.

| Command | Approximate cycles |
|---|---|
| ORG | 8 |
| WPR / RPR | 6 |
| WPTN / RPTN | `4*n+8` / `4*n+10` |
| RD / WT / MOD | 12 / 8 / 8 |
| CLR / SCLR | `(2*x+8)*y+12` / `(4*x+6)*y+12` |
| CPY / SCPY | `(6*x+10)*y+12` |
| AMOVE / RMOVE | 56 |
| ALINE / RLINE | `P*L+18` |
| ARCT / RRCT | `2*P*(A+B)+54` |
| APLL / RPLL | `sum(P*L+16)+8` |
| CRCL / ELPS | `8*d+66` / `10*d+90` |
| AARC / RARC | `8*d+18` |
| AEARC / REARC | `10*d+96` |
| AFRCT / RFRCT | `(P*A+8)*B+18` |
| PAINT | `(18*A+102)*B-58`, **rectangular filling only** |
| DOT | 18 |
| PTN | `(P*A+10)*B+20` |
| AGCPY / RGCPY | `((P+2)*A+10)*B+70` |

**Unresolved:** mapping these approximate command counts to elapsed time for
this board's oscillator, interleaved accesses and refresh arbitration. The
manual's memory-cycle definition alone does not prove that every command-cycle
entry should be multiplied uniformly by two or four. The application note
(1986, printed pp. 9-10) explicitly describes computation overlapping display
accesses and missed drawing opportunities when computation is longer.

**INFERRED model bounds to investigate:** the existing display-register decode
places hypothetical 2CLK near 2.9184-3 MHz for a roughly 50 Hz raster. That is
not a measured oscillator. A host study should sweep an explicitly labelled
cycle-to-time conversion and preserve the raw command counts, rather than
select a rate simply because it makes the Amiga perform better. The manual's
rectangular PAINT estimate must not silently be applied to arbitrary regions.

## Prototype requirements

Use a separate opt-in host experiment, retaining the ordinary harness as the
reference. Keep the original instruction stream and device operations. Model
an eight-word write queue separately from the current command; report WFE and
CED independently. Do not implement drawing duration by globally suppressing
WFE while a command runs: an empty queue can legitimately interrupt while that
command is busy, allowing the host to fill the queue for the next command.

Keep byte staging, abort, parameter collection and read FIFO behavior explicit.
Stop loudly on overflow or an unsupported timing case. Do not reuse
`drawingWorkCount()` as physical chip cycles: it counts renderer checks and
plots, including implementation-specific eligibility work and cache behavior.
Snapshots from the current synchronous model must not silently resume into a
different device-timing model.

For identical input workloads, record IRQ count and words per FIFO interrupt,
status backpressure, completed-command ordering, board cycles at animation
boundaries and AY write times. Compare raw timing separately from the approved
shuffle presentation policy; changing both together would conceal causality.

## Independent visual evidence and remaining comparison

The user-supplied Finnish clip `ref/footage/pokeri-200mk-2BI-eUaPCOc.mkv` is the
independent reference, 25 recorded frames/s. Its old contact sheet confirms
multiple distinct sideways deck shapes; it cannot identify a physical oscillator
or distinguish adjacent PAL frames. Existing host measurement finds the raw
shuffle submission loop takes only 40.6 board-ms without approved pacing.
Those observations justify research, not a calibrated command-duration model.

Still measure explicit start/end bounds (with at least the clip's 40 ms frame
uncertainty) for deal, reveal and Double; separate user decision time from
rendering. Then combine those observations with the host sweep into a concrete
proposal. Any adoption needs a separate user decision and regeneration of the
host/native replay reference, device state/snapshot schema and timing oracles.


## Frame-indexed footage comparison (2026-09-30)

**MEASURED:** `ffprobe` confirms 25/1 frames/s, start PTS 0, with the
extracted original timestamps increasing by 0.040 s. The fine sheets use
original frame `n` at `n/25` seconds, not the earlier five-per-second preview.
The phone records CRT refresh/afterglow and occasionally partial raster updates;
these observations cannot distinguish adjacent PAL fields or determine an
ACRTC completion instant. Allow at least one recorded frame per event and
additional ambiguity where afterglow obscures an edge. No button-press latency
can be inferred because the controls are outside the view.

| Visible event | Original frame / time |
|---|---|
| First back visibly leaves the deck for the five-card deal | 80 / 3.20 s (preceding frame still at deck) |
| Five dealt backs are aligned | 110 / 4.40 s; frame 109 still shows last-card motion |
| First face is visible | 112 / 4.48 s; preceding frame shows its moving cover |
| All five faces are visible | 116 / 4.64 s; preceding frame still covers the last face |
| Double transition begins covering the old hand | 319 / 12.76 s; preceding frame still has the faces |
| Five old cards are covered | 326 / 13.04 s; preceding frame has unfinished fifth back |
| Gathering returns to the upper-left deck | approximately 354–355 / 14.16–14.20 s |
| Double's single back starts moving from the deck | 412 / 16.48 s |
| Single back reaches the lower position | 420 / 16.80 s |
| A six is visible after the choice | 429 / 17.16 s; frame 428 still has the back |

Thus first visible departure to first revealed face in the initial five-card
deal spans **1.28 s**; first-to-last visible face spans **0.16 s**. Each
interval has at least approximately ±0.04 s endpoint-sampling uncertainty;
CRT/phone integration can widen it. Covering the old hand during Double takes
about **0.28 s**, and covering plus gathering about **1.4 s**. The single
Double card's visible travel takes about **0.32 s**. Its subsequent pause
until 17.16 s includes an unseen player choice and is not a graphics duration.
The following card removal is another animation, not part of that reveal.

**MEASURED host comparison:** identical original-input scenarios at bet three,
from the same ready snapshot, were captured every 40 board-ms with raw timing
and with the approved consumer shuffle pacing. Raw first visible departure /
first face / all faces occur at 4.72 / 5.80 / 6.00 s; paced at
5.00 / 6.08 / 6.28 s. Both therefore give approximately **1.08 s** for the
deal-to-first-face interval and **0.20 s** for first-to-last reveal, at the
same sampling resolution. The hands differ because pacing changes RNG
execution; these are comparable animation shapes, not identical drawing loads.
The host palette is the labelled research palette and is not used to judge
physical colours. Both runs finish without a device error or watchdog reset.

**DERIVED:** ordinary dealing/revealing is already substantially paced by the
original program with synchronous chip execution. It is not valid to treat
all animation time as omitted chip latency, or to apply a single slowdown
factor solely to match one sequence. The approximately 0.20 s difference in
the sampled deal interval remains to be explained by the timing sweep; it is
not proof of a particular clock frequency. Shuffle policy delays its start,
but does not materially change these two measured deal/reveal intervals.

Local evidence: `tmp/t11-footage/full/`, `timestamps.txt`,
`deal-fine.png`, `double-entry-fine.png`, `double-deal-fine.png`,
`{raw,paced}-frame-*.ppm`, `{raw,paced}.catalog`, `deal.inputs` and
`host-boundaries.png`. Fine footage extraction uses no frame-rate conversion:

```
ffmpeg -i ref/footage/pokeri-200mk-2BI-eUaPCOc.mkv -vf 'crop=720:490:320:80,scale=360:245' -fps_mode passthrough tmp/t11-footage/full/%04d.png
```

The optional command-duration/FIFO prototype and its IRQ/AY sweep remain.
The host Double workload still needs the same phase-separated comparison.
No production timing, presentation or replay behavior changes here.


## Host FIFO scheduler foundation

`host/acrtc_timing_fifo.h` is an isolated research scheduler, not an installed
device model. A supplied policy validates command formats, supplies an explicit
duration and commits the command only on completion. No oscillator conversion
or drawing-duration formula is assumed by the scheduler. It has an eight-word
queue independent of the running command and of parameter collection. WFE can
therefore be asserted while CED is clear. The existing board's high/low byte
staging is explicit; changing the address selection cancels that partial byte
without aborting a collected command. Variable counts follow the current board
profile's word-count interpretation; other WPTN interpretations require an
explicit policy extension before use.

The core advances through every due completion even when one tick spans
multiple commands. Aborting cancels queued, partial and running commands without
committing their effects. FIFO overflow, unknown formats/durations, completion
errors and clock overflow are loud stops. It deliberately provides no snapshot
compatibility: harness integration must reject incompatible saved states as
specified above. This core alone does not model read FIFO, register accesses,
physical status propagation delays or command execution.

**MEASURED synthetic validation:** `make harness-acrtc-timing-check` passes
independent WFE/CED and interrupt-enable cases, exact eight-word occupancy,
partial-byte/word-sized accesses, address-selection semantics, long variable
parameter collection, cancelled completions, fault propagation, 101 partitions
of a multi-command deadline sequence and 1,000 complete FIFO ring wraps.
Known expected completion timestamps are checked, not just two paths agreeing.
No original ROM data is included in these fixtures. Normal host and Amiga
builds do not include this scheduler. Next is the opt-in controller adapter,
explicit duration hypotheses and the requested IRQ/AY timing sweep.


## Delayed renderer adapter foundation

`host/acrtc_timing_device.h` wraps the existing Hd63484 renderer without
changing it. Commands use the shared format/reserved-bit decoder, then remain
uncommitted until the supplied duration expires. Only then are their original
words sent to the ordinary renderer. The adapter combines its FIFO/CED state
with the renderer's read-FIFO/error/area status and keeps CCR interrupt enables
live. Control-register accesses retain the existing auto-increment behavior.
An internal completion must not select the FIFO through the external address
port: doing so would wrongly cancel a half-read result. The adapter preserves
the address selector and read-byte phase while completing commands.

**MEASURED synthetic validation:** `make harness-acrtc-device-check` proves
pixels and RPR results remain absent before their deadlines, WFE IRQ can be
asserted during drawing, queued commands see their predecessors' completed
state, and ABT prevents a pending write from taking effect. It also checks a
half-consumed read result across an unrelated completion and complete
VRAM/register/read-order equality against synchronous execution. These tests
use an explicitly synthetic ten-tick duration, not the manual's cycle table.

The adapter refuses an initially partial command, attached native surface/card
cache, byte-count WPTN interpretation or the unintegrated shuffle-presentation
policy. A changed GBM/memory-width register during execution is a loud stop:
its hardware latch timing has not been established. Duration-policy rejection
also stops without executing the command. This is still a standalone host
test component. Harness bus/IRQ/tick integration, state-format isolation,
documented duration hypotheses and actual workload sweeps remain; normal
host/SDL/Amiga builds do not include it.


## Isolated harness integration (2026-09-30)

`make harness-acrtc-research` builds `build/pokeri-host-timing`. All C++
translation units have separate objects so the research Board layout cannot be
mixed with normal objects. The research macro is forbidden in freestanding
builds. Normal host/SDL/Amiga builds retain their synchronous path.

The adapter receives all decoded video bus accesses and elapsed instruction
cycles; its IRQ participates in the existing encoder priority. A CPU watchdog
reset preserves in-flight video work, just as it preserves the ordinary device.
Read/write/tick faults propagate to the Board's loud stop. Timed snapshots are
rejected both at the CLI and Board serializer; automatic diagnostics omit the
otherwise misleading synchronous board/CPU snapshots, retaining RAM, VRAM,
trace, coverage, AY events and final images. Saved-state restore, replay,
relocation, window mode and shuffle-pacing combinations are refused explicitly.

For integration testing only, `--acrtc-fixed-cycles N` requires an explicit
**synthetic** duration in board cycles for every command. It is not the manual's
cycle model and must not be interpreted as a hardware speed estimate. Zero is
useful as an equivalence control. Completions occur in the scheduler at their
exact deadline, but the CPU observes IRQs at its next instruction boundary;
ordinary command-log timestamps use the enclosing instruction's end cycle.

Reproduce the cold eight-second integration experiment (use a fresh output
prefix so no prior NVRAM is loaded):

```
build/pokeri-host-timing --devices --acrtc-fixed-cycles 0 \
  --skip-hardware-tests --serial-peer --system-hz 100 --input-hz 50 \
  --watchdog-ms 400 --watchdog-reset-us 50000 --ay-clock 1000000 \
  --auto-setup --ms 8000 --stall-instructions 0 --out tmp/timing-zero
```

**MEASURED:** synchronous and zero-delay runs match all 262,144 RAM bytes,
524,288 VRAM bytes, 177,536 uncropped display indices, 131,072 coverage bytes,
the complete 5,315,455-byte device trace and device summary. Both execute
7,904,133 instructions / 64,000,000 cycles, deliver 8,685 IRQs and 60 AY writes,
and reach Ready at cycle 51,124,920. No watchdog reset occurs. The ten-cycle
sample has the same instruction/IRQ/Ready counts. At 100 cycles per command,
Ready is 51,044,908 with 8,685 IRQs; at 1,000 it is 50,965,018 with 6,441 IRQs.
All finish with 60 AY writes and no reset/fault. These are sensitivity results,
not performance improvements or physical calibration; differing CPU/device
interleaving can even move the acknowledgment-driven setup earlier.
Evidence: `tmp/t11-integration/{sync,fixed-0,fixed-10,fixed-100,fixed-1000}*`.

**MEASURED validation:** FIFO, adapter and Board integration suites pass,
including IRQ priority, reset preservation, snapshot refusal and all three
fault-propagation paths. Ordinary `harness-check`, `harness-platform-check`
and `harness-native-check` pass. Manual-derived duration policies, actual
FIFO-word-per-IRQ and AY-lateness sweeps, and the host Double comparison remain.


## Command-specific host hypothesis and first sweep (2026-09-30)

`--acrtc-table-hz N` selects `host/acrtc_duration.h` instead of the synthetic
fixed duration. N is an explicit effective table-cycle frequency, not a claimed
oscillator reading. Each duration is rounded up to an integral board cycle.
Malformed commands, unsupported renderer groups and conversion overflow stop.
No normal build uses this policy.

**DERIVED formulas / INFERRED geometry:** supported register, memory, movement,
line, polyline, rectangle, pattern and copy commands use the table above with
the renderer's signed, inclusive rectangle extents and endpoint-excluded line
dots. Curves execute on a private renderer copy to count contour visits before
colour/pattern masking, then use the appropriate table coefficient and fixed
cost. That contour algorithm is still our approximation of chip geometry.
PAINT counts filled dots and contiguous scanline runs on the private copy and
uses `18*dots + 102*runs - 58`, agreeing with the table for a rectangular region.
Its extension to irregular regions is **INFERRED**, not in the manual. An empty
region currently receives zero duration; its real setup cost is unknown. These
assumptions must remain visible in any subsequent proposal or sensitivity study.
The policy does not count eligibility checks or reuse `drawingWorkCount()`.
The private run commits no authoritative pixels, CP, read results or cache state.

`host/acrtc_timing_report.py` summarizes event logs. The original handler entry
at `$2E26` and guarded RTE at `$2E8A` delimit its words-per-service counter;
separate IRQ records carry cumulative word totals. Inter-interrupt deltas must
not be mistaken for handler word counts, because foreground writes also exist.
More than eight words can be delivered in one handler: its loop keeps feeding
while the processor consumes the FIFO. An eight-word queue is not an eight-word
lifetime cap per invocation.

**MEASURED**, cold initialization with automatic cabinet setup, same eight-second
absolute board-time budget and zero initial persistent state:

| Hypothesis | Ready board-s | Total IRQs | Video IRQs/completed services | Mean words/service | Full-FIFO status reads |
|---|---:|---:|---:|---:|---:|
| Zero command duration | 6.390615 | 8,685 | 5,870 | 5.422 | 0 |
| 1.5M table cycles/s | 7.580800 | 4,946 | 2,154 | 13.584 | 989 |
| 3M table cycles/s | 6.800712 | 6,227 | 3,422 | 9.254 | 620 |
| 6M table cycles/s | 6.510626 | 6,992 | 4,181 | 7.613 | 472 |

All reach Ready with no error/reset and exactly the same sequence of 60 AY
register/value writes. Their board-time shifts relative to zero delay range
from zero to 1.330 / 0.550 / 0.180 seconds respectively. These are displacement
of initialization events, **not native late-write measurements**. The zero-delay
observer still matches the ordinary reference's entire RAM/VRAM/pixel/coverage/
device trace and summary. At 3M, 7,663 commands have completed at the endpoint;
231 duration estimates use inferred curve/PAINT geometry. At 1.5M, one command
is still in flight (6,995 estimated, 6,994 completed), so its raw cycle total
must not be interpreted as completed work. Different endpoints/progress mean
aggregate totals alone are insufficient to select a physically credible rate.

Evidence: `tmp/t11-table-final/{0,1500000,3000000,6000000}*`, including
`report.json`. Reproduce using the preceding command with `--acrtc-table-hz N`
instead of `--acrtc-fixed-cycles 0`, then:

```
python3 host/acrtc_timing_report.py --reference tmp/t11-table-final/0-events.txt \
  tmp/t11-table-final/1500000-events.txt tmp/t11-table-final/3000000-events.txt \
  tmp/t11-table-final/6000000-events.txt
```

**MEASURED validation:** manual-count fixtures, signed dimensions, private
radius-two contour (12 dots), rectangular/nonrectangular PAINT estimates,
unchanged authoritative state and conversion bounds pass. All adapter/Board
integration and ordinary host/platform/native-model suites pass. Full gameplay,
Double, animation-boundary and AY-write sweeps remain before the T11 proposal;
this startup experiment does not establish a replacement timing contract.


## Gameplay, accepted Double and bounded-study proposal (2026-09-30)

The isolated `--acrtc-double-scenario` driver reuses `DoubleScenario`,
`AmigaKeyEvents`, read-acknowledged button transitions and cabinet coin packets.
It runs at 50 Hz after fresh automatic setup at 8 MHz. It reads the dealt ranks
and suits to select holds, then presses Double only when the ROM indicates it
is available. It never supplies card, credit, CPU or RAM results. Separate
markers record the keypress and the ROM clearing its Double-ready flag. A run
cannot count as accepted solely because it pressed the key. Unsupported clocks,
external scripts and accounting overrides are refused for this driver.

**MEASURED:** zero / 1.5 / 3 / 6M table-rate scenarios all finish an accepted
Double and the subsequent Big choice without a fault or watchdog reset, in
round 2 / 2 / 1 / 1. They use the same player policy and identical initial
coin/Deal offsets after Ready. Different timing changes the ROM RNG, so later
hands, hold keys, win music phase and round counts differ. They are **not
identical-card replays**; neither aggregate session cost nor a per-note
one-to-one comparison is valid across those different games.

Twenty-five-fps headless frame captures were repeated from fresh prefixes.
All four repeat runs match their uncaptured runs' entire RAM, VRAM, final pixel
indices and bus trace. The screenshots therefore add no emulated delay. These
are raw current-frame samples, not a physical raster simulation or the native
presentation policy. Fine sheets use 40 ms spacing; allow at least ±40 ms per
boundary (±80 ms for a derived interval), plus ambiguity during partial drawing.

| Rate hypothesis | First visible departure | First exposed face | All five faces exposed | Departure-to-face | Reveal span |
|---|---:|---:|---:|---:|---:|
| Zero | 8.60 s | 9.72 s | 9.88 s | 1.12 s | 0.16 s |
| 1.5M | 11.04 s | 12.40 s | 12.60–12.64 s | 1.36 s | 0.20–0.24 s |
| 3M | 9.64 s | 10.80–10.84 s | 11.00–11.04 s | 1.16–1.20 s | about 0.20 s |
| 6M | 8.72 s | 9.80 s | 10.00 s | 1.08 s | 0.20 s |
| Physical footage | 3.20 s | 4.48 s | 4.64 s | 1.28 s | 0.16 s |

“Exposed” distinguishes the moving back uncovering the face from finishing every
pip underneath: delayed rendering can leave a partial face for another sample.
Do not fit a precise frequency to these ranges or to different hands.

Double's visible covering begins at approximately 42.92 / 47.48 / 22.76 / 21.88 s
for zero / 1.5 / 3 / 6M. Its five-card covering phase takes under 0.08 / about
0.28 / about 0.20 / about 0.08 s, versus 0.28 s in the physical clip. The complete
cover-and-gather sequence returns to the upper-left deck at roughly 43.60 /
49.16 / 23.80 / 22.68 s: about 0.68 / 1.68 / 1.04 / 0.80 s after visible covering,
versus roughly 1.4 s in the clip. The initial zero-delay covering can start and
finish between adjacent captures; no exact lower duration follows from them.
The later single-card travel is about 0.24 s in both zero (44.64–44.88) and 3M
(25.60–25.84), versus about 0.32 s physically. The player's Big/Small decision
is excluded. These independent phases do not identify one unambiguous rate.

### Original sound writes, not native service lateness

| Rate | Double key → ROM acceptance | Key → next AY write | AY gap straddling keypress | Video services in first 2 s after key |
|---|---:|---:|---:|---:|
| Zero | 23.279 ms | 78.421 ms | 439.759 ms | 590 |
| 1.5M | 23.018 ms | 558.300 ms | 1289.662 ms | 151 |
| 3M | 33.109 ms | 208.975 ms | 519.739 ms | 178 |
| 6M | 53.566 ms | 129.077 ms | 339.689 ms | 266 |

These are **MEASURED host board-time gaps**, including intentional silence,
original command scheduling and differing win-music phases. They are not proof
that a note should decay over that interval, nor an A/B native audio regression.
The known native excess-delay measurements remain separate. The experiment
shows why adding display delay cannot simply be assumed to cure sound timing:
it also changes when the original program issues its next sound command.

Evidence: `tmp/t11-double/{0,1500000,3000000,6000000}*` with catalogs and
`report.json`; repeated images and fine sheets in `tmp/t11-double-frames/`.
Reproduce the prior cold-start command with `--acrtc-double-scenario --ms 280000`,
`--video-catalog tmp/new-catalog.txt` and a fresh output prefix. It terminates
when the external player's sequence finishes. Add `--palette-rom 0
--frame-every 2` for 25-fps captures; they stay local. The report tool emits
acceptance, AY gaps and the first-two-second service count. Synthetic driver
tests prove external coin delivery, read-acknowledged releases, unchanged game
RAM and ROM-ready gating. All four actual runs independently verify acceptance.

### Proposal and disposition

**Recommendation: retain the synchronous production model for now; retain this
host-only prototype as a research comparison.** The bounded T11 deliverable is
complete: manual evidence, an optional timed FIFO/command model, startup and
gameplay interrupt/word/backpressure/audio measurements, physical phase bounds
and this adoption proposal. This is not a claim that the old model faithfully
models chip timing, or that physical calibration is solved.

The evidence supports finite FIFO capacity and distinguishing WFE from CED.
It does **not** establish the board's effective command-cycle rate, exact curve
geometry, irregular/empty-PAINT cost, mid-command register latching, raster
arbitration or read-FIFO behavior. Choosing a rate to reduce native traps or
stretch the shuffle would fit our performance problem rather than identify
hardware behavior. Continue the already-approved native T13/T14 experiments
under the existing contract instead of silently rebasing their reference.

If a later decision adopts timed execution, the concrete scope must include:
1. Select and document a rate/arbitration/PAINT hypothesis against multiple
   physical phases and audio, with its remaining uncertainty explicit.
2. Complete latch/abort/queue and read-side semantics; preserve every original
   bus and IRQ boundary and loud unsupported cases. Define completion-visible
   state for the native blitter and cached drawing paths.
3. Version all saved device/snapshot and replay formats to carry FIFO, collected
   and running command, byte phase, remaining duration and conversion phase;
   refuse old snapshots. Regenerate host references, native exact ECS/AGA
   replays and timing/CPU oracles rather than comparing incompatible models.
4. Re-measure accepted Double, shuffle and draw under the production
   presentation/input/audio policies, with cold/warm live24, memory cleanup,
   watchdog and WHDLoad gates. Report original AY scheduling separately from
   native excess delay and PAL-driven envelope decay.

That adoption is a **new explicit user decision**, not authorized by building
this bounded host experiment. No normal SDL or Amiga timing changed here.
