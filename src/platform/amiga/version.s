| AmigaOS version string for RAY Pokeri.
|
| Format (https://wiki.amigaos.net/wiki/Version_Strings):
|     $VER: <name> <version>.<revision> (<dd>.<mm>.<yyyy>)
|
| NOTHING REFERENCES THIS STRING, AND NOTHING MAY.  AmigaDOS `Version` and archive tools
| find it by SCANNING the file for "$VER: ", so it only has to be PRESENT in the load
| image.  That is exactly the shape --gc-sections deletes, so the section carries the ELF
| SHF_GNU_RETAIN flag ("R"), which keeps it with no relocation pointing at it.
| Re-verify after ANY change to LDFLAGS, elf2hunk flags or this file:
|     strings out/RAYPokeri | grep '\$VER:'
|
| The date is deliberately HARDCODED rather than stamped at build time, so identical input
| gives a byte-identical build (the standard "is this a stale build?" check).
	.section .rodata.version,"aR"
	.balign 2
.ifdef POKERI_WHD_DEBUG_MAP
    .globl pokeriVersionString
pokeriVersionString:
.endif
	.asciz "$VER: RAYPokeri 0.90 (30.09.2026)"
	.balign 2

| Retained writable startup descriptor, patched only by the WHDLoad slave:
| mode word (1 = WHDLoad), reserved word, and the slave's resload_SaveFile
| entry (D0=size, A0=name, A1=data; returns D0), or 0 for DOS saves.
	.section .data.whdload,"awR"
	.balign 4
	.ascii "POK!SAVE"
	.global pokeriWhdLoad,pokeriWhdSave
pokeriWhdLoad:
	.word 0,0
pokeriWhdSave:
	.long 0

| Diagnostic-only relocated section anchors; no probe instructions execute.
.ifdef POKERI_WHD_DEBUG_MAP
    .balign 4
    .globl nativeWhdDebugMap
nativeWhdDebugMap:
    .ascii "POK!DEBUGMAP0001"
    .long nativePlayReady,pokeriVersionString,pokeriWhdLoad,pendingFrames
    .long nativeWhdDebugMap
.endif
