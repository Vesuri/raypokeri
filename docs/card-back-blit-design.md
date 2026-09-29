# Pre-rendered card back and one-blit command replacement

Status: **implemented and enabled by default**, approved by the user on 2026-09-27.
Exact-rendering gates pass. The user explicitly approved enabling the measured
improvement while retaining the unmet 20 ms card/admission and audio targets. The target is the complete red
lattice-and-club face-down card shown by the user. This supplements
[native-burst-plan.md](native-burst-plan.md); it does not declare its latency
or audio acceptance gates passed.

## Result to deliver

Prepare the verified card-back recipe at build time and load its proved image,
mask and progress data into Chip RAM at startup (see
[card-cache-preparation.md](card-cache-preparation.md)). Recognize subsequent equivalent
HD63484 command sequences and replace their raster work with **one queued
hardware blit for all four planes**. Keep the original 68008 instructions,
FIFO byte protocol, drawing-register results and observable memory effects.
A cache miss uses the authoritative renderer. It must not turn an unknown
command into success.

The one-blit target includes writing the authoritative emulated VRAM. Drawing
only into the final display buffer would leave RD, copies and later composition
with stale pixels and is not an acceptable shortcut. Moving-window composition
remains separate; the card is not recreated merely because its window moves.

## Evidence and boundaries

**MEASURED:** the isolated traced recipe reconstructs the requested artwork:
79 commands / 260 words, consisting of 10 WPR, 11 AMOVE, 21 RMOVE, 17 RFRCT,
four CRCL, four ELPS, three RPLL and nine PAINT. Its conservative image box is
88 by 100 logical pixels. Twenty-six matching mnemonic sequences occur in the
captured hand. Matching mnemonics alone is not enough to admit a cache hit.
The actual words, drawing context and address bounds must match as below.
The native display window includes eight leading pixels and spans 96 pixels;
that window geometry is not the card's coverage mask.

**MEASURED (code):** display buffers are row-interleaved, but authoritative
VRAM is four separate 128 KB planes. AmigaSurface::blitPlanes consequently
submits four blits. A cached source bitmap alone cannot make that destination
layout support one ordinary rectangular blit across all four planes.

**DERIVED (hardware manual):** A/B shifts continue across blitter rows; the
carry does not reset at each row. The source padding below prevents one plane
row leaking into the next. See the local ADCD hardware manual, chapter 6,
section on shifting and masking (`HARD_6`). This is an OCS-compatible design;
it requires neither AGA fetches nor enhanced blitter size registers.

## 1. Generate a recipe, not a replacement game routine

Add a reproducible host catalog tool. It runs the original ROM using the host
harness and a checked deterministic setup/deal scenario, captures complete FIFO
words, and locates the known card-back command shape. It must obtain the recipe
from a fresh run, not depend on an existing ignored trace. Confirm it by rendering
that sequence alone and by observing it at several translated destinations.
Extract exact opcode/parameter constraints and identify which AMOVE coordinates
translate with the initial card anchor. Relative coordinates and all other
parameters must agree exactly; do not wildcard entire words or trust a hash.

The output is a local generated descriptor under `amiga/generated/`, containing:

- the complete canonical command words and command boundaries;
- coordinate-translation annotations, derived and checked against the samples;
- required entry-state fields and geometry restrictions;
- provenance/version information for regeneration.

Verify ROM identities in the build tool, as the existing waveform generator
does. Do not add runtime SHA calculation. A changed ROM or an unrecognized
recipe disables generation with a useful build error rather than silently
importing another card. The game code is never disassembled into C or executed
by Musashi on the Amiga. Only the existing device renderer interprets this
HD63484 data. The default build now prepares the cache on the host; see
[build-time preparation](card-cache-preparation.md). No recipe bytes, pixels, masks, screenshots or
ROM-derived generated headers are committed. Commit the generator and synthetic
tests; build dependencies include the ROMs, renderer, generator and scenario.

An implementation investigation must establish the reproducible extraction
boundary. No currently unidentified producer entry point is assumed here.
Any newly identified code entry points go into the normal symbol/entry tables.

## 2. Prepare the bitmap before entering the guest

`CardBackCache::prepare()` performs the following proof once during the build.
The native loader installs its exact result before live board clocks and audio
start; `CARD_PREPARED=0` retains the original runtime preparation described below:

1. Allocate the permanent Chip RAM image/mask storage and bounded temporary
   scratch storage. No cache allocation or rasterization occurs on a hit.
