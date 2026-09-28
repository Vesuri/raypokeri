# Startup, artwork copies, scrolling and shuffle sound

User follow-up, 2026-09-28. Continue the existing performance work without
changing game decisions, hiding faults, or trading correctness for speed.
The memory audit is complete in `471b832`; see memory-audit.md.

## Work order and acceptance

1. **Interleaved copies.** Capture actual attract/doubling commands. Combine
   the four compatible plane transfers into one queued blit. Preserve masks,
   shifter prefetch, storage seams, overlap rejection and the 1,023-row OCS
   limit. Run native self-tests and full replay equality on ECS and AGA, then
   compare native benchmarks and live scrolling. No AGA-only dependency.
2. **Cold setup.** Measure initialization and the 100 acknowledged reserve
   coins separately; attribute time to original execution, command feeding,
   raster work and presentation. Reduce repeated artwork construction and
   setup costs. Keep original accounting execution and external acknowledgments.
   Compare fresh starts against the current SDL cold-start experience; do not
   use a warm snapshot as evidence for cold-start performance.
3. **Remaining artwork.** Catalog both fonts, card numbers, large/small suits,
   and J/Q/K/JOKER. Distinguish already resident offscreen images from shapes
   rebuilt procedurally. Optimize the existing copies first; add exact guarded
   sequence/raster caches only for remaining repeated raster work. Include
   colour/pattern/ROP, orientation, state side effects, observation barriers and
   source modifications in the proof. Derive assets locally; never commit them.
4. **Shuffle sound.** Reconcile the visible shuffle with the original sound
   call and callback ordering and the physical footage. The new presentation
   waits prolong a non-reentrant callback. Do not silently move a sound call,
   invent AY writes, or make the whole guest scheduler reentrant. Propose any
   required scheduling change with its safety conditions before adopting it.
5. **Persistence / development fixtures.** Establish retained main-RAM bounds,
   pointer/checksum handling and reset behavior before extending native saves.
   Keep prepared local fixtures for warm development runs; make cold-start
   tests explicit. Preserve live saves between ordinary launches. Do not seed
   a fresh drive with today's all-zero $D0000 file and claim faster startup.

## Evidence so far

- **MEASURED:** current native warm launch with existing `nvram.bin` still
  inserts 100 reserve coins and takes 2,183 PAL frames (43.66 s) to Ready.
  That file contains 32,768 zero bytes. SDL's immediate second launch loads a
  complete clean-start snapshot. Native accounting lives in main RAM; metadata
  includes pointers and checksums, so relocation across launches matters.
- **MEASURED:** the paced host shuffle has zero AY writes over 30 steps.
  **DERIVED:** `$1AA0A` selects sound 9 *after* calling the shuffle. The ROM also
  defers scheduled callbacks while one is active. This is not evidence of a
  Paula DMA malfunction. Physical sound timing still needs comparison.
- **MEASURED:** attract scrolling includes 261 `$EC00` copies of a 211×20 strip
  from offscreen Y=-850, advancing source X one pixel per update; wrap uses
  additional split copies. These transfers are disjoint, not in-place scrolls.
- **MEASURED (earlier full selector enumeration):** ordinary face cards copy
  their striped inset; J/Q/K copy complete picture insets. Four 17×17 suit/rank
  copies precede the inset. Those assets already exist offscreen in the ROM's
  rendering scheme. See card-back-blit-design.md, “Shared white-card prefix”.

Local traces: `tmp/render-attract{.catalog,-copies.log}`,
`tmp/shuffle-check-full-events.txt`, `amiga/.run/warm-start-audit/gdb-out.log`.
The interleaved copy implementation is under validation; no new speedup or
completion claim is made yet.

Do not run `window-memory-test` again in this session: the user asked to stop
because failures in the installed SDL library loader produced repeated popups.
Use the non-SDL headless harness for research and muted native diagnostic runs.
