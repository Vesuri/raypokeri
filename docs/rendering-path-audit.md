# Rendering paths and remaining Double latency

Audit started 2026-09-29 at `b0b22e9`. This is the current investigation for
layout rendering, remaining scalar work and command-delivery overhead.

## Existing paths (code inspection)

| Operation | Fast route | Refusal/remaining work |
|---|---|---|
| Solid RFRCT / CLR | Blitter fill; short spans use planar CPU words | Nonuniform RFRCT patterns, unsupported pitch/bounds/wrap, work-limit semantics |
| Black/white large rectangles | One interleaved constant blit per up-to-255-row chunk | Full 292-row clears require two OCS-size jobs |
| Gray bars / coloured boxes | Four constant plane blits | Already hardware rectangles; no flood-fill search |
| Card backs | One masked interleaved cached blit | Recipe/context/bounds mismatch; backgrounds outside original or all-white guard proof |
| Face-up card body | Shared cached white rounded prefix | Same guards; inset/stripes follow original commands |
| Fonts | Cached PTN plane tiles, then resident VRAM copies | Tile misses expand words; unsupported tile size/zoom use scalar drawing; physical seams now split into cached blits |
| Ranks, suits, J/Q/K/Joker | Resident offscreen planar images copied with blitter | Upright overlap/bounds refusals; 180-degree copies use CPU reversed words |
| Scrolling | Blitter copy of resident strip | Unsupported overlap/wrapping still falls back |
| Curves | Cached outline row masks, CPU word writes | Patterned/unsupported stamp cases use individual points; first outline builds cache |
| Polygon lines | Uniform planar Bresenham words; axial segments use rectangles | Other colour/pattern modes use scalar plotting |
| PAINT outside admitted card sequences | Word-parallel scanline search; blitter/CPU spans | Still does eligibility, seed-stack and visited-span work; genuinely background dependent |

A rectangle command is not inherently a slow path: replacing a gray bar's
four constant blits with a bitmap cache would add source DMA. Likewise, ranks,
suits, picture insets and complete Joker already reside in offscreen VRAM;
a duplicate cache would need invalidation without removing the copy.

## Baseline qualification

The last detailed-logger excess audio delay was 693 ms; the latest production-
FIFO-path ledger baseline is 570 ms. The latter's five cached Double backs
still take 55–57 ms each, about 750 hooked operations each. Inclusive blitter
wait is 0–0.04 ms within these feeds. Faster layout fills alone cannot remove
that command-delivery cost. Original IRQ and instruction boundaries remain
mandatory. See [previous paired evidence](double-transition-performance.md).

The diagnostic renderer now counts actual selected drawing routes, excluding
commands skipped by card recognition. These counters compile out of normal
builds. Native route counts and before/after measurements will follow below.

## Sampled native routes

**MEASURED:** `amiga/.run/render-audit` counts routes actually executed (recognized
card commands are excluded). At Ready / after the sampled hand:

| Route | Startup | Post-Ready increment |
|---|---:|---:|
| Accelerated RFRCT | 47 | 64 |
| Scalar RFRCT | 0 | 0 |
| Axis-aligned line rectangles | 646 | 24 |
| Planar CPU lines | 457 | 30 |
| Scalar lines | 0 | 0 |
| Cached curve stamps | 108 | 16 |
| PAINT searches | 89 | 18 |
| Cached PTN tiles | 608 | 10 |
| Scalar PTN | 15 | 0 |
| Upright accelerated copies | 29 | 298 |
| Rotated CPU-word copies | 5 | 15 |
| Scalar copies | 0 | 0 |
| Accelerated CLR | 1 | 0 |

This scripted run did not reach Double; it is not an audio-latency comparison.
Its large elapsed idle interval does not constitute broad gameplay coverage.
The 18 post-Ready PAINTs accompany guard-rejected card sequences. Keeping a
fallback for an unproved background is required for correct fill reachability;
it is not permission to replace arbitrary flood fills with rectangles.

**DERIVED, cross-checked against the native count:** the host boot command stream
contains exactly 15 otherwise-valid 15×14 font tiles that cross a physical
608-pixel plane-row seam. All use the supported unzoomed pattern window. The
new backend splits those into cached blits, preserving phase and ROP; other
pitches split into individual native rows only when a seam requires it.
No glyph or ROM data is stored in this document.

## Black/white fill experiment

Identical plane values now share an interleaved constant blit. OCS height limits
require splitting at 255 logical rows, so a 292-row full-screen clear uses two
blits instead of four. Tiny CPU fills and coloured-plane rectangles retain their
existing paths. Gray bars already use hardware constant fills; a cached bitmap
would introduce source reads without avoiding destination writes.

**MEASURED:** paired DMA-inclusive batches of 32 full 608×292 clears, PAL E-clock
709,379 Hz: black 588,639 → 587,455 ticks; white 588,447 → 587,422 ticks.
That is about 25.93 → 25.88 ms per black clear, only 0.20% elapsed improvement.
The reduction in submissions is real, but DMA bandwidth dominates this workload.
It cannot explain or fix hundreds of milliseconds of sound delay.
Local evidence: `.run/clear-bench{-baseline,}` and frozen `tmp/perf/Pokeri-clear*`.

The first expanded self-test contaminated the old copy-test fixture with its
tall-fill background. Both old and new renderers failed that test. Restoring
the retained packed oracle fixes the fixture; no renderer workaround was added.
**MEASURED:** the corrected clear-only AGA test and exact replay match all
262,144 RAM bytes, 524,288 VRAM bytes, 172,064 pixels and 60 AY writes at
7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs.
The combined tile-seam/copy-program candidate also passes these exact ECS and
AGA gates (`copy-program-{ecs,aga}`), including the expanded native hardware
self-tests. Host platform/native checks and the native instruction audit pass.

## Copy setup and scrolling

Equal-pitch non-overlapping rectangles now use a constant-time overlap proof
instead of walking their rows. A 16-entry cache retains verified blitter
register programs keyed by dimensions, alignment and ROP. Every hit rechecks
current bounds/overlap and supplies current source/destination pointers; it
never caches source pixels. Mask allocation is stable for the surface lifetime.
Native tests cover shifted pointers, changed source pixels and all four ROPs;
host tests compare 500,200 overlap pairs with the old ordered-row oracle.

**MEASURED:** synthetic scrolling batches, same A1200 configuration:
attract 470,481 → 455,197 E-clock ticks (3.25% lower), doubling
363,731 → 347,790 (4.38% lower). These are batch improvements, not a measured
whole-game speedup. The intermediate overlap-only values were 460,915/357,989.
The copy-program benchmark records 265 hits.

**MEASURED:** the completed `wall-envelope` live Double run uses the combined
graphics changes. At Ready there are 745 accelerated PTNs and zero scalar PTNs
(previously 730/15); the physical-seam refusals are eliminated. It records 803
copy-program hits over the run, zero copy bounds/overlap refusals, no scalar
RFRCT/line/curve/copy routes, error=0 and watchdog resets=0. The separate
`copy-program-fast` adaptive run did not reach Double and is excluded from
sound comparisons. Guard-dependent PAINT remains intentionally conservative.

The sound-envelope clock mismatch and its approved correction are measured
separately in [live-envelope-clock-experiment.md](live-envelope-clock-experiment.md).
The corrected fade does not remove delayed game-generated sound writes.
