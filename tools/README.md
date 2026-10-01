# Tools

Usage is in each script's help/docstring. Test procedures live in
[docs/testing.md](../docs/testing.md), package policy in
[docs/release.md](../docs/release.md).

| Tool | Purpose |
| --- | --- |
| roms.py | Verify the four user-supplied chips; copy/unpack into ignored rom/ |
| native_tables.py | Verify ROMs and generate native descriptors and original patch guards into ignored headers |
| card_back.py | Extract/prove the translated card recipe and prepare local generated artwork |
| package_release.py | Assemble the exact installer package with authored blank save slots |
| check_release.py | Independent LH5 decompression, CRCs, allowlist and input-identity checks |
| check_release_code.py | Reject diagnostic code/markers and HUNK debug/symbol records |
| installer_icon.py | Build Amiga icons and installer tooltypes |
| test_release_installer.py | Run actual Installer 43 against isolated fresh/reuse/reinstall/remove/error fixtures |
| test_whdload.py | Isolated WHDLoad or standalone load/game/save/exit checks |
| test_whdload_keys.py | Human-operated emergency/normal exit verification |
| ghidra | Ignored symlink to the shared Ghidra installation; research only |

Generated patch guards and artwork do contain ROM-derived bytes. They are never
committed. The binary loads the original chips at runtime; the archive contains
no ROM files. Removed experimental tools remain available in Git history.

Ghidra entry points and names are authored metadata in
`ghidra_scripts/entrypoints.csv` and `disasm/symbols.csv`. Disassembly output stays
ignored; it is never transliterated into a replacement game implementation.
