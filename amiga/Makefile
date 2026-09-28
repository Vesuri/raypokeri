# Pokeri — Amiga cross-build (m68k-amiga-elf-gcc)
#
# Usage (from amiga/ with the shared toolchain on PATH via `. env.sh`):
#   make          — build out/Pokeri
#   make clean    — remove obj/ and out/
#   ./run.sh      — boot in FS-UAE (A1200, Kickstart 3.1; left mouse button quits)
#   ./debug.sh    — source-level debug via FS-UAE GDB stub
#
# Structure mirrors Rescue on Fractalus's amiga/Makefile: the code lives under
# $(REPO)/src (framework in src/platform/amiga/framework), this directory holds only
# build infrastructure.  Objects go to a flat obj/ keyed by basename (there are no
# basename collisions across the source trees); sources are located via vpath.
#
# The framework is the dA JoRMaS template (~/Documents/dA JoRMaS/Template/C++), hardware
# subset only.  Its *Assembler.s are kept PRISTINE (as the SAS/C build assembles them) and
# bridged for gcc/vasm at build time by the %Assembler.o rule below — copied verbatim from
# the template's Makefile, see framework/GCC-PORT.md.  ASSEMBLER is on by default
# (Util.h); `make NO_ASSEMBLER=1` builds the portable C++ bodies instead.

OUT      = out/Pokeri
# The AmigaOS executable itself is extension-less (`out/Pokeri`); OUT is also the stem for
# the build by-products next to it (.elf/.map/.s).
EXE      = $(OUT)
CC       = m68k-amiga-elf-gcc
AS       = m68k-amiga-elf-as
VASM     = vasmm68k_mot
OBJDUMP  = m68k-amiga-elf-objdump
SDKDIR   = $(abspath $(dir $(shell which $(CC)))../m68k-amiga-elf/sys-include)
SUPPORT  = $(abspath $(dir $(shell which $(CC)))../../support)

REPO      = ..
AMIGA_SRC = $(REPO)/src/platform/amiga
FRAMEWORK = $(AMIGA_SRC)/framework

$(shell mkdir -p obj out)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
CXX_SRCS := \
    $(FRAMEWORK)/AmigaHardware.cpp \
    $(FRAMEWORK)/Bitmap.cpp \
    $(FRAMEWORK)/CopperList.cpp \
    $(FRAMEWORK)/Palette.cpp \
    $(FRAMEWORK)/Sprite.cpp \
    $(FRAMEWORK)/Util.cpp \
    $(AMIGA_SRC)/GCCRuntime.cpp \
    $(AMIGA_SRC)/Pokeri.cpp \
    $(AMIGA_SRC)/Native.cpp \
    $(AMIGA_SRC)/NativeTiming.cpp \
    $(AMIGA_SRC)/PaulaAy.cpp \
    $(AMIGA_SRC)/AmigaScreen.cpp \
    $(AMIGA_SRC)/AmigaSurface.cpp \
    $(AMIGA_SRC)/AmigaInput.cpp \
    $(AMIGA_SRC)/NvramFile.cpp \
    $(AMIGA_SRC)/BoardRuntime.cpp \
    $(REPO)/src/native/Hook.cpp \
    $(REPO)/src/board/Board.cpp \
    $(REPO)/src/board/PlanarSurface.cpp \
    $(REPO)/src/board/Hd63484.cpp \
    $(REPO)/src/board/Hd63484Drawing.cpp \
    $(REPO)/src/board/CardBackCache.cpp \
    $(REPO)/src/board/SerialPeer.cpp \
    $(REPO)/src/board/AyAudio.cpp \
    $(REPO)/src/main.cpp

SUPPORT_C_SRCS := $(SUPPORT)/gcc8_c_support.c
# Validated cached raster completion; set CACHED_RASTER=0 for comparison.
CARD_CACHE ?= 1
CACHED_RASTER ?= $(CARD_CACHE)
# Exact cached parameter writes / relative moves; 0 retains raster-only.
CACHED_CONTROLS ?= $(CACHED_RASTER)
ASM_SRCS       := $(SUPPORT)/gcc8_a_support.s $(AMIGA_SRC)/version.s $(AMIGA_SRC)/NativeEntry.s
ifeq ($(CACHED_RASTER),1)
ASM_SRCS += $(AMIGA_SRC)/CachedRaster.s
endif

