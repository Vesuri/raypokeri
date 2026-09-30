# Boot artwork study (T12)

2026-09-30. Design study only; no boot-cache implementation or default change.
Pricing-menu variants are measured below; the complete proof/data layout and
native copy costs and proposed semantic proof are now recorded below.
Implementation remains behind the required go/no-go.

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


## Command-boundary payload study

**MEASURED:** `host/boot_artwork_study.cpp` replays all 71,997 captured ACRTC
bus accesses through the ordinary renderer, checks all 27,873 read results
(including two outside the command-completion interval above), and matches
all 524,288 final VRAM bytes. It converts changed words with the existing
PlanarSurface implementation and verifies every final packed/planar word.
Deliberately altered status reads and final VRAM bytes are rejected.

Of 5,576 commands, 1,124 change pixels. Across all command boundaries there
are 76,685 changed packed words in 16,905 contiguous runs. A simple encoding
(two bytes per changed word, six per run, four per command offset including
an end sentinel) occupies 277,108 bytes. In native interleaved planar order,
there are 81,461 changed words in 31,892 runs: **376,582 bytes** with that
encoding. Add 47,776 recipe bytes and still-unmeasured semantic progress
records. This is a sizing experiment, not an adopted cache format.

**DERIVED:** per-command deltas would make each completed command's pixels
immediately authoritative, avoiding deferred prefix reconstruction. Their
fragmentation costs much more storage and installation work than the 116,966
byte final-image estimate. Neither scheme permits installing future artwork
early. A final-image scheme must instead prove and materialize intermediate
state at every observation/fallback, and account for that machinery's cost.
The trade-off remains open pending the native copy benchmark and full proof.

The ordinary CLR command changes no bytes because the initial allocation is
already zero. Rectangles account for 50,413 packed changed words, PTN tiles
10,661, copies 6,566; the remaining line/curve/paint commands account for the
rest. These are changed-word counts, not drawing cost or physical chip timing.

Reproduce after capturing an ordinary first-main trace:

```
make build/boot-artwork-study
build/boot-artwork-study tmp/t12-boot-study/first-main-trace.csv tmp/t12-boot-study/first-main-vram.bin
```

The tool is host-only and embeds no original artwork or command words. Local
results: `tmp/t12-boot-study/progress.txt`. All generated/captured data remains
ignored. Settings variants and realistic native costs are still required
before the implementation decision.


## Retained pricing variants

**MEASURED:** physical service inputs reach TESTI 13, PELIN HINNOITTELU.
Deal advances the selected parameter; with KOLIKON 1 ARVO selected, Hold 5
(PIA1 PA6 falling) changes its displayed value from 1 to 2 and Hold 4
(PA7 falling) changes it from 1 to 11. These are separate experiments from
the same snapshot, using original menu code and no internal-state edits.
The actual changed accounting blocks were wrapped in the existing persistence
format. Both values survive a fresh CPU boot; the changed byte appears at
$43FA3 and in the observed accounting copies at $440AB/$441AB. NVRAM at
$D0000 does not change in these experiments.

At the same first-main boundary as the baseline, both variants produce exactly
5,576 commands / 23,888 words and all 524,288 VRAM bytes match. This extends
the sampled invariance evidence to retained pricing changes; it is not proof
for every setting. The proposed exact recipe/context guard and fallback remain
mandatory for other settings.

Do not compare entire fixed-duration catalogs as though all their commands
were boot artwork: subsequent operator/menu/accounting work differs. In the
8-second captures the changed-price fixtures have 5,626 commands while the
cold catalog has 7,709. Their first divergence is command 5,577, after the
5,576-command common prefix. Explicit first-main breakpoints isolate the
identical artwork and prevent those later commands entering the proposed cache.

Evidence: `tmp/t12-boot-study/pricing{,-hold4,-hold5}.*`,
`setting-{hold4,hold5}{,-first}*`, and `settings.inputs`. The first-main harness
exit code 2 is the requested breakpoint, not a device failure. All generated
artifacts remain ignored. Further service settings can safely fall back; the
study still needs native installation costs and the semantic-state proof.


## Native copy cost and recommendation

**MEASURED:** the explicitly gated `BOOT_COPY_BENCHMARK=1 PROFILE_SUPPORT=1`
build decodes the real generated planar run layouts into the ordinary Chip
VRAM allocation. The generator independently reconstructs both layouts and
checks every word before exporting its ignored header. The native test starts
from cleared storage, checks bounds, copies words and verifies every final
planar word across both layouts and display modes. Clearing and verification
are outside the timed interval. It preserves neither recipe matching nor
semantic state because this is an isolated copy benchmark, not a boot cache.

Median of three trials, milliseconds:

| Machine / display DMA | Final-image runs | Command-delta runs |
|---|---:|---:|
| A1200, off | 62.064 | 261.439 |
| A1200, on | 63.248 | 262.831 |
| ECS A500+, off | 415.781 | 1,703.104 |
| ECS A500+, on | 766.906 | 2,633.894 |

