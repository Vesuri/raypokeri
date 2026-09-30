# Native host cross-build: no Amiga assembler or build utilities required.
WHDLOAD ?= $(HOME)/.local/share/amiga/WHDLoad
NDK ?= $(HOME)/.local/opt/m68k-amiga-elf/sys-include
VASM ?= $(HOME)/.local/vasmm68k_mot
OUT := ../build/whdload

.PHONY: all test smoke boot-test load-test FORCE
all: $(OUT)/Pokeri.slave
test: $(OUT)/PokeriTest.slave
smoke: $(OUT)/Smoke.slave
boot-test: $(OUT)/BootTest.slave
load-test: $(OUT)/LoadTest.slave
$(OUT)/BootTest.slave: VASMFLAGS += -DBOOTONLY=1
$(OUT)/LoadTest.slave: VASMFLAGS += -DLOADONLY=1

$(OUT)/Smoke.slave: SmokeSlave.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -pic -x -devpac -Fhunkexe -nosym -I"$(WHDLOAD)/Include" -I"$(NDK)" -o $@ $<

$(OUT)/PokeriTest.slave: VASMFLAGS += -DTEST=1

# Reassemble the small slave on every invocation: SDK includes and assembler
# options must never leave an old diagnostic slave in a release package.
$(OUT)/Pokeri.slave $(OUT)/PokeriTest.slave $(OUT)/BootTest.slave $(OUT)/LoadTest.slave $(OUT)/SaveCallback.slave $(OUT)/SaveEarlyAbort.slave $(OUT)/SaveUncached.slave $(OUT)/AllUncached.slave: PokeriSlave.s FORCE
	@mkdir -p $(OUT)/include/lvo
	@for lib in dos exec graphics expansion; do ln -sf "$(NDK)/lvo/$${lib}_lib.i" "$(OUT)/include/lvo/$$lib.i"; done
	$(VASM) -m68020 -pic -x -devpac -Fhunkexe -nosym $(VASMFLAGS) -I$(OUT)/include -I"$(WHDLOAD)/Include" -I"$(WHDLOAD)/Src/sources" -I"$(NDK)" -o $@ $<

# Runner-free DOS save-pattern reproducer; never included in a release.
.PHONY: save-smoke
save-smoke: $(OUT)/SaveSmoke
$(OUT)/SaveSmoke: SaveSmoke.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -x -devpac -Fhunkexe -nosym -I"$(NDK)" -o $@ $<

.PHONY: offset-smoke
offset-smoke: $(OUT)/OffsetSmoke.slave
$(OUT)/OffsetSmoke.slave: SmokeSlave.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -pic -x -devpac -Fhunkexe -nosym -DOFFSET_SAVE=1 -I"$(WHDLOAD)/Include" -I"$(NDK)" -o $@ $<

.PHONY: save-update-smoke
save-update-smoke: $(OUT)/SaveUpdateSmoke
$(OUT)/SaveUpdateSmoke: SaveSmoke.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -x -devpac -Fhunkexe -nosym -DUPDATE_SAVE=1 -I"$(NDK)" -o $@ $<

.PHONY: big-save-smoke
big-save-smoke: $(OUT)/BigSaveSmoke.slave $(OUT)/BigOffsetSmoke.slave
$(OUT)/BigOffsetSmoke.slave: BIG_SAVE_FLAGS += -DOFFSET_SAVE=1
$(OUT)/BigSaveSmoke.slave $(OUT)/BigOffsetSmoke.slave: SmokeSlave.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -pic -x -devpac -Fhunkexe -nosym -DBIG_SAVE=1 $(BIG_SAVE_FLAGS) -I"$(WHDLOAD)/Include" -I"$(NDK)" -o $@ $<

.PHONY: examine-save-smoke
examine-save-smoke: $(OUT)/ExamineSaveSmoke.slave
$(OUT)/ExamineSaveSmoke.slave: SmokeSlave.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -pic -x -devpac -Fhunkexe -nosym -DBIG_SAVE=1 -DOFFSET_SAVE=1 -DEXAMINE_SAVE=1 -I"$(WHDLOAD)/Include" -I"$(NDK)" -o $@ $<