ifndef NO_ASSEMBLER
FRAMEWORK_ASM_SRCS := \
    $(FRAMEWORK)/UtilAssembler.s \
    $(FRAMEWORK)/AmigaHardwareAssembler.s \
    $(FRAMEWORK)/BitmapAssembler.s \
    $(FRAMEWORK)/CopperListAssembler.s
else
EXTRA_DEFINES += -DNO_ASSEMBLER
endif

vpath %.cpp $(sort $(dir $(CXX_SRCS)))
vpath %.s   $(sort $(dir $(ASM_SRCS) $(FRAMEWORK_ASM_SRCS)))
vpath %.c   $(SUPPORT)

CXX_OBJS  := $(addprefix obj/,$(notdir $(CXX_SRCS:.cpp=.o)))
C_OBJS    := $(addprefix obj/,$(notdir $(SUPPORT_C_SRCS:.c=.o)))
S_OBJS    := $(addprefix obj/,$(notdir $(ASM_SRCS:.s=.o)))
VASM_OBJS := $(addprefix obj/,$(notdir $(FRAMEWORK_ASM_SRCS:.s=.o)))
OBJECTS   := $(CXX_OBJS) $(C_OBJS) $(S_OBJS) $(VASM_OBJS)

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------
WARN      = -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable
# -m68000: the target is an A500.  68020-only constructs (scaled index, 32-bit mulu.l/
# divu.l, bitfield ops) trap on a 68000.  -msoft-float: no FPU.
CCFLAGS   = -g -MP -MMD -m68000 -msoft-float -O2 -nostdlib -fomit-frame-pointer \
            -fno-tree-loop-distribution -ffunction-sections -fdata-sections $(WARN)
CPPFLAGS  = $(CCFLAGS) $(EXTRA_DEFINES) -DPOKERI_FREESTANDING -fno-threadsafe-statics -fcheck-new -I"$(abspath $(AMIGA_SRC))/board-runtime" -fno-rtti -fno-exceptions -fno-use-cxa-atexit -std=gnu++17 \
            -fpermissive -Wno-write-strings -Wno-pointer-arith \
            -include "$(abspath $(FRAMEWORK))/SASCCompat.h" \
            -I"$(abspath $(FRAMEWORK))" -I"$(abspath $(FRAMEWORK))/compat-include" \
            -I"$(abspath $(AMIGA_SRC))" -I"$(abspath $(REPO)/src)"
