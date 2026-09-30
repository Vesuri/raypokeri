# ACRTC command timing study (T11)

2026-09-30. Research only. No production timing, FIFO, replay or default has
changed. This is the evidence collected so far, not T11's completed proposal.
The host timing experiment and the measured deal/reveal/Double comparison are
still required.

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
