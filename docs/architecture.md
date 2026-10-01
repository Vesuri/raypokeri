# RAY Pokeri architecture

Current implementation, 2026-10-01. Hardware evidence is in [rom-set.md](rom-set.md)
and [hardware.md](hardware.md); measured costs are in [performance.md](performance.md).

## Original program and shared board

The original 68008 instructions run natively on the Amiga. Relocation and checked
hardware-access hooks connect them to the portable C++ devices in `src/board/`.
There is no translation of the game into C and no Musashi in the Amiga executable.
The host harness uses Musashi as a CPU oracle and the same devices as the port.
A shared-model replay proves implementation equivalence, not physical accuracy:
a device bug shared by both platforms needs independent evidence to discover it.

| Original address | Model / evidence |
| --- | --- |
| $00000–$0FFFF | 77POK30, vectors and reset code — MEASURED |
| $10000–$1FFFF | 77POK38 — MEASURED chip order |
| $20000–$2FFFF | 77POK34 — MEASURED chip order |
| $30000–$3FFFF | PARA200J parameter/service module — MEASURED |
| $40000 onward | Work RAM, A6=$48B00 — MEASURED accesses |
| $D0000–$D7FFF | Battery RAM model; unused by covered gameplay — INFERRED hardware role |
| $F6000/$F6002 | HD63484 address/status and FIFO — DERIVED, strong |
| $FB002–$FB00B | Three ACIA register pairs — MEASURED |
| $FB014–$FB01F | Three PIAs; PIA0 includes AY strobes — MEASURED |

The address mask is 20 bits. Writes to ROM are ignored, including the reset
routine's ROM-versus-RAM test. The former DUART-at-$C0000 hypothesis is not part
of the observed processor-board path. The RAMDAC/$E0000 path belongs to the
unselected 2 MiB video configuration. Unknown accesses stop loudly.

## Native placement and hook contract

The Amiga Board allocation contains 256 KiB ROM, a 64 KiB guest RAM window
($40000–$4FFFF) and a non-addressable 4 KiB canary. A separate 64 KiB device guard
represents $F0000–$FFFFF. The host's ordinary board retains its larger research
RAM mapping; relocated-mode checks enforce the compact native windows.
ROM is loaded directly into aligned storage, with no duplicate image or blanket
pre-clear. Bases must be disjoint, even, within 24 bits, and ROM is 256-byte aligned.
Out-of-window fixups fail before takeover. The reset routine itself relocates
RAM after the checked hook at $2194 supplies its D7 delta.

`host/tables/` contains relocation/access descriptors and coverage metadata, not
ROM bytes. `tools/native_tables.py` verifies the user's ROM hashes and generates
ignored headers containing patch guards, hook descriptors and cycle metadata.
Normal native startup checks file sizes and every patched word; it does not
calculate SHA-256. The original module loader still copies and fixes up its own
initialized data. Only approved diagnostics, checksum branches and bounded hook
sequences are bypassed or fused. All original register/CCR results and established
interrupt-promotion boundaries are preserved.

Line-A encodes a hardware or privileged-instruction hook. Short assembly paths
handle common FIFO, status, PIA, ACIA, SR, stack and TRAP operations; checked
fallbacks use `Hook.cpp` and `Board`. FIFO-control triples, command-feed loops,
and the joined video handler avoid redundant exception transitions. The joined
handler retains the established promotion points; it is not an authorization to
combine arbitrary instructions or defer interrupts.

Original code runs in physical user mode with virtual supervisor state (SR/IPL,
SSP and USP). Amiga exception frames use a private service stack, never guest RAM.
Guest exception frames are six-byte 68000 frames on every CPU. Native frames
follow the detected CPU. A private Fast-RAM VBR is used where appropriate in
standalone mode; WHDLoad retains its own VBR. All vectors are restored on exit.