ASFLAGS   = -mcpu=68000 -g --register-prefix-optional -I"$(SDKDIR)"
# Detailed direct-call counters are a separate diagnostic build, so normal
# execution pays no extra branches or writes for this distribution study.
ifeq ($(DISPATCH_PROFILE),1)
EXTRA_DEFINES += -DPOKERI_DISPATCH_COUNTS
ASFLAGS += --defsym POKERI_DISPATCH_COUNTS=1
endif
# Wall-time ledger (reserved CIA timer scopes, per-command/VBI records): diagnostic only.
ifeq ($(TIME_LEDGER),1)
EXTRA_DEFINES += -DPOKERI_TIME_LEDGER
ASFLAGS += --defsym POKERI_TIME_LEDGER=1
endif
# Validated bounded cached-card repair; CARD_DAMAGE=0 retains full invalidation.
CARD_DAMAGE ?= 1
ifeq ($(CARD_DAMAGE),1)
EXTRA_DEFINES += -DPOKERI_CARD_DAMAGE
endif
# Validated completed-card presentation; disable with CARD_PRESENT=0.
CARD_PRESENT ?= $(if $(filter 0,$(CARD_CACHE)),0,$(CARD_DAMAGE))
ifeq ($(CARD_PRESENT),1)
ifeq ($(CARD_CACHE),0)
$(error CARD_PRESENT requires CARD_CACHE)
endif
ifneq ($(CARD_DAMAGE),1)
$(error CARD_PRESENT requires CARD_DAMAGE=1)
endif
EXTRA_DEFINES += -DPOKERI_CARD_PRESENT
endif
# CPU reads can overlap disjoint display copies; writes still drain all DMA.
# READ_ONLY_DMA=0 retains the serialized comparison path.
READ_ONLY_DMA ?= 1
ifeq ($(READ_ONLY_DMA),1)
EXTRA_DEFINES += -DPOKERI_READ_ONLY_DMA
endif
# Keep original safe points inline in the live register-resident feeder.
# INLINE_BOUNDARY=0 retains the measured comparison path.
INLINE_BOUNDARY ?= 1
ifeq ($(INLINE_BOUNDARY),1)
ASFLAGS += --defsym POKERI_INLINE_BOUNDARY=1
endif
# Per-word observation counters are not part of guest/device state. Compile
# them into explicit diagnostic variants, with no branch in ordinary feeds.
ifneq ($(filter 1,$(FEED_COUNTERS) $(DISPATCH_PROFILE) $(TIME_LEDGER)),)
ASFLAGS += --defsym POKERI_FEED_COUNTS=1
endif
# Approved measured improvement; full feed/audio latency targets remain open.
ifeq ($(CACHED_CONTROLS),1)
ifneq ($(CACHED_RASTER),1)
$(error CACHED_CONTROLS requires CACHED_RASTER)
endif
EXTRA_DEFINES += -DPOKERI_CACHED_CONTROLS
endif
ifeq ($(CACHED_RASTER),1)
ifeq ($(CARD_CACHE),0)
$(error CACHED_RASTER requires CARD_CACHE)
endif
EXTRA_DEFINES += -DPOKERI_CACHED_RASTER
ASFLAGS += --defsym POKERI_CACHED_RASTER=1
endif
ifeq ($(CARD_CACHE),1)
VIDEO_INTERLEAVED = 1
EXTRA_DEFINES += -DPOKERI_CARD_CACHE
endif
ifeq ($(VIDEO_INTERLEAVED),1)
EXTRA_DEFINES += -DPOKERI_VIDEO_INTERLEAVED
endif
ifeq ($(CARD_OBSERVER),1)
EXTRA_DEFINES += -DPOKERI_CARD_OBSERVER

endif
LDFLAGS   = -Wl,--emit-relocs,--gc-sections,-Ttext=0,-Map=$(OUT).map

.PHONY: all clean audit

all: $(EXE) audit

$(EXE): $(OUT).elf
	$(info Elf2Hunk $@)
	@elf2hunk $(OUT).elf $(EXE)

$(OUT).elf: $(OBJECTS)
	$(info Linking $@)
	@$(CC) $(CCFLAGS) $(LDFLAGS) $(OBJECTS) -o $@
	@$(OBJDUMP) --disassemble --no-show-raw-ins -S $@ > $(OUT).s 2>/dev/null || true

# The 68000 has no 32-bit mul/div: a libgcc __mulsi3/__divsi3/... call in the image means
# some expression silently went 32-bit.  Fail the build rather than find it in a profile.
audit: $(OUT).elf
	@if $(OBJDUMP) -d $(OUT).elf | grep -E '__(u?div|u?mod|mul)si3' >/dev/null; then \
	    echo "audit: 32-bit software mul/div in $(OUT).elf:"; \
	    $(OBJDUMP) -d $(OUT).elf | grep -E '__(u?div|u?mod|mul)si3' | head; exit 1; \
	fi

obj/PaulaAy.o: generated/PaulaWaves.h

generated/PaulaWaves.h: $(REPO)/tools/paula_waves.py $(REPO)/tools/roms.py $(REPO)/rom/PARA200J
	python3 $(REPO)/tools/paula_waves.py

