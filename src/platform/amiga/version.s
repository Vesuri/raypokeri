| AmigaOS version string for Pokeri.
|
| Format (https://wiki.amigaos.net/wiki/Version_Strings):
|     $VER: <name> <version>.<revision> (<dd>.<mm>.<yyyy>)
|
| NOTHING REFERENCES THIS STRING, AND NOTHING MAY.  AmigaDOS `Version` and archive tools
| find it by SCANNING the file for "$VER: ", so it only has to be PRESENT in the load
| image.  That is exactly the shape --gc-sections deletes, so the section carries the ELF
| SHF_GNU_RETAIN flag ("R"), which keeps it with no relocation pointing at it.
| Re-verify after ANY change to LDFLAGS, elf2hunk flags or this file:
|     strings out/Pokeri | grep '\$VER:'
|
| The date is deliberately HARDCODED rather than stamped at build time, so identical input
| gives a byte-identical build (the standard "is this a stale build?" check).
	.section .rodata.version,"aR"
	.balign 2
.ifdef POKERI_WHD_DEBUG_MAP
    .globl pokeriVersionString
pokeriVersionString:
.endif
	.asciz "$VER: Pokeri 0.3 (30.09.2026)"
	.balign 2

| Retained writable startup descriptor, patched only by the WHDLoad slave.
	.section .data.whdload,"awR"
	.balign 4
	.ascii "POK!SAVE"
	.global pokeriWhdLoad
pokeriWhdLoad:
	.word 0,0

| Diagnostic-only relocated section anchors; no probe instructions execute.
.ifdef POKERI_WHD_DEBUG_MAP
    .balign 4
    .globl nativeWhdDebugMap
nativeWhdDebugMap:
    .ascii "POK!DEBUGMAP0001"
    .long nativePlayReady,pokeriVersionString,pokeriWhdLoad,pendingFrames
    .long nativeWhdDebugMap
.endif
