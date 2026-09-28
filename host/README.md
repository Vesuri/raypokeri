# Host game and reference harness

Musashi runs the original, unmodified ROMs here only. `src/board/` contains the
portable device models; the Amiga build does not link Musashi or host backends.
Use clang/clang++ and GNU Make 3.81 from the repository root:

```
make roms-check
make harness-check
make harness-scenarios
make harness SDL=1
```

Run the game with no options:

```sh
build/pokeri-host-sdl
```

It starts with zero player credits and winnings, the ROM's minimum bet of 1,
live audio and no time limit. Press C to insert a coin. The operator setup fills
only the payout reserve (100 units); it does not supply player/test credits.

Normal play skips the coin-op hardware diagnostics: RAM patterns, PIA/AY/timer/
watchdog tests, video-memory tests and checksums. Required RAM clearing, module
loading, device setup and graphics initialization still run. The shared cabinet
setup advances on the ROM's door/refill/accounting state and completed serial
transactions; it no longer waits for a fixed 40.5-second script. It supplies
external inputs only, never player balances or game decisions.

The first launch saves a clean startup snapshot locally under
`tmp/sdl-clean-start-<executable SHA-256>.state` beside the ROM directory.
Later launches restore that untouched state. Played sessions never overwrite it.
ROM identity and snapshot ABI are checked; a changed executable gets a new cache.
`--cold-boot` repeats initialization and refreshes the cache. `--hardware-tests`
restores the original diagnostics and disables the automatic cache for that run.
The research harness retains its original diagnostic boot by default;
`--skip-hardware-tests --auto-setup` selects the normal-game policy there.
Explicit snapshots must be restored with their matching hardware-test policy.
See [startup-policy.md](../docs/startup-policy.md).

No audio/frame/trace captures are written by default. ROMs are found in `rom/`,
or beside the executable's parent directory. Research/custom hardware options
and explicit input/state files bypass the automatic cache.

Use `--mute` for silent play, `--ms 60000` for one minute of play after setup,
or `--instructions N` for a bounded instruction run after setup. Escape, closing
the window or Ctrl-C exits normally. Each launch starts fresh; an explicit
`--load-state` skips setup. `--wav`, `--frames` (final PPM), and `--frame-every N`
request captures individually. `--capture` or `--out tmp/name` enables the full
research capture bundle. `--research` restores the old harness defaults,
including absolute budget endpoints and diagnostic files.

The scenario check creates attract, dealt-hand, win, double-up and service-display
captures in `tmp/`, then compares uninterrupted execution with snapshot replay.
It compares CPU context, all work RAM, NVRAM, device state, coverage, frame pixels,
and the resumed WAV suffix. ROMs are required for scenarios, not synthetic tests.

```
build/pokeri-host --devices --serial-peer \
  --system-hz 100 --input-hz 50 --watchdog-ms 400 --watchdog-reset-us 50000 \
  --ay-clock 1000000 --palette-rom 0 --inputs host/scenarios/play.inputs \
  --ms 65500 --frame-every 100 --wav --save-state tmp/play.state --out tmp/play

build/pokeri-host-sdl --load-state tmp/scenario-attract.state
```

**Research profile, not measured hardware timing:** CPU 8 MHz with Musashi's 68000
cycle table (not a 68008 bus model), system/input signals 100/50 Hz, watchdog
warning at 400 ms and reset 50 ms later, video capture cadence 50 Hz, AY 1 MHz.
The standalone SDL game enables this profile; external signals and AY rendering
default off in the research harness. The watchdog reset is necessary
for the original self-test; RAM survives it. The former `$023FA` loop was error
04, not attract. The corrected path reaches real game and service screens.

For startup wall-time diagnostics, run
`POKERI_STARTUP_TIMING=1 build/pokeri-host-sdl`. It reports ROM/core initialization,
SDL creation/presentation and the first ROM display frame. The startup window is
presented immediately; it no longer waits for the emulated display to be enabled.

## Controls and input scripts