2. Feed the canonical recipe through the existing HD63484 renderer attached
   to a small scratch Surface. Optional accelerated Surface methods decline,
   so the reference primitive semantics determine the result.
3. Record actual written-pixel coverage, including writes whose colour equals
   the initial background. Never infer transparency by testing for black or
   comparing the final image to a cleared buffer. Rounded-corner holes remain
   uncovered even if their current background happens to be white.
4. Track reads used for drawing decisions. Every such pixel must have been
   defined by the earlier recipe, or have an explicit guarded input dependency.
   The user approved a guarded exception on 2026-09-27: the four corner
   PAINTs may reuse the cache only when all 68 previously undefined corner
   pixels satisfy the measured fill predicate (neither colour 1 nor 15).
   Otherwise use the ordinary renderer. Other unverified dependencies reject
   the candidate.
   Masked preservation of untouched bits is distinct from a decision that
   depends on their value. A scratch bounds failure disables the cache.
5. Record semantic results after every command: changed parameters, CP/DP,
   flags, drawing work and failure state. Verify translation rules for these
   results; do not assume that final CP is simply the image corner. PAINT's
   last visited position is particularly relevant.
6. Pack the image and coverage into the Chip bitmap, release temporary scratch
   storage, and leave the real board state and its command counters untouched.

Background independence is an acceptance condition to prove, not a conclusion
from a few visually matching samples. Supplement the read-dependency check
with differential tests on every solid colour and randomized backgrounds.
The initial supported case is the exact opaque replace recipe; XOR, transparent
colour modes, alternative patterns or unverified masks use the normal renderer.

Startup construction uses a separate small video context and bounded canvas;
never allocate another full emulated VRAM or CPU image. Record preparation time
and the peak allocation. Failure to allocate this optional cache keeps the game
on the uncached path; renderer errors remain real errors, not success.

## 3. Interleave authoritative VRAM for the single-blit destination

Introduce an Amiga backing layout with four consecutive plane rows per physical
608-pixel row. The guest still sees the same 512 KB packed-word address space.
There is no chunky shadow, per-frame conversion or second authoritative image.
Keep the packed host reference and existing separate-plane backend as oracles.

For guest word address `a` after the installed-memory mask:

```
q = a >> 2                 // 16-pixel group in one plane
row = q / 38
column = q % 38
native_word(a, plane) = row * 152 + plane * 38 + column
bit = 15 - ((a & 3) * 4 + pixel_within_guest_word)
```

Each plane row is 38 words / 76 bytes; all four together are 304 bytes.
Allocate 1,725 such rows: **524,400 bytes**, only 112 bytes more than the current
512 KB allocation. The partial last row is padding, never additional guest
memory. Apply guest wrapping before mapping. This layout remains a bijection
for every valid word and all four planes, including accesses outside the
visible screens. Other HD memory widths still work through mapped accessors;
only rectangles that match this physical row geometry take the one-blit path.

**MEASURED (design arithmetic check, not native validation):** all 262,144
native words map to distinct addresses within that allocation.

Update the storage boundary consistently: packed read/write, pixel access,
CPU plane leases, PAINT, line/curve masks, fills, copies, rotated copies,
pattern tiles, screen composition and diagnostic capture. No code may keep
assuming `plane + planeWords`. In particular, changing just `attach()` is unsafe.
Expose prepared row pointers/strides to hot drawing loops; resolve row/column
once per row or segment, not by division at every plotted pixel. General word
access can use the existing native DIVU word helper; no 32-bit software divide
or multiply is permitted. Split/fallback safely at physical row and memory wrap.

Change raw capture decoding to describe this layout and its padding, then
compare the canonical guest VRAM bytes. Preserve the existing portable snapshot
wire format. Diagnostic scripts must not dump the first 512 KB of the new
allocation and interpret it as four separate planes.

Land and validate this storage change separately before enabling recognition.
Benchmark ordinary non-card operations too; a fast card must not hide a broad
regression from expensive address mapping.

## 4. One masked blit, all four planes

Permanent source layout, top display row first:

```
image: y0/p0, y0/p1, y0/p2, y0/p3, y1/p0, ...
mask:  y0/m,  y0/m,  y0/m,  y0/m,  y1/m,  ...
```

