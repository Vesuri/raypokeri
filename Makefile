## Pokeri — repository-level tools.
## The host harness is a research tool; the Amiga executable is the product.
## The Amiga build:  cd amiga && . ./env.sh && make

.PHONY: all help roms roms-check program-image harness-check

all: help

help:
	@echo "Pokeri — RAY video poker (68008 + HD63484 + AY-3-8912) -> Amiga port"
	@echo
	@echo "  make roms [SRC=path.zip|dir]  verify your ROM dump and unpack it to rom/ (git-ignored)"
	@echo "  make roms-check               re-verify rom/"
	@echo "  make harness                  build the host-only Musashi research harness"
	@echo "  make harness-check            run synthetic CPU/memory diagnostic checks"
	@echo "  make program-image            concatenate the three program chips -> disasm/program.bin"
	@echo
	@echo "The Amiga build:  cd amiga && . ./env.sh && make"

roms:
	python3 tools/roms.py $(SRC)

roms-check:
	python3 tools/roms.py --check

# The three program chips form one contiguous $00000-$2FFFF space in ADDRESS order 30, 38, 34 —
# not name order; the ROM's own module checksum passes only this way (docs/rom-set.md).
# This is the flat image a Ghidra import (base $0, 68000 big-endian) starts from.
program-image: roms-check
	@mkdir -p disasm
	cat rom/77POK30 rom/77POK38 rom/77POK34 > disasm/program.bin
	@echo "disasm/program.bin: $$(wc -c < disasm/program.bin) bytes"

# Musashi is exclusively a host research dependency. Generated sources stay in build/.
HOST_CC = clang
HOST_CXX = clang++
HOST_DEFS = -DM68K_EMULATE_INT_ACK=1 -DM68K_EMULATE_RESET=1 -DM68K_EMULATE_TRACE=1 -DM68K_INSTRUCTION_HOOK=1 -DM68K_EMULATE_ADDRESS_ERROR=1 -DM68K_EMULATE_010=0 -DM68K_EMULATE_EC020=0 -DM68K_EMULATE_020=0 -DM68K_EMULATE_030=0 -DM68K_EMULATE_040=0
HOST_FLAGS = -include host/musashi_hooks.h -O2 -g -Ihost/musashi -Ibuild $(HOST_DEFS) -MMD -MP
HOST_OBJS = build/m68kcpu.o build/m68kops.o build/m68kdasm.o build/softfloat.o build/main.o build/board.o build/hd63484.o build/hd63484drawing.o build/cardbackcache.o build/videooutput.o build/display.o build/serialpeer.o build/boardstate.o build/cpustate.o build/ayaudio.o build/wavoutput.o
.PHONY: harness
ifeq ($(SDL),1)
harness: build/pokeri-host-sdl
else
harness: build/pokeri-host
endif
build:
	mkdir -p $@
build/m68kmake: host/musashi/m68kmake.c | build
	$(HOST_CC) -O2 $< -o $@
build/m68kops.h: build/m68kmake host/musashi/m68k_in.c
	build/m68kmake build host/musashi/m68k_in.c
build/m68kops.c: build/m68kops.h
	@test -f $@ || build/m68kmake build host/musashi/m68k_in.c
build/m68kops.o: build/m68kops.c build/m68kops.h Makefile
	$(HOST_CC) $(HOST_FLAGS) -c $< -o $@
build/m68kcpu.o: host/musashi/m68kcpu.c build/m68kops.h Makefile
	$(HOST_CC) $(HOST_FLAGS) -c $< -o $@
build/m68kdasm.o: host/musashi/m68kdasm.c Makefile | build
	$(HOST_CC) $(HOST_FLAGS) -c $< -o $@
build/softfloat.o: host/musashi/softfloat/softfloat.c Makefile | build
	$(HOST_CC) $(HOST_FLAGS) -c $< -o $@
build/main.o: host/main.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@
build/pokeri-host: $(HOST_OBJS) build/window.o
	$(HOST_CXX) $^ -o $@
-include $(HOST_OBJS:.o=.d) build/feed-m68kcpu.d build/feed-m68kops.d

harness-check: harness-opcode-check build/musashi-bus-error-test build/pokeri-host build/board-test build/hd63484-test build/display-test build/reference-test build/output-panel-test
	build/pokeri-host --self-test
	build/board-test
	build/hd63484-test
	build/hd63484-test --interleaved
	build/display-test
	build/reference-test
	build/output-panel-test
	build/musashi-bus-error-test