SDL is optional. `make harness SDL=1` builds `build/pokeri-host-sdl`; the ordinary
`build/pokeri-host` stays free of SDL. Keys:

| Key | Input |
|---|---|
| Space | Deal/draw |
| B | Bet |
| 1–5 | Hold the corresponding card |
| Return | Collect; with door open, enter the ROM's refill/accounting mode |
| D | Double |
| Left / Right | Big / small selection (Left won on an 8 in the measured scenario) |
| C | Coin event, ACIA0 application command 3 (requires diagnostic serial peer) |
| F1 | Toggle cabinet door |
| F2 | Service switch; release advances TESTI while the door is open |
| Escape | Exit (captures only when requested) |

The SDL window shows native logical pixels, scaled to fit; no CRT aspect or
analog filter is claimed. Live audio is enabled by default in the standalone
SDL game. In research mode, add `--live-audio` to `--window`. It uses the same 44.1 kHz mono PCM as WAV capture, and can be combined with `--wav`. An AY clock
must be set with `--ay-clock` or restored from a snapshot (the scenario snapshots
already contain it). In research mode, the window remains silent without `--live-audio`. Playback
uses a bounded queue and waits in wall time without changing emulated state.
WAV capture remains available in either build. Use a scenario checkpoint for an
initialized cabinet.

`--inputs PATH` accepts ascending absolute emulated milliseconds, comments with
`#`, and four fields per event. Integers accept `0x` hexadecimal notation:

```
# milliseconds  PIA-number  side(0=A,1=B)  physical byte
0      1       0         0xff
0      1       1         0x7f
0      2       0         0x08
18000  1       1         0x3f
# diagnostic ACIA0 transport packet: header, payload width + data
19000  packet  1         0x20000
19500  packet  0x31      0x20100
# no-payload coin event
20000  packet  3         0
# alternatively inject an individual receive byte, without transport assistance
21000  rx      0         0x30
```

For `packet`, field 3 is the six-bit application header. Field 4 has payload
length (0, 1 or 2) in bits 16–17 and payload bytes in the low 16 bits. Payload
bytes cannot have bit 7 set. The diagnostic peer adds framing, alternating
sequence bits and checksums at a hypothetical 1 ms per byte. It acknowledges
transport packets but does **not** fabricate application replies to unknown
commands. Its received application packets are supplied explicitly by scripts.
This is sufficient for the reference scenarios, not a complete coin/meter unit.
All physical accesses and transmitted/received serial bytes remain traced.

`play.inputs` opens the cabinet, establishes peripheral status, supplies coin
inputs, enters refill mode, supplies a 100-coin reserve, closes the door, then
plays and doubles. All game decisions, bookkeeping and card selection run in
original code. Milestones: setup 25 s; attract 40.5 s; deal 47.5 s; win 56 s;
double 65.5 s. `service.inputs` reaches TESTI 5 and selects its display pattern
by 39 s. The scripts have no RAM writes, ROM patches or forced outcomes.

## Frames, sound and saved state

The shared compositor produces **608×292 indexed pixels** from the upper, base,
lower and window registers, respecting their memory widths and start addresses.
`--frame-every N` writes every Nth nominal frame; a final frame is
written when diagnostics, `--frames`, or periodic capture is enabled. `--frame-hz` changes capture cadence,
not a discovered oscillator. The Pokeri viewport includes all 608 stored pixels
so the right header border and margin remain visible. This supersedes the
nominal 576-pixel timing interpretation without changing register values.
Odd-width moving windows retain the complete final display fetch.

The research default palette is explicitly diagnostic; standalone play uses bank 0. `--palette-rom 0..3` reads one of
the ROM's RAMDAC banks at runtime; no original palette bytes are in the source.
Bank 0 is a useful comparison candidate, with red backs and blue boxes, but the
512 KB board's palette hardware is unidentified. RGB levels, card-center
texture and the filmed CRT's color cast are not yet calibrated.

