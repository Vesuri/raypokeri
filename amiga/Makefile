# RAY Pokeri — Amiga cross-build (m68k-amiga-elf-gcc)
#
# Usage (from amiga/ with the shared toolchain on PATH via `. env.sh`):
#   make          — build out/RAYPokeri
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

OUT      = out/RAYPokeri
# The AmigaOS executable itself is extension-less (`out/RAYPokeri`); OUT is also the stem for
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
    $(AMIGA_SRC)/RuntimeStart.cpp \
    $(AMIGA_SRC)/Pokeri.cpp \
    $(AMIGA_SRC)/Native.cpp \
    $(AMIGA_SRC)/NativeTiming.cpp \
    $(AMIGA_SRC)/PaulaAy.cpp \
    $(AMIGA_SRC)/PaulaNoise.cpp \
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
ASM_SRCS := $(SUPPORT)/gcc8_a_support.s $(AMIGA_SRC)/version.s $(AMIGA_SRC)/NativeEntry.s \
    $(AMIGA_SRC)/NativeServiceRedirect.s $(AMIGA_SRC)/NativeMemset.s \
    $(AMIGA_SRC)/NativeVideoIrq.s $(AMIGA_SRC)/CachedRaster.s

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
            -fno-unroll-loops -fno-unroll-all-loops -fno-tree-loop-distribution -ffunction-sections -fdata-sections $(WARN)
CPPFLAGS  = $(CCFLAGS) $(EXTRA_DEFINES) -DPOKERI_FREESTANDING -fno-threadsafe-statics -fcheck-new -I"$(abspath $(AMIGA_SRC))/board-runtime" -fno-rtti -fno-exceptions -fno-use-cxa-atexit -std=gnu++17 \
            -fpermissive -Wno-write-strings -Wno-pointer-arith \
            -include "$(abspath $(FRAMEWORK))/SASCCompat.h" \
            -I"$(abspath $(FRAMEWORK))" -I"$(abspath $(FRAMEWORK))/compat-include" \
            -I"$(abspath $(AMIGA_SRC))" -I"$(abspath $(REPO)/src)"
ASFLAGS   = -mcpu=68000 -g --register-prefix-optional -I"$(SDKDIR)"

