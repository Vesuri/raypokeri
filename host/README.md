# Host game and reference harness

Musashi is host-only. The Amiga runs the original program natively and shares
`src/board/` device models with this reference; it does not link the emulator.
Build with clang/clang++ and GNU Make 3.81:

```sh
make roms-check
make harness SDL=1
build/pokeri-host-sdl
```

The SDL game starts with zero player credits, minimum bet 1, live audio, no
captures and no time limit. Cold initialization fills only the 100-unit payout
reserve. Subsequent launches restore the untouched clean-start snapshot in
`tmp/sdl-clean-start-<executable SHA-256>.state`; played sessions never replace it.
`--cold-boot` rebuilds it. The approved coin-op diagnostic bypass keeps original
initialization/accounting. Cabinet setup waits for actual external-input and
serial acknowledgements, without the old fixed setup delays.

## SDL controls and options

These deliberately differ from the Amiga function-key layout.

| Key | Action |
| --- | --- |
| Space | Deal/draw |
| B | Bet |
| 1–5 | Hold |
| Return | Collect; with door open, refill/accounting |
| D | Double |
| Left / Right | Big / small |
| C | Insert coin |
| F1 / F2 | Door / service switch |
| Escape | Exit |

`--mute` silences playback. `--ms 60000` limits normal play after setup/cache
restore; `--instructions N` similarly bounds it. `--frames` requests a final PPM,
`--wav` audio, and `--frame-every N` periodic frames. `--capture` or `--out tmp/name`
enables full diagnostics. `--research` restores harness defaults and absolute
budgets. Research options/input files disable the automatic clean-start cache.
`POKERI_STARTUP_TIMING=1` reports startup stages; do not confuse Mac wall time
with emulated board or PAL time.

## Headless research

`make harness` builds `build/pokeri-host` without SDL. Use `--help` for the current
options. `make harness-unit`, `make harness-rom` and `make harness-elf` replace
historical collections of one-off targets; see [testing](../docs/testing.md) for
requirements and native qualification procedures.

The research signal profile is inferred: 8 MHz board cycles from Musashi's
68000 timing (not a 68008 bus model), 100/50 Hz system/input signals, 400 ms
watchdog warning plus 50 ms to reset, AY 1 MHz. Standalone SDL enables it;
headless external signals are off unless requested. `--skip-hardware-tests
--auto-setup` selects normal startup in headless runs; original diagnostics
remain available for research.

Input scripts have ascending absolute emulated milliseconds and four fields:

```text
# time   device    side/header  value
0        1         0            0xff
0        1         1            0x7f
19000    packet    1            0x20000
19500    packet    0x31         0x20302
20000    packet    3            0
21000    rx        0            0x30
```

PIA devices 0–2 use side 0=A, 1=B. Packet headers are six bits; bits 16–17 of
value encode payload length 0–2, with payload in the low 16 bits. Payload bytes
cannot set bit 7. `rx` injects an ACIA byte directly. The serial peer provides
framing, retry handling, payout sensors and meter completion; it does not supply
arbitrary replies to unknown commands. Original ROM code owns balances.

## Captures, state and fidelity

The shared compositor produces 608×292 pixels, including the right header margin;
Amiga output crops to 608×283. Runtime palette bank 0 is the normal comparison
palette; physical analogue levels remain uncalibrated. Host AY synthesis models
tone/noise/envelope; it is a reference for the faster approximate Paula backend.

Full snapshots include CPU, RAM, devices, timers and pending queues. They require
the same ROM identity, core ABI and host byte order. With explicit `--load-state`,
budgets are absolute endpoints and a new input script skips events before the
saved time. Old research snapshots retain their recorded cabinet status. These
are trusted local development files, not release saves. `--accounting-ram` uses
the versioned retained accounting format with original warm initialization;
`--retained-ram` is a broader research hypothesis.

ROM scenarios compare uninterrupted and restored execution, pixels, audio and
state. Relocation checks run original and two disjoint placements and validate
pointer/stack-fragment provenance. Covered code is not a proof of all operator
paths. Current native replay compares 65,536 RAM bytes, 524,288 VRAM bytes,
172,064 cropped pixels and 60 AY writes; see the exact procedure in testing.md.
Keep all derived output in ignored tmp/. Never run the known-problematic SDL
window-memory diagnostic routinely on this machine.
