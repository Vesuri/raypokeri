# Phase 3 — relocated host reference

Phase 3 is complete for the recorded scenario coverage. The original instructions
run at two independent placements with the old ROM, RAM and device address ranges
unmapped. Hardware transactions require an exact audited access descriptor.
No Amiga implementation or native Line-A handler is included in this phase.

## Reproduce

```
make harness-check
make harness-relocation-check
```

The second target verifies the four original chip SHA-256 hashes, runs synthetic
relocation checks, then drives setup, attract, deal, win, double-up and service
at the original addresses and both placements below. All captures stay in `tmp/`.
It also compares uninterrupted execution with restored checkpoints, regenerates
and checks the metadata catalog, and deliberately removes required records to
prove that missing fixups and hooks stop execution.

| Profile | ROM base | RAM base (original `$40000`) | Device guard base (original `$80000`) |
|---|---:|---:|---:|
| Reference | `$000000` | `$040000` | `$080000` |
| A | `$100000` | `$200000` | `$300000` |
| B | `$512300` | `$684680` | `$923400` |

The second placement changes low address bits as well as the upper address.
The ROM allocation needs 256-byte alignment because the original parameter
address helper clears its low byte. RAM and guard bases must be even. All
ranges must be disjoint, above the old board space, and within 24-bit memory.
The ROM allocation reserves an extra 32 bytes for the guarded vector-data shadow.
Invalid or overflowing placements are rejected, rather than truncated.

A standalone run uses the original model's explicit experimental clocks:

```
build/pokeri-host --devices --serial-peer \
  --system-hz 100 --input-hz 50 --watchdog-ms 400 --watchdog-reset-us 50000 \
  --ay-clock 1000000 --palette-rom 0 --stall-instructions 100000000 \
  --bypass-module-checksums --io-table host/tables/io-accesses.csv \
  --rom-base 0x100000 --ram-base 0x200000 --device-base 0x300000 \
  --inputs host/scenarios/relocation-play.inputs --ms 69500 --wav \
  --out tmp/relocated-play
```

## Loader and hook contract

The loader verifies every original chip with SHA-256 **before any mutation**.
This pins all patched operands and instructions to the recorded ROM revision.
It then applies `host/tables/relocations.csv`: 91 long operands comprising 38 ROM
references (37 vectors plus the parameter-module base), four RAM addresses,
three accounting-address addends, and 46 device addresses. Entries are offsets
and operation kinds only. The original module loader still copies initialized
data and applies its own code/RAM pointer-fixup lists. Its 107 executed RAM jump
stubs do not need an independently fabricated game-code replacement.

The user authorized a **temporary runtime checksum bypass**. Two branches,
listed in `control-hooks.csv`, skip checksum validation at `$10AE/$110C` while
retaining module header/entry checks and the original runtime loader. This is
explicitly enabled by `--bypass-module-checksums`; ordinary Phase 2 runs retain
their original validation. Skipping the loop changes boot timing and the deal,
so `relocation-play.inputs` is a separate legal switch sequence. It wins 10 mk
through the original game/accounting code, then plays a Double that the ROM
resolves as a loss (see the 2026-10-01 retune below).

At `$2194`, the host bootstrap hook supplies D7 with `ram_base - $40000`, using
the reset routine's existing relocation mechanism. Three separate accounting
addends and three absolute RAM operands required additional recorded fixups.

The device guard covers the translated `$80000–$FFFFF` namespace. Every access
must match PC, original register address, width and direction in
`io-accesses.csv` before it reaches `Board`. `io-sites.csv` supplies the operation
and effective-address descriptors for the native implementation later. The
host CPU still executes these original instructions; its memory callbacks are
the table dispatcher. This phase does **not** claim to test a native Line-A
instruction emulator. Eleven observed RESET instructions also require their
explicit `reset-hooks.csv` entries before they may reset the modeled peripherals.

Five scheduler instructions dereference null list sentinels and observe data in
the original vector page. These accesses cannot simply use relocated vector
values: one treats a vector value as a numeric deadline. `low-vector-hooks.csv`
therefore directs exactly those sites to an immutable copy of the original
32-byte vector data, created from the verified ROM at runtime. The host borrows
the specified address register for that instruction, executes the original
comparison/test, and restores the register afterward. It permits only the
listed shadow offset and width. Original address zero remains unmapped.