Both runs return without error, restore vectors, and match 42,607 final-image
words / 5,292 runs or 81,461 delta words / 31,892 runs. The measurement clock
is 709,379 Hz. Display-on uses the existing synthetic composition test to
activate raster DMA; it is not a whole-boot run or a replica of each actual
startup display phase. Command offsets, per-command entry, semantic records,
matching, initialization and extra executable load time are not in these copy
figures. No OS calls occur inside the copy loop; clock reads bracket each job.

**MEASURED attribution:** the first original main-loop entry occurs in field
376 of the current T9 cold trace. Fields 0–376 cover 7.54 s, with at most one
field of boundary uncertainty. Inclusive `Hd63484::draw` accounts for 29.12%,
approximately 2.196 s. This includes ordinary pattern-register processing and
some work a prepared cache must retain. The interval contains 18,404 Line-A
entries and 4,067 virtual IRQs; those cannot simply be removed by a pixel cache.

**DERIVED ceiling:** subtracting the 0.263 s delta installation cost leaves
less than approximately 1.93 s available before matching/semantic/setup costs.
**INFERRED planning range:** roughly 1–1.8 s on A1200 is a plausible experiment
target, not an established speedup. It would principally help warm boot as
well as cold boot's shared artwork phase; the 100-coin refill remains.

Recommend benchmarking the command-delta design below if approximately
450 KB of additional Fast-memory/executable data is acceptable. It gives a
simpler fidelity proof than deferring the whole image. Do not implement the
smaller final-image shortcut without proving all intermediate observations.
No default cache is enabled by this study.

### Proposed guarded command-delta design

1. Generate an exact local recipe, command offsets and native planar deltas
   from the ordinary renderer. All outputs stay ignored and depend on ROM
   verification, renderer sources and the preparation generator. No runtime
   SHA or duplicate full VRAM allocation.
2. Admit only the known cleared initial VRAM and complete controller context.
   Match every original command word in order. Keep WPR, WPTN, ORG and moves
   on their existing semantic routes; skip only drawing that has a verified
   generated record. Keep all status/CCR accesses and interrupts unchanged.
3. Prove each drawing transition against both packed and accelerated planar
   renderers. Validate that only PR$0C/$0D, PR$10–$13, RWP, drawing-work count and
   area/stopped status change; reject generation if any other semantic field
   changes. Preserve ordinary finishCommand/status/count behavior. Work counts
   must match the selected native acceleration path, including diagnostic
   bounds; the existing card-cache scalar/rectangle distinction shows why
   one scalar work count is insufficient.
4. Apply that command's pixels to authoritative VRAM before exposing completion,
   then its verified semantic effect. A possible fixed record is 22 bytes
   (six parameter words, RWP, work count, status/stopped word), plus a separate
   record if an acceleration path differs. There are 1,166 candidate drawing
   commands; 1,124 change pixels. Recipe 47,776 + offsets 22,308 + delta runs
   354,274 + one 22-byte record per candidate 25,652 = **450,010 bytes**, before
   context/format headers and any extra work-count variants. This is a design
   estimate; the generator must prove and report the actual final layout.
5. Any mismatch, unsupported context or unexpected write ends admission before
   that command and executes it normally. Completed prefixes are already exact,
   so reads and failed matches need no deferred pixel reconstruction. A partial
   FIFO command has no new pixels, just as in the ordinary model. Suspend the
   card recognizer during this prefix; after rejection, restart it empty so
   suffixes use ordinary drawing without stale partial recognition.
6. Require host equality after **every** command, interruptions and every
   deliberately mismatched recipe boundary, not just final-image equality.
   Include read-FIFO/control reads, abort, wrapping, nonzero initial VRAM and
   unsupported drawing-work cases. Then exact ECS/AGA replay, cold/warm live24,
   accepted Double, native memory/cleanup checks and paired startup timing.
   Retain only a measured improvement; report executable/memory growth and
   failure-path costs. The existing renderer remains the fallback.

The implementation go/no-go is now the remaining T12 decision. These proof
steps are requirements for implementation, not claims that an unbuilt cache
has passed them.

### Reproduction and isolation

```
make build/boot-artwork-study
build/boot-artwork-study tmp/t12-boot-study/first-main-trace.csv tmp/t12-boot-study/first-main-vram.bin --native-benchmark-header
cd amiga
. ./env.sh
make clean
make -j8 PROFILE_SUPPORT=1 BOOT_COPY_BENCHMARK=1
```

Run with `native-benchmark` and `native-display`, using the read-only
`.run/t12-copy-{aga,ecs}/copy.gdb` probes. Generated
`amiga/generated/BootCopyStudy.h` contains local byte-derived data and must
never be committed. Frozen executable: `tmp/t12-copy-benchmark`.
The normal build was restored and all its allocated ELF sections match the
validated T9 executable exactly. Evidence: `.run/t12-copy-{aga,ecs}/gdb-out.log`,
`/tmp/pokeri-t12-boot-attribution.log`, and T9's
`reduced-boot-boundary/fields.tsv`. Clean before changing benchmark flags.
