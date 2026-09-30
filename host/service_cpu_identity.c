/* Test-only identity probe. This vendored revision's public CPU_TYPE getter
 * omits 68030; read the same internal field set by m68k_set_cpu_type without
 * modifying the vendor or assuming that the requested model was selected. */
#include "musashi/m68kcpu.h"
unsigned pokeri_service_cpu_type(void) {
    switch (m68ki_cpu.cpu_type) {
        case CPU_TYPE_000: return M68K_CPU_TYPE_68000;
        case CPU_TYPE_020: return M68K_CPU_TYPE_68020;
        case CPU_TYPE_030: return M68K_CPU_TYPE_68030;
        case CPU_TYPE_040: return M68K_CPU_TYPE_68040;
        default: return M68K_CPU_TYPE_INVALID;
    }
}
