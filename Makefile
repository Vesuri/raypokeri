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
HOST_OBJS = build/m68kcpu.o build/m68kops.o build/m68kdasm.o build/softfloat.o build/main.o build/board.o build/hd63484.o build/hd63484drawing.o build/videooutput.o build/display.o build/serialpeer.o build/boardstate.o build/cpustate.o build/ayaudio.o build/wavoutput.o
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
-include $(HOST_OBJS:.o=.d)

harness-check: build/pokeri-host build/board-test build/hd63484-test build/display-test build/reference-test build/output-panel-test
	build/pokeri-host --self-test
	build/board-test
	build/hd63484-test
	build/display-test
	build/reference-test
	build/output-panel-test

build/board.o: src/board/Board.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/hd63484.o: src/board/Hd63484.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/board-test: host/board_test.cpp src/board/Board.cpp src/board/SerialPeer.cpp src/board/AyAudio.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/*.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/board_test.cpp src/board/Board.cpp src/board/SerialPeer.cpp src/board/AyAudio.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp -o $@

build/hd63484drawing.o: src/board/Hd63484Drawing.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/hd63484-test: host/hd63484_test.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/PlanarSurface.cpp src/board/*.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/hd63484_test.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/PlanarSurface.cpp -o $@

build/videooutput.o: host/VideoOutput.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/display.o: src/board/Display.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@
build/display-test: host/display_test.cpp src/board/Display.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/*.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/display_test.cpp src/board/Display.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp -o $@

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

build/reference-test: host/reference_test.cpp src/board/Board.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/AyAudio.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/*.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/reference_test.cpp src/board/Board.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/AyAudio.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp -o $@

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
harness-native-check: build/live-clock-test build/native-hook-test build/board-runtime-test build/replay-test build/word-runtime-test build/sha256-test
	build/native-hook-test
	build/live-clock-test
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
harness-platform-check: build/planar-test build/ay-backend-test
	build/planar-test
	build/ay-backend-test
build/planar-test: host/planar_test.cpp src/board/PlanarSurface.cpp src/board/Display.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/*.h Makefile | build
	clang++ -std=c++11 -Wall -Wextra -O2 host/planar_test.cpp src/board/PlanarSurface.cpp src/board/Display.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp -o $@
build/ay-backend-test: host/ay_backend_test.cpp src/platform/amiga/PaulaPeriods.h src/board/Board.cpp src/board/AyAudio.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp src/board/*.h Makefile | build
	clang++ -std=c++11 -Wall -Wextra -O2 host/ay_backend_test.cpp src/board/Board.cpp src/board/AyAudio.cpp src/board/BoardState.cpp src/board/SerialPeer.cpp src/board/Hd63484.cpp src/board/Hd63484Drawing.cpp -o $@

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
