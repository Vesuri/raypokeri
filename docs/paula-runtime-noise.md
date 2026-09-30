# Shared runtime Paula noise — 2026-09-30

The user requested a pre-change release archive followed by inexpensive runtime
noise mixing, without a separate rendered waveform for every note. The previous
bank release is preserved locally as `dist/Pokeri-0.90-before-runtime-noise.lha`.

## Implementation

Three 8,192-byte buffers are generated once at startup: AY-style 17-bit LFSR
noise, the same noise AND-gated with a two-sample square wave, and an eight-sample
square wave. Paula plays these buffers directly, so all 24,576 bytes must be Chip
RAM. The generator state and 128-byte lookup tables remain in ordinary memory.
There is no temporary noise source in Chip RAM and no embedded waveform bank.
The separate 32-byte pure-tone allocation is unchanged.

Paula supplies note pitch. Mixed noise consequently follows tone playback rate,
ignoring the independent AY noise divider: this is the requested approximation,
not exact AY tone/noise emulation. Noise-only playback follows the noise divider,
clamped to Paula's safe DMA rate. The two fixed gates cover the tone range without
per-note rendering. Envelopes, register writes and game sound scheduling remain
unchanged; live envelopes still advance on PAL time.

While noise is audible, each VBI replaces eight samples in each shared buffer
(24 output bytes). The cursor wraps through the buffers; this is sparse evolution
of finite loops, not continuous full-rate synthesis. Noise is not independently
regenerated for each voice. Refresh occurs after screen VBI publication.

DMA slices are powers of two between 2 and 256 bytes, limited to about 8 ms unless
one Paula word itself exceeds that duration. A slower period is applied after two
DMA boundaries have flushed previously queued longer slices. Faster periods can
apply immediately. Pure tones use this handoff then disable their slice interrupt.
The level-4 server queues pointers/lengths and performs no sample generation.

## Checks and measurements

**MEASURED:** `make harness-paula-check` checks generated noise against the
independent AY model, both fixed gates, bounded refresh writes, pitch/divider
arithmetic and 221,184 cases of the linked 68000 DMA server (wraps, slice sizes,
period handoffs, interrupt acknowledgement and preserved registers). It also runs
the linked noise generator under Musashi and compares its memory to the portable
implementation. Build the Amiga ELF first and source `amiga/env.sh` for these checks.

**MEASURED:** noise refresh costs 834 core 68000 cycles; generating all three
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
