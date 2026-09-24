# Host reference harness

Musashi runs the original, unmodified ROMs here only. `src/board/` contains the
portable device models; the Amiga build does not link Musashi or host backends.
Use clang/clang++ and GNU Make 3.81 from the repository root:

```
make roms-check
make harness-check
make harness-scenarios
make harness SDL=1
```

The scenario check creates attract, dealt-hand, win, double-up and service-display
captures in `tmp/`, then compares uninterrupted execution with snapshot replay.
It compares CPU context, all work RAM, NVRAM, device state, coverage, frame pixels,
and the resumed WAV suffix. ROMs are required for scenarios, not synthetic tests.

```
build/pokeri-host --devices --serial-peer \
  --system-hz 100 --input-hz 50 --watchdog-ms 400 --watchdog-reset-us 50000 \
  --ay-clock 1000000 --palette-rom 0 --inputs host/scenarios/play.inputs \
  --ms 65500 --frame-every 100 --wav --save-state tmp/play.state --out tmp/play

build/pokeri-host-sdl --devices --load-state tmp/scenario-attract.state \
  --ms 120000 --palette-rom 0 --window --live-audio --out tmp/window
```

**Research profile, not measured hardware timing:** CPU 8 MHz with Musashi's 68000
cycle table (not a 68008 bus model), system/input signals 100/50 Hz, watchdog
warning at 400 ms and reset 50 ms later, video capture cadence 50 Hz, AY 1 MHz.
External signals and AY rendering default off. The watchdog reset is necessary
for the original self-test; RAM survives it. The former `$023FA` loop was error
04, not attract. The corrected path reaches real game and service screens.

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
| Escape | Stop and write final captures |

The SDL window shows native logical pixels, scaled to fit; no CRT aspect or
analog filter is claimed. Add `--live-audio` to `--window` to hear the AY output
live. It uses the same 44.1 kHz mono PCM as WAV capture, and can be combined with `--wav`. An AY clock
must be set with `--ay-clock` or restored from a snapshot (the scenario snapshots
already contain it). Without `--live-audio`, the window remains silent. Playback
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

The shared compositor produces **576×292 indexed pixels** from the upper, base,
lower and window registers, respecting their memory widths and start addresses.
`--frame-every N` writes every Nth nominal frame; the final frame is always
written once the display is configured. `--frame-hz` changes capture cadence,
not a discovered oscillator. The odd-width window's right edge remains nominal.

The default palette is explicitly diagnostic. `--palette-rom 0..3` reads one of
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
PC; idle can legitimately hit it. A breakpoint's boundary instruction completes
before stopping; its event context is from before execution. Status 0 means
budget completion, 1 means a usage/file/backend error, and 2 a diagnostic stop.

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
