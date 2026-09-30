# Native short presses and hold-selection workload

Investigation and correction: 2026-09-29. The reproduction below describes
`8164ca3`; read-acknowledged delivery now replaces the timer-consumed latch.

## Confirmed lost-input path

**MEASURED:** complete down/up pairs for three different Hold keys arrive at
`amigaInputKey`, survive in `pressed[]`, and are copied to the PIA pins by
`amigaInputApply`. They are then cleared on the next generated 50 Hz input edge,
before the ROM's switch sample changes. None of the three cards becomes held.
The ROM hold-ready flag remains 1, so its action gate does not explain this loss.

The test used the existing external diagnostic key API. It changed no game
state, callback, interrupt or ROM instruction. All three release events were
moved into the same delivery batch as their presses. A local probe recorded
pins, the ROM's last switch sample and selection flags before each application.
See [ROM evidence](rom-set.md#hold-selection-input-investigation-2026-09-29).

**DERIVED:** a generated input-clock edge is not acknowledgment that the ROM
has sampled the switches. The frontend must retain a pending press until a
guest read observes it. Repeated taps also need an observed release between
presses; a single Boolean key-down latch can merge them. Preserve real held-key
behavior and independent simultaneous buttons. Do not solve this with an
arbitrary longer minimum pulse or by changing the original hold-ready gate.

Acceptance: retain queued press/release order until observed PIA data reads.
The tests below cover short and repeated taps, overlapping keys, long holds
and scan boundaries; the native reproduction verifies actual selection changes,
not just counts of external events delivered. The CIA keyboard handshake itself
was not exercised by the synthetic key API and remains a separate possible
source if misses persist after this confirmed defect is fixed.

## What the apparent idle state costs

**MEASURED:** A1200 cycle-exact diagnostic, Hold → Draw interval:

| Work | Time |
|---|---:|
| Board interval | 2.00 s |
| PAL elapsed | 2.26 s |
| Original guest execution | 0.95 s |
| Full C dispatcher services, inclusive | 0.28 s |
| Short C services, inclusive | 0.22 s |
| Masked dispatcher prologue | 0.07 s |
| Assembly/exception/IRQ residual | 0.62 s |
| Profiler observer estimate | 0.12 s |
| Presentation, included above | 0.01 s |
| Video command work, included above | 0.08 s |

Of 114 VBI PC samples, 60 (52.6%) are the original decrement/branch delay at
`$2442/$2444`. It is busy-waiting, not computing card artwork. The interval has
8,111 counted hooked operations and 138 completed graphics commands: 69 AMOVE
and 69 AGCPY. This is an interval including hold-label updates, not a perfectly
static screen. Scope values are observer-corrected estimates; elapsed time
still includes the diagnostic overhead and must not be sold as normal-release
performance. VBI samples underrepresent masked and higher-priority interrupt
work. The previously validated opt-in idle-loop experiment is relevant to the
busy-wait cost; it does not fix lost key delivery.

Local captures: `amiga/.run/input-idle-ledger`,
`tmp/input-idle-ledger-{marks,frames,samples}.bin`,
`tmp/input-idle-summary.txt`, `tmp/perf/Pokeri-input-idle-ledger(.elf)`,
`amiga/.run/input-short-tap`, `tmp/perf/Pokeri-input-tap(.elf)`.
Both diagnostic runs finish their 160-million-cycle budgets without a native
error or watchdog reset. At the end of that investigation the ordinary executable was restored to `8164ca3`;
temporary probe arrays and altered diagnostic releases are not production code.

## Read-acknowledged delivery

The native keyboard now counts each physical transition. Fixed per-button
queues retain alternating down/up levels until a PIA input-data read observes
them. A release must be read before the next queued press is presented; held
keys remain held and separate buttons advance independently. DDR/control reads
and output-only bits cannot acknowledge a level. Overflow is a logged fault,
not silent event loss. Coin/door events preserve their incoming counts too.

`Board::inputRead` is an optional external frontend observer, excluded from
serialized device state. Only the native live frontend installs it. Both the
generic and specialized device-read paths use the same `readPia` endpoint.
Original ROM instructions, scan scheduling and hold-ready gating are unchanged.

**MEASURED:** the prior failing three-tap native test now changes the selection
mask from 0 to 8, then 24, then 26: cards 4, 5 and 2 are all held. All down/up
pairs arrive before the same frontend application. The run completes all 24
external input events without a fault or watchdog reset. Local probe:
`amiga/.run/input-read-probe`, `tmp/input-read-samples.bin`. The synthetic
headless test covers delayed reads, two queued taps, overlapping keys, sustained
holds, per-pin acknowledgment, DDR/control exclusions and overflow. Host
harness/platform/native suites and native short/feed CPU proofs pass.

**MEASURED:** ECS and AGA replays matches all 262,144 RAM bytes, 524,288 VRAM bytes,
172,064 displayed pixels and 60 AY writes at 7,904,133 instructions,
64,000,000 cycles and 8,685 interrupts. Both complete without a native error or watchdog reset.


## T4: compact event snapshot (2026-09-29)

**MEASURED:** the old frontend held 128 key levels,
128 press counters and 128 transition counters. `amigaInputApply` copied and
cleared the latter two arrays, not three arrays as the initial profile summary
said. Only 15 event keys are consumed: 12 PIA buttons plus door, coin and lamp
presses. Escape is a separate level check.

`AmigaKeyEvents` retains those 15 levels/transition counters, three service
press counters and Escape's level. The atomic snapshot copies 15 event counts;
unknown keys never accumulate unused counters. Original read-acknowledged button
queues are unchanged. Repeated down events are ignored, service press counts
are retained, and transition saturation still raises a fault. No allocation or
OS operation is added to the latch; the existing Disable/Enable pair protects
its bounded snapshot.

Synthetic tests cover all mapped keys together, multiple taps before a snapshot,
held levels across snapshots, ignored keys, Escape, service and button overflow,
and 50,000 events compared with the previous full-array semantics. The snapshot
feeds the same `ReadLatchedButtons` tested with delayed/overlapping reads.

**MEASURED:** external gameplay traces `t2-absolute-play` → `t4-input-play`
give `amigaInputApply` 438.6 → 123.0 µs/call (847/850 calls), inclusive of its
callees. Its workload share is 2.06% → 0.58%. This is about 72% lower per-call
cost, or 1.58% CPU time at 50 calls/second; live workload phases differ. The
candidate trace accepted Double in round 5 and exited cleanly.

Headless harness/platform/native suites pass. Exact ECS/AGA replay matches all
262,144 RAM bytes, 524,288 VRAM bytes, 172,064 displayed pixels and 60 AY writes
at 7,904,133 instructions / 64,000,000 cycles / 8,685 IRQs. Cold/warm live24
passes on both machines with no error/reset and restored vectors. AGA board/PAL
ratios are 0.9695/0.9751; ECS ratios are 0.2708/0.2739, so ECS remains a
compatibility pass rather than real-time performance. Physical CIA key reception
is not newly proved by these injected-key and synthetic tests.

The normal-code Double run accepts round 5 (50 key transitions), with no error
or reset: board/PAL ratio 0.9638, AY batch median 12.5 ms, largest consecutive
sound-write excess 241.600 ms. Different hands prevent interpreting that last
number as a controlled improvement over T3. The card/audio deadline remains open.
Local evidence: `tmp/t4-input-{aga,ecs}-reference-*`,
`amiga/.run/t4-input-live-{aga,ecs}-{cold,warm}`, and
`tmp/t4-input-double-report.txt`. The restored normal build matches the frozen
T4 executable in every allocated ELF section.

## Cabinet function-key layout (2026-09-30)

Amiga gameplay now uses F1–F5 Hold1–5, F6 Double, F7 Low, F8 High, F9 Bet,
F10 Collect and Space Deal. Enter supplies coins, Escape saves/quits, and
Delete/O/L are door/operator/lamps. Help is the WHDLoad emergency quit key.
No old gameplay aliases or C remain. The existing fifteen-event snapshot and
read-acknowledged queues are unchanged. Pin-oracle tests verify each physical
button, no service crossover, short-tap latching and removal of old shortcuts.
Standard live24 and Double diagnostic inputs now use the same function keys;
historical log analysis still recognizes older Double key codes. SDL keyboard
controls are unchanged; this request concerns the Amiga keyboard layout.

**MEASURED validation:** board/input tests pass, including explicit function-row
PIA pin oracles, all removed shortcuts, Enter coin, Escape levels, short taps,
50,000 random reference events and overflow. Native normal live24 completes
all 24 remapped events with status 4, error/reset 0 and restored vectors.
Evidence: `tmp/function-keys-test.log`, `tmp/function-keys-live24-summary.txt`,
`amiga/.run/function-keys-live24/gdb-out.log`. This verifies injected raw-key
processing, not a new physical-keyboard observation. WHDLoad persistence-tool
tests (3) and graphics-probe tests (5) pass. The 206,415-byte 0.3 archive passes
independent decompression/CRC/build identity and Help-header verification.
