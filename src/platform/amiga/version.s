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
	.asciz "$VER: Pokeri 0.1 (24.09.2026)"
	.balign 2