Live level-2/3/4/6 wrappers chain Exec and pause the guest clock. A trace-free
Line-A redirect after return from the interrupt supplies a safe guest boundary;
a software-requested level-2 interrupt can request service when necessary.
The VBI updates time, input and Paula; it never calls original game code. Original
interrupt handlers run at eligible guest boundaries with virtual IPL checks.
Diagnostic replay follows its recorded instruction/cycle/IRQ schedule and retains
its separate tracing requirements. WHDLoad replay requires NoVBRMove; normal play
does not. The blitter completion interrupt drains the graphics queue independently.

## Clock, startup, presentation and input

The approved bounded clock follows PAL time subject to earned guest throughput.
CIA-A timer A measures guest intervals outside device services. E-clock ticks
convert to 8 MHz reference cycles with 361/32 (about +0.034% rounding error).
Reference charges for hooked instructions are unscaled; measured guest time gets
K credit. Adjacent hooks earn no false guest interval. CPU probes with 12.5%
headroom can lower the requested K: 1.5 for boot, 4 after Ready. Credit is bounded
to one PAL frame during boot and three in play, with at most two 10 ms ticks
queued. This is a calibrated model policy, not a measurement of the original
68008 bus timing. Changing it requires a timing decision and renewed validation.

Normal boot skips the approved coin-op tests but executes initialization and
accounting. Until Ready, startup-only fast-forward removes PAL pacing, mutes
preparation sounds and bounds guest work. Cold cabinet setup acknowledges door,
status, refill and accounting progress: it adds 100 reserve coins, closes the door
and reaches zero player credits. There is no 40.5-second setup script. A warm
boot restores retained accounting and exchanges status without refilling.

Presentation requests come from completion of the original system tick, not an
independent presentation timer. Composition waits for graphics-drain and card
recognition boundaries; a partial match defers composition until success or
fallback. Tracking also protects failed cache admissions from partial-card
presentation. Consumer-paced shuffle markers let the original producer continue
sound scheduling while each visible shuffle step is published at its boundary.
There is no claim to reproduce the physical board's vsync relationship.

Button down/up transitions are queued until a PIA input-data read observes them;
DDR/control reads cannot acknowledge a key. Coin/door requests wait for the
serial link's application-idle boundary. Queue overflow is a logged fault.
Amiga controls are in the release ReadMe; SDL deliberately keeps its separate
host keyboard mapping. Delete/O/L are Amiga door/operator/lamp controls.

## Planar graphics

The HD63484 model owns command parsing, registers, FIFO and drawing semantics.
`AmigaSurface` supplies authoritative 512 KiB planar VRAM: four interleaved plane
rows per logical row. `AmigaScreen` composes into two display buffers with fixed
Copper lists. The viewport is 608×292; Amiga crops five top and four bottom rows
to 608×283, starting at PAL line $1D. Physical rows are padded to 640 pixels.
AGA fetches require chipset detection; ECS keeps its supported fetch mode.
COP1LC is published without COPJMP1 and buffer ownership retires on the next
frame, so a late interrupt cannot restart the Copper list midway down the screen.

| Drawing operation | Fast route and necessary fallback |
| --- | --- |
| Black/white clear | Interleaved constant blits; 292 rows need two chunks |
| Gray bars / coloured rectangles | Constant plane fills; small spans use CPU words |
| Face-down card | One masked four-plane cached blit after an exact command match |
| Face-up body | Shared white rounded-card prefix; details remain resident copies |
| Fonts and pattern tiles | Cached interleaved PTN blits; split at physical seams |
| Ranks, suits, J/Q/K/Joker | Original offscreen artwork, upright blitter copies |
| 180-degree copies | Bit-reversed planar words for non-overlap; scalar overlap fallback |
| Scrolling | Blitter copy of the resident strip |
| Curves | Cached outline row masks; patterned/unsupported cases plot points |
| Lines | Planar incremental Bresenham; axial solid lines become rectangles |
| PAINT | Word-parallel eligibility search and span fills; background dependence retained |