`--wav --ay-clock Hz` writes mono 16-bit 44.1 kHz PCM. Tone, shared 17-bit noise,
all 16 envelope shapes, mixer gates and register-13 retriggering are modeled in
portable C++. The approximate logarithmic DAC and DC filter are not an analog
amplifier model. Clock/rate uncertainty and instruction-boundary register timing
limit fidelity. No automatic audio match to the footage is claimed.

`--save-state tmp/name.state` captures full board state, timers, FIFOs, pending
input/serial queues, AY phases, Musashi scalar context and diagnostic counters.
`--load-state` restores at an instruction boundary, with live callbacks rebound.
Snapshots are versioned and require the same ROM hash, pinned core ABI and host
byte order. They are development artifacts, not a portable release save format.
Output paths and palette are not machine state. Restored machine configuration
wins over clock/signal CLI settings; `--inputs` replaces the saved input queue
and skips events before the saved time. **Time/instruction budgets are absolute
endpoints**, including after restore. Malformed headers, incompatible versions and truncated state stop.
Snapshots are trusted local artifacts; the format has no integrity checksum.

`--retained-ram tmp/name.bin` instead starts the CPU from reset with saved main
RAM and saves it afterward (missing file starts empty). This is an explicit
**full main-RAM retention hypothesis**, not a claim about physical battery wiring.
A fresh boot preserved the ROM's initialized reserve and credits without an
attention error. The separate `$D0000` NVRAM mapping remains unaccessed on this
path. Exact physical RAM extent, mirroring and retained subranges remain open.
Retained RAM cannot be combined with full snapshot loading.

All runtime output belongs in ignored `tmp/`:

- `-trace.csv`: every device transaction, instruction number, PC, address, size,
  direction and value. This includes every HD63484 FIFO word.
- `-events.txt`: exceptions, reset context, AY writes, scripted inputs and the
  first 20,000 decoded video commands (later faults are still printed).
- `-devices.txt`: IRQ/AY/HD63484 counts, registers and VRAM digest.
- `-coverage.bin`: executed-PC bitmap; PC p is bit `p&7` in byte `p>>3`.
- `-context.txt`: registers and recent disassembled PCs. Never commit it.
- `-ram.bin`, `-nvram.bin`: main RAM and separate mapped NVRAM captures.
- `-final.ppm`, `-frame-NNNNNN.ppm`, `.wav`, `.state`: reference outputs/state.

## Diagnostics and limits

Unknown device accesses and unsupported drawing modes stop loudly. `--probe`
is the separate Phase 0 zero-read logging mode, never evidence of hardware
success. ROM writes are ignored, including the reset routine's self-write test.
The 20-bit ROM order is **30, 38, 34, PARA**, not chip-name order. The loader
checks sizes; `make roms-check` verifies the supplied ROM hashes.

`--break-pc ADDRESS`, `--watch-write ADDRESS` and `--stall-instructions N` expose
research state. The default stall guard is 20 million instructions without a new
PC; idle can legitimately hit it. Standalone play disables that heuristic
(`--stall-instructions 0`) while retaining unknown-access and CPU-fault stops. A breakpoint's boundary instruction completes
before stopping; its event context is from before execution. Status 0 means
budget completion or normal window/Ctrl-C exit, 1 means a usage/file/backend error, and 2 a diagnostic stop.

```
build/pokeri-host --disasm 0x209e --disasm-end 0x2106 --out tmp/watchdog
```

Disassembly is research only. Never commit ROMs, captures, state or byte-for-byte
derivatives. See `docs/rom-set.md` for evidence tags, drawing approximation limits,
read-FIFO underflows and the unresolved physical peripheral/timing questions.

## Phase 3 relocation

`make harness-relocation-check` runs the full scenario suite at two ROM/RAM/device
placements and compares it with an unrelocated reference using the same explicit
checksum bypass. It verifies all original chip hashes before patching and checks
complete CPU/device state, every RAM byte, frames, audio, coverage and device
traces. Missing fixups/hooks, corrupt ROMs and invalid layouts must fail loudly.
See [the relocation contract](../docs/phase3-relocation.md) for the exact tables,
placements, special vector-data hooks, snapshot formats and coverage limits.

