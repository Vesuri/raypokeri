# Framework upstream

The framework is the dA JoRMaS hardware subset described in `GCC-PORT.md`.

The canonical framework is AmigaXDev's `share/framework` (installed as `$AMIGA_FRAMEWORK`); this
copy was last compared with it at AmigaXDev commit 2e02af5. `python3 -m amigaxdev.framework diff
Pokeri` (with `amiga/env.sh` sourced) shows which files are identical, local fixes or fixes
still to take.
