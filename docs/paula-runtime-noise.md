# Shared runtime Paula noise — 2026-09-30

The user requested a pre-change release archive followed by inexpensive runtime
noise mixing, without a separate rendered waveform for every note. The previous
bank release is preserved locally as `dist/Pokeri-0.90-before-runtime-noise.lha`.

## Implementation

Five 8,192-byte buffers are generated once at startup: AY-style 17-bit LFSR
noise, and the same noise AND-gated with 2-, 8-, 32- and 128-sample square
waves. Paula plays these buffers directly, so all 40,960 bytes must be Chip
RAM. The generator state and 128-byte lookup tables remain in ordinary memory.
There is no temporary noise source in Chip RAM and no embedded waveform bank.
The separate 32-byte pure-tone allocation is unchanged.

Paula supplies note pitch. Mixed noise consequently follows tone playback rate,
ignoring the independent AY noise divider: this is the requested approximation,
not exact AY tone/noise emulation. Noise-only playback follows the noise divider,
clamped to Paula's safe DMA rate. The longest gate that respects Paula's minimum safe period is selected; this
keeps mixed noise at a higher sample rate without per-note rendering. Envelopes, register writes and game sound scheduling remain
unchanged; live envelopes still advance on PAL time.

While noise is audible, each VBI replaces eight samples in each shared buffer
(40 output bytes). The cursor wraps through the buffers; this is sparse evolution
of finite loops, not continuous full-rate synthesis. Noise is not independently
regenerated for each voice. Refresh occurs after screen VBI publication.

DMA slices are powers of two between 2 and 256 bytes, limited to about 8 ms unless
one Paula word itself exceeds that duration. A slower period is applied after two
DMA boundaries have flushed previously queued longer slices. Faster periods can
apply immediately. Pure tones use this handoff then disable their slice interrupt.
The level-4 server queues pointers/lengths and performs no sample generation.

## Checks and measurements

**MEASURED:** `make harness-paula-check` checks generated noise against the
independent AY model, all four fixed gates, bounded refresh writes, pitch/divider
arithmetic and 221,184 cases of the linked 68000 DMA server (wraps, slice sizes,
period handoffs, interrupt acknowledgement and preserved registers). It also runs
the linked noise generator under Musashi and compares its memory to the portable
implementation. Build the Amiga ELF first and source `amiga/env.sh` for these checks.

**MEASURED (initial two-gate implementation, superseded below):** noise refresh costs 834 core 68000 cycles; generating all three
buffers costs 538,932 cycles. The DMA handler costs 306–422 core cycles per call.
**DERIVED:** at 7.09 MHz, generation is about 0.118 ms per audible VBI (0.59% of a
50 Hz CPU budget) and 76 ms at startup. These figures exclude Chip RAM contention,
Exec/exception overhead and the other VBI work; they are not whole-game timings.

**MEASURED:** muted A1200 live24 reaches 480,000,000 board cycles with all 24
inputs, status 4, no error or watchdog reset, and restored vectors. It records
236 noise refreshes, 514/735/739 audio interrupts and 495 AY writes. Evidence:
`amiga/.run/runtime-noise-live24/gdb-out.log`. Host platform tests also pass. The actual stripped release reaches Ready and
completes another 1,000 dispatches without error/reset in an A1200 cold-start
smoke check: `amiga/.run/runtime-noise-release/gdb-out.log`.

The buffers have finite periodicity and share source content; voice phase and
analogue response are approximate. No subjective audio comparison or physical
Amiga calibration is claimed by these muted and digital checks.


## Mixed-noise crackle correction (2026-09-30)

The user reported crackly shuffle/deal sounds. **DERIVED:** the original two-
and eight-sample gates made noise vary at a low multiple of the tone frequency.
For AY tone period 568, present in the gameplay sound trace, the two-sample loop
updated noise at about 220 Hz. This can sound like individual crackles/random
amplitude modulation rather than broadband noise. This is a plausible audible
cause, not a subjective confirmation or an identified DMA fault.

The replacement chooses 2/8/32/128 samples per tone cycle (thresholds 18, 70,
280), keeping the hardware period at least 124. Period 568 now plays its 128-
sample shape at about 14.2 kHz, with the same target note pitch subject to Paula
period rounding. No original sound writes, envelopes, pure tones or noise-only
playback are changed. Noise still follows playback pitch rather than an exact
independent AY divider. The extra two shared loops cost 16 KiB Chip RAM; no
per-note sample bank or continuous mixer is introduced.

**MEASURED:** independent tests check all 4,096 tone periods against the longest
safe gate and integer pitch formula, all four noise gates against the AY LFSR,
refresh bounds, and 221,184 linked DMA cases. The generator now costs
1,012–1,028 core cycles per refresh and at most 725,304 at startup. **DERIVED:**
about 0.145 ms per audible VBI and 102 ms initialization at 7.09 MHz, excluding
DMA contention and OS overhead. Release size is 246,808 bytes.

**MEASURED (updated backend):** A1200 live24 passes at 480,000,000 cycles with
24 inputs, 529 noise refreshes, 1,712/1,574/1,709 audio interrupts, 1,380 AY writes,
no errors/resets and restored vectors. Evidence:
`amiga/.run/noise-detail-live24/gdb-out.log`. An earlier run was externally killed
before completion and is not validation evidence. Final release/audio tests and
archive audits pass (`tmp/noise-detail/tests-final.log`, `release-final.log`).
The archive is 135,641 bytes. Debug runs remain muted: the low-rate defect is
corrected, but by-ear improvement is not established by these tests.
