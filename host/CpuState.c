// Host-only: Musashi scalar context prefix, with live tables/callbacks retained.
// Files are pinned to this core ABI and host representation by the harness header.
#include "m68kcpu.h"
#include <stddef.h>
#include <string.h>
unsigned pokeri_cpu_state_size(void){return offsetof(m68ki_cpu_core,cyc_instruction);}
void pokeri_cpu_state(void *bytes,int load) {
    m68ki_cpu_core context;
    m68k_get_context(&context);
    if(load){memcpy(&context,bytes,pokeri_cpu_state_size());m68k_set_context(&context);}
    else memcpy(bytes,&context,pokeri_cpu_state_size());
}
