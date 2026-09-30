# Visual reference (real-machine footage)

The only independent check on the HD63484 and AY models: a bug shared by the harness and the
Amiga build passes the RAM-equality gate (`docs/bringup-plan.md`), but it won't match footage.
Frames stay local in `ref/footage/` (git-ignored, not ours to distribute); this file records
what they show, with the source.

## `pokeri-200mk-2BI-eUaPCOc.mkv` — THE FINNISH VERSION: our ROM set  ⭐ primary reference

YouTube `2BI-eUaPCOc`, "200 mk pokeri, tuplaus kuudesta 192 markkaan" (a win of 6 mk doubled up
to 192 mk).  50.7 s, 1280×720 25 fps H.264 plus Opus audio; handheld phone camera, CRT at an
angle.  Supplied by the user 2026-09-24.  It matches our `PARA200J`: **every on-screen string
seen is in it** — hand names `$1CD1`, `TUPLAATKO` `$1D4C`, `VALITTU` `$1D5C`, `TAI` `$1D64`,
the initials block `$1E36` (MEASURED).

Timeline, initially sampled at 2 s; the opening deal is corrected from the
frame-indexed 2026-09-30 review below:

| t (s) | Screen |
|---|---|
| 0–1.6 | Existing face-up hand is covered and gathered back to the upper-left deck |
| roughly 1.6–3.2 | Sideways shuffle at the deck, then the next deal starts |
| 3.20–4.40 | Five backs are dealt to the lower row |
| 4.48–4.64 | Five faces appear: Q♠ 9♥ 5♣ 8♥ Q♣; pay table follows |
| 6–12 | Hold phase: `VALITTU` ("selected") under each held card in the bottom bar; unheld cards turn over and are redrawn |
| 12–14 | Win → double-up: bar turns **magenta**, `PANOS [ 6 ] TUPLAATKO` ("double?") |
| 14–40 | Double-up rounds: one card dealt face down from the deck, bar shows `[stake] 56 TAI 89` or a ticker (`123456 TAI 89JQK`-like), the stake doubles 6 → 12 → 24 → 48 → 96 → 192 |
| 40–46 | A 2♣ is revealed; a block of initials appears top centre (`KJL JOLA OH KK MZ ES` / `RM PJP KP SL TT LM` / `EL JT ML TP`) — developer credits or an Easter egg tied to the event; `VOITOT` blanks, then counts |
| 46–50 | Back to normal: pay table with amounts at bet 3 (yellow): `VIITOSET 120 MK`, `VÄRISUORA 90`, `NELOSET 45`, `TÄYSKÄSI 30`, `VÄRI 15`, `SUORA 12`, `KOLMOSET 6`, `KAKSI PARIA 6`; `VOITOT 110`; bar back to cyan |

**MEASURED timing qualification (2026-09-30):** the original 25 fps frames
bound the deal-to-first-face interval near 1.28 s and the five-face reveal
near 0.16 s. CRT afterglow and phone exposure limit accuracy; these are not
chip-completion or button-response measurements. See the phase-separated
[ACRTC timing study](acrtc-timing-study.md#frame-indexed-footage-comparison-2026-09-30)
for exact frame numbers, host comparison and Double observations.

Double-up rule, INFERRED from the `56 TAI 89` prompt and the TUPLAUS convention: guess small
(A–6) or big (8–K), with 7 losing.  Confirm in the code.

Screen composition (INFERRED from the footage; the harness measures the real values):

- **Black** playfield (the English variant is blue: a palette or parameter difference).
- **Top bar**, light cyan: `PELIT` (credits in games) boxed left, white on royal blue with a cyan
  frame; `PANOS` (bet) centre in blue text beside a yellow coin disc with the bet (3) in it;
  `VOITOT` (wins) boxed right.  The `PANOS` label greys out during double-up.
- **Bottom bar**: cyan in the base game, **magenta/lilac during double-up** (a colour-register
  change or a redraw; the harness will tell), holding `VALITTU` markers, or the double-up
  stake box and prompt.
- **Cards** large (5 across the lower half): white body, black/red pips on a cream centre panel,
  index top left and rotated bottom right.  The card back is a red lattice with a club motif.
  The deck top left shows a stacked-card offset as cards leave it.
- **Text**: one bitmap font, white or yellow, fixed height.  The pay table is 8 rows.
- **Colours** seen: black, white, royal blue, light cyan, cyan, yellow, red, cream, magenta/lilac,
  greyed blue — about 10.
- **Audio** is on the track (a loud room, but the machine's AY sounds for deal, hold and
  double-up are audible), extractable with `ffmpeg -vn` for the Phase 2 AY check.

## `english-paytable-youtube.png` — English version, idle/deal screen

A rough phone-camera-quality frame from a YouTube video (source supplied by the user,
2026-09-24).  ⚠ **It shows a DIFFERENT VARIANT from our ROM set**, so use it for style and
layout, not content:

| | Footage (English) | Our set (`PARA200J`, Finnish) |
|---|---|---|
| Hands paid | 9: Royal flush 200, Five-of-a-kind 100, Straight flush 40, Four-of-a-kind 15, Full house 7, Flush 4, Straight 3, Three-of-a-kind 2, Two pairs 2 (at bet 1.00) | 8: `KAKSI PARIA`, `KOLMOSET`, `SUORA`, `VÄRI`, `TÄYSKÄSI`, `NELOSET`, `VÄRISUORA`, `VIITOSET` — **no royal flush** (MEASURED, strings at `$1CD1`) |
| Money | `Credits` / `Bet` / `Wins` | `MK` (markka), `PANOS` (bet), `VOITOT` (wins), `PELIT` (credits/games) |

Five-of-a-kind in both implies a wild card (joker) in the deck.  All user-visible text is in
`PARA200J` (the program chips hold none), so the language/rules module is swappable
(DERIVED).

What the frame shows (INFERRED from a low-quality photo; confirm against the harness):

- **Layout:** a top bar with three boxed fields (Credits, Bet, Wins).  Below it, on a blue field,
  a face-down card top left (red-on-white back; likely the double-up card, cf. `TUPLAATKO` =
  "double?"), and the pay table right of it as two columns, hand name and amount.  Five large
  face-up cards along the bottom, then a pale bar with a short cyan marker on the right.
- **Colours:** roughly 8–10 distinct: black, white, royal blue (background), dark blue (field
  boxes), cyan (box borders, labels, marker), red (hearts/diamonds, card back), pale
  yellow/cream (card centre panels), light grey (bottom bar), and a yellow disc behind the bet
  value, which is a coin icon, not a video annotation (it appears in the Finnish footage
  below too).  That comfortably fits an A500's 16–32 colours, *if* the real palette is that
  small.
- **Text:** one bitmap font, blocky, all rows the same height.  About nine pay-table rows fill
  the height between the top bar and the cards.
- **Cards:** full pip layouts (e.g. the 8 of clubs), index corners top left and rotated bottom
  right, with the centre on a cream panel.  Drawn with HD63484 fills plus pattern/bitmap
  blits, presumably; the Phase 1 command histogram will tell.