The card-back recipe has 79 commands / 260 FIFO words. A build-time generated
88×100 image plus coverage mask occupies 11,200 Chip bytes. Admission checks
command/context/geometry and 68 corner-background pixels. Proven eligible,
all-white and left-eligible/right-white cases use the cache; other predicates
fall back. The first 29 commands / 69 words serve all 60 face-up selectors as a
white prefix without another bitmap. Accepted commands preserve semantic state
and work counts; mismatch, readback, observation or teardown materializes the
necessary prefix. No underlying drawing is discarded on an unproved match.

Other artwork already resides in offscreen VRAM under the ROM's own scheme;
a second immutable cache would require invalidation and would not remove the
copy. A small copy-program cache reuses blitter setup, not source pixels. CPU
reads wait for pending writes; CPU writes, leases, eviction and destruction drain
all conflicting DMA. A whole-startup-artwork cache was declined.

## Paula sound

Three Paula voices follow the three AY channels; audio.device reserves all four
hardware channels. Pure tones use short square loops and hardware periods.
Noise uses five shared evolving 8 KiB DMA buffers (40,960 Chip bytes): noise alone
and four fixed square-gated variants. These are generated at startup and refreshed
in bounded slices, not embedded as a per-note bank. Only data played by Paula
belongs in Chip RAM. Combined tone/noise follows tone pitch rather than the AY
noise divider; this is an accepted approximation, not exact AY synthesis.

After Ready, envelopes advance in the PAL VBI independently of delayed game
execution. Original register writes still set/restart the envelope. Replay and
preparation use board time. Late original sound writes are a distinct remaining
performance issue; real-time decay does not cure those delays. Diagnostic
launchers mute host output, while normal run.sh has audio.

## Coin hardware and persistence

`SerialPeer` models successful payout sensors, cashbox transfers, meter pulses,
stop, retries and crossed link requests. Byte spacing (1 ms) and coin spacing
(100 ms) are inferred. Normal status $0302 inhibits automatic cashbox draining.
The ROM owns every accounting update; the port never invents a balance. Jams,
physical stock and sensor calibration are not modelled. Exact commands and
experimental evidence remain in [rom-set.md](rom-set.md).

`nvram.bin` contains 32,768 bytes. `accounting.bin` is a 940-byte versioned/CRC
record of the 928-byte $43E60–$441FF accounting region, without runtime pointers.
Original boot validates/repairs restored accounting. Clean exit saves only after
Ready; failed startup and replay never overwrite it. Malformed saves stop.
Each file is rewritten in place once: one DOS Write standalone or one
resload_SaveFile through the slave's POK!SAVE descriptor. No backup/temporary files
or renames are used. WHDLoad requires installer-created blank slots because
creating new files with its write cache hung in the tested low-memory setup.
See [release.md](release.md) for install and quit behavior.

## Allocation, teardown and build constraints

Board/guard, vectors, screen buffers/Copper lists, VRAM/caches and audio have
explicit owners. Keep original allocation pointers when aligning usable storage.
Drain DMA and restore OS display/vectors/CIA/audio/input before freeing their
resources. No interrupt handler allocates heap memory. Fatal service-stack exits
cannot unwind C++ locals; the tracked allocation sweep runs after normal cleanup.
Static owners must release swept pointers beforehand (the previous exit Guru).

The freestanding runtime has no libstdc++, FPU or OS math dependency. Build audits
reject 32-bit software multiply/divide and misaligned even-sized data objects of
four or more bytes in .data/.bss. Use alignas(4) for these globals: m68k GCC's
natural two-byte alignment otherwise makes 68020 longword accesses more expensive.
Release conversion strips HUNK symbols/debug; keep the ELF for diagnostics.
The slave reserves 864 KiB Chip plus 1,472 KiB OtherMem including Kickstart;
reservation is not a measurement of consumption. Tests and limitations are in
[testing.md](testing.md).