build/board.o: src/board/Board.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/hd63484.o: src/board/Hd63484.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/board-test: src/ReadLatchedButtons.h src/AmigaKeyEvents.h host/board_test.cpp src/board/Board.cpp src/board/SerialPeer.cpp src/board/AyAudio.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/*.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/board_test.cpp src/board/Board.cpp src/board/SerialPeer.cpp src/board/AyAudio.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o $@

build/hd63484drawing.o: src/board/Hd63484Drawing.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/hd63484-test: host/hd63484_test.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/PlanarSurface.cpp src/board/*.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/hd63484_test.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/PlanarSurface.cpp -o $@

build/videooutput.o: host/VideoOutput.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/display.o: src/board/Display.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@
build/display-test: host/display_test.cpp src/board/Display.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/*.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/display_test.cpp src/board/Display.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o $@

build/serialpeer.o: src/board/SerialPeer.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/boardstate.o: src/board/BoardState.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@
build/cpustate.o: host/CpuState.c Makefile | build
	$(HOST_CC) $(HOST_FLAGS) -c $< -o $@

build/ayaudio.o: src/board/AyAudio.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@
build/wavoutput.o: host/WavOutput.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/window.o: host/Window.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@
build/window-sdl.o: host/Window.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -DPOKERI_SDL $$(sdl2-config --cflags) -c $< -o $@
build/pokeri-host-sdl: $(HOST_OBJS) build/window-sdl.o
	$(HOST_CXX) $^ $$(sdl2-config --libs) -o $@
-include build/window.d build/window-sdl.d

build/reference-test: src/Startup.h src/CabinetInput.h host/reference_test.cpp src/board/Board.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/AyAudio.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/*.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/reference_test.cpp src/board/Board.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/AyAudio.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o $@

.PHONY: harness-scenarios
harness-scenarios: build/pokeri-host
	python3 host/scenarios/check.py --verify

# Phase 3 preparation only: these do not establish relocation completeness.
.PHONY: harness-access-audit harness-access-check
harness-access-audit: build/pokeri-host
	python3 host/phase3_audit.py
harness-access-check: build/pokeri-host
	python3 host/phase3_check.py

build/relocation-test: host/relocation_test.cpp host/Relocation.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/relocation_test.cpp -o $@

.PHONY: harness-relocation-check
harness-relocation-check: build/pokeri-host build/relocation-test
	build/relocation-test
	python3 host/relocation_check.py

build/native-hook-test: host/native_hook_test.cpp src/native/Hook.cpp src/native/Hook.h src/native/PreparedHook.h build/m68kcpu.o build/m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/native_hook_test.cpp src/native/Hook.cpp build/m68kcpu.o build/m68kops.o build/softfloat.o -o $@
.PHONY: harness-native-check
harness-native-check: build/irq-cache-test build/retained-accounting-test build/shuffle-queue-test harness-memory-check build/live-clock-test build/native-hook-test build/board-runtime-test build/replay-test build/word-runtime-test build/sha256-test
	build/retained-accounting-test
	build/shuffle-queue-test
	build/native-hook-test
	build/live-clock-test
	build/irq-cache-test
	build/board-runtime-test
	build/replay-test
	build/word-runtime-test
	build/sha256-test

build/board-runtime-test: host/board_runtime_test.cpp src/platform/amiga/board-runtime/Support.h src/platform/amiga/board-runtime/stdint.h
	$(HOST_CXX) -std=c++14 -Wall -Wextra -nostdinc++ -Isrc/platform/amiga/board-runtime -fsanitize=address,undefined host/board_runtime_test.cpp -o $@


build/replay-test: host/replay_test.cpp host/Replay.h src/native/Replay.h
	$(HOST_CXX) -std=c++11 -Wall -Wextra -fsanitize=address,undefined host/replay_test.cpp -o $@

build/sha256-test: host/sha256_test.cpp src/native/Sha256.h
	$(HOST_CXX) -std=c++11 -Wall -Wextra -fsanitize=address,undefined host/sha256_test.cpp -o $@

build/word-runtime-test: host/word_runtime_test.cpp src/platform/amiga/BoardRuntime.cpp
	$(HOST_CXX) -std=c++11 -Wall -Wextra -fsanitize=address,undefined $^ -o $@

.PHONY: harness-platform-check
harness-platform-check: build/native-timing-test build/pattern-tile-test build/planar-test build/ay-backend-test build/frame-swap-test
	build/native-timing-test
	python3 host/native_card_cost.py --self-test
	build/pattern-tile-test
	build/planar-test
	build/planar-test --interleaved
	build/ay-backend-test
	build/frame-swap-test
build/planar-test: host/planar_test.cpp src/board/PlanarSurface.cpp src/board/Display.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/*.h Makefile | build
	clang++ -std=c++11 -Wall -Wextra -O2 host/planar_test.cpp src/board/PlanarSurface.cpp src/board/Display.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o $@
build/ay-backend-test: host/ay_backend_test.cpp src/platform/amiga/PaulaPeriods.h src/board/Board.cpp src/board/AyAudio.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/*.h Makefile | build
	clang++ -std=c++11 -Wall -Wextra -O2 host/ay_backend_test.cpp src/board/Board.cpp src/board/AyAudio.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o $@

build/live-clock-test: host/live_clock_test.cpp src/native/LiveClock.h src/board/WordMath.h | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -Isrc host/live_clock_test.cpp -o $@

# Requires the native cross-build and its objdump in PATH; no ROMs in the test.
build/native-short-flags-test: host/native_short_flags_test.cpp build/m68kcpu.o build/m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@
.PHONY: harness-short-check
harness-short-check: build/native-short-flags-test
	python3 host/native_short_check.py

build/output-panel-test: host/output_panel_test.cpp src/platform/amiga/OutputPanel.h src/board/WordMath.h | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -Isrc $< -o $@

# Linked native experiment versus an independent synthetic CPU sequence.
FEED_FLAGS = $(filter-out -DM68K_EMULATE_020=0,$(HOST_FLAGS)) -DM68K_EMULATE_020=1
build/feed-m68kcpu.o: host/musashi/m68kcpu.c build/m68kops.h Makefile
	$(HOST_CC) $(FEED_FLAGS) -c $< -o $@
build/feed-m68kops.o: build/m68kops.c build/m68kops.h Makefile
	$(HOST_CC) $(FEED_FLAGS) -c $< -o $@
build/native-feed-test: host/native_feed_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@
.PHONY: harness-feed-check
build/native-feed-loop-test: host/native_feed_loop_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@
harness-feed-check: build/native-feed-test build/native-feed-loop-test
	python3 host/native_feed_check.py

build/native-delay-test: host/native_delay_test.cpp src/native/DelayBudget.h build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -Isrc host/native_delay_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o -o $@
.PHONY: harness-delay-check
harness-delay-check: build/native-delay-test
	python3 host/native_delay_check.py

# ROM-dependent sound-bank checks: generated audio and manifests stay ignored.
.PHONY: harness-paula-check
harness-paula-check: build/paula-catalog-check
	python3 tools/paula_waves.py --manifest
	$(HOST_CXX) -std=c++11 -O2 host/paula_wave_test.cpp src/board/Board.cpp src/board/AyAudio.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o build/paula-wave-test
	build/paula-catalog-check
	build/paula-wave-test

build/paula-catalog-check: host/paula_catalog_check.cpp build/m68kcpu.o build/m68kops.o build/softfloat.o | build
	$(HOST_CXX) -std=c++11 -O2 -Ihost/musashi $^ -o $@

.PHONY: harness-paula-stream-check
harness-paula-stream-check: build/paula-stream-test
	python3 host/paula_stream_check.py
build/paula-stream-test: host/paula_stream_test.cpp build/m68kcpu.o build/m68kops.o build/softfloat.o | build
	$(HOST_CXX) -std=c++11 -O2 -Ihost/musashi $^ -o $@

build/frame-swap-test: host/frame_swap_test.cpp src/platform/FrameSwap.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/frame_swap_test.cpp -o $@

# Card-cache feasibility tools. The generic observer/catalog tests use no ROMs.
.PHONY: harness-card-check card-back-proof
harness-card-check: build/command-sequence-test
	build/command-sequence-test
	python3 host/card_back_catalog_test.py

build/command-sequence-test: host/command_sequence_test.cpp src/board/CommandSequenceObserver.h | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $< -o $@

card-back-proof: harness
	python3 tools/card_back.py
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/card_back_proof.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o build/card-back-proof
	build/card-back-proof

build/cardbackcache.o: src/board/CardBackCache.cpp src/board/CardBackCache.h Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

.PHONY: harness-card-cache-check
# Portable aggregate-state proof, before native batching is considered.
.PHONY: harness-cache-batch-check
harness-cache-batch-check: build/card-back-cache-test
	build/card-back-cache-test --raster-batch
	build/card-back-cache-test --raster-batch-native

harness-card-cache-check: build/card-back-cache-test
	build/card-back-cache-test

amiga/generated/CardBackRecipe.h: tools/card_back.py tools/roms.py host/main.cpp $(wildcard src/board/*.h) $(wildcard src/board/*.cpp) $(wildcard rom/*) host/scenarios/play.inputs
	python3 tools/card_back.py

build/card-back-cache-test: host/card_back_cache_test.cpp host/cached_raster_reference.h host/cached_batch_reference.h src/native/CachedBatch.h amiga/generated/CardBackRecipe.h $(wildcard src/board/*.h) $(wildcard src/board/*.cpp) | build
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra -Isrc host/card_back_cache_test.cpp src/board/BoardState.cpp src/board/Board.cpp src/board/AyAudio.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/PlanarSurface.cpp src/board/CardBackCache.cpp -o $@


# Execute original face-up producers; generated command data remains local.
.PHONY: harness-face-up-check
harness-face-up-check: build/face-up-catalog-check build/card-back-cache-test
	build/face-up-catalog-check tmp/faceup-catalog.words
	build/card-back-cache-test tmp/faceup-catalog.words
build/face-up-catalog-check: host/face_up_catalog_check.cpp amiga/generated/CardBackRecipe.h build/m68kcpu.o build/m68kops.o build/m68kdasm.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra $< build/m68kcpu.o build/m68kops.o build/m68kdasm.o build/softfloat.o -o $@

.PHONY: harness-shuffle-check
harness-shuffle-check: build/pokeri-host
	python3 host/shuffle_wait_check.py

.PHONY: harness-memory-check
harness-memory-check:
	python3 host/amiga_memory_test.py

.PHONY: harness-window-memory-check
harness-window-memory-check: build/window-memory-test
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/window-memory-test

build/window-memory-test: host/window_memory_test.cpp host/Window.cpp host/Window.h host/VideoOutput.cpp src/board/SerialPeer.cpp Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -DPOKERI_SDL -DSDL_CreateRenderer=pokeriTestCreateRenderer $$(sdl2-config --cflags) host/window_memory_test.cpp host/Window.cpp host/VideoOutput.cpp src/board/SerialPeer.cpp $$(sdl2-config --libs) -o $@


build/shuffle-queue-test: host/shuffle_queue_test.cpp src/native/ShuffleQueue.h src/board/Hd63484.h src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/shuffle_queue_test.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o $@

build/retained-accounting-test: host/retained_accounting_test.cpp src/RetainedAccounting.h
	@mkdir -p build
	$(HOST_CXX) -O2 -std=c++11 -Wall -Wextra $< -o $@

build/pattern-tile-test: host/pattern_tile_test.cpp src/board/Surface.h src/board/PlanarLayout.h
	@mkdir -p build
	$(HOST_CXX) -O2 -std=c++11 -Wall -Wextra $< -o $@

build/solid-color-test: host/pattern_tile_test.cpp src/board/Surface.h src/board/PlanarLayout.h
	@mkdir -p build
	$(HOST_CXX) -O2 -std=c++11 -Wall -Wextra -DPOKERI_SOLID_COLOR_PLANES $< -o $@
.PHONY: harness-solid-color-check
harness-solid-color-check: build/solid-color-test
	build/solid-color-test

# Host-only synchronous fault unwinding; no Amiga runtime dependency.
build/musashi-bus-error-test: host/musashi_bus_error_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@
.PHONY: harness-bus-error-check
harness-bus-error-check: build/musashi-bus-error-test
	build/musashi-bus-error-test

.PHONY: harness-startup-check
harness-startup-check: build/pokeri-host
	python3 host/startup_dwell_check.py

# Deterministic clock exercises the actual diagnostic Scope implementation.
build/native-timing-test: host/native_timing_test.cpp src/platform/amiga/NativeTiming.h | build
	$(HOST_CXX) -std=c++17 -Wall -Wextra -O2 -DPOKERI_TIME_LEDGER -DPOKERI_TIMING_TEST host/native_timing_test.cpp -o $@

# Standalone cached-raster grant/kernel proof; not a live feeder switch.
build/native-batch-test: host/native_batch_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@
.PHONY: harness-native-batch-check
harness-native-batch-check: build/native-batch-test
	python3 host/native_batch_check.py --elf amiga/out/Pokeri.elf

build/native-raster-test: host/native_raster_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@
.PHONY: harness-raster-check
harness-raster-check: build/native-raster-test build/card-back-cache-test
	build/card-back-cache-test --raster-grant
	build/card-back-cache-test --raster-controls
	build/card-back-cache-test --raster-absolute
	python3 host/native_raster_check.py

.PHONY: harness-card-damage-check
build/card-damage-test: host/card_damage_test.cpp src/platform/CardDamage.h src/board/WordMath.h | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/card_damage_test.cpp -o $@
harness-card-damage-check: build/card-damage-test
	build/card-damage-test

.PHONY: harness-fast-cache-ledger-check
build/fast-cache-ledger-test: host/card_back_cache_test.cpp host/cached_raster_reference.h host/cached_batch_reference.h src/native/CachedBatch.h amiga/generated/CardBackRecipe.h $(wildcard src/board/*.h) $(wildcard src/board/*.cpp) | build
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra -Isrc -DPOKERI_TIME_LEDGER -DPOKERI_LEDGER_FAST_CACHE host/card_back_cache_test.cpp src/board/BoardState.cpp src/board/Board.cpp src/board/AyAudio.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/PlanarSurface.cpp src/board/CardBackCache.cpp -o $@
harness-fast-cache-ledger-check: build/fast-cache-ledger-test
	build/fast-cache-ledger-test --raster-controls

.PHONY: harness-card-canvas-check
harness-card-canvas-check: build/card-canvas-test
	build/card-canvas-test

build/card-canvas-test: host/card_canvas_test.cpp $(wildcard src/board/*.cpp) $(wildcard src/board/*.h) | build
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra host/card_canvas_test.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp -o $@

# Opt-in native FIFO-control experiment; independent CPU and shared device model.
build/native-fifo-control-test: host/native_fifo_control_test.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/PlanarSurface.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@

.PHONY: harness-fifo-control-check
harness-fifo-control-check: build/native-fifo-control-test
	python3 host/native_fifo_control_check.py

.PHONY: harness-startup-budget-check
build/startup-budget-test: host/startup_budget_test.cpp src/native/StartupBudget.h src/native/DelayBudget.h src/board/WordMath.h | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -Isrc $< -o $@
harness-startup-budget-check: build/startup-budget-test
	build/startup-budget-test

.PHONY: harness-exception-frame-check
harness-exception-frame-check: build/native-exception-frame-test
	python3 host/native_exception_frame_check.py
build/native-exception-frame-test: host/native_exception_frame_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -Isrc host/native_exception_frame_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o -o $@

build/native-fifo-value-test: host/native_fifo_value_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@

.PHONY: harness-fifo-value-check
harness-fifo-value-check: build/native-fifo-value-test
	python3 host/native_fifo_value_check.py --elf amiga/out/Pokeri.elf

# Opt-in native video-IRQ shortcut; actual linked C/assembly and synthetic state.
build/native-video-irq-test: host/native_video_irq_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@
.PHONY: harness-video-irq-check
harness-video-irq-check: build/native-video-irq-test
	python3 host/native_video_irq_check.py --elf amiga/out/Pokeri.elf

# Local artwork only: the renderer and recipe must both precede generation.
build/card-back-prepare: host/card_back_prepare.cpp amiga/generated/CardBackRecipe.h $(wildcard src/board/*.h) $(wildcard src/board/*.cpp) | build
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra host/card_back_prepare.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o $@
amiga/generated/CardBackPrepared.h: build/card-back-prepare amiga/generated/CardBackRecipe.h Makefile
	python3 tools/roms.py --check
	build/card-back-prepare $@
.PHONY: card-back-prepared
card-back-prepared: amiga/generated/CardBackPrepared.h

build/prepared-card-test: host/card_back_cache_test.cpp host/cached_raster_reference.h host/cached_batch_reference.h src/native/CachedBatch.h amiga/generated/CardBackPrepared.h $(wildcard src/board/*.h) $(wildcard src/board/*.cpp) | build
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra -Isrc -DPOKERI_CARD_PREPARED host/card_back_cache_test.cpp src/board/BoardState.cpp src/board/Board.cpp src/board/AyAudio.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/PlanarSurface.cpp src/board/CardBackCache.cpp -o $@
.PHONY: harness-prepared-card-check
harness-prepared-card-check: build/prepared-card-test
	build/prepared-card-test
	build/prepared-card-test --raster-absolute
	build/prepared-card-test --raster-batch-native

build/native-handler-exit-test: host/native_handler_exit_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@
.PHONY: harness-handler-exit-check
harness-handler-exit-check: build/native-handler-exit-test
	python3 host/native_handler_exit_check.py

.PHONY: harness-handler-entry-check
harness-handler-entry-check: build/native-handler-entry-test
	python3 host/native_handler_entry_check.py
build/native-handler-entry-test: host/native_handler_entry_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@

# Initial Amiga/WHDLoad release. Use the normal build, never a diagnostic image.
.PHONY: release release-check
release:
	. amiga/env.sh && $(MAKE) -C amiga clean && $(MAKE) -C amiga
	$(MAKE) -C whdload
	python3 tools/package_release.py amiga/out/Pokeri dist
	python3 tools/check_release.py dist/Pokeri-$$(cat VERSION).lha
release-check:
	python3 tools/check_release.py dist/Pokeri-$$(cat VERSION).lha

build/irq-cache-test: host/irq_cache_test.cpp src/native/IrqCache.h src/board/Board.cpp src/board/AyAudio.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/*.h | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -Isrc host/irq_cache_test.cpp src/board/Board.cpp src/board/AyAudio.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o $@

# Optional Amiga memory-fill oracle; source amiga/env.sh for the assembler.
.PHONY: harness-memset-check
harness-memset-check: build/native-memset-test build/native-memset.o
	python3 host/native_memset_check.py
build/native-memset.o: src/platform/amiga/NativeMemset.s | build
	m68k-amiga-elf-as -m68000 -o $@ $<
build/native-memset-test: host/native_memset_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra $^ -o $@

.PHONY: harness-product-check
harness-product-check: build/native-product-test build/native-product-fixture.elf
	python3 host/native_product_check.py
build/native-product-fixture.elf: host/native_product_fixture.cpp src/board/WordMath.h | build
	m68k-amiga-elf-gcc -m68000 -std=c++11 -O2 -Isrc -Isrc/platform/amiga/board-runtime -fno-exceptions -fno-rtti -nostdlib -Wl,-Ttext=0x1000,-e,pokeriTickProduct $< -o $@
build/native-product-test: host/native_product_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra $^ -o $@

.PHONY: harness-word180-check
harness-word180-check: build/planar-word180-test
	build/planar-word180-test
	build/planar-word180-test interleaved
build/planar-word180-test: host/planar_test.cpp src/board/PlanarSurface.cpp src/board/Display.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/*.h Makefile | build
	clang++ -std=c++11 -Wall -Wextra -O2 -DPOKERI_COPY180_WORD_PLANES host/planar_test.cpp src/board/PlanarSurface.cpp src/board/Display.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o $@

# Share short-fill geometry across planes; independent pixel and edge oracle.
build/planar-small-fill-test: host/planar_test.cpp src/board/PlanarSurface.cpp $(wildcard src/board/*.h)
	@mkdir -p build
	$(HOST_CXX) -O2 -std=c++11 -Wall -Wextra -DPOKERI_SMALL_FILL_WORD_PLANES $< src/board/PlanarSurface.cpp -o $@
.PHONY: harness-small-fill-check
harness-small-fill-check: build/planar-small-fill-test
	build/planar-small-fill-test
	build/planar-small-fill-test interleaved

build/hd63484-dense-curve-test: host/hd63484_test.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/PlanarSurface.cpp $(wildcard src/board/*.h)
	@mkdir -p build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -DPOKERI_DENSE_CURVE_STAMPS $< src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/PlanarSurface.cpp -o $@
.PHONY: harness-dense-curve-check
harness-dense-curve-check: build/hd63484-dense-curve-test
	build/hd63484-dense-curve-test
	build/hd63484-dense-curve-test --interleaved

build/startup-quiet-test: host/startup_quiet_test.cpp src/native/StartupBudget.h src/board/WordMath.h
	@mkdir -p build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -Isrc $< -o $@
.PHONY: harness-startup-quiet-check
harness-startup-quiet-check: build/startup-quiet-test
	build/startup-quiet-test

build/startup-board-tick-test: host/startup_board_tick_test.cpp src/native/StartupBudget.h $(wildcard src/board/*.h) $(wildcard src/board/*.cpp)
	@mkdir -p build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -Isrc $< src/board/Board.cpp src/board/BoardState.cpp src/board/AyAudio.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp -o $@
.PHONY: harness-startup-board-tick-check
harness-startup-board-tick-check: build/startup-board-tick-test
	build/startup-board-tick-test

build/native-startup-delay-test: host/native_startup_delay_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -Ihost host/native_startup_delay_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o -o $@

# Build the opt-in native candidate first; ELF may point at a frozen build.
STARTUP_DELAY_ELF ?= amiga/out/Pokeri.elf
.PHONY: harness-startup-delay-check
harness-startup-delay-check: build/native-startup-delay-test
	python3 host/native_startup_delay_check.py --elf $(STARTUP_DELAY_ELF)

# Offline bus replay and payload sizing only; never linked into the Amiga build.
build/boot-artwork-study: host/boot_artwork_study.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/PlanarSurface.cpp $(wildcard src/board/*.h) | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/boot_artwork_study.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/PlanarSurface.cpp -o $@

# Research-only scheduler; no production device or timing policy changes.
build/acrtc-timing-fifo-test: host/acrtc_timing_fifo_test.cpp host/acrtc_timing_fifo.h | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $< -o $@
.PHONY: harness-acrtc-timing-check
harness-acrtc-timing-check: build/acrtc-timing-fifo-test
	build/acrtc-timing-fifo-test

build/acrtc-timing-device-test: host/acrtc_timing_device_test.cpp host/acrtc_timing_device.h host/acrtc_timing_fifo.h src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/PlanarSurface.cpp $(wildcard src/board/*.h) | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/acrtc_timing_device_test.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/PlanarSurface.cpp -o $@
.PHONY: harness-acrtc-device-check
harness-acrtc-device-check: build/acrtc-timing-device-test
	build/acrtc-timing-device-test

# Isolated research ABI: every C++ translation unit is rebuilt, never mixed with
# normal Board layouts. The CPU C objects contain no Board representation.
TIMING_SOURCES = host/main.cpp src/board/Board.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp host/VideoOutput.cpp src/board/Display.cpp src/board/SerialPeer.cpp src/board/BoardState.cpp src/board/AyAudio.cpp host/WavOutput.cpp host/Window.cpp
TIMING_OBJECTS = $(addprefix build/timing/,$(TIMING_SOURCES:.cpp=.o))
build/timing/%.o: %.cpp Makefile
	mkdir -p $(dir $@)
	$(HOST_CXX) $(HOST_FLAGS) -DPOKERI_HOST_ACRTC_TIMING=1 -std=c++11 -Wall -Wextra -c $< -o $@
build/pokeri-host-timing: $(TIMING_OBJECTS) build/m68kcpu.o build/m68kops.o build/m68kdasm.o build/softfloat.o build/cpustate.o
	$(HOST_CXX) $^ -o $@
-include $(TIMING_OBJECTS:.o=.d)
.PHONY: harness-acrtc-research
harness-acrtc-research: build/pokeri-host-timing

build/acrtc-timing-board-test: host/acrtc_timing_board_test.cpp $(filter-out build/timing/host/%.o,$(TIMING_OBJECTS))
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -DPOKERI_HOST_ACRTC_TIMING=1 $^ -o $@
.PHONY: harness-acrtc-board-check
harness-acrtc-board-check: build/acrtc-timing-board-test
	build/acrtc-timing-board-test

build/acrtc-duration-test: host/acrtc_duration_test.cpp host/acrtc_duration.h $(filter-out build/timing/host/%.o,$(TIMING_OBJECTS))
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -DPOKERI_HOST_ACRTC_TIMING=1 $(filter-out %.h,$^) -o $@
.PHONY: harness-acrtc-duration-check
harness-acrtc-duration-check: build/acrtc-duration-test
	build/acrtc-duration-test

build/acrtc-scenario-test: host/acrtc_scenario_test.cpp host/acrtc_scenario.h $(filter-out build/timing/host/%.o,$(TIMING_OBJECTS))
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -DPOKERI_HOST_ACRTC_TIMING=1 $(filter-out %.h,$^) -o $@
.PHONY: harness-acrtc-scenario-check
harness-acrtc-scenario-check: build/acrtc-scenario-test
	build/acrtc-scenario-test

build/native-sound-test: host/native_sound_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o src/board/Board.cpp src/board/AyAudio.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/CardBackCache.cpp src/board/*.h src/native/IrqCache.h
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 -Isrc $(filter %.cpp %.o,$^) -o $@
.PHONY: harness-sound-check
harness-sound-check: build/native-sound-test
	python3 host/native_sound_check.py

.PHONY: harness-handler-setup-check
harness-handler-setup-check: build/native-handler-setup-test
	python3 host/native_handler_setup_check.py
build/native-handler-setup-test: host/native_handler_setup_test.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@

# Host-only executed-opcode compatibility observations.
build/opcode-audit-test: host/opcode_audit_test.cpp host/OpcodeAudit.h build/m68kdasm.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/opcode_audit_test.cpp build/m68kdasm.o -o $@
.PHONY: harness-opcode-check
harness-opcode-check: build/opcode-audit-test
	build/opcode-audit-test
	python3 host/opcode_audit_test.py

.PHONY: harness-opcode-scenarios
harness-opcode-scenarios: build/pokeri-host harness-opcode-check
	python3 host/opcode_audit_scenarios.py

# W3 owns a separate CPU oracle with 030/040 enabled. Do not change the
# original 68000 game harness or other native test objects as a side effect.
SERVICE_CPU_FLAGS = $(filter-out -DM68K_EMULATE_020=0 -DM68K_EMULATE_030=0 -DM68K_EMULATE_040=0,$(HOST_FLAGS)) -DM68K_EMULATE_020=1 -DM68K_EMULATE_030=1 -DM68K_EMULATE_040=1
build/service-m68kcpu.o: host/musashi/m68kcpu.c build/m68kops.h Makefile
	$(HOST_CC) $(SERVICE_CPU_FLAGS) -c $< -o $@
build/service-m68kops.o: build/m68kops.c build/m68kops.h Makefile
	$(HOST_CC) $(SERVICE_CPU_FLAGS) -c $< -o $@
build/service-cpu-identity.o: host/service_cpu_identity.c Makefile
	$(HOST_CC) $(SERVICE_CPU_FLAGS) -c $< -o $@
-include build/service-m68kcpu.d build/service-m68kops.d build/service-cpu-identity.d

# W3 production primitive, isolated before live scheduler integration.
.PHONY: harness-service-redirect-check
harness-service-redirect-check: build/native-service-redirect-test build/native-service-redirect.o
	python3 host/native_service_redirect_check.py
build/native-service-redirect.o: src/platform/amiga/NativeServiceRedirect.s | build
	m68k-amiga-elf-as -m68000 -o $@ $<
build/native-service-redirect-test: host/native_service_redirect_test.cpp build/service-m68kcpu.o build/service-m68kops.o build/softfloat.o build/service-cpu-identity.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@

# Run this against an explicit frozen SERVICE_REDIRECT=1 executable.
build/native-service-entry-test: host/native_service_entry_test.cpp build/service-m68kcpu.o build/service-m68kops.o build/softfloat.o build/service-cpu-identity.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@

# Program exit status must survive all finalizers.
build/runtime-start-test: host/runtime_start_test.cpp build/service-m68kcpu.o build/service-m68kops.o build/softfloat.o build/service-cpu-identity.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@

# Whole original-handler reference; fixture outputs remain local in tmp/.
build/native-video-handler-reference: host/native_video_handler_reference.cpp build/feed-m68kcpu.o build/feed-m68kops.o build/softfloat.o
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 $^ -o $@
.PHONY: harness-video-handler-reference
harness-video-handler-reference: build/native-video-handler-reference
	python3 host/native_video_handler_reference.py
