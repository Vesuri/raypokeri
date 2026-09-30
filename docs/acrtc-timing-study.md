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