Each source plane row has seven words (14 bytes): 88 image bits followed by
24 zero bits. The coverage mask has the same zero tail. Duplicate each row's
mask for the four planes so A and B advance together without interrupt-time
pointer changes. Each bitmap is 5,600 bytes; image plus mask is **11,200 bytes**,
plus allocation alignment. No sixteen-alignment bitmap bank is needed.

For destination bit offset `s` in 0..15:

- width in words: `n = ceil((88 + s) / 16)`, either 6 or 7;
- height: `100 * 4 = 400` blitter rows;
- A = coverage, B = image, C/D = destination VRAM;
- minterm `$CA`: `D = (A & B) | (~A & C)`;
- A and B right shift by `s`; first/last A masks are `$FFFF`;
- A/B modulo: `14 - 2*n`; C/D modulo: `76 - 2*n`;
- submit one register block, one BLTSIZE write, one completion event.

For `s <= 8`, six source words are fetched; the last word has at least eight
low zero bits, enough to clear carry into the next plane row. For `s > 8`,
seven words are fetched and the last is all zero. Both A and B satisfy this
rule. All bytes outside the generated coverage remain unchanged through C.

**MEASURED (design simulation):** the zero-tail rule matches independent bit
shifts at all sixteen alignments through 400 consecutive rows. Actual OCS/ECS
blitter differential tests are still required; the simulation is not acceptance.

Admit only an in-bounds, non-wrapping rectangle whose complete transfer words
fit the 608-pixel backing row and allocation. Logical coordinates must not wrap.
The real renderer handles unsupported placements. Source cache storage is
immutable and disjoint from destination. Queue this job behind earlier drawing;
later CPU reads/writes use the existing synchronization rule. Cache storage
cannot be freed while its DMA is pending. Composition reads are ordered behind
it. Do not draw only to the visible front buffer.

## 5. Recognize complete command semantics incrementally

Place recognition at the shared complete-command boundary, before raster
execution. Keep the byte/word assembly and partial-FIFO state authoritative.
The initial cache admits only the generated exact recipe and validated entry
context: depth, memory width, origin/address restrictions, masks, pattern bits,
colour/operation modes and all other fields found by dependency validation.
Use full comparisons, not only opcode groups, PC addresses or a checksum.

The guest still executes and feeds every command. This optimization does not
skip the original command-producing routine or replace game decisions.
The existing verified feed-loop hook and nominal cycle accounting remain.

The matcher has three states:

| State | Behavior |
|---|---|
| Idle | Execute normally, or begin an eligible candidate at the exact recipe prefix. |
| Matching | Verify each command; apply its proven semantic effects, retain its raster work as a bounded pending prefix. |
| Materialize | On a full match enqueue the one blit; otherwise replay only the already accepted prefix through the normal rasterizer before proceeding. |

Candidates require a healthy device with no pending fault or earlier candidate.
Keep a fixed 260-word buffer and bounded entry-state snapshot. The first AMOVE
establishes translation; earlier WPRs do not draw and must still have their real
effects. Validate translated coordinates without overflow before any affected
command is suppressed. Once translated, every AMOVE and unchanged relative
parameter must match. Preserve every command's counters and logging exactly
once, as well as current position, drawing pointer, status and work accounting.
Use small generated semantic deltas rather than copying a full register array
for every word. On the final command, queue the stamp and return to Idle.

This is **not** recognition after drawing 78 commands normally; that would
retain most of the cost. It is also **not** drawing the complete card on its
first command, which would expose future pixels before the ROM submits them.
The matched prefix has logically completed only its own commands.

### Observations, interrupts and mismatches

A pending prefix must be materialized before any observer needs its pixels:
VRAM reads, copying from or modifying its destination, actual display composition,
forced frame capture, snapshots, reset/abort, teardown or a mismatching command.
Ordinary live periodic composition is deferred while a sequence is incoming, as
authorized by the user on 2026-09-29; the timer itself is not a pixel observer.
Initially use a conservative global pixel-observation barrier; optimize ranges
only after tests justify it. Parameter reads may use the exact updated semantic
state, but can conservatively flush in the first implementation.

A FIFO ready/error status read does **not** automatically demand pixels. For
an individually verified prefix command, return its exact current model flags,
including per-command CED/ARD/CER results and partial-input state. An unexpected
opcode or invalid parameter must first materialize the valid prefix, then run
the original checked path and report its real error. Do not report a generic
“ready/no error” result. This cache does not implement a guessed asynchronous
FIFO or change the host reference's timing model.

