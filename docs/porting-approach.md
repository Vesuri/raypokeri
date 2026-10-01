# Porting approach

RAY Pokeri runs on a 68008.  The Amiga's 68000 executes the same instruction set (the 68008 is
a 68000 with an 8-bit bus and a smaller address space), so, as in the Vette port, **the
original instructions run natively** — no transliteration, unlike the 6502 → C pipeline of
Rescue on Fractalus.  The port is then:

1. **Relocation (implemented).** The four chips occupy `$00000–$3FFFF`; the
   native RAM window is `$40000–$4FFFF` (host research mapping remains larger). Never map these over
   Amiga vectors or Exec state. Build tools verify original hashes; the native loader checks sizes/patch words, applies
   committed relocation descriptors and replaces covered hardware accesses
   with checked Line-A hooks and bounded assembly fast paths. Tables come from runtime access/coverage audits; Ghidra
   is a research aid. See [testing](testing.md) for coverage and
   checksum-bypass limits.
2. **Hardware services.**  Every access to a Pokeri device becomes a call into an Amiga
   implementation, hooked at the access site, preserving all live registers and condition
   codes (Vette rule):
   - **HD63484 ACRTC** — a command-driven graphics processor (FIFO of drawing commands:
     lines, rectangles, fills, pattern/bitmap copies, into its own frame buffer, plus
     display-window/scroll registers).  Implement the command set the game actually uses on
     Amiga bitplanes, with the blitter for fills/copies/lines where it pays.  Use the implemented shared reference and its measured command streams
     to guide the Amiga backend.
   - **AY-3-8912 PSG** — 3 square-wave tone channels, a noise generator and an envelope
     generator, plus one I/O port.  Map onto Paula: square waves from short chip-RAM loops
     with a period per channel, noise and envelope in software.  Rescue on Fractalus's
     POKEY → Paula translation is the prior art (its `docs/sfx-events.md`).
   - **Inputs / lamps / coin mech / hopper / meters** — buttons to keyboard/joystick; coins
     and payout are external serial events. Original ROM code performs all accounting;
     the model supplies successful virtual sensor/meter completions.
3. **Interrupts.** The observed game path takes level 5 with device vectors;
   levels 2 and 7 also have handlers. Hooks preserve virtual SR/IPL and stack
   state while the physical CPU runs in user mode. Diagnostic replay supplies
   the reference schedule. Live VBI delivery happens at safe guest boundaries,
   never by calling game code inside an Amiga interrupt or service.

## Reference and current validation

The host Musashi harness is the reference; Musashi never enters the Amiga build.
Shared device models provide drawing, audio, inputs and deterministic snapshots.
The SDL host is playable. Native diagnostic execution passes a complete work-RAM
comparison at identical allocations and instruction/cycle/IRQ boundaries.

Direct boot, live graphics/Paula audio, inputs, retained accounting and WHDLoad
are implemented. Exact replay and live timing are separate gates. The calibrated
bounded guest clock admits original interrupts at safe boundaries; aggregate
near-real-time performance does not eliminate drawing bursts. See
[architecture](architecture.md), [testing](testing.md) and
[remaining work](remaining-work.md).

## Prior art to read before designing

| Where | What |
|---|---|
| `~/Documents/Vette/docs/amiga-arch.md` | Running original 68000 code on the Amiga: loader, hooks, Page 0, interrupt/callback rules |
| `~/Documents/Vette/docs/static-map.md` | How the static code map + trap inventory was gated |
| `~/Documents/Rescue on Fractalus/docs/m68k-optimisation.md` | 68000 cost rules for any native code |
| `~/Documents/Rescue on Fractalus/docs/headless-fsuae.md` | The headless FS-UAE + gdb measure loop (`amiga/diag_run.sh` here) |
| `~/Documents/Rescue on Fractalus/docs/sfx-events.md` | Sound-chip → Paula translation |
| `~/Documents/Rescue on Fractalus/docs/whdload-slave.md` | WHDLoad installation and wrapper |
| `~/Documents/Rescue on Fractalus/amiga/ARCH.md` | Display takeover and the VBI choice |
