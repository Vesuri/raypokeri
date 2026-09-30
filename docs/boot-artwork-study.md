# Boot artwork study (T12)

2026-09-30. Design study only; no boot-cache implementation or default change.
Settings-menu variants and the complete proof/data layout remain to be studied
before asking for the required go/no-go.

## Current measurements

**MEASURED:** the headless host captures 5,576 completed commands / 23,888
16-bit words from startup through first main-loop entry. The stream is exactly
identical for fresh accounting, retained zero credits / 100 reserve coins, and
retained three credits / 103 reserve coins. The nonzero fixture was produced
by original coin and bet inputs after a valid Ready snapshot, then its real
accounting bytes were wrapped in the existing checked persistence format.
The next boot preserves those three credits. This is evidence for these
fixtures, not for all service-menu settings.

The first and final command complete at instructions 23,626 and 598,963. The
first-main breakpoint stops at instruction 600,502 / cycle 6,769,610; the host
finishes the intercepted instruction before stopping. The first automatic
operator action occurs later (cycle 6,881,024 cold / 6,801,008 retained).
Both cutoffs contain the same complete command stream.

| Command | Count |
|---|---:|
| WPR | 2,461 |
| ORG | 1 |
| AMOVE / RMOVE | 573 / 753 |
| WPTN / PTN | 622 / 621 |
| CLR / RFRCT / PAINT | 1 / 81 / 107 |
| CRCL / ELPS | 20 / 45 |
| APLL / RPLL | 81 / 87 |
| RLINE / RARC / REARC | 20 / 6 / 53 |
| AGCPY / DOT | 25 / 19 |

**MEASURED bus barriers:** between those command completions there are 27,871
status-read bytes and 4,033 direct register writes, all to CCR low (register 3,
interrupt enables). There are no read-FIFO bytes or direct control-register
reads, and no other direct control-register writes in that interval. The
address selector still changes to reach the FIFO and CCR. The recognizer must
preserve these accesses and every original interrupt; they cannot be discarded
just because the artwork stream is invariant.

**MEASURED data-size estimate:** the complete packed VRAM image is 524,288
bytes. Its nonzero words occupy 46,961 words in 2,525 runs: 109,072 bytes if
stored as raw nonzero runs with six-byte address/length descriptors. Converting
that captured image to the existing four-plane, 38-word-per-plane interleaved
layout gives 524,400 allocated bytes, including padding. There are 42,607
nonzero words in 5,292 runs, or 116,966 bytes with the same descriptors.
These are estimates for a prepared payload, not a chosen serialization or a
measured native load/copy cost. A raw exact recipe adds 47,776 bytes; progress
metadata, validation descriptors and any decoding code would be additional.
No artwork, recipe words or captured state is committed.

## Constraints for the proposal

Recognition must compare the complete incoming recipe and entry context.
WPR, pattern writes, moves, status and IRQ semantics still execute in order.
Each skipped drawing command needs exact current-position, RWP, drawing-work
and error/area effects, proved against the ordinary renderer. A final bitmap
alone cannot supply those intermediate states.

The default renderer begins with a cleared, owned VRAM allocation. Prove that
initial state and the complete expected register/memory-width context before
admitting the cache. Any different entry state, parameter word, unexpected
read, abort or observation must materialize the exact matched prefix and use
the ordinary renderer. Do not expose future pixels from the completed boot
image as if they were an earlier command's result. CCR interrupt-enable writes
must not invalidate an otherwise valid pixel sequence, but their semantics
remain live.

At completion, write the prepared image into authoritative planar VRAM, using
the existing allocation; a display-only shortcut would leave later copies of
fonts, ranks, suits and pictures wrong. Partial-recognition/read tests and full
ECS/AGA state equality are mandatory. Installation should follow the existing
card-cache preparation approach: generated ignored data from verified local
ROMs and the shared renderer, exact content/context validation, no runtime ROM
hash and no second copy of the whole framebuffer.

This only removes drawing work. All 23,888 incoming words, IRQs and original
initialization/accounting still run. T8c's measured drawing time is a useful
upper bound for the scoped saving, not a promise to remove all warm startup.
Use the current T9 trace and an isolated installation benchmark to quantify a
realistic saving before the go/no-go.

Local evidence: `tmp/t12-boot-study/{cold,warm-zero,warm-credits}.catalog`,
matching event/RAM captures, `first-main-*`, `inventory.json`; headless logs
`/tmp/pokeri-t12-*.log`. `--auto-setup` intentionally yields to an explicit
input file, so coin fixtures must resume an already initialized snapshot or
provide their entire cabinet setup. Snapshot restores must use the same
`--skip-hardware-tests` policy as their source.
