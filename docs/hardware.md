# The machine: boards, chips, controls

What is known about the physical machine, from a board photo and two Finnish articles.  Source
tags: **PHOTO** (read off the board photo; strong), **HV** (hilavitkutin.com, 2017, summarising a
RAY video with a designer), **KH** (kasinohai.com, 2026, an affiliate casino guide: useful
detail, but it has clear errors, so treat it as a lead, not a fact).  A claim confirmed by the code
or the harness moves into `docs/rom-set.md` as MEASURED/DERIVED.

Local copies (git-ignored): `ref/articles/board.jpg` (1200×967), `ref/articles/cabinet-open.jpg`.
Sources:
- https://hilavitkutin.com/2017/10/27/nain-luotiin-legendaarinen-rayn-1985-videopokeri-pelikone-suunnittelija-kertoo/
- https://www.kasinohai.com/ray-pokeri (blocks scripted fetches; the user saved a copy)

## The system: several boards

| Board | Contents | Source |
|---|---|---|
| **Game processor** "RAY PCB 5003-2 PELIPROSESSORI" | CPU, RAM, EPROMs, PIAs, ACIAs; details below | PHOTO |
| **Video board** (RAY's own design) | Hitachi **HD63484**, **512 KB** of video memory, a **16-colour palette** | KH |
| **Light and sound board** (RAY's own) | General Instrument **AY-3-8912** (3 channels), volume control, its own amplifier | KH |
| **Coin-mechanism controller** | Drives the hopper and the payout lock; a banknote selector was already designed in 1986, though notes were only accepted ~20 years later | KH |
| Hopper, Mars coin validator (1 mk and 5 mk coins), Philips speaker, Nanao monitor | | KH |

DERIVED: neither the HD63484 nor the AY is on the processor board, so both are reached over the
board interconnect (the large edge connector on the processor board's right side).  This fits the
harness finding that the AY is driven through a PIA port with strobe bits (`docs/rom-set.md`):
the PIA *is* the bus to the sound board.

## Game processor board PCB 5003-2 (PHOTO)

| Part | Where | Role |
|---|---|---|
| **TS68008CP8** (Thomson MC68008, 48-pin DIP, 8 MHz grade) | centre | CPU, 20 address lines (1 MB) |
| Oscillator can near the right edge, marked like `FOX… 16.000` (partly legible) | Q1 area | INFERRED 16 MHz → CPU clock 8 MHz |
| **2 × Toshiba TC5565APL-12** (8 K × 8 SRAM) | right, below the EPROMs | **16 KB work RAM** (KH says 16 KB too).  Fits the reset RAM test ranges `$40000–$43600` |
| **4 EPROM sockets IC30, IC34, IC38, IC43** | centre-bottom | Our chips are named after these sockets: `77POK30/34/38` = IC30/34/38, `PARA200J` = IC43 (INFERRED).  ⚠ Address order is IC30 `$00000`, **IC38 `$10000`, IC34 `$20000`**, PARA `$30000` (MEASURED via the ROM's module checksum, `docs/rom-set.md`) |
| **3 × EF6821P PIA** (IC2, IC4, IC5) | top row | Parallel I/O: buttons, lamps, the sound-board bus.  Matches the three PIA groups the harness found at `$FB014–$FB01F` |
| **3 × EF6850P ACIA** (IC3, IC37, IC42) | top left, bottom | Serial links, two of them next to the coin-mechanism (`RAHAKONEISTO`) and meter (`LASKINYKSIKKÖ`) connectors.  Matches the three serial pairs at `$FB002–$FB00B` |
| Ceramic resonator X1 + **2 × MC74HC4060** (IC8, IC9, 14-stage oscillator/dividers) | top centre | INFERRED source of the periodic PIA signals the boot waits for (system tick, input scan, watchdog).  The resonator frequency can't be read in the photo, so the harness's 100 Hz / 50 Hz / 400 ms values stay hypotheses |
| HCF4050, 74HC00/04/74/138/245/373 glue, four PALs labelled `5003/11`, `/13`, `/23`, `/33` | | Address decoding: the PAL equations are the true memory map |
| ULN2003AN drivers | | Lamp, meter and coin-mechanism drivers |
| Regulator on a large heatsink, bulk capacitors | left | Power |

**No MC68681 DUART is visible on this board.**  The `$C0000` device that `docs/rom-set.md`
reads as a 68681 must be on another board, or be a different part.  It is still unconfirmed.

Board-mounted controls and indicators: `STATUS` 7-segment display, `HALT` LED, LEDs under
`TÄYTÄ VOITONMAKSUKONE` ("refill the payout machine"), push buttons `ASKELLUS` (step),
`HUOLTO` (service) and `KYTKIN 1` (switch 1).  Connectors: `KÄYTTÖKYTKIMET` (operator switches),
`RAHAKONEISTO` (coin mechanism), `LASKINYKSIKKÖ` (meter unit), `KASSAMUISTI` (cash memory,
presumably the battery-backed module; cf. `$D0000–$D7FFF`), two ribbon connectors at the top,
and the edge connector.

### EPROM labels on the photographed board

| Socket | Label | Part |
|---|---|---|
| IC30 | `6455-3.6J1  5003/IC 30  3.7.90` | TMS 27C256 (32 KB) |
| IC34 | `6455-3.6J1  5003/IC 34  3.7.90` | TMS 27C256 (32 KB) |
| IC38 | `5003/IC38  6454-3.0  24.05.88` | NMC 27C256 (32 KB) |
| IC43 | `5003/IC43  6454-3.0  24.05.88` | NMC 27C256 (32 KB) |

The photographed board carries **a different software build from our set**: 32 KB EPROMs, and
two software numbers (6455 v3.6 J1 from 3 July 1990, 6454 v3.0 from 24 May 1988).  Our program
chips are 64 KB, with genuine code in both halves of IC30 and IC38.  So the sockets take either
size, and ours is another revision.  The `PARA…J` suffix and the `J1` suffix may be related.
`77POK38` contains the strings `PCB5502`, `PCB5501`, `PCB5003`, `PCB5002`: the program knows
several board revisions (MEASURED string; its use is unexamined).

## Game rules and controls (KH unless noted)

- Five-card draw against the machine.  The minimum win is two pairs or three of a kind (matches
  our pay tables).
- The joker is a wild card (implied by our five-of-a-kind row).
- Bets of 1–5 mk.  Maximum win: first **100 mk** machines, soon **200 mk** ones (matches
  `PARA200J`, and the user's recollection).  KH's RTP table (royal flush 100, etc.) is for a
  different variant and does not match our pay tables; ignore it.
- **Double-up (tuplaus):** after a win, double or collect.  One card is guessed as **big (8–13)**
  or **small (1–6)**, with ace = 1 (so 7 always loses; matches the footage's `56 TAI 89`
  prompt).  No jokers in the double-up.  It can be repeated; 2 mk doubled six times is 128 mk.
- **Payout button (voitonmaksu):** the first press moves a win up into the `VOITOT` (wins)
  counter, and a further press pays `VOITOT` out as coins from the hopper.  Buttons light up
  when they are usable.
- The shuffle/deal animation and the slow reveal of the fifth card were RAY's own touches.
- The double-up tune, "a cheerful piano rascal", was made by programmer **Kimmo Koskinen**.
  With a single sound chip, he had to work to get a bass line under the melody.  The two-note
  losing sound ("KOSH", HV: "GOSH") became a cultural reference.

## People

| Person | Role | Source |
|---|---|---|
| Kimmo Koskinen | Programmer; the double-up tune and sound work; later RAY–Veikkaus innovation manager, narrating the RAY video | KH, HV |
| Olli Hämäläinen | RAY product design manager in the 1980s; the screen layout was sketched on paper first | KH |

INFERRED: the initials block in the footage (`KJL JOLA OH KK MZ ES / RM PJP KP SL TT LM / EL JT
ML TP`, `PARA200J` `$1E36`) is the development team.  `KK` and `OH` fit the two names above.

## History (HV, KH)

- Project codename **VIRA** ("videorahapeli", video money game).  In development from 1985,
  and on sites across Finland from 1986.  Built at RAY's own facility in Leppävaara, Espoo.
- The idea came from a video poker machine that a RAY executive brought back from Las Vegas.
- Anecdote: a debug build once ended up in a machine and paid out all weekend from a deck of only
  hearts and spades.  So debug builds of the program with a restricted deck existed.
