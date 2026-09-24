# Host bring-up harness (Phase 0)

Musashi is used only here, for research. Nothing in the Amiga build references it.
The shared portable board/device layer is deferred at the hardware gate described
in `docs/rom-set.md`; Phase 1 has not reached idle.

From the repository root, with clang/clang++ and GNU Make 3.81:

```
make roms-check
make harness-check
build/pokeri-host --instructions 10000000 --out tmp/strict
build/pokeri-host --probe --instructions 100000000 --out tmp/phase0
build/pokeri-host --ms 100 --clock 8000000 --out tmp/timed
```

Default execution stops on the first unmapped access, returning exit status 2.
`--probe` explicitly enables Phase 0 zero-read logging stubs so startup can be
observed until it stalls. A probe run is never evidence that hardware works.
All ROM writes are ignored, including startup's code write/readback probes.
Every byte address is masked to 20 bits; ROM occupies 00000–3FFFF, RAM 40000–7FFFF.
ROM chip order is 77POK30, 77POK34, 77POK38, PARA200J. The loader checks sizes;
`make roms-check` verifies hashes. `--rom-dir` overrides the chip directory.

The clock default (8 MHz) is an **unmeasured placeholder** for time budgets.
Musashi runs the 68000 instruction set with its 68000 cycle table. It does not
model the 68008's extra bus cycles; do not infer board timing from these counts.
Instruction budgets count actual instruction-hook invocations, excluding reset
cycles. A time budget ends at the next instruction boundary.

Output prefixes must be under `tmp/`:

- `-trace.csv`: every device transaction, including instruction number, issuing PC,
  masked address, size in bytes, direction and value (hexadecimal addresses/values).
- `-coverage.bin`: 131072-byte bitmap; PC `p` is bit `p&7` of byte `p>>3`.
- `-context.txt`: final reason, registers and the last 128 executed PCs, disassembled
  for research only. Never commit it, even when it shows RAM-resident code.
- `-events.txt`: context at each newly encountered unmapped address and every CPU
  exception. Each repeated access remains in the CSV. Software TRAPs are expected
  runtime calls; faults and unexpected interrupt vectors stop execution.

`--stall-instructions N` sets the no-new-PC watchdog (default 20 million). This
is a heuristic, not proof of idle; inspect context to distinguish an error loop,
a timed delay and legitimate idle. STOP without an interrupt source also stops.
Status 0 means budget completion, 1 means usage/file error, 2 means a diagnostic
stop. The currently measured cold-reset probe returns 2 at the fatal startup loop.

Optional research disassembly stays local:

```
build/pokeri-host --disasm 0x2106 --disasm-end 0x2132 --out tmp/startup-test
```

`make harness-check` uses hand-authored synthetic instructions (no ROM input) to
check ROM write protection, RAM byte order, address masking/wrap, unknown-access
stops, instruction counting, coverage and privilege exception reporting.
The build tracks header/configuration dependencies and generates Musashi opcode
sources exclusively in `build/`. See `musashi/README.md` for the pinned revision,
licences and the optional exception-observation hook added to the core.
