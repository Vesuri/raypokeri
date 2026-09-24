# Default read-only probe for diag_run.sh: runs after the SIGINT breaks gdb's `continue`.
# The FS-UAE stub serves memory READS but silently drops WRITES — inject inputs from C.
# ⚠ gdb aborts the whole file at the first unknown symbol: keep this in step with the source.
continue
printf "vbiCount=%u\n", Pokeri::vbiCount
printf "pc=%08x\n", $pc
detach
quit
