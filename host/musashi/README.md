Vendored from https://github.com/kstenerud/Musashi at
`313ebf1bd9f4d0d93341eb5ce21fd8a119e9dbdd`.

Configuration is supplied by the host Makefile. The local core change is
an optional POKERI_EXCEPTION_CALLBACK in m68ki_jump_vector and the IRQ path (m68kcpu.h),
so the harness can report the actual exception vector, including privilege
violations; expected software TRAPs are logged without stopping.
MIT permission notice retained in LICENSE, readme.txt and source headers.
SoftFloat retains its own notices. Host analysis only; never link into Amiga.