Amiga VBI/audio interrupts continue normally. An original interrupt or another
command producer can interrupt recognition; an unrelated command breaks the
match and materializes the prefix. Never execute guest code from an Amiga ISR.
Physical presentation in the ISR only retires already prepared buffers; any
new composition in main-thread service takes the observation barrier. The live
periodic request is latched until recognition is idle and a display buffer is
available. This includes the opening register/move commands before any pixels
are deferred. Completion or failed recognition releases the request; repeated
timer expirations coalesce into one refresh. It does not stop guest execution,
interrupts or audio, or suppress a game pixel read/copy. Shuffle queue frames,
forced captures and diagnostic replay keep their existing observation rules.

Fallback must not duplicate protocol effects, statistics or logs. Use the same
raster algorithms with a scratch semantic context initialized from the saved
entry state, attached to the real Surface. Replay exactly the accepted commands
for their pixels, without re-consuming FIFO words or publishing their register
changes twice. The live state already represents that prefix. Keep a recursion
bypass while materializing and preserve any incomplete input word/command.

Snapshots/captures force materialization, keeping their existing canonical
state format. Diagnostic replay runs the same recognizer and observation rules;
it is not silently exempted from correctness checks. A trace that observes a
partial card will legitimately take a prefix fallback rather than one final blit.

Thus **one blit is the normal complete-match path**, not a promise to combine a
sequence across arbitrary observable intermediate states. Measure barrier and
mismatch rates. If normal dealing consistently interrupts the candidate, stop
and revise the design; do not disable observations to inflate hit counts.

## 6. CPU cost and audio deadlines

One blit removes raster work, not the ROM's 260 FIFO-word writes. Measure
recognition/feeding, semantic updates, queue submission and DMA completion
separately. The matcher must not introduce another heavyweight general dispatch
per word. A lightweight branch in the existing endpoint can select the fixed
matcher, while retaining the same byte-phase and checked state semantics.

Neither K nor the three-frame credit policy changes in this implementation.
The sound sequencer remains original code. The purpose of reducing the long
synchronous card workload is to let that code run sooner; moving only Paula
updates to another interrupt would not manufacture the missing sequencer writes.

Record maximum host-wall gaps between expected AY updates during shuffle,
landing, reveal and double-up, along with envelope advancement. Correlate the
actual same sound sequence/record, not arbitrary differing hands. Use optional
CIA-ledger timestamps outside ordinary builds, never ReadEClock per access.
Also measure guest system-tick lateness and the longest continuous service.
Measure these gaps against real PAL elapsed time and the matched effect’s
reference duration, anchored at its start; measuring only board-cycle deltas
would conceal exactly the stretched-sound problem. A cache hit is not accepted
merely because the final AY register hash matches.

Targets to measure, not established results: complete common cached-card work
within one 20 ms PAL frame on A1200, including command admission; eliminate
card-induced sound-update delays above one PAL frame relative to the sound's
scheduled board deadline. If feeding alone exceeds that budget, report it and
optimize the checked admission path instead of claiming the blit solved timing.
Other drawing can still delay sound; this change is not a general scheduling fix.

## 7. Implementation sequence and correctness gates

1. **Catalog, proof harness and observational matcher.** Fresh ROM execution,
   exact recipe extraction, dependency/coverage checks and per-command semantic
   deltas. First run a measurement-only matcher on the existing native backend:
   leave every draw unchanged and count full sequences, mismatches and actual
   pixel-observation boundaries. This determines whether the common sequence
   survives intact before committing to the layout migration. Measure matching
   overhead and distinguish a no-op presentation call from a real VRAM read.
   If mandatory observations systematically split it, resolve that design issue
   before stage 2. Synthetic tests are committed; ROM fixtures remain ignored.
2. **Interleaved backing storage.** All mapped operations, capture tools and
   hot CPU loops updated. Keep the card cache disabled. Run exhaustive mapping,
   existing primitive tests, ECS/AGA replay and non-card performance comparisons.
3. **Startup cache and native blit.** Implement the bounded scratch Surface,
   one-time generation, allocation lifecycle and actual one-job four-plane copy.
4. **Matcher and barriers.** Enable opt-in with counters for starts, full hits,
   mismatch stage, barrier reason, prefix replays, generated bytes and blit jobs.
   Add a same-executable disable switch for controlled comparisons.
5. **Acceptance and default.** Enable only after the tests and live measurements
   below pass. Keep the uncached path available for unsupported inputs and
   diagnostic comparison. Update the current plans with measured results.

