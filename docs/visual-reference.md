# Visual reference (real-machine footage)

The only independent check on the HD63484 and AY models: a bug shared by the harness and the
Amiga build passes the RAM-equality gate (`docs/bringup-plan.md`), but it won't match footage.
Frames stay local in `ref/footage/` (git-ignored, not ours to distribute); this file records
what they show, with the source.

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
  yellow/cream (card centre panels), light grey (bottom bar).  A yellow blob on the bet value
  may be a video annotation.  That comfortably fits an A500's 16–32 colours, *if* the real
  palette is that small.
- **Text:** one bitmap font, blocky, all rows the same height.  About nine pay-table rows fill
  the height between the top bar and the cards.
- **Cards:** full pip layouts (e.g. the 8 of clubs), index corners top left and rotated bottom
  right, with the centre on a cream panel.  Drawn with HD63484 fills plus pattern/bitmap
  blits, presumably; the Phase 1 command histogram will tell.