ifneq ($(filter 1,$(CARD_OBSERVER) $(CARD_CACHE)),)
obj/Native.o: generated/CardBackRecipe.h
generated/CardBackRecipe.h: $(REPO)/tools/card_back.py $(REPO)/tools/roms.py $(REPO)/host/main.cpp $(wildcard $(REPO)/src/board/*.cpp) $(wildcard $(REPO)/rom/*) $(REPO)/host/scenarios/play.inputs
	python3 $(REPO)/tools/card_back.py
endif

obj/Native.o: generated/NativeTables.h

generated/NativeTables.h: $(REPO)/tools/native_tables.py $(REPO)/src/native/ShuffleWait.h $(wildcard $(REPO)/host/tables/*.csv)
	$(MAKE) -C $(REPO) harness
	python3 $(REPO)/tools/native_tables.py

obj/%.o : %.cpp
	$(info C++  $(notdir $<))
	@$(CC) $(CPPFLAGS) -c -o $@ "$(abspath $<)"

obj/gcc8_c_support.o : gcc8_c_support.c
	$(info C    $(notdir $<))
	@$(CC) $(CCFLAGS) -c -o $@ "$<"

# The framework *Assembler.s are the same files the SAS/C build assembles, bridged for
# gcc/vasm at build time (verbatim from the template Makefile — see its comment block):
# SAS/C sections -> ELF, unused __CX[MD]33 imports dropped, `movec vbr,d0` emitted as its
# encoding, the a4 small-data base dropped, C++ static-data symbols SAS/C-mangled ->
# Itanium-mangled, cross-object `bsr _blitter*` -> `jsr` (elf2hunk can't represent
# R_68K_PC16 across objects), and dot-in-middle local labels -> underscores.
# -no-opt: keep branch sizes as written.
obj/%Assembler.o : %Assembler.s
	$(info VASM $(notdir $<) (gcc-bridged))
	@sed -E \
	    -e 's/^[[:space:]]*section[[:space:]]+code/\tsection\t.text,code/' \
	    -e 's/^[[:space:]]*section[[:space:]]+__MERGED,data/\tsection\t.data,data/' \
	    -e 's/^[[:space:]]*section[[:space:]]+__MERGED,bss/\tsection\t.bss,bss/' \
	    -e 's/bsr([[:space:]]+_blitter)/jsr\1/g' \
	    -e '/xref[[:space:]]+__CX[MD]33/d' \
	    -e 's/movec[[:space:]]+vbr,d0/dc.w\t$$4e7a,$$0801/' \
	    -e 's/__4Util\(a4\)/__4Util/g' \
	    -e 's/_blitterQueueBufferEnd__13AmigaHardware/_ZN13AmigaHardware21blitterQueueBufferEndE/g' \
	    -e 's/_blitterQueueToBeBlitted__13AmigaHardware/_ZN13AmigaHardware23blitterQueueToBeBlittedE/g' \
	    -e 's/_blitterQueueAddPosition__13AmigaHardware/_ZN13AmigaHardware23blitterQueueAddPositionE/g' \
	    -e 's/_blitterQueueBuffer__13AmigaHardware/_ZN13AmigaHardware18blitterQueueBufferE/g' \
	    -e 's/_hasQueuedBlits__13AmigaHardware/_ZN13AmigaHardware14hasQueuedBlitsE/g' \
	    -e 's/_octants__13AmigaHardware/_ZN13AmigaHardware7octantsE/g' \
	    -e 's/_sin__4Util/_ZN4Util3sinE/g' \
	    -e 's/_cos__4Util/_ZN4Util3cosE/g' \
	    -e 's/([A-Za-z0-9_])\.([A-Za-z][A-Za-z0-9_]+)/\1_\2/g' \
	    "$(abspath $<)" > obj/$*Assembler.gcc.s
	@$(VASM) -m68000 -Felf -quiet -x -no-opt -nowarn=62 -o $@ obj/$*Assembler.gcc.s

# ⚠ Must stay BELOW the %Assembler.o rule: Apple's GNU Make 3.81 takes the FIRST matching
# pattern rule, not the most specific, so above it this would assemble the pristine SAS/C .s
# with gas.
obj/%.o : %.s
	$(info ASM  $(notdir $<))
	@$(AS) $(ASFLAGS) -o $@ "$(abspath $<)"

clean:
	@$(RM) -r obj out

-include $(CXX_OBJS:.o=.d) $(C_OBJS:.o=.d)