Required differential coverage:

- Packed oracle, separate-plane and row-interleaved storage: all valid addresses,
  memory wrap, every depth supported by the model, direct reads/writes and all
  existing graphics primitives. Verify padding is neither exposed nor aliased.
- Cache versus scalar renderer: all sixteen X alignments, multiple translations,
  every solid background plus random/checker backgrounds, corner preservation,
  final complete state and command logs. Test physical row/allocation edges.
- Mutate each recipe opcode/parameter and each required entry-state field.
  Every mismatch must give the exact fallback result, including failures.
- At every one of the 79 command boundaries, insert status/parameter/VRAM reads,
  copying, composition, reset/abort, snapshot, interrupt-driven unrelated writes
  and end-of-run. Include truncated words, partial byte phases and early errors.
- Compare semantic state after every prefix, not only after the whole card.
  Validate last-PAINT CP/DP, work counters, command counters and all flags.
- Execute the real queued masked blit on ECS and AGA, all alignments and background
  patterns. Assert exactly one BLTSIZE submission per full hit, with no per-plane
  completion chaining. Test consecutive hits and ordering with earlier/later DMA.
- Existing host harness/platform/native and relevant assembly-oracle suites;
  68000 arithmetic audit; full native replay equality on A500+/ECS and A1200/AGA
  in RAM, canonical VRAM, cropped pixels and AY writes.
- Fresh-state live24 plus repeated shuffle/reveal/double scenarios. Report hit
  rates, startup overhead, Chip/Fast memory, maximum sound-update lateness,
  card latency and ordinary non-warp game time. Debug audio remains muted;
  a normal run keeps audio enabled for an explicit listening check.
- Restore the ordinary build after measurement and prove profiling code is absent.

Do not silently reinterpret a failed observation test as permission to batch
past it. Any need to change guest-visible timing, status or snapshot boundaries
is a separate architectural decision. This design proposes result reuse and a
storage-layout change; it does not authorize replacing the original game logic.

## Implementation evidence — stage 1 (2026-09-27)

**MEASURED:** `tools/card_back.py` verifies the four local ROM hashes and runs
47.5 board-seconds from fresh NVRAM using `host/scenarios/play.inputs`. The new
optional `--video-catalog` records complete words without the human-readable
log's truncation. It extracts 11 exactly equal translated recipes at eight
anchors, captures their initial contexts/backgrounds, and emits ignored local
headers. No producer entrypoint or ROM routine replacement is assumed.

**MEASURED:** the scalar proof finds 8,652 written pixels in the 88×100 box.
Four corner PAINTs each make 29 previously undefined reads: 116 reads at 68
unique pixels. Solid colours 1 and 15 stop those fills; therefore an unconditional
cached card is incorrect. The user approved the destination guard above.
`host/card_back_proof.cpp` checks all 16 solid colours, each dependency pixel
individually with all 16 colours, 64 random guarded backgrounds, and all actual
captured backgrounds: **1,039 guarded equal cases / 140 rejected cases**.
Nine of the 11 actual backgrounds qualify; the two startup samples do not.
Equality includes written coverage, final pixels and all drawing parameters.
Work counters/prefix state and actual hardware blits still require later gates.

**MEASURED:** the diagnostic-only `CARD_OBSERVER=1` build leaves all drawing
unchanged and calls the exact translated-word observer after every completed
command. Only visible, nonempty composition regions count as pixel observations;
no-op `present()` calls do not. Fresh native A1200 live24 completed at 480M
cycles, all 24 inputs, zero resets/errors. It found **17 complete sequences,
12 with no intervening composition observation, five interrupted**. There were
44 prefix starts; 27 failed at stages 1, 4 or 29. Thus the conservative observation
barrier does not systematically prevent complete sequences. These are upper
bounds on hits: context/background admission is not yet applied natively.
The observation build's game phase was 55.74 PAL seconds (frames 2318→5105),
versus the earlier ordinary 55.54-second run; hands differ, so this is not an
isolated overhead benchmark. Local evidence: `amiga/.run/card-observer-live`,
`tmp/card-back-proof.log`, `tmp/card-back-catalog/`.

Synthetic catalog/observer tests pass via `make harness-card-check`. Next is
stage 2 storage migration with recognition disabled, followed by startup cache,
barriers and real queued-blit/equality/latency gates. No speedup is claimed yet.

### Stage 2 — interleaved storage (2026-09-27)