CPU reset-vector reads are confined to the host reset adapter. For subsequent
virtual exceptions, the host sets Musashi's internal VBR to the relocated vector
table. **A real 68000 has no VBR**: this is a host adapter for the synthetic
exception mechanism already planned for Phase 4, not a new guest instruction.

`rom-write-hooks.csv` records three ignored writes to the protected program
image, including its ROM/RAM probes. A writable Amiga copy must preserve these
ignored-write semantics. Nine hardware-access instructions are only two bytes;
a native hook must replace only one opcode word and use the recorded original
length when resuming.

## Verification and coverage

All six milestones match across the three placements in:

- Full Musashi scalar state, with only three-way-proven placement deltas allowed.
- Every work-RAM byte, with rebased fields validated against both independent
  deltas and the original-address reference.
- Full portable device state, including VRAM, FIFOs, serial queues, AY phases,
  timers, PIA state and NVRAM; no device-summary-only substitute.
- Instruction/cycle counts, PC coverage, frame pixels and captured audio.
- Every device transaction, including order, PC, size, direction, value and the
  exact expected guard-address delta.

Partly overwritten saved pointers leave fragments in unused stack space. Those
bytes are **not ignored**: `--ram-provenance` retains their last-write evidence
across snapshots. The comparison requires matching writer PCs, logical widths,
addresses and instruction counts, a complete saved value differing by the
placement delta in both runs, and a matching surviving byte. Musashi's split
MOVEM.L bus writes are joined for diagnostics only. Other unexplained bytes fail.

The relocated uninterrupted run and checkpoint chain have byte-identical final
snapshots and matching PCM suffixes. Negative checks reject missing ROM, RAM,
device, low-vector, D7-bootstrap, RESET and I/O records; incompatible snapshots;
invalid placements; and a modified ROM before patching.

The coverage union contains **17,901 PCs: 17,793 in ROM and 108 in RAM**.
`coverage.json` records its digest and scope. All **29 covered absolute-long
operands** have relocation records; `relocation_catalog.py` reproduces that
static audit and the reset/ROM-write catalogs. An address-looking integer is
not automatically classified as a pointer. The committed RESET table may hold
more sites than this scenario reaches: native boot also reaches the hardware-test
failure RESETs `$20E8/$212A/$2132/$2164` (added 2026-09-25). The check requires
every observed site to be listed and every extra row to be a real RESET opcode.

**Retune (2026-10-01).** The coin/meter peripheral model (`66fb6f4`, bisected)
changes serial timing, so the old deal press dealt a losing hand and the check
had been failing since. The deal press moves 10 ms to 41.01 s (searched over
41.00–42.59 s with the scripted holds unchanged). With the corrected peer
every searched winning deck's Double shows 7♣ for either guess and every Double
press time tried, so the ROM itself selects the losing card; no fixture choice
restores the old won Double. The `double` milestone therefore requires the
pending-win byte to clear with no Collect pressed, i.e. a resolved Double.
`$40A03`, the high byte of an overwritten RAM pointer on the stack in `deal`,
was added to the provenance watch list and passes the same-writer rule.

Known gaps remain outside the scenario union: other service/accounting modes,
payout/hopper firmware, other serial transactions, the DUART, 2 MB video/RAMDAC,
and writable-ROM development mode. Unknown device accesses and missing hooks
remain loud stops. Phase 2's physical palette, clock and drawing-fidelity limits
are unchanged. These tests prove relocation against that reference, not new
physical-hardware accuracy.

Relocated snapshots use version 3, or version 4 when writer evidence is enabled;
placements and patched-image identity must match on restore. They retain the
same host ABI/endianness constraints as Phase 2 snapshots. Coverage and history
use canonical board offsets; CPU registers contain actual relocated addresses.
`-trace.csv` includes `cpu_address`; `-cpu-state.bin`, `-board-state.bin` and
`-ram-writers.csv` provide ignored audit artifacts. Never commit those outputs.
