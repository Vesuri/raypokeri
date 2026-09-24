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
HOST_OBJS = build/m68kcpu.o build/m68kops.o build/m68kdasm.o build/softfloat.o build/main.o build/board.o build/hd63484.o
.PHONY: harness
harness: build/pokeri-host
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
build/pokeri-host: $(HOST_OBJS)
	$(HOST_CXX) $^ -o $@
-include $(HOST_OBJS:.o=.d)

harness-check: harness build/board-test
	build/pokeri-host --self-test
	build/board-test

build/board.o: src/board/Board.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/hd63484.o: src/board/Hd63484.cpp Makefile | build
	$(HOST_CXX) $(HOST_FLAGS) -std=c++11 -Wall -Wextra -c $< -o $@

build/board-test: host/board_test.cpp src/board/Board.cpp src/board/Hd63484.cpp src/board/*.h Makefile | build
	$(HOST_CXX) -std=c++11 -Wall -Wextra -O2 host/board_test.cpp src/board/Board.cpp src/board/Hd63484.cpp -o $@