# Versioned diagnostic callback, deliberately incompatible with release descriptor.
.PHONY: save-callback-smoke
save-callback-smoke: $(OUT)/SaveCallbackSmoke $(OUT)/SaveCallback.slave
$(OUT)/SaveCallback.slave: VASMFLAGS += -DCALLBACK_SAVE=1
$(OUT)/SaveCallbackSmoke: SaveSmoke.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -x -devpac -Fhunkexe -nosym -DCALLBACK_SAVE=1 -I"$(NDK)" -o $@ $<

.PHONY: save-callback-no-read-smoke
save-callback-no-read-smoke: $(OUT)/SaveCallbackNoReadSmoke $(OUT)/SaveCallback.slave
$(OUT)/SaveCallbackNoReadSmoke: SaveSmoke.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -x -devpac -Fhunkexe -nosym -DCALLBACK_SAVE=1 -DNO_READ_SAVE=1 -I"$(NDK)" -o $@ $<

.PHONY: directory-save-smoke
directory-save-smoke: $(OUT)/DirectorySaveSmoke.slave
$(OUT)/DirectorySaveSmoke.slave: SmokeSlave.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -pic -x -devpac -Fhunkexe -nosym -DBIG_SAVE=1 -DDATA_DIRECTORY=1 -I"$(WHDLOAD)/Include" -I"$(NDK)" -o $@ $<

.PHONY: memory-save-smoke
memory-save-smoke: $(OUT)/MemorySaveSmoke.slave
$(OUT)/MemorySaveSmoke.slave: SmokeSlave.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -pic -x -devpac -Fhunkexe -nosym -DBIG_SAVE=1 -DDATA_DIRECTORY=1 -DGAME_MEMORY=1 -I"$(WHDLOAD)/Include" -I"$(NDK)" -o $@ $<

.PHONY: metadata-directory-save-smoke
metadata-directory-save-smoke: $(OUT)/MetadataDirectorySaveSmoke.slave
$(OUT)/MetadataDirectorySaveSmoke.slave: SmokeSlave.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -pic -x -devpac -Fhunkexe -nosym -DBIG_SAVE=1 -DDATA_DIRECTORY=1 -DEXAMINE_SAVE=1 -I"$(WHDLOAD)/Include" -I"$(NDK)" -o $@ $<

.PHONY: two-file-save-smoke
two-file-save-smoke: $(OUT)/TwoFileSaveSmoke.slave
$(OUT)/TwoFileSaveSmoke.slave: SmokeSlave.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68000 -pic -x -devpac -Fhunkexe -nosym -DBIG_SAVE=1 -DDATA_DIRECTORY=1 -DSECOND_SAVE=1 -I"$(WHDLOAD)/Include" -I"$(NDK)" -o $@ $<

.PHONY: save-early-abort-smoke
save-early-abort-smoke: $(OUT)/SaveEarlyAbort.slave
$(OUT)/SaveEarlyAbort.slave: VASMFLAGS += -DCALLBACK_SAVE=1 -DSAVE_EARLY_ABORT=1

.PHONY: save-uncached-test
save-uncached-test: $(OUT)/SaveUncached.slave
$(OUT)/SaveUncached.slave: VASMFLAGS += -I$(OUT)/cache-include -DDONT_CACHE_SAVES=1

.PHONY: all-uncached-test
all-uncached-test: $(OUT)/AllUncached.slave
$(OUT)/AllUncached.slave: VASMFLAGS += -I$(OUT)/cache-include -DDONT_CACHE_SAVES=1 -DDONT_CACHE_ALL=1

# SDK overlay is used only by the rejected/diagnostic cache-pattern experiments.
$(OUT)/SaveUncached.slave $(OUT)/AllUncached.slave: $(OUT)/cache-include/whdload/kick31.s
$(OUT)/cache-include/whdload/kick31.s: FORCE
	python3 ../tools/whdload_kick_include.py "$(WHDLOAD)/Src/sources/whdload/kick31.s" $@

# Bare exception-forwarding measurement; not part of all or release packaging.
.PHONY: exception-benchmark
exception-benchmark: $(OUT)/ExceptionBenchmark.slave
$(OUT)/ExceptionBenchmark.slave: ExceptionBenchmark.s FORCE
	@mkdir -p $(OUT)
	$(VASM) -m68020 -pic -x -devpac -Fhunkexe -nosym -I"$(WHDLOAD)/Include" -I"$(NDK)" -o $@ $<
