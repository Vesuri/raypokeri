## Pokeri — repository-level tools.
## There is no host game build: the Amiga executable is the product.
## The Amiga build:  cd amiga && . ./env.sh && make

.PHONY: all help roms roms-check program-image

all: help

help:
	@echo "Pokeri — RAY video poker (68008 + HD63484 + AY-3-8912) -> Amiga port"
	@echo
	@echo "  make roms [SRC=path.zip|dir]  verify your ROM dump and unpack it to rom/ (git-ignored)"
	@echo "  make roms-check               re-verify rom/"
	@echo "  make program-image            concatenate the three program chips -> disasm/program.bin"
	@echo
	@echo "The Amiga build:  cd amiga && . ./env.sh && make"

roms:
	python3 tools/roms.py $(SRC)

roms-check:
	python3 tools/roms.py --check

# The three program chips form one contiguous $00000-$2FFFF space (docs/rom-set.md):
# this is the flat image a Ghidra import (base $0, 68000 big-endian) starts from.
program-image: roms-check
	@mkdir -p disasm
	cat rom/77POK30 rom/77POK34 rom/77POK38 > disasm/program.bin
	@echo "disasm/program.bin: $$(wc -c < disasm/program.bin) bytes"