Phase 2 commands keep their original runtime checksum validation. Phase 3 uses
`--bypass-module-checksums` and `host/scenarios/relocation-play.inputs`, since
skipping the checksum loop changes the deal's timing. The bypass is temporary
and explicitly authorized, not inferred hardware behavior.

The earlier access-only audit remains available as `make harness-access-audit`
and `make harness-access-check`, after creating the Phase 2 scenario captures.
The full relocation gate is self-contained apart from the original ROMs.

## Phase 4 native validation

`make harness-native-check` runs synthetic instruction-hook comparisons against
Musashi plus the freestanding container, replay-parser, integer arithmetic and
SHA-256 tests. None of the native executable's sources links Musashi.

`--record-replay tmp/file` records only the cold-boot reference's instruction/
cycle boundaries, external inputs and IRQ identities; it does not record game
results for native injection. It requires the explicit checksum bypass, device
models and a fresh zeroed machine. Native preparation and the Amiga diagnostic
commands are in [the Phase 4 notes](../docs/phase4-preflight.md).

After a successfully completed Amiga diagnostic, `python3 host/native_check.py`
reruns the host at the actual Amiga allocation bases and compares every RAM byte.
It rejects incomplete native runs, unverified vector restoration and different
instruction/cycle/PC/IRQ endpoints. No stack, timer or RNG region is excluded.

Phase 4's native diagnostic boot and full-RAM gate have passed. The user approved
deferring live VBI-paced boot validation to Phase 5's Amiga device backends; the
playable SDL host remains the interactive reference.


## Phase 5 platform comparisons

`make harness-platform-check` verifies packed/planar equivalence and the bounded
AY envelope backend. The HD63484 command tests also run against both storage
formats. Final captures now include `-vram.bin` (big-endian packed words) and
`-indices.bin` (logical colour indices), independently of palette conversion.
`host/planar_capture_check.py` compares a paired native planar capture, the
cropped frame and AY register stream; `host/native_check.py --live-boot` compares
RAM at the replay-to-live boundary after verifying a clean native exit.
See [the Amiga notes](../docs/phase5-amiga.md) for capture details and limitations.

Standalone regression: after `make harness-scenarios` and building both host
binaries, run `python3 host/sdl_play_check.py`. It uses SDL dummy drivers, checks
the no-option launch from another directory, clean exit and opt-in captures,
and compares initialized RAM/VRAM/pixels with the attract scenario.

`make harness-paula-check` builds and checks the ROM-dependent offline Amiga
noise/mixed-tone bank. It verifies all 125 sound data records by executing the
original ROM reader, and compares all generated samples with the AY reference
and offline filter. Generated data lives in ignored `amiga/generated/` and
`tmp/`; it must never be committed. The ordinary Amiga build regenerates the
bank when the generator or parameter ROM changes. Playback retains live ROM
sequencing and envelopes, with no runtime sample synthesis.

After the native build, `make harness-paula-stream-check` with the Amiga
toolchain on PATH verifies the actual linked audio DMA server in Musashi,
including all preserved registers, wrap/tail lengths, IRQ acknowledgment and
68000 core-cycle counts. It contains no original ROM bytes.


`make harness-card-cache-check` compares the guarded card-back/shared-white
cache against independent packed rendering, including every alignment,
background eligibility, command mutation and prefix observation.
`make harness-face-up-check` additionally executes the original face-up producer
for all 60 suit/rank selectors and compares complete card outputs in both
planar layouts. The generated recipe and command catalog remain ignored local
ROM-derived files; no game code is translated or linked into the Amiga.


### Shuffle timing comparisons

Normal SDL play uses consumer-paced shuffle frames, preserving the original
sound callback ordering. Research mode opts in with `--shuffle-vblank`;
`--shuffle-producer-vblank` retains the old producer-wait reference and
`--no-shuffle-vblank` disables the port pacing policy. `--shuffle-frames` writes
individual frames only when explicitly requested, under a `tmp/` output prefix.
See `docs/shuffle-pacing.md` for snapshot compatibility and validation.
