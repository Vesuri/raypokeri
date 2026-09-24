# Porting approach (initial — revise as the research lands)

Pokeri runs on a 68008.  The Amiga's 68000 executes the same instruction set (the 68008 is
a 68000 with an 8-bit bus and a smaller address space), so, as in the Vette port, **the
original instructions run natively** — no transliteration, unlike the 6502 → C pipeline of
Rescue on Fractalus.  The port is then:

1. **Relocation.**  The ROM code assumes it lives at `$00000–$2FFFF` with RAM up to about
   `$40F00`, which on the Amiga is chip RAM and Exec's vector page.  Never map the original
   memory over Amiga vectors/Exec state (a Vette hard rule).  Options, to be chosen once
   the code's addressing is understood:
   - rewrite absolute addresses at load time from a relocation table derived from the
     disassembly (what Vette did for its segments);
   - run from a copied image with the game's RAM/IO addresses redirected by patching the
     access sites.
   Either way it needs a complete list of absolute references, which the Ghidra pass gives.
2. **Hardware services.**  Every access to a Pokeri device becomes a call into an Amiga
   implementation, hooked at the access site, preserving all live registers and condition
   codes (Vette rule):
   - **HD63484 ACRTC** — a command-driven graphics processor (FIFO of drawing commands:
     lines, rectangles, fills, pattern/bitmap copies, into its own frame buffer, plus
     display-window/scroll registers).  Implement the command set the game actually uses on
     Amiga bitplanes, with the blitter for fills/copies/lines where it pays.  Inventory the
     commands first; do not implement the whole chip.
   - **AY-3-8912 PSG** — 3 square-wave tone channels, a noise generator and an envelope
     generator, plus one I/O port.  Map onto Paula: square waves from short chip-RAM loops
     with a period per channel, noise and envelope in software.  Rescue on Fractalus's
     POKEY → Paula translation is the prior art (its `docs/sfx-events.md`).
   - **Inputs / lamps / coin mech / hopper / meters** — buttons to keyboard/joystick; coins
     and payout become an Amiga-side credit model.  Scope decision for later: how much of
     the operator side (service menus, books, hopper) the port keeps.
3. **Interrupts.**  The game's level-2 and NMI handlers are driven from the Amiga VBI at safe
   points, never from inside an Amiga interrupt calling back into the original code
   (another Vette rule).

## Reference / oracle

There is no MAME driver, so there is no ready-made fidelity oracle.  Options: build a
minimal host-side 68000 harness (e.g. Musashi) that boots the ROM set with stubbed devices
and logs the device accesses, and use it as both the research tool and the reference.
Decide before the device work starts.

## Prior art to read before designing

| Where | What |
|---|---|
| `~/Documents/Vette/docs/amiga-arch.md` | Running original 68000 code on the Amiga: loader, hooks, Page 0, interrupt/callback rules |
| `~/Documents/Vette/docs/static-map.md` | How the static code map + trap inventory was gated |
| `~/Documents/Rescue on Fractalus/docs/m68k-optimisation.md` | 68000 cost rules for any native code |
| `~/Documents/Rescue on Fractalus/docs/headless-fsuae.md` | The headless FS-UAE + gdb measure loop (`amiga/diag_run.sh` here) |
| `~/Documents/Rescue on Fractalus/docs/sfx-events.md` | Sound-chip → Paula translation |
| `~/Documents/Rescue on Fractalus/docs/whdload-slave.md` | The eventual WHDLoad install |
| `~/Documents/Rescue on Fractalus/amiga/ARCH.md` | Display takeover and the VBI choice |
