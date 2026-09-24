#ifndef POKERI_FREESTANDING_STDINT
#define POKERI_FREESTANDING_STDINT
#ifndef __m68k__
#include_next <stdint.h>
#else
// Matches SASCCompat.h and the m68k Amiga ABI.
typedef char int8_t; typedef unsigned char uint8_t;
typedef short int16_t; typedef unsigned short uint16_t;
typedef long int32_t; typedef unsigned long uint32_t;
typedef long long int64_t; typedef unsigned long long uint64_t;
#endif
#endif