# Public packages omit diagnostic replay, benchmarks, self-tests, automatic
# inputs and research markers. Development builds retain them; clean when
# switching RELEASE.
RELEASE ?= 0
ifeq ($(RELEASE),1)
EXTRA_DEFINES += -DPOKERI_RELEASE
ASFLAGS += --defsym POKERI_RELEASE=1
endif
# Symbol map for the WHDLoad exit/Double tests (tools/test_whdload_*.py).
ifeq ($(WHD_DEBUG_MAP),1)
ASFLAGS += --defsym POKERI_WHD_DEBUG_MAP=1
endif
# Normal builds omit dormant profiling branches from hot services.
# Explicit profiling/ledger builds retain native-measure automatically.
PROFILE_SUPPORT ?= $(if $(filter 1,$(DISPATCH_PROFILE) $(TIME_LEDGER)),1,0)
ifeq ($(PROFILE_SUPPORT),0)
ifneq ($(filter 1,$(DISPATCH_PROFILE) $(TIME_LEDGER)),)
$(error PROFILE_SUPPORT=0 cannot use DISPATCH_PROFILE/TIME_LEDGER)
endif
EXTRA_DEFINES += -DPOKERI_NO_PROFILE_SUPPORT
ASFLAGS += --defsym POKERI_NO_PROFILE_SUPPORT=1
endif
# Per-site direct-call counters (host/native_dispatch_profile.py).
ifeq ($(DISPATCH_PROFILE),1)
EXTRA_DEFINES += -DPOKERI_DISPATCH_COUNTS
ASFLAGS += --defsym POKERI_DISPATCH_COUNTS=1
endif
# Wall-time ledger (reserved CIA timer scopes, per-command/VBI records).
ifeq ($(TIME_LEDGER),1)
EXTRA_DEFINES += -DPOKERI_TIME_LEDGER
ASFLAGS += --defsym POKERI_TIME_LEDGER=1
endif
# Three startup-boundary CIA TOD timestamps (startup-elapsed.gdb,
# test_whdload.py --expect-startup-profile).
ifeq ($(STARTUP_PROFILE),1)
EXTRA_DEFINES += -DPOKERI_STARTUP_PROFILE
endif
# Post-service VBI beam samples.
ifeq ($(VBI_LATENCY),1)
EXTRA_DEFINES += -DPOKERI_VBI_LATENCY
endif
# FS-UAE instruction-trace layout: Board storage inside the code hunk (trace.sh).
ifeq ($(TRACE_CODE),1)
EXTRA_DEFINES += -DPOKERI_TRACE_CODE
endif
# Keyboard-only winning/Double workloads.
ifeq ($(PAYOUT_SCENARIO),1)
EXTRA_DEFINES += -DPOKERI_PAYOUT_SCENARIO
endif
ifeq ($(DOUBLE_SCENARIO),1)
EXTRA_DEFINES += -DPOKERI_DOUBLE_SCENARIO
endif
# Live observation counts are diagnostic bookkeeping; replay always counts.
ifneq ($(filter 1,$(LIVE_INSTRUCTION_COUNTS) $(FEED_COUNTERS) $(DISPATCH_PROFILE) $(TIME_LEDGER)),)
EXTRA_DEFINES += -DPOKERI_LIVE_INSTRUCTION_COUNTS
ASFLAGS += --defsym POKERI_LIVE_INSTRUCTION_COUNTS=1
endif
# Per-word observation counters, compiled only into diagnostic variants.
ifneq ($(filter 1,$(FEED_COUNTERS) $(DISPATCH_PROFILE) $(TIME_LEDGER)),)
ASFLAGS += --defsym POKERI_FEED_COUNTS=1
endif
LDFLAGS   = -Wl,--emit-relocs,--gc-sections,-Ttext=0,-Map=$(OUT).map -Wl,--wrap=memset

.PHONY: all clean audit

all: $(EXE) audit
ifeq ($(RELEASE),1)
all: release-audit
.PHONY: release-audit
release-audit: $(EXE)
	python3 ../tools/check_release_code.py $(OUT).elf $(EXE) --objdump $(OBJDUMP)
endif

# Strip runtime HUNK symbols; retain the separate ELF/map for debugging.
$(EXE): $(OUT).elf
	$(info Elf2Hunk $@)
	@elf2hunk $(OUT).elf $(EXE) -s

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

# Card-back recipe and prepared artwork, generated locally from the ROMs.
obj/Native.o: generated/CardBackRecipe.h generated/CardBackPrepared.h
generated/CardBackPrepared.h: generated/CardBackRecipe.h $(REPO)/host/card_back_prepare.cpp $(REPO)/Makefile $(wildcard $(REPO)/src/board/*.h) $(wildcard $(REPO)/src/board/*.cpp)
	$(MAKE) -C $(REPO) card-back-prepared
generated/CardBackRecipe.h: $(REPO)/tools/card_back.py $(REPO)/tools/roms.py $(REPO)/host/main.cpp $(wildcard $(REPO)/src/board/*.h) $(wildcard $(REPO)/src/board/*.cpp) $(wildcard $(REPO)/rom/*) $(REPO)/host/scenarios/play.inputs
	python3 $(REPO)/tools/card_back.py

obj/Native.o: generated/NativeTables.h

generated/NativeTables.h: $(REPO)/tools/native_tables.py $(REPO)/src/native/ShuffleWait.h $(wildcard $(REPO)/host/tables/*.csv)
	$(MAKE) -C $(REPO) harness
	python3 $(REPO)/tools/native_tables.py

obj/%.o : %.cpp
	$(info C++  $(notdir $<))
	@$(CC) $(CPPFLAGS) -c -o $@ "$(abspath $<)"

obj/gcc8_c_support.o : gcc8_c_support.c Makefile
	$(info C    $(notdir $<))
	@$(CC) $(CCFLAGS) -D_start=pokeriUnusedSharedStart -c -o $@ "$<"

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
