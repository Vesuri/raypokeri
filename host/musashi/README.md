Vendored from https://github.com/kstenerud/Musashi at
`313ebf1bd9f4d0d93341eb5ce21fd8a119e9dbdd`.

Configuration is supplied by the host Makefile. Local core changes are
an optional POKERI_EXCEPTION_CALLBACK in m68ki_jump_vector and the IRQ path (m68kcpu.h),
so the harness can report the actual exception vector, including privilege
violations; expected software TRAPs are logged without stopping.
MIT permission notice retained in LICENSE, readme.txt and source headers.
SoftFloat retains its own notices. Host analysis only; never link into Amiga.

On BSD/macOS, synchronous bus-error unwinding uses `sigsetjmp(..., 0)` /
`siglongjmp`, matching the existing address-error path. It does not preserve a
host signal mask that the emulator callbacks never change. The prior `setjmp`
called signal-mask/alternate-stack syscalls on every `m68k_execute` invocation;
those dominated single-instruction CPU-oracle runs. Other platforms retain the
upstream setjmp/longjmp path. Neither guest exception handling nor cycles change.
