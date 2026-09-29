# Build-time card-cache preparation

Status: validated; `CARD_PREPARED=1` is the default (2026-09-29).

The existing card-back and shared white-card-prefix cache used to render and
prove its recipe twice on the Amiga at every launch. The build now runs the
same `CardBackCache::prepare()` on the host and embeds its output in the local
executable. This moves device-renderer work out of startup; it does not replace
original initialization, accounting, game instructions or sound scheduling.
Other artwork caches retain their existing policies.

## Data and ownership

`host/card_back_prepare.cpp` emits `amiga/generated/CardBackPrepared.h` only
from the verified local recipe and shared renderer. The generated header stays
ignored, alongside the ROM-derived recipe. No artwork bytes are committed.
Build dependencies cover ROM verification, recipe extraction, renderer sources
and headers, and the generator. Output is replaced atomically after successful
preparation. `make card-back-prepared` regenerates it when required.

At native startup, `installPrepared()` checks the format, exact recipe words,
command boundaries and complete entry context. It validates guard bounds,
drawing-work bounds and image coverage before writing any destination pixels.
It then copies the image/mask into the existing Chip allocation and installs the
same guards and command-progress records. There is no additional file I/O or
runtime hashing. Interrupted matches still restore the ordinary shadow renderer.
`CARD_PREPARED=0` retains runtime preparation for comparison; clean the build
when changing flags. `native-no-card-cache` retains its existing behavior.

## Measurements

**MEASURED:** paired A1200 cold/warm runs, same starting accounting and three
CIA-A TOD samples at 50 Hz. These include native preparation but exclude
executable loading and CRT work before preparation. Resolution is 20 ms.
Startup fast-forward is enabled in both columns; these savings are independent
of its earlier timing-policy change.

| Launch | Preparation before / after | Original initialization before / after | Total Ready before / after |
|---|---:|---:|---:|
| Cold | 3.12 / 0.70 s | 23.58 / 23.62 s | 26.70 / **24.32 s** |
| Retained accounting | 3.16 / 0.76 s | 9.70 / 9.70 s | 12.86 / **10.46 s** |

The profiled executable grows from 448,884 to 461,812 bytes (+12,928).
Both cold runs reach Ready at 47,120,000 board cycles and complete all 24 live
inputs, 30 shuffle steps and 60 in-motion AY writes. Warm runs reach Ready at
4,640,000 cycles, preserve retained accounting and insert zero coins; their
shorter 32-million-cycle budget covers four inputs, 30 shuffle steps and 60 AY
writes, not the full 24-input scenario.

**MEASURED:** prepared-cache ECS cold/warm totals are 118.54 / 55.38 s,
including 4.34 / 4.48 s preparation. These have no paired old-cache baseline and
are not an ECS speedup claim. Cold completes 24 inputs; warm completes four.
Both have 30 shuffle steps and 45 in-motion AY writes. All these live runs
finish without a native error, watchdog reset or Exec Alert; the verified CRT
post-destructor boundary has an empty heap.

Local evidence: `amiga/.run/card-prepared-{cold-before,cold-after,warm-before,
warm-after,cold-ecs,warm-ecs}` and `tmp/perf/Pokeri-card-prepared-{before,after}`.

## Correctness gates

**MEASURED:** the prepared 68000 data matches runtime preparation byte for byte:
11,200 image/mask bytes, 948 progress bytes and 272 guard bytes. Both report
8,652 covered pixels, 68 guards and a proved white prefix.

`make harness-prepared-card-check` compares the generated data with fresh
preparation and exercises the cache through the existing differential and
interruption tests. Twelve malformed descriptors fail before destination
writes. Ordinary mode covers 2,912 cases; raster mode covers 2,915 cases and
123,269 grant completions. Batch mode covers 2,088 cuts, 237,120 borrowed words,
510 partial re-admissions and 3,399 refusals. Host/platform/native-model suites
and the native software-arithmetic audit pass. Regeneration is byte-identical.

Exact ECS and AGA replay each match all 262,144 RAM bytes, 524,288 VRAM bytes,
172,064 pixels and 60 AY writes at 7,904,133 instructions / 64,000,000 cycles /
8,685 IRQs. Evidence: `tmp/card-prepared-{aga,ecs}-compare.log`,
`tmp/card-prepared-check2.log`, `tmp/card-prepared-headless.log` and
`amiga/.run/card-prepared-data-{before,after}`.

**MEASURED:** the normal uninstrumented release also completes a fresh cold
24-input run, 30 shuffle steps and 60 in-motion AY writes. At Ready its startup
fast-forward, audio mute, clock debt and queued ticks are all zero. It reaches
post-destructor cleanup with an empty heap and no reset or alert. Evidence:
`amiga/.run/card-prepared-release/gdb-out.log`.

The normal build excludes the startup timestamp instrumentation. Neither the
opt-in video IRQ shortcut nor its assembly frame experiment is enabled by this
change. Cold initialization still costs about 23.6 seconds; the startup parity,
20 ms card/audio and sustained gameplay real-time objectives remain open.
