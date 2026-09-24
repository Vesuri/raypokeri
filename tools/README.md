# tools/

Everything here is reachable from a build, a documented workflow, or a `make` target — nothing is
scratch (one-off probes live in `tmp/`).  The file's own docstring/header has the usage.

| Tool | Role |
|---|---|
| `roms.py` | Verifies the user-supplied EPROM set against the recorded SHA-256s and unpacks it to `rom/` (git-ignored). `make roms`, `make roms-check`. |
| `native_tables.py` | Verifies the ROM set and generates ignored native operation/offset tables from committed access and relocation metadata. Invoked by the Amiga build; no ROM bytes are emitted. |
| `ghidra` | Symlink to the SHARED Ghidra install (`~/.local/share/ghidra`, also used by the Rescue on Fractalus and Vette repos). Git-ignored. |

Ghidra scripts live in `../ghidra_scripts/` (`MarkEntries` + `entrypoints.csv`, `ApplyNames` +
`../disasm/symbols.csv`, `ExportListing`, `DumpCallGraph`).