**MEASURED:** opt-in `VIDEO_INTERLEAVED=1` maps authoritative VRAM into
38-word plane rows. The installed 512 KB uses 262,200 words / 524,400 bytes;
padding is not guest memory. Host oracles cover the mapping bijection and
padding, every packed word, pixel ROPs, line octants, rotated copies, short
fills, display alignment/wrap, and the existing HD primitive suite in both
separate and interleaved layouts (including the model's 2 MB address case).
The general mapper uses native DIVU with cached neighbouring rows; it emits
no 32-bit software division. Raw capture tools now require layout metadata.

**MEASURED:** the first native implementation regressed live24 to 113.60 PAL
seconds for approximately 48 game-seconds. The ledger attributed 19.35 seconds
of the deal phase to 52 group-59 copies: physical row seams had sent large
copies into per-pixel fallback. Logical screen start addresses also need not
be multiples of 152 words. Fills, copies and composition now split at physical
seams into wide vertical strips, preserving blitter execution. Prefetched tail
bits may cross a plane-row boundary only where the destination mask discards
them; all source fetches remain inside the allocation.

**MEASURED:** the corrected uncached live24 takes **56.18 PAL seconds / 48.05
board-seconds** (frames 2357→5166), all 24 inputs, zero watchdog resets/errors,
zero rejected copy bounds. The previous ordinary baseline was 55.54 seconds;
hands vary, so this establishes no broad regression rather than an isolated
per-command improvement. Cache admission and raster replacement are still off.

**MEASURED:** actual Agnus tests pass on A1200/AGA and A500+/ECS, including
new physical-seam copies/composition across all sixteen source/destination
alignments and all copy ROPs. Both replay comparisons match all 262,144 RAM
bytes, 524,288 canonical VRAM bytes, 163,008 cropped pixels and 30 AY writes
at 7,008,979 instructions / 64,000,002 cycles / 7,831 IRQs. Host suites and
the native arithmetic audit pass. Local evidence: `tmp/card-seams-*-compare.log`,
`amiga/.run/card-seams-{aga,ecs,live}`, and `amiga/.run/card-layout-ledger`.
The layout remains opt-in until the later cache and latency gates.

### Stages 3–5 — guarded cache, measured default (2026-09-27)

**MEASURED:** startup renders the local descriptor on a bounded 88×100 scratch
Surface and records scalar and accelerated-rectangle prefix semantics. The
immutable Chip image/mask occupies 11,200 bytes; the native cache object is
6,078 bytes, excluding renderer container allocations. The temporary pixel and
coverage arrays total 9,900 bytes. Native measured preparation is 1,961,883
E-clock ticks (2.77 seconds); exact peak heap allocation is not yet measured.
There is no packed VRAM clone, runtime hash, Musashi, or allocation on a hit.

**DERIVED / MEASURED:** CCR low writes at each feed-batch boundary only change
interrupt enables. An initially over-conservative barrier caused every card to
fall back at stage 13. They now retain their ordinary register effects without
flushing pixels. Status and those writes are tested at every prefix; all actual
pixel observations, drawing-context changes, aborts, mismatches and teardown
materialize the accepted prefix. Host snapshots retain their canonical format.

**MEASURED:** `make harness-card-cache-check` passes 1,760 differential cases /
679 hits: both planar layouts, every alignment, solid/random backgrounds,
translation/bounds, every recipe word mutation, required-context mutations,
every prefix observation and snapshot, complete logs and semantic work state.
Accelerated rectangle prefixes have an independent packed fill oracle.
The normal harness/platform/native suites and native arithmetic audit pass.

**MEASURED:** ECS and AGA execute 80 synthetic masked stamps across all sixteen
alignments, holes/backgrounds and queued work; every stamp submits exactly one
BLTSIZE. Both original-ROM replays then make two real cache hits and match all
262,144 RAM bytes, 524,288 canonical VRAM bytes, 163,008 cropped pixels and 30
AY writes at 7,008,979 instructions / 64,000,002 cycles / 7,831 interrupts.
Evidence: `amiga/.run/card-correct-{aga,ecs}`, `tmp/card-correct-*-compare.log`.

**MEASURED:** ordinary A1200 live24 completes all inputs, no watchdog reset or
error, with 9 full-card blits from 44 candidate starts. Nine candidates reject
the corner guard; six accepted prefixes are materialized (four actual pixel
observations, two recipe mismatches). Gameplay takes 55.46 PAL seconds for
48.01 board-seconds; differing hands prevent attributing the whole-session
change solely to caching. Local evidence: `amiga/.run/card-cache-hits`.

**MEASURED:** paired same-executable CIA-ledger runs record complete card feeds
from the first opcode word through the final command. Subtracting calibrated
timestamp-reader cost, seven full hits average 136.13 ms (123.60–154.60 ms).
The uncached run's 17 complete cards average 223.07 ms (195.56–271.47 ms).
The observer changes scheduling and hit incidence (7 rather than 9); these are
instrumented workload results, not a claim of identical hands or unprofiled
per-card latency. Actual isolated blits with AGA hires DMA take 4.00–5.57 ms,
including submission and completion wait, across all alignments.

**MEASURED:** AY write-to-Paula application stays within 18.54 ms in these runs.
That does not mean sounds have their correct duration: matched eight-or-more
register/value sequences still show hundreds of milliseconds of stretched
sound gaps around drawing, and envelope-only application gaps reach 536 ms.
The original guest cannot issue its next sound change while command servicing
holds it up. Startup/service gaps must not be compared as gameplay effects.
The 20 ms complete-card and one-frame sound-stretch gates remain **unmet**.
No timing contract, future sound stop, or guest routine replacement was added.

The user explicitly approved default activation with those performance gates
remaining open. `CARD_CACHE=0` restores the uncached separate-plane build;
`native-no-card-cache` disables cache use in the same executable. In a timing
build that marker retains preparation/observation for paired measurements.
`TIME_LEDGER=1` alone includes the card/audio event journal. Ordinary builds
contain neither the event journal nor its timestamps. `host/card_timing.py`
reads local event dumps, corrects measured timestamp overhead, and correlates
exact AY write runs with cycle-stamped host logs. Evidence:
`amiga/.run/card-time2-{on,off}`, `tmp/card-time2-*-report.txt`,
`amiga/.run/card-dma-bench`. Physical listening and uninstrumented non-warp
latency remain to be checked; this is an accepted partial performance gain.


### Shared white-card prefix (2026-09-27)

The face-up follow-up uses the same guarded cache. Original-code enumeration
of all 60 suit/rank selector pairs proves that each begins with the first
29 commands / 69 FIFO words of the card back: six WPR, four AMOVE, eight RMOVE,
three RFRCT, four CRCL and four PAINT. Forty ordinary cards
then blit the striped/black inset; twelve picture cards blit complete picture
insets instead. The remaining selectors are special-image/blank-card cases.
The inset was already cached offscreen by the original program and copied by
AGCPY; it is not procedurally redrawn for each card. Four suit/rank copies
precede it, so drawing the inset early would change observable prefixes.
See `rom-set.md`, “Face-up card common background”.

Startup now additionally proves that command 29 has exactly the final card's
coverage and that every covered pixel is colour 15. Both scalar and rectangle
semantics must agree. The existing four-plane coverage mask is therefore also
the opaque white image: **no additional resident bitmap or Chip RAM**. An extra
1,100-byte coverage checkpoint is temporary startup scratch. If this proof
fails, the white-prefix optimization stays disabled; ordinary rendering remains.

On an observation or mismatch after at least 29 matched commands, one guarded
blit materializes the white prefix. WPR/MOVE and proven CP/work results restore
the shadow prefix state, then any already-matched later commands are replayed
normally. Shorter prefixes retain the old full replay. A complete 79-command
back still waits for its full one-blit stamp, without a preceding white blit.
The 68-pixel eligibility guard, exact word/context checks, ordered source reads
and all observation barriers are unchanged. Inset/rank/suit blits stay in their
original order. This caches the genuinely common part of every face-up card;
it does not stamp the pictured striped template onto picture/special cards.

**MEASURED (host):** `make harness-face-up-check` executes all 60 original
producer calls without translating game code and compares their complete
outputs against the packed renderer in both planar layouts. Together with
all alignments/backgrounds, every recipe-word mutation, every prefix barrier,
snapshots and accelerated-work comparisons: **2,456 cases, 679 back hits and
1,241 white-prefix hits**, with exact pixels, state, work and command logs.
**MEASURED (isolated A1200, active AGA hires DMA):** three paired white-prefix
feeds plus flush/DMA completion total 77,964 E-clock ticks uncached and 29,649
cached at 709,379 Hz: **36.63 → 13.93 ms per background (61.97% reduction)**.
Both modes use the same executable, prepared cache, coordinates, cleared
background and shared command feeder; only white-prefix reuse differs. These
figures exclude guest exception/feed-loop overhead and are not whole-card or
whole-game latency. The test makes three admitted white hits, no native error.
Evidence: `amiga/.run/white-cache-bench-aga/gdb-out.log`,
`nativeWhiteBenchTicks` in the explicit `native-benchmark` path.


**MEASURED (ordinary A1200 live24):** all 24 input transitions complete with
zero native error or watchdog reset, nine complete-back hits and nine shared
white-prefix hits. This run has 59 candidates and 49,368 fed words, versus
44 candidates / 44,459 words in the preceding width-only run. Its different
hand takes 59.28 PAL seconds for 48.05 board-seconds (frames 2,349–5,313);
the whole-session figures therefore do not isolate cache cost or establish
an overall speedup. The paired prefix benchmark above is the evidence for
retaining the optimization. No audio-duration improvement is claimed from
this muted debugger run. Evidence: `amiga/.run/white-cache-live/gdb-out.log`.


**MEASURED (ECS/A500+ counterpart):** the same three paired prefixes take
393,119 / 138,243 E-clock ticks, or 184.72 / 64.96 ms per background. All three
cached prefixes are admitted with no native error and restored vectors. This
checks the OCS-compatible blitter path; the current performance target remains
A1200. Evidence: `amiga/.run/white-cache-bench-ecs/gdb-out.log`.


**MEASURED (regression gates):** final ECS and AGA boot replay agree with the
host on all 262,144 RAM bytes, 524,288 canonical VRAM bytes, 172,064 visible
pixels and 30 AY writes at 7,008,979 instructions / 64,000,002 cycles / 7,831
IRQs. These boot replays exercise two complete backs but no white-prefix hits;
the full face-up cases are covered by original-producer host comparisons and
the native prefix benchmarks/live run above. The existing 80 synthetic native
masked-blit tests pass on each chipset. Host harness/platform/native suites and
the native arithmetic audit pass; ordinary output contains no ledger symbols.
Evidence: `tmp/white-cache-{aga,ecs}-compare.log`,
`tmp/white-cache-host-tests.log`, `tmp/face-up-tests.log`.


## Approved shuffle pacing experiment (2026-09-28)

The user separately approved VBlank waits at verified shuffle frame boundaries.
This does not change cached drawing results or enable comprehensive chip timing.
It is now enabled for normal SDL/bounded-clock Amiga play after watchdog-on
host and ECS/AGA live checks. Added waits are excluded from watchdog time by
explicit user approval; see [shuffle-pacing.md](shuffle-pacing.md).

## Periodic composition deferral (2026-09-29)

**Implemented by user request:** `CardBackCache::sequenceIncoming()` exposes the
existing matched-prefix state without materializing pixels. The native periodic
presentation request remains pending until that state clears and a buffer is
available. Reset clears the request. The optional fast IRQ path yields to a
pending request. Mandatory pixel observations and diagnostic replay are unchanged.

**MEASURED:** the muted A1200 cold live24 run checked all **2,557** ordinary
periodic composition calls: **zero** occurred with a matched prefix. **Six**
successful complete-card hits had a request waiting. The cache recorded zero
pixel-observation barriers (reason 4); three mismatch barriers remained. All
24 inputs, 30 shuffle steps and 60 shuffle AY writes completed, with zero watchdog
resets and an empty heap after static destructors. Breakpoint instrumentation
makes this a correctness check, not a performance measurement. Local evidence:
`amiga/.run/pending-composition-live-aga/`.

The cache regression suite checks every one of the 80 command boundaries,
including the initial non-raster command: inspecting recognition preserves the
full match and final blit. Existing mutation, pixel-observation, snapshot and
partial FIFO tests continue to exercise mandatory fallbacks.

**Validation:** `harness-card-cache-check`, `harness-prepared-card-check`,
`harness-check`, `harness-platform-check` and `harness-native-check` pass. The
normal Amiga build/audit passes. Both ECS and AGA diagnostic replays match all
262,144 RAM bytes, 524,288 VRAM bytes, 172,064 cropped pixels and 60 AY writes at
7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs; both restore vectors and
exit without errors. Local captures are under
`amiga/.run/pending-composition-replay-{ecs,aga}/` and
`tmp/pending-composition-{ecs,aga}-check.log`. The default executable is the tested
candidate, also saved locally as `tmp/perf/Pokeri-pending-composition`.
