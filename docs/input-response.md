# Native short presses and hold-selection workload

Investigation: 2026-09-29, normal code at `8164ca3`. No input fix is included
in this investigation. The normal executable has been restored.

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

Next implementation: tie frontend delivery to observed PIA data reads, preserving
queued press/release order. Test short taps during quiet hold selection and
drawing bursts, repeated taps on one key, overlapping keys, long holds, and
release/repress around scan boundaries. Verify actual selection changes, not
just the count of external events delivered. The CIA keyboard handshake itself
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
error or watchdog reset. The ordinary executable is restored to `8164ca3`;
temporary probe arrays and altered diagnostic releases are not production code.
